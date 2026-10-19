#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hcore/hatomic.h"
#include "hcore/hbuf.h"
#include "hcore/hdef.h"
#include "hv/hloop.h"
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

#include "hmsg.h"
#include "hnode.h"

#define HNODE_REQUEST_TIMEOUT_MAX 10000  // 10S
#define HNODE_REQUEST_COUNT_INIT 16
#define HNODE_RECONN_INTERVAL 2
#define HROKER_LOG(FMT, ...) printf(FMT, ##__VA_ARGS__)

// typedef struct h

typedef struct hnode_ {
  uint16_t id;
  uint16_t server_port;

  uint8_t status;
  uint8_t reconn_cnt;
  uint8_t reconn_interval;  // s
  uint8_t reconn_max_retry_cnt;

  char server_ip[16];

  hhash_hnode_subscrib_handler_t* map_subpool;  // subscrib
  // hhash_intptr_t* map_reppool;//request

  hio_t* io;
  hloop_t* loop;
  htimer_t* timer_reconn;
  pthread_t thread_io;
  hatomic_uint32_t ato_req_seq;
  pthread_mutex_t map_subpool_mtx;
  // pthread_mutex_t map_reppool_mtx;
  int req_ctx_count;
  hrequest_ctx_t* req_ctx;

} hnode_t;

static uint64_t hcommon_clock_now_ns() {
  struct timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  return at.tv_sec * 1e9 + at.tv_nsec;
}
static double hcommon_clock_now_ms() {
  struct timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  double now = at.tv_sec * 1e3 + at.tv_nsec / 1e6;
  return now;
}
static void hnode_time_offset(struct timespec* ts, int mill) {
  ts->tv_nsec += (mill * 1e6);  // 100ms
  if (ts->tv_nsec >= 1000000000L) {
    ts->tv_sec++;
    ts->tv_nsec -= 1000000000L;
  }
}
static unpack_setting_t* hnode_unpack_setting_get() {
  static int flag_init = 0;
  static unpack_setting_t unpack_setting;
  if (0 == flag_init) {
    memset(&unpack_setting, 0, sizeof(unpack_setting_t));
    unpack_setting.package_max_length = 1024 * 1024;  // 单个包最大长度限制
    unpack_setting.mode = UNPACK_BY_LENGTH_FIELD;
    unpack_setting.length_field_offset =
        offsetof(hmsg_t, payload_size);  // 长度字段在包头部的偏移量
    unpack_setting.length_field_bytes = sizeof(
        ((hmsg_t*)0)->payload_size);  // 长度字段占几个字节（如 2 字节代表长度）
    unpack_setting.length_field_coding = ENCODE_BY_LITTEL_ENDIAN;  // 大端或小端
    unpack_setting.body_offset =
        sizeof(hmsg_t);  // 消息体起始偏移量（跳过长度字段本身）
    flag_init = 1;
  }
  return &unpack_setting;
}
static int hnode_topic_had_sub(hnode_t* node, int32_t topic_ex) {
  int r = HNODE_RET_OK;
  pthread_mutex_lock(&node->map_subpool_mtx);
  r = HIHASH_EXIST(node->map_subpool, topic_ex);
  pthread_mutex_unlock(&node->map_subpool_mtx);
  return r;
}

static void hnode_on_connect(hio_t* io);
static void hnode_on_close(hio_t* io);
static void hnode_on_respond(const hmsg_t* msg, void* p);
static int hnode_subscribe_i(hnode_t* node, uint8_t flag_data, uint16_t topic,
                            hnode_subscrib_handler_func_t handle_func,
                            void* handle_func_param);

