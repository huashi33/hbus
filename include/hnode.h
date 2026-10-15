#ifndef _HNODE_H_
#define _HNODE_H_
#include <pthread.h>
#include <stdint.h>
#include "hcore/hhash.h"
#include "hcore/hbuf.h"
#include "hcore/hatomic.h"
#include "hv/hloop.h"

#include "hmsg.h"
#define HNODE_STATUS_INIT 0
#define HNODE_STATUS_DISCONNECT 1
#define HNODE_STATUS_CONNECTING 2
#define HNODE_STATUS_CONNECTED 3

// #define HNODE_RECONN_INTERVAL_DEFAULT 2000
// #define HNODE_RECONN_MAX_RETRY_CNT_DEFAULT 5


#define HNODE_RET_OK 0
#define HNODE_RET_EDISCONNECT 1
#define HNODE_RET_ETIMEOUT 2

typedef void(*hnode_subscrib_handler_func_t)(const hmsg_t*,void*);
typedef struct hnode_subscrib_handler_{
  hnode_subscrib_handler_func_t handle;
  void* handle_param;
}hnode_subscrib_handler_t;
typedef struct hrequest_ctx_{
  pthread_mutex_t mtx;
  pthread_cond_t cv;
  hbuf_t* rep;
  int complet;
}hrequest_ctx_t;
HIHASH_DEFINE(uint16_t,hnode_subscrib_handler_t)

HIHASH_DEFINE(uint32_t,intptr_t)


typedef struct hnode_ hnode_t;



hnode_t* hnode_init(uint16_t id,const char* svr_ip,uint16_t svr_port);
int hnode_destroy(hnode_t*);
int hnode_status(hnode_t*);

int hnode_publish(hnode_t* node, uint16_t topic, uint16_t to_node_id, const void* payload, uint32_t payload_size);
int hnode_subscribe(hnode_t* node, uint16_t topic,hnode_subscrib_handler_func_t handle,void* userdata);
int hnode_unsubscribe(hnode_t* node, uint16_t topic);
// 
int hnode_request(hnode_t* node, uint16_t topic, uint16_t to_node_id, hbuf_t* req,hbuf_t* rep,uint32_t timeout);

int hnode_publish_in_threadio(hnode_t* node,void* d,size_t s);

uint16_t hnode_id(hnode_t* node);
#endif