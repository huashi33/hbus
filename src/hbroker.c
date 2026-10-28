
#include <stdint.h>
#include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include "hcore/hbuf.h"
#include "hcore/hdef.h"
#include "hcore/hhash.h"
#include "hmsg.h"
#include "hv/hloop.h"
#include "hv/hsocket.h"

HIHASH_DEFINE(int32_t, hbuf_t);
typedef struct hsession_ {
  hio_t *io;
  hbuf_t buf_topics;
  // hhash_hbuf_t* hbus_ctx->map_topic_sessions; // key: topic, value:
  // hbuf_t(session_t),global
  uint16_t node_id;
  uint16_t timeout_sec;
} hsession_t;

typedef struct hbus_ctx_ {
  // key: topic, value: hbuf_t(session_t),global
  hhash_hbuf_t *map_topic_sessions;
} hbus_ctx_t;

#define HSESSION_TIMEOUT_SEC 30
// #define HROKER_LOG(FMT,...) printf(FMT,##__VA_ARGS__)
#define HROKER_LOG(FMT, ...) printf(FMT, ##__VA_ARGS__)
#define HBROKER_LISTEN_PORT 55000

#define HBROKER_RET_OK 0

static hbus_ctx_t *hbus_ctx_get() {
  static hbus_ctx_t hbus_ctx;
  return &hbus_ctx;
}
static void hbus_ctx_free() {
  hbus_ctx_t *hbus_ctx = hbus_ctx_get();
  HIHASH_FINI(hbus_ctx->map_topic_sessions);
}

static int hbus_ctx_session_add(int32_t topic, hsession_t *s) {
  hbus_ctx_t *hbus_ctx = hbus_ctx_get();

  HIHASH_ITERATE it = HIHASH_FIND(hbus_ctx->map_topic_sessions, topic);
  if (HIHASH_ITERATE_VALID(hbus_ctx->map_topic_sessions, it)) {
    // topic already exist
    hbuf_t *buf_sessions = HIHASH_GETVPTR(hbus_ctx->map_topic_sessions, it);
    size_t session_count = HBUF_PTR_COUNT(buf_sessions);
    for (size_t i = 0; i < session_count; ++i) {
      hsession_t *tmp_session = HBUF_PTR_AT(buf_sessions, hsession_t, i);
      if (tmp_session == s) {
        // already exist
        HROKER_LOG("node(%d, %d) sub,already-sub,topic(%08X):\n", s->node_id,
                   hio_fd(s->io), topic);
        return HBROKER_RET_OK;
      }
    }
    HBUF_PTR_PUSH(buf_sessions, s);
  } else {
    // new topic
    hbuf_t b;
    hbuf_init(&b, sizeof(hsession_t *) * 8);
    HBUF_PTR_PUSH(&b, s);
    HIHASH_SET(hbus_ctx->map_topic_sessions, topic, b);
  }
  HROKER_LOG("node(%d,%d):+ topic(%08X)\n", s->node_id, hio_fd(s->io), topic);

  return HBROKER_RET_OK;
}
static int hbus_ctx_session_rm(int32_t topic_ex, hsession_t *s) {
  hbus_ctx_t *hbus_ctx = hbus_ctx_get();
  HIHASH_ITERATE it = HIHASH_FIND(hbus_ctx->map_topic_sessions, topic_ex);
  if (!HIHASH_ITERATE_VALID(hbus_ctx->map_topic_sessions, it)) {
    HROKER_LOG("node(%d,%d):never sub topic(%08X)\n", s->node_id, hio_fd(s->io),
               topic_ex);
    return HBROKER_RET_OK;
  }

  hbuf_t *buf_sessions = HIHASH_GETVPTR(hbus_ctx->map_topic_sessions, it);
  size_t session_count = HBUF_PTR_COUNT(buf_sessions);
  for (size_t i = 0; i < session_count; ++i) {
    hsession_t *tmp_session = HBUF_PTR_AT(buf_sessions, hsession_t, i);
    if (tmp_session == s) {
      hbuf_remove(buf_sessions, i, sizeof(intptr_t));
      HROKER_LOG("node(%d,%d):- topic(%08X)\n", s->node_id, hio_fd(s->io), topic_ex);

      break;
    }
  }

  if (0 == HBUF_PTR_COUNT(buf_sessions)) {
    hbuf_fini(buf_sessions);
    HIHASH_DEL(hbus_ctx->map_topic_sessions, topic_ex);
  }
  return HBROKER_RET_OK;
}

static hsession_t *hbus_seesion_alloc(hio_t *io) {
  hsession_t *s = (hsession_t *)malloc(sizeof(hsession_t));
  if (!s) {
    HROKER_LOG("malloc error\n ");
    return NULL;
  }
  s->node_id = 0;
  s->io = io;
  s->timeout_sec = HSESSION_TIMEOUT_SEC;

  int r = 0;
  r = hbuf_init(&s->buf_topics, sizeof(uint16_t) * 16);
  if (r) {
    HROKER_LOG("hbuf_init error\n ");
    free(s);
    return NULL;
  }

  return s;
}
static void hbus_seesion_free(hsession_t *s) {
  size_t topic_num = HBUF_ITEM_COUNT(&s->buf_topics, int32_t);
  for (size_t i = 0; i < topic_num; ++i) {
    int32_t tmp_topic = *HBUF_ITEM_AT(&s->buf_topics, int32_t, i);
    hbus_ctx_session_rm(tmp_topic, s);
  }

  hbuf_fini(&s->buf_topics);
  free(s);
}

// static unpack_setting_t unpack_setting;
// static hhash_hbuf_t* map_topic_nodes = NULL;  // key: topic, value:
// hbuf_t(session_t) static hhash_hbuf_t* map_node_topics = NULL;  // key:
// node_id, value: hbuf_t(topic)

uint64_t hcommon_clock_now_ns() {
  struct timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  return at.tv_sec * 1e9 + at.tv_nsec;
}
double hcommon_clock_now_ms() {
  struct timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  double now = at.tv_sec * 1e3 + at.tv_nsec / 1e6;
  return now;
}

static void on_read(hio_t *io, void *buf, int readbytes) {
  hmsg_t *msg = (hmsg_t *)buf;
  if (!HMSG_HEAD_VALID(msg,readbytes)) {
    HROKER_LOG("msg head check fail,bytes:%d,payload_size:%u\n",
               readbytes, msg->payload_size);
    return;
  }

  hbus_ctx_t *hbus_ctx = hbus_ctx_get();
  hsession_t *session = (hsession_t *)hio_context(io);
  // HROKER_LOG("<[%d](%d)-(%d)->(%d)\n",readbytes,msg->from,msg->msg_id,msg->to);
  
  int topic_ex = HMSG_TOPICEX(msg->version,msg->flags,msg->topic);
  if(HMSG_APPID_BROKER != msg->to){
    HIHASH_ITERATE it = HIHASH_FIND(hbus_ctx->map_topic_sessions, topic_ex);
    if (HIHASH_ITERATE_VALID(hbus_ctx->map_topic_sessions, it)) {
      hbuf_t *buf_sessions = HIHASH_GETVPTR(hbus_ctx->map_topic_sessions, it);
      size_t session_num = HBUF_PTR_COUNT(buf_sessions);
      // hsession_t *sessions = (hsession_t *)sbuf->data;
      int send_num = 0;
      for (size_t i = 0; i < session_num; i++) {
        hsession_t *tmp_sessions = HBUF_PTR_AT(buf_sessions, hsession_t, i);
        if (msg->to == tmp_sessions->node_id ||
            HMSG_APPID_EVERYONR == msg->to) {
          ++send_num;
          hio_write(tmp_sessions->io, buf, readbytes);
          // HROKER_LOG("> forward:byte(%d),(%d)-(%d)-(%d)\n", readbytes,  msg->from, msg->topic, msg->to);
        }
      }
      HROKER_LOG("> forward:byte(%d) num:%d\n", readbytes, send_num);
    } else {
      HROKER_LOG("< not-found,(%d)(%08X)(%d)\n", msg->from, topic_ex, msg->to);
    }
  }
  else{
    if (HMSG_FCMD_SUB & msg->flags) {
      // add sub
      uint8_t flags_to_sub = HMSG_FLAGS(HMSG_FCMD_DATA,HMSG_FLAGS_DATA(msg->flags));
      int topic_to_sub = HMSG_TOPICEX(0,flags_to_sub,msg->topic);
      size_t topic_num = HBUF_ITEM_COUNT(&session->buf_topics, int32_t);
      for (size_t j = 0; j < topic_num; ++j) {
        int32_t tmp_topic = *HBUF_ITEM_AT(&session->buf_topics, int32_t, j);
        if (tmp_topic == topic_to_sub) {
          // already exist
          HROKER_LOG("node(%d, %d) sub,already-sub,topic(0x%08X):\n",
                    session->node_id, hio_fd(session->io), tmp_topic);
          return;
        }
      }
      hbus_ctx_session_add(topic_to_sub, session);
      hbuf_push(&session->buf_topics, &topic_to_sub, sizeof(int32_t));

    } else if (HMSG_FCMD_UNSUB == msg->flags) {
      // remove sub
      uint8_t flags_to_unsub = HMSG_FLAGS(HMSG_FCMD_DATA,HMSG_FLAGS_DATA(msg->flags));
      int topic_to_unsub = HMSG_TOPICEX(0,flags_to_unsub,msg->topic);
      size_t topic_num = HBUF_ITEM_COUNT(&session->buf_topics, int32_t);
      for (size_t j = 0; j < topic_num; ++j) {
        int32_t tmp_topic = ((int32_t *)session->buf_topics.data)[j];
        if (tmp_topic == topic_to_unsub) {
          hbuf_remove(&session->buf_topics, j, sizeof(int32_t));
          hbus_ctx_session_rm(tmp_topic, session);
          break;
        }
      }
    } else if (HMSG_FCMD_REGISTER == msg->flags) {
      session->node_id = msg->from;
      HROKER_LOG("register %d\n", session->node_id);
    } else {
      HROKER_LOG("unknown msg->flags=%d\n", msg->flags);
    }
  }
}

static void on_close(hio_t *io) {
  double now_ms = hcommon_clock_now_ms();
  HROKER_LOG("[%.3f]fd(%d) disconnected!\n", now_ms, hio_fd(io));

  // TODO,unsub node in every topic
  hsession_t *s = (hsession_t *)hio_context(io);
  hbus_seesion_free(s);
}

static unpack_setting_t *hbus_unpack_setting_get() {
  static int flag_init = 0;
  static unpack_setting_t unpack_setting;
  if (0 == flag_init) {
    memset(&unpack_setting, 0, sizeof(unpack_setting_t));
    unpack_setting.package_max_length = 1024 * 1024; // 单个包最大长度限制
    unpack_setting.mode = UNPACK_BY_LENGTH_FIELD;
    unpack_setting.length_field_offset =
        offsetof(hmsg_t, payload_size); // 长度字段在包头部的偏移量
    unpack_setting.length_field_bytes = sizeof(
        ((hmsg_t *)0)->payload_size); // 长度字段占几个字节（如 2 字节代表长度）
    unpack_setting.length_field_coding = ENCODE_BY_LITTEL_ENDIAN; // 大端或小端
    unpack_setting.body_offset =
        sizeof(hmsg_t); // 消息体起始偏移量（跳过长度字段本身）
    flag_init = 1;
  }
  return &unpack_setting;
}
static void on_accept(hio_t *io) {
  // 1. 正确获取客户端 IP 字符串的写法
  char peeraddrstr[SOCKADDR_STRLEN] = {0};
  SOCKADDR_STR(hio_peeraddr(io), peeraddrstr);

  double now_ms = hcommon_clock_now_ms();
  HROKER_LOG("[%.3f]fd(%d) connected!, addr: %s\n", now_ms, hio_fd(io),
             peeraddrstr);

  // 2. 正确的 libhv 回调绑定函数名
  hio_setcb_read(io, on_read);
  hio_setcb_close(io, on_close);
  hio_set_unpack(io, hbus_unpack_setting_get());

  hsession_t *s = hbus_seesion_alloc(io);
  hio_set_context(io, s);

  // 开始读取
  hio_read_start(io);
}

static void on_timer(htimer_t* timer) {
  HROKER_LOG("timer: %llu\n", hcommon_clock_now_ns());
  hbus_ctx_t *ctx = hbus_ctx_get();
  HIHASH_ITERATE it = HIHASH_ITERATE_BEGIN(ctx->map_topic_sessions);
  while (HIHASH_ITERATE_VALID(ctx->map_topic_sessions,it)) {
    int32_t topic_ex = HIHASH_GETK(ctx->map_topic_sessions, it);
    hbuf_t* buf_sessions = HIHASH_GETVPTR(ctx->map_topic_sessions, it);
    size_t sessio_num = HBUF_PTR_COUNT(buf_sessions);
    HROKER_LOG("topic(%08X):",topic_ex);
    for (size_t i = 0; i < sessio_num; ++i) {
      hsession_t* s = HBUF_PTR_AT(buf_sessions, hsession_t, i);
      HROKER_LOG("%d ",s->node_id,hio_fd(s->io));
    }
    HROKER_LOG("\n");
    it = HIHASH_ITERATE_NEXT(it);
  }
  
}

int main(int argc, char **argv) {
  printf("hello \n");
  int port = HBROKER_LISTEN_PORT;
  if (argc > 1) {
    port = atoi(argv[1]);
  }

  // 1. 创建底层事件循环核心
  hloop_t *loop = hloop_new(0);

  // 2. 创建 TCP 服务端监听
  hio_t *listenio = hloop_create_tcp_server(loop, "0.0.0.0", port, on_accept);
  if (listenio == NULL) {
    fprintf(stderr, "监听端口 %d 失败!\n", port);
    return -1;
  }

  htimer_add(loop, on_timer, 15000, INFINITE);
  printf("listen: 0.0.0.0:%d ...\n", port);

  // 3. 启动事件循环（进入主运行事件轮询，阻塞）
  hloop_run(loop);

  // 4. 退出后清理事件循环资源
  hloop_free(&loop);

  hbus_ctx_free();
  return 0;
}