static void hnode_reconnect(htimer_t* timer) {
  hnode_t* node = (hnode_t*)hevent_userdata(timer);
  if (HNODE_STATUS_DISCONNECT == hatomic_load(&node->status)) {
    hatomic_store(&node->status, HNODE_STATUS_CONNECTING);
    ++node->reconn_cnt;
    HROKER_LOG("[%d/%d]connecting...\n", node->reconn_cnt,
               node->reconn_max_retry_cnt);
    node->io =
        hloop_create_tcp_client(node->loop, node->server_ip, node->server_port,
                                hnode_on_connect, hnode_on_close);
    hio_set_context(node->io, node);
  }
}
static void hnode_on_close(hio_t* io) {
  HROKER_LOG("connection closed,error=%d\n", hio_error(io));
  hnode_t* node = (hnode_t*)hio_context(io);
  node->status = HNODE_STATUS_DISCONNECT;
}
static void hnode_on_read(hio_t* io, void* buf, int readbytes) {
  hmsg_t* msg = (hmsg_t*)buf;
  int32_t topic_ex = HMSG_TOPICEX_INMSG(msg);
  HROKER_LOG("recv msg,(%d)-(%08X)-(%d),bytes(%d)\n", msg->from, topic_ex,
             msg->to, readbytes);
  if (sizeof(hmsg_t) > readbytes ||
      (sizeof(hmsg_t) + msg->payload_size != readbytes)) {
    HROKER_LOG("recv invalid data, byte(%d)\n", readbytes);
    return;
  }

  hnode_t* node = (hnode_t*)hio_context(io);

  if (HMSG_FCMD_DATA & msg->flags) {
    HIHASH_ITERATE it = HIHASH_FIND(node->map_subpool, topic_ex);
    if (HIHASH_ITERATE_VALID(node->map_subpool, it)) {
      hnode_subscrib_handler_t handler = HIHASH_GETV(node->map_subpool, it);
      handler.handle(msg, handler.handle_param);
    }
  } else {
    HROKER_LOG("recv invalid msg,(%d)-(%08X)-(%d),bytes(%d)\n", msg->from,
               topic_ex, msg->to, readbytes);
  }
}
static void hnode_register(hnode_t* node) {
  hmsg_t msg = {.magic = HMSG_MAGIC,
                .version = 0,
                .flags = HMSG_FCMD_REGISTER,
                .topic = 0,
                .from = node->id,
                .to = HMSG_APPID_BROKER,
                .seq = 0,
                .payload_size = 0};
  hio_write(node->io, &msg, sizeof(hmsg_t));
}

static void hnode_on_status(const hmsg_t* msg, void* p) {
  hnode_t* node = (hnode_t*)p;
  hmsg_t msg_ret = {.magic = HMSG_MAGIC,
                    .version = 0,
                    .flags = HMSG_FLAGS(HMSG_FCMD_DATA, HMSG_FDATA_REQREP),
                    .topic = msg->topic,
                    .from = node->id,
                    .to = msg->from,
                    .seq = msg->seq,
                    .payload_size = 0};
  hbuf_t buf;
  hbuf_init(&buf, sizeof(hmsg_t) * 2);
  hbuf_push(&buf, &msg_ret, sizeof(msg_ret));

  uint32_t payload_size = 0;
  pthread_mutex_lock(&node->map_subpool_mtx);
  HIHASH_ITERATE it = HIHASH_ITERATE_BEGIN(node->map_subpool);
  while (HIHASH_ITERATE_VALID(node->map_subpool, it)) {
    uint16_t tpc = HIHASH_GETK(node->map_subpool, it);
    hbuf_push(&buf, &tpc, sizeof(tpc));
    it = HIHASH_ITERATE_NEXT(it);
    payload_size += sizeof(tpc);
  }
  pthread_mutex_unlock(&node->map_subpool_mtx);
  hmsg_t* p_msg_ret = (hmsg_t*)buf.data;
  p_msg_ret->payload_size = payload_size;

  hio_write(node->io, buf.data, buf.len);
  hbuf_fini(&buf);

  HROKER_LOG("respond status:(%d)-(%08X)-(%d)\n", msg_ret.from, HMSG_TOPICEX_INMSG(p_msg_ret),
             msg_ret.to);
}

static void hnode_on_connect(hio_t* io) {
  hnode_t* node = (hnode_t*)hio_context(io);
  int err = hio_error(io);
  if (err != 0) {
    HROKER_LOG("connect failed: %d\n", err);
    hatomic_store(&node->status, HNODE_STATUS_DISCONNECT);
    return;
  }

  node->reconn_cnt = 0;
  hnode_register(node);
  hio_set_unpack(io, hnode_unpack_setting_get());
  hio_setcb_read(io, hnode_on_read);
  hio_read_start(io);
  hatomic_store(&node->status, HNODE_STATUS_CONNECTED);
  HROKER_LOG("connected: %d\n", node->id);
  //
  hnode_subscribe_i(node, HMSG_FDATA_REQREP,
                    HMSG_MSGID_STATUS, hnode_on_status, node);
}

static void* hnode_thread_io_main(void* p) {
  hnode_t* node = (hnode_t*)p;
  HROKER_LOG("thread start\n");
  pthread_mutex_init(&node->map_subpool_mtx, NULL);
  HIHASH_INIT(node->map_subpool);

  node->loop = hloop_new(0);
  // create reconnect timer
  node->timer_reconn = htimer_add(node->loop, hnode_reconnect,
                                  node->reconn_interval * 1000, INFINITE);
  hevent_set_userdata(node->timer_reconn, node);

  hatomic_store(&node->status, HNODE_STATUS_DISCONNECT);
  hnode_reconnect(node->timer_reconn);

  hloop_run(node->loop);

  // remove timer
  htimer_del(node->timer_reconn);
  node->timer_reconn = NULL;

  hloop_free(&node->loop);
  HIHASH_FINI(node->map_subpool);
  HROKER_LOG("thread stop\n");
  return 0;
}

hnode_t* hnode_init(uint16_t id, const char* svr_ip, uint16_t svr_port,
                    int request_count) {
  hnode_t* node = (hnode_t*)calloc(1, sizeof(hnode_t));
  node->id = id;
  node->status = HNODE_STATUS_INIT;
  strncpy(node->server_ip, svr_ip, sizeof(node->server_ip) - 1);
  node->server_port = svr_port;
  node->req_ctx_count = HNODE_REQUEST_COUNT_INIT;
  HC_GETMINPOW(request_count, node->req_ctx_count);
  node->req_ctx =
      (hrequest_ctx_t*)calloc(node->req_ctx_count, sizeof(hrequest_ctx_t));
  for (int i = 0; i < node->req_ctx_count; ++i) {
    hrequest_ctx_t* ctx = node->req_ctx + i;
    pthread_mutex_init(&ctx->mtx, NULL);
    pthread_cond_init(&ctx->cv, NULL);
  }
  node->reconn_cnt = 0;
  if (0 == node->reconn_interval) {
    node->reconn_interval = HNODE_RECONN_INTERVAL;
    node->reconn_max_retry_cnt = 10;
  }

  pthread_create(&node->thread_io, NULL, hnode_thread_io_main, node);
  return node;
}
int hnode_destroy(hnode_t* node) {
  if (node) {
    pthread_t tid = pthread_self();
    if (tid == node->thread_io) {
      HROKER_LOG("should not be called in io-thread\n");
      return 1;
    }

    hloop_stop(node->loop);
    pthread_join(node->thread_io, NULL);
    pthread_mutex_destroy(&node->map_subpool_mtx);

    for (int i = 0; i < node->req_ctx_count; ++i) {
      hrequest_ctx_t* ctx = node->req_ctx + i;
      pthread_mutex_destroy(&ctx->mtx);
      pthread_cond_destroy(&ctx->cv);
    }
    free(node->req_ctx);

    free(node);
  }
  return HNODE_RET_OK;
}
int hnode_status(hnode_t* node) { return hatomic_load(&node->status); }

int hnode_publish(hnode_t* node, uint16_t topic, uint16_t to_node_id,
                  const void* payload, uint32_t payload_size) {
  if (hatomic_load(&node->status) != HNODE_STATUS_CONNECTED) {
    return HNODE_RET_EDISCONNECT;
  }
  hmsg_t* msg = (hmsg_t*)calloc(1, sizeof(hmsg_t) + payload_size);
  msg->magic = HMSG_MAGIC;
  msg->version = 0;
  msg->flags = HMSG_FLAGS(HMSG_FCMD_DATA, HMSG_FDATA_PUBSUB);
  msg->topic = topic;
  msg->from = node->id;
  msg->to = to_node_id;
  msg->seq = 0, msg->payload_size = payload_size;
  if (payload_size) {
    memcpy(msg->payload, payload, payload_size);
  }

  hio_write(node->io, msg, sizeof(hmsg_t) + payload_size);
  free(msg);
  return HNODE_RET_OK;
}

static int hnode_subscribe_i(hnode_t* node, uint8_t flag_data, uint16_t topic,
                             hnode_subscrib_handler_func_t handle_func,
                             void* handle_func_param) {
  if (hatomic_load(&node->status) != HNODE_STATUS_CONNECTED) {
    return HNODE_RET_EDISCONNECT;
  }

  uint8_t flags = HMSG_FLAGS(HMSG_FCMD_DATA,flag_data);
  int32_t topic_ex = HMSG_TOPICEX(0, flags, topic);
  if (HNODE_RET_OK == hnode_topic_had_sub(node, topic_ex)) {
    return HNODE_RET_OK;
  }

  flags = HMSG_FLAGS(HMSG_FCMD_SUB,flag_data);
  hmsg_t msg = {.magic = HMSG_MAGIC,
                .version = 0,
                .flags = flags,
                .topic = topic,
                .from = node->id,
                .to = HMSG_APPID_BROKER,
                .seq = 0,
                .payload_size = 0};
  hnode_subscrib_handler_t handler = {handle_func, handle_func_param};
  int r = hio_write(node->io, &msg, sizeof(hmsg_t));
  if (r) {
    pthread_mutex_lock(&node->map_subpool_mtx);
    HIHASH_SET(node->map_subpool, topic_ex, handler);
    pthread_mutex_unlock(&node->map_subpool_mtx);
    return HNODE_RET_OK;
  }

  HROKER_LOG("hio_write failed: %d\n", hio_error(node->io));
  return HNODE_RET_ESEND;
}

int hnode_unsubscribe_i(hnode_t* node, uint16_t flags, uint16_t topic) {
  if (hatomic_load(&node->status) != HNODE_STATUS_CONNECTED) {
    return HNODE_RET_EDISCONNECT;
  }

  if (HNODE_RET_OK != hnode_topic_had_sub(node, topic)) {
    return HNODE_RET_OK;
  }

  // uint8_t flags = HMSG_FLAGS(HMSG_FCMD_UNSUB,HMSG_FDATA_PUBDATA)
  int32_t topic_ex = HMSG_TOPICEX(0, flags, topic);
  hmsg_t msg = {.magic = HMSG_MAGIC,
                .version = 0,
                .flags = flags,
                .topic = topic,
                .from = node->id,
                .to = HMSG_APPID_BROKER,
                .seq = 0,
                .payload_size = 0};
  int r = hio_write(node->io, &msg, sizeof(hmsg_t));
  if (r) {
    pthread_mutex_lock(&node->map_subpool_mtx);
    HIHASH_DEL(node->map_subpool, topic_ex);
    pthread_mutex_unlock(&node->map_subpool_mtx);
    return HNODE_RET_OK;
  }

  HROKER_LOG("hio_write failed: %d\n", hio_error(node->io));
  return HNODE_RET_ESEND;
}

int hnode_subscribe(hnode_t* node, uint16_t topic,
                    hnode_subscrib_handler_func_t handle_func,
                    void* handle_func_param) {
  return hnode_subscribe_i(node, HMSG_FDATA_PUBSUB,
                           topic, handle_func, handle_func_param);
}

int hnode_unsubscribe(hnode_t* node, uint16_t topic) {
  return hnode_unsubscribe_i(
      node, HMSG_FLAGS(HMSG_FCMD_UNSUB, HMSG_FDATA_PUBSUB), topic);
}

static void hnode_on_respond(const hmsg_t* msg, void* p) {
  hnode_t* node = (hnode_t*)p;
  hrequest_ctx_t* ctx = &node->req_ctx[msg->seq & (node->req_ctx_count - 1)];
  int ctx_status = hatomic_load(&ctx->status);
  if (HREQUEST_CTX_STATUS_BUSY == ctx_status && msg->seq == ctx->seq) {
    pthread_mutex_lock(&ctx->mtx);
    hbuf_push(ctx->rep, msg->payload, msg->payload_size);
    hatomic_store(&ctx->status, HREQUEST_CTX_STATUS_IDLE);
    pthread_cond_signal(&ctx->cv);
    pthread_mutex_unlock(&ctx->mtx);
  } else {
    HROKER_LOG("respond not find ctx, (%d)-(%d)-(%d)\n", msg->from, msg->topic,
               msg->to);
  }
}

static hrequest_ctx_t* hnode_register_request_ctx(hnode_t* node, uint32_t seq,
                                                  hbuf_t* rep) {
  hrequest_ctx_t* ctx = calloc(1, sizeof(hrequest_ctx_t));
  pthread_mutex_init(&ctx->mtx, NULL);
  pthread_cond_init(&ctx->cv, NULL);
  ctx->rep = rep;
  ctx->seq = seq;
  ctx->status = 0;

  // pthread_mutex_lock(&node->map_reppool_mtx);
  // HIHASH_SET(node->map_reppool, seq, (intptr_t)ctx);
  // pthread_mutex_unlock(&node->map_reppool_mtx);
  return ctx;
}
static void hnode_unregister_request_ctx(hnode_t* node, hrequest_ctx_t* ctx) {
  pthread_mutex_destroy(&ctx->mtx);
  pthread_cond_destroy(&ctx->cv);

  // pthread_mutex_lock(&node->map_reppool_mtx);
  // HIHASH_DEL(node->map_reppool, ctx->seq);
  // pthread_mutex_unlock(&node->map_reppool_mtx);
  free(ctx);
}

int hnode_request(hnode_t* node, uint16_t topic, uint16_t to_node_id,
                  hbuf_t* req, hbuf_t* rep, uint32_t timeout) {
  if (!node || !req || !rep) {
    return HNODE_RET_EPARAM;
  }
  if (HNODE_STATUS_CONNECTED != node->status) {
    return HNODE_RET_EDISCONNECT;
  }
  if (HNODE_REQUEST_TIMEOUT_MAX < timeout) {
    timeout = HNODE_REQUEST_TIMEOUT_MAX;
    HROKER_LOG("timeout exceed max(%d), set to max\n",
               HNODE_REQUEST_TIMEOUT_MAX);
  }
  int ret = hnode_subscribe_i(node, HMSG_FDATA_REQREP,
                             topic, hnode_on_respond, node);

  // get ctx
  uint32_t seq = hatomic_inc(&node->ato_req_seq, 1);
  hrequest_ctx_t* ctx = &node->req_ctx[seq & (node->req_ctx_count - 1)];
  int req_ctx_status_expect = HREQUEST_CTX_STATUS_IDLE;
  if (!__atomic_compare_exchange_n(&ctx->status, &req_ctx_status_expect,
                                   HREQUEST_CTX_STATUS_BUSY, false,
                                   __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
    return HNODE_RET_EBUSY;
  }
  ctx->seq = seq;
  ctx->rep = rep;
  hbuf_clear(ctx->rep);

  // send request
  hmsg_t* msg = (hmsg_t*)calloc(1, sizeof(hmsg_t) + req->len);
  msg->magic = HMSG_MAGIC;
  msg->version = 0;
  msg->flags = HMSG_FLAGS(HMSG_FCMD_DATA, HMSG_FDATA_REQREP);
  msg->topic = topic;
  msg->from = node->id;
  msg->to = to_node_id;
  msg->seq = seq;
  msg->payload_size = req->len;
  if (msg->payload_size) {
    memcpy(msg->payload, req->data, msg->payload_size);
  }
  hio_write(node->io, msg, sizeof(hmsg_t) + msg->payload_size);
  free(msg);

  // wait respond or timeout
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  hnode_time_offset(&ts, timeout);

  pthread_mutex_lock(&ctx->mtx);
  while (HREQUEST_CTX_STATUS_BUSY == hatomic_load(&ctx->status)) {
    ret = pthread_cond_timedwait(&ctx->cv, &ctx->mtx, &ts);
    // if (ret == ETIMEDOUT) {
    //   break;
    // }
    if (ret) {
      // HROKER_LOG("wait failed: %s\n", strerror(ret));
      break;
    }
  }
  pthread_mutex_unlock(&ctx->mtx);
  return ETIMEDOUT == ret ? HNODE_RET_ETIMEOUT : HNODE_RET_OK;
}

uint16_t hnode_id(hnode_t* node) { return node->id; }