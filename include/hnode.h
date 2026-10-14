#ifndef _HNODE_H_
#define _HNODE_H_
#include <stdint.h>
#include "hcore/hhash.h"
#include "hv/hloop.h"
// #include "hv/hthread.h"
#include "hmsg.h"

#define HNODE_STATUS_INIT 0
#define HNODE_STATUS_RECONNECTING 1
#define HNODE_STATUS_DISCONNECT 2
#define HNODE_STATUS_CONNECTED 3


#define HNODE_RET_OK 0
#define HNODE_RET_EDISCONNECT 0

typedef void(*hnode_subscrib_handler_func_t)(const hmsg_t*,void*);
typedef struct hnode_subscrib_handler_{
  hnode_subscrib_handler_func_t handle;
  void* userdata;
}hnode_subscrib_handler_t;

HIHASH_DEFINE(uint16_t,hnode_subscrib_handler_t)

typedef struct hnode_ {
  uint16_t id;
  uint16_t status;//0 disconnect;1 connect
  uint16_t server_port;
  uint16_t algn;

  char server_ip[16];

  hhash_hnode_subscrib_handler_t *map_msgid_handler;
  hio_t* io;
  hloop_t* loop;
  pthread_t thread_io;
  htimer_t* timer_reconn;
} hnode_t;

int hnode_init(hnode_t*);
int hnode_fini(hnode_t*);


int hnode_publish(hnode_t* node, uint16_t msg_id, uint16_t to_node_id, const void* payload, uint32_t payload_size);
int hnode_subscribe(hnode_t* node, uint16_t msg_id,hnode_subscrib_handler_func_t handle,void* userdata);
int hnode_unsubscribe(hnode_t* node, uint16_t msg_id);
int hnode_request(hnode_t* node, uint16_t to_node_id, uint16_t msg_id, const void* payload, uint32_t payload_size);


#endif