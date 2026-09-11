#ifndef _HNODE_H_
#define _HNODE_H_
#include <stdatomic.h>
#include <stddef.h>

#include "hcore/hatomic.h"
#include "hcore/hds.h"
#include "hmsg.h"
#include "nng/nng.h"
#include "nng/supplemental/util/platform.h"

typedef int (*hnode_subscrib_handler_t)(const hmsg_t* hmsg, void* p);
typedef struct hnode_suber_ {
  void* param;
  hnode_subscrib_handler_t h;
} hnode_suber_t;

typedef struct hnode_status_ {
  uint16_t node_id;
  uint16_t sub_msg_id_count;
  uint16_t sub_msg_id[];
} hnode_status_t;

typedef struct hnode_req_ctx_ {
  uint32_t seq;
  uint16_t status;  // 0 idle;1:wait;2:responded
  uint16_t msg_id_replay;
  hbuf_t* replay;

  nng_mtx* mtx;
  nng_cv* cv;
} hnode_req_ctx_t;

HIHASH_DEFINE(uint16_t, hnode_suber_t)
HIHASH_DEFINE(uint32_t, uintptr_t)

typedef struct hnode_ {
  uint16_t node_id;
  nng_socket pub_sock;
  nng_socket sub_sock;
  nng_aio* sub_aio;
  hatomic_uint32_t req_seq;
  HIHASH_TYPEDEF(uint16_t, hnode_suber_t) *suber;
  HIHASH_TYPEDEF(uint32_t, uintptr_t) *reqer;

} hnode_t;

NNG_DECL int hnode_init(hnode_t* n, int node_id);
NNG_DECL int hnode_fini(hnode_t* n);
NNG_DECL int hnode_publish(hnode_t* n, uint16_t msg_id, const void* d, uint32_t s,
                  uint16_t target_node_id, uint32_t seq);
NNG_DECL int hnode_subscribe(hnode_t* n, uint16_t msg_id, hnode_subscrib_handler_t h,
                   void* p);
NNG_DECL int hnode_request(hnode_t* n, uint16_t msg_id, uint16_t target_node_id,
                            hbuf_t* req, hbuf_t* res, int timeout);

#endif