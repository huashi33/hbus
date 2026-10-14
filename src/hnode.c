#include "hcore/hdef.h"
#include "hv/hloop.h"
#include "hv/hplatform.h"
#include <pthread.h>
#define _POSIX_C_SOURCE 200809L
#include "hnode.h"
#include "hmsg.h"
#include <time.h>
#include <stdio.h>
#include "hv/herr.h"



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

static void hnode_on_connect(hio_t* io);
static void hnode_on_close(hio_t* io);
static void hnode_reconnect(htimer_t* timer) {
  hnode_t* node = (hnode_t*)hevent_userdata(timer);
  node->timer_reconn = NULL;

  node->status = HNODE_STATUS_RECONNECTING;
  printf("reconnect...\n");
  node->io = hloop_create_tcp_client(node->loop,
                                     node->server_ip,
                                     node->server_port,
                                     hnode_on_connect,
                                     hnode_on_close);
  hio_set_context(node->io, node);
}
static void hnode_on_close(hio_t* io) { 
  HROKER_LOG("connection closed,error=%d\n", hio_error(io));
  printf("error=%d %s\n",
        hio_error(io),
        hv_strerror(hio_error(io)));

  hnode_t* node = (hnode_t*)hio_context(io);
  node->status = HNODE_STATUS_DISCONNECT;
  if(!node->timer_reconn){
    htimer_t *t = htimer_add(node->loop, hnode_reconnect, 2000, 1);
    hevent_set_userdata(t, node);
  }

  
}

static void hnode_on_read(hio_t* io, void* buf, int readbytes) {
  hnode_t* node = (hnode_t*)hio_context(io);
  hmsg_t* msg = (hmsg_t*)buf;
  uint64_t* pay = (uint64_t*)msg->payload;
  pay[4] = hcommon_clock_now_ns();
  // double now_ms = hcommon_clock_now_ms();
  // HROKER_LOG("[%.3f]recv,byte(%d),(%d)(%d)(%d)\n",now_ms,readbytes,msg->from,msg->msg_id,msg->to);
  HIHASH_ITERATE it = HIHASH_FIND(node->map_msgid_handler, msg->msg_id);
  if(HIHASH_ITERATE_VALID(node->map_msgid_handler, it)) {
    hnode_subscrib_handler_t handler = HIHASH_GETV(node->map_msgid_handler,it);
    //TODO: thread poll
    handler.handle(msg,handler.userdata);
  }
  else{
    HROKER_LOG("recv,not fund,(%d)(%d)(%d)\n",msg->from,msg->msg_id,msg->to);
  }
}

static void hnode_register(hnode_t* node){
  hmsg_t msg = {
    .msg_id = 0,
    .from = node->id,
    .flags = HMSG_FLAG_REGISTER,
    .payload_size = 0
  };
  hio_write(node->io, &msg, sizeof(hmsg_t));   
}

static void hnode_on_connect(hio_t* io) {
  int err = hio_error(io);
  if (err != 0) {
      printf("connect failed: %d\n", err);
      return;
  }

  printf("connected!\n");
  hnode_t* node = (hnode_t*)hio_context(io);


  hnode_register(node);
  hio_setcb_read(io, hnode_on_read);
  hio_read_start(io);
  node->status = HNODE_STATUS_CONNECTED;
  // const char* msg = "hello server";
  // hio_write(io, msg, strlen(msg));
}

static void* hnode_thread_io_main(void* p){
  
  hnode_t *node = (hnode_t*)p;
  printf("thread start\n");
  HIHASH_INIT(node->map_msgid_handler);


  node->loop = hloop_new(0);
  node->io = hloop_create_tcp_client(node->loop,
                                     node->server_ip,
                                     node->server_port,
                                     hnode_on_connect,
                                     hnode_on_close);
  hio_set_context(node->io, node);
  hloop_run(node->loop);
  hloop_free(&node->loop);
  printf("thread stop\n");
  return 0;
}

int hnode_init(hnode_t* node) {
  if (HNODE_STATUS_INIT == node->status) {
    pthread_create(&node->thread_io,NULL, hnode_thread_io_main,node);
  }
  return HNODE_RET_OK;
}
int hnode_fini(hnode_t* node) {
  if (node->timer_reconn) {
    htimer_del(node->timer_reconn);
    node->timer_reconn = NULL;
  }
  if (node->io) {
      hio_close(node->io);
      node->io = NULL;
  }

  hloop_stop(node->loop);
  pthread_join(node->thread_io, NULL);
  // hthread_join(node->thrd);
  node->status = HNODE_STATUS_INIT;
  HIHASH_FINI(node->map_msgid_handler);
  return HNODE_RET_OK;
}

int hnode_publish(hnode_t* node, uint16_t msg_id, uint16_t to_node_id,
                     const void* payload, uint32_t payload_size) {
  if (HNODE_STATUS_CONNECTED != node->status) {
    return HNODE_RET_EDISCONNECT;
  }
  hmsg_t* msg = (hmsg_t*)calloc(1,sizeof(hmsg_t)+ payload_size);
  msg->msg_id = msg_id;
  msg->from = node->id;
  msg->to = to_node_id;
  msg->flags = HMSG_FLAG_DATA;
  msg->payload_size = payload_size;
  if(payload_size){
    uint64_t*pay = (uint64_t*)payload;
    pay[1] = hcommon_clock_now_ns();
    memcpy(msg->payload,payload,payload_size);
  }
  hio_write(node->io, msg, sizeof(hmsg_t)+ payload_size);             
  free(msg);
  return HNODE_RET_OK;
}
int hnode_subscribe(hnode_t* node, uint16_t msg_id,hnode_subscrib_handler_func_t handle_func,void* userdata) { 
  if (HNODE_STATUS_CONNECTED != node->status) {
    return HNODE_RET_EDISCONNECT;
  }
  hmsg_t msg = {
    .msg_id = msg_id,
    .from = node->id,
    .flags = HMSG_FLAG_SUB,
    .payload_size = 0
  };
  hio_write(node->io, &msg, sizeof(hmsg_t));   

  hnode_subscrib_handler_t handler = {handle_func,userdata};
  HIHASH_SET(node->map_msgid_handler,msg_id,handler);

  return HNODE_RET_OK;
}
int hnode_unsubscribe(hnode_t* node, uint16_t msg_id) {
  if (HNODE_STATUS_CONNECTED != node->status) {
    return HNODE_RET_EDISCONNECT;
  }
  hmsg_t msg = {
    .msg_id = msg_id,
    .from = node->id,
    .flags = HMSG_FLAG_UNSUB,
    .payload_size = 0
  };
  hio_write(node->io, &msg, sizeof(hmsg_t));  
  
  HIHASH_DEL(node->map_msgid_handler,msg_id);
  return HNODE_RET_OK;
}
int hnode_request(hnode_t* node,uint16_t msg_id, uint16_t to_node_id, 
                  const void* payload, uint32_t payload_size) {
  
  return 0;
}
