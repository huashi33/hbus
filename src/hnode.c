#include "hcore/hatomic.h"
#include "hcore/hbuf.h"
#include "hcore/hdef.h"
#include "hv/hloop.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define _POSIX_C_SOURCE 200809L
#include "hnode.h"
#include "hmsg.h"
#include <time.h>
#include <stdio.h>

#define HNODE_REQUEST_TIMEOUT_MAX 10000 //10S



typedef struct hnode_ {
  uint16_t id;
  uint16_t server_port;

  uint8_t status;
  uint8_t reconn_cnt;
  uint8_t reconn_interval;//s
  uint8_t reconn_max_retry_cnt; 

  char server_ip[16];

  hhash_hnode_subscrib_handler_t *map_subpool;
  hio_t* io;
  hloop_t* loop;
  pthread_t thread_io;
  htimer_t* timer_reconn;

  hatomic_uint32_t ato_req_seq;
  hhash_intptr_t* map_reqpool;
} hnode_t;

#define HROKER_LOG(FMT,...) printf(FMT,##__VA_ARGS__)

uint64_t hcommon_clock_now_ns() {
  struct timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  return at.tv_sec * 1e9 + at.tv_nsec;
}
double hcommon_clock_now_ms() {
  struct timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  double now = at.tv_sec * 1e3 + at.tv_nsec/1e6;
  return now;
}
static unpack_setting_t *hnode_unpack_setting_get() {
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

static void hnode_on_connect(hio_t* io);
static void hnode_on_close(hio_t* io);
static void hnode_reconnect(htimer_t* timer) {
  hnode_t* node = (hnode_t*)hevent_userdata(timer);
  if(HNODE_STATUS_DISCONNECT == hatomic_load(&node->status)){
    hatomic_store(&node->status, HNODE_STATUS_CONNECTING);
    ++node->reconn_max_retry_cnt;
    HROKER_LOG("[%d/%d]connecting...\n",node->reconn_cnt,node->reconn_max_retry_cnt);
    node->io = hloop_create_tcp_client(node->loop,
                                      node->server_ip,
                                      node->server_port,
                                      hnode_on_connect,
                                      hnode_on_close);
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
  if(sizeof(hmsg_t) > readbytes || (sizeof(hmsg_t) + msg->payload_size != readbytes)){
    HROKER_LOG("recv invalid data, byte(%d)\n", readbytes);
    return;
  }
  hnode_t* node = (hnode_t*)hio_context(io);
  hnode_subscrib_handler_t handler = {0};
  HNODE_OPERATE_THREADSAFE(
    HIHASH_ITERATE it = HIHASH_FIND(node->map_subpool, msg->topic);
    if(HIHASH_ITERATE_VALID(node->map_subpool, it)){
      handler = HIHASH_GETV(node->map_subpool, it);
    },
    &node->mtx_mapsub
  );

  if(handler.handle){
    handler.handle(msg,handler.handle_param);
  }
  
}

static void hnode_register(hnode_t* node){
  hmsg_t msg = {
    .topic = 0,
    .from = node->id,
    .flags = HMSG_FLAG_REGISTER,
    .payload_size = 0
  };
  hio_write(node->io, &msg, sizeof(hmsg_t));   
}

static void hnode_on_connect(hio_t* io) {
  hnode_t* node = (hnode_t*)hio_context(io);
  int err = hio_error(io);
  if (err != 0) {
      HROKER_LOG("connect failed: %d\n", err);
      hatomic_store(&node->status,HNODE_STATUS_DISCONNECT);
      return;
  }
  hnode_register(node);

  hio_set_unpack(io, hnode_unpack_setting_get());

  hio_setcb_read(io, hnode_on_read);
  hio_read_start(io);

  hatomic_store(&node->status,HNODE_STATUS_CONNECTED);
}

static void* hnode_thread_io_main(void* p){
  hnode_t *node = (hnode_t*)p;
  HROKER_LOG("thread start\n");
  // pthread_mutex_lock(&node->mtx_node);
  // HIHASH_INIT(node->map_subpool);
  // HIHASH_INIT(node->map_reqpool);
  // pthread_mutex_unlock(&node->mtx_node);
  HNODE_OPERATE_THREADSAFE(HIHASH_INIT(node->map_subpool);,&node->mtx_mapsub);
  HNODE_OPERATE_THREADSAFE(HIHASH_INIT(node->map_reqpool);,&node->mtx_mapreq);
  
  
  node->loop = hloop_new(0);
  // create reconnect timer
  node->timer_reconn = htimer_add(node->loop, hnode_reconnect, node->reconn_interval,INFINITE);
  hevent_set_userdata(node->timer_reconn, node);
  
  hatomic_store(&node->status,HNODE_STATUS_DISCONNECT);
  hnode_reconnect(node->timer_reconn);

  hloop_run(node->loop);
  
  // remove timer
  htimer_del(node->timer_reconn);
  node->timer_reconn = NULL;

  hloop_free(&node->loop);
  pthread_mutex_lock(&node->mtx_node);
  HIHASH_FINI(node->map_subpool);
  HIHASH_FINI(node->map_reqpool);
  pthread_mutex_unlock(&node->mtx_node);
  HROKER_LOG("thread stop\n");
  return 0;
}
// MUST CLEAR NODE before call
hnode_t* hnode_init(uint16_t id,const char* svr_ip,uint16_t svr_port) {
  hnode_t* node = (hnode_t*)calloc(1, sizeof(hnode_t));
  node->id = id;
  node->status =  HNODE_STATUS_INIT;
  strncpy(node->server_ip, svr_ip, sizeof(node->server_ip)-1);
  node->server_port = svr_port;
  pthread_mutex_init(&node->mtx_node,NULL);

  pthread_create(&node->thread_io,NULL, hnode_thread_io_main,node);
  return node;
}
int hnode_destroy(hnode_t* node) {
  if(node){
    pthread_t tid = pthread_self();
    if(tid == node->thread_io){
      HROKER_LOG("should not be called in io-thread\n");
      return 1;
    }

    hloop_stop(node->loop);
    pthread_join(node->thread_io, NULL);
    free(node);
  }
  return HNODE_RET_OK;
}
int hnode_status(hnode_t* node){
  return hatomic_load(&node->status);
}

// typedef struct hnode_event_data_{
//   int flag;
//   hnode_t* node;
//   void* data;
// }hnode_event_data_t;
// static void hnode_exec_in_threadio(hevent_t* ev){
//   hnode_event_data_t* ed = (hnode_event_data_t*)ev->userdata;
//   if(HMSG_FLAG_DATA == ed->flag){
//     hio_write(ed->node->io, ed->data, sizeof(hmsg_t)+ ((hmsg_t*)(ed->data))->payload_size);
//     free(ed->data);
//     free(ed);
//   }
//   else if(HMSG_FLAG_SUB == ed->flag){

//   }
// }

int hnode_publish(hnode_t* node, uint16_t topic, uint16_t to_node_id,
                     const void* payload, uint32_t payload_size) {
  
  if(hatomic_load(&node->status) != HNODE_STATUS_CONNECTED ){
    return HNODE_RET_EDISCONNECT;
  }
  hmsg_t* msg = (hmsg_t*)calloc(1,sizeof(hmsg_t)+ payload_size);
  msg->topic = topic;
  msg->from = node->id;
  msg->to = to_node_id;
  msg->flags = HMSG_FLAG_DATA;
  msg->payload_size = payload_size;
  if(payload_size){
    memcpy(msg->payload,payload,payload_size);
  }
  
  // hnode_event_data_t* user_data = (hnode_event_data_t*)malloc(sizeof(hnode_event_data_t));
  // user_data->flag = HMSG_FLAG_DATA;
  // user_data->node = node;
  // user_data->data = msg;
  // hevent_t ev = {0};
  // ev.cb = hnode_exec_in_threadio;
  // ev.userdata = user_data;
  // hloop_post_event(node->loop, &ev);

  hio_write(node->io, msg, sizeof(hmsg_t)+ payload_size);             
  free(msg);
  return HNODE_RET_OK;
}
int hnode_subscribe(hnode_t* node, uint16_t topic,hnode_subscrib_handler_func_t handle_func,void* userdata) { 
  if(hatomic_load(&node->status) != HNODE_STATUS_CONNECTED ){
    return HNODE_RET_EDISCONNECT;
  }
  hmsg_t msg = {
    .topic = topic,
    .flags = HMSG_FLAG_SUB,
    .from = node->id,
    .seq = 0,
    .payload_size = 0
  };

  hnode_subscrib_handler_t handler = {handle_func,userdata};
  pthread_mutex_lock(&node->mtx_node);
  HIHASH_SET(node->map_subpool,topic,handler);
  pthread_mutex_unlock(&node->mtx_node);
  hio_write(node->io, &msg, sizeof(hmsg_t));   
  return HNODE_RET_OK;
}
int hnode_unsubscribe(hnode_t* node, uint16_t msg_id) {
  if (HNODE_STATUS_CONNECTED != node->status) {
    return HNODE_RET_EDISCONNECT;
  }
  hmsg_t msg = {
    .topic = msg_id,
    .flags = HMSG_FLAG_UNSUB,
    .from = node->id,
    .seq = 0,
    .payload_size = 0
  };
  pthread_mutex_lock(&node->mtx_node);
  HIHASH_DEL(node->map_subpool,msg_id);
  pthread_mutex_unlock(&node->mtx_node);
  hio_write(node->io, &msg, sizeof(hmsg_t));  
  return HNODE_RET_OK;
}

static void hnode_on_respond(const hmsg_t* msg,void* p){
  hrequest_ctx_t* ctx = NULL;
  hnode_t* node = (hnode_t*)p;
  pthread_mutex_lock(&node->mtx_node);
  HIHASH_ITERATE it = HIHASH_FIND(node->map_reqpool, msg->seq);
  if(HIHASH_ITERATE_VALID(node->map_reqpool,it)){
    ctx = (hrequest_ctx_t*)HIHASH_GETV(node->map_reqpool, it);
  }
  if(ctx){
    hbuf_push(ctx->rep,msg->payload,msg->payload_size);
    ctx->complet = 1;
    pthread_cond_signal(&ctx->cv);
  }
  else{
    HROKER_LOG("respond not find ctx, (%d)(%d)(%d)\n", msg->from, msg->topic, msg->to);
  }
  pthread_mutex_unlock(&node->mtx_node);
  
}
static void hnode_time_offset(struct timespec *ts,int mill){
  ts->tv_nsec += (mill*1e6);  // 100ms
  if (ts->tv_nsec >= 1000000000L) {
      ts->tv_sec++;
      ts->tv_nsec -= 1000000000L;
  }
}
static hrequest_ctx_t* hnode_request_alloc(hbuf_t* rep){
  hrequest_ctx_t* ctx = calloc(1, sizeof(hrequest_ctx_t));
  pthread_mutex_init(&ctx->mtx,NULL);
  pthread_cond_init(&ctx->cv, NULL);
  ctx->rep = rep;
  return ctx;
}
static void hnode_request_free(hrequest_ctx_t* ctx){
  pthread_mutex_destroy(&ctx->mtx);
  pthread_cond_destroy(&ctx->cv);
  free(ctx);
}


int hnode_request(hnode_t* node,uint16_t topic, uint16_t to_node_id, 
                  hbuf_t* req,hbuf_t* rep,uint32_t timeout) {
  if (HNODE_STATUS_CONNECTED != node->status) {
    return HNODE_RET_EDISCONNECT;
  }
  if(HNODE_REQUEST_TIMEOUT_MAX < timeout){
    timeout = HNODE_REQUEST_TIMEOUT_MAX;
    HROKER_LOG("timeout exceed max(%d), set to max\n", HNODE_REQUEST_TIMEOUT_MAX);
  }

  int r = 0;
  r = hnode_subscribe(node, topic, hnode_on_respond, node);
  HROKER_LOG("[%08X]node(%d),topic(%d)",r,node->id,topic);
  
  hmsg_t* msg = (hmsg_t*)calloc(1, sizeof(hmsg_t)+ req->len);
  msg->from = node->id;
  msg->to =  to_node_id;
  msg->topic = topic;
  msg->flags = HMSG_FLAG_REQUEST;
  msg->payload_size = req->len;
  if(msg->payload_size){
    memcpy(msg->payload, req->data, msg->payload_size);
  }
  msg->seq = hatomic_inc(&node->ato_req_seq, 1);
  // put reqer in req-pool
  hbuf_clear(rep);
  hrequest_ctx_t* ctx = hnode_request_alloc(rep);
  pthread_mutex_lock(&node->mtx_node);
  HIHASH_SET(node->map_reqpool, msg->seq, (intptr_t)ctx);
  pthread_mutex_unlock(&node->mtx_node);
  //request
  hio_write(node->io, msg, sizeof(hmsg_t)+ msg->payload_size);
  
  //wait respond or timeout
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  hnode_time_offset(&ts,timeout);
  int ret;
  pthread_mutex_lock(&ctx->mtx);
  while(!ctx->complet){
    ret = pthread_cond_timedwait(&ctx->cv, &ctx->mtx,&ts);
    if(ret == ETIMEDOUT){
      break;
    }
    if(ret){
      HROKER_LOG("wait failed: %s\n", strerror(ret));
      break;
    }
  }
  pthread_mutex_unlock(&ctx->mtx);

  pthread_mutex_lock(&node->mtx_node);
  HIHASH_DEL(node->map_reqpool, msg->seq);
  pthread_mutex_unlock(&node->mtx_node);

  hnode_request_free(ctx);
  free(msg);
  return ETIMEDOUT == ret ? HNODE_RET_ETIMEOUT: HNODE_RET_OK;
}


int hnode_publish_in_threadio(hnode_t* node,void* d,size_t s){
  hio_write(node->io,d,s);
  return 0;
}

uint16_t hnode_id(hnode_t* node){
  return node->id;
}