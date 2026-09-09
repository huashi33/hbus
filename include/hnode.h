#ifndef _HNODE_H_
#define _HNODE_H_
#include <stdatomic.h>
#include <stddef.h>

#include "hcore/hatomic.h"
#include "hcore/hds.h"
#include "hmsg.h"
#include "nng/nng.h"
#include "nng/supplemental/util/platform.h"

typedef int (*hbus_subscrib_handler_t)(const hmsg_t* hmsg, void* p);
typedef struct hbus_suber_ {
  void* param;
  hbus_subscrib_handler_t h;
} hbus_suber_t;

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

HIHASH_DEFINE(uint16_t, hbus_suber_t)
HIHASH_DEFINE(uint32_t, uintptr_t)

typedef struct hnode_ {
  uint16_t node_id;
  nng_socket pub_sock;
  nng_socket sub_sock;
  nng_aio* sub_aio;
  hatomic_uint32_t req_seq;
  HIHASH_TYPEDEF(uint16_t, hbus_suber_t) *suber;
  HIHASH_TYPEDEF(uint32_t, uintptr_t) *reqer;

} hnode_t;

int hnode_init(hnode_t* n, int node_id);
int hnode_fini(hnode_t* n);
int hnode_publish(hnode_t* n, uint16_t msg_id, const void* d, uint32_t s,
                 uint16_t target_node_id, uint32_t seq);
int hnode_subscrib(hnode_t* n, uint16_t msg_id, hbus_subscrib_handler_t h,
                  void* p);
int hnode_request(hnode_t* n, uint16_t msg_id, uint16_t target_node_id,
                            hbuf_t* req, hbuf_t* res, int timeout);

// namespace hbus {
// typedef int (*subscrib_handler_t)(const hmsg_t* hmsg, void* p);
// typedef struct hsuber_ {
//   void* param;
//   subscrib_handler_t h;
// } hsuber_t;

// typedef struct hnode_status_ {
//   uint16_t node_id;
//   uint16_t sub_msg_id_count;
//   uint16_t sub_msg_id[];
// } hnode_status_t;

// typedef struct hnode_req_ctx_ {
//   uint32_t seq;
//   uint16_t status;  // 0 idle;1:wait;2:responded
//   uint16_t msg_id;
//   void* respond;
//   uint32_t res_size;

//   std::mutex mtx;
//   std::condition_variable cv;
// } hnode_req_ctx_t;

// class hnode {
//  private:
//   uint16_t node_id;
//   nng_socket pub_sock;

//   nng_socket sub_sock;
//   nng_aio* sub_aio;

//   std::atomic<uint32_t> req_seq;
//   std::unordered_map<uint16_t, hsuber_t> suber;          // msg_id
//   std::unordered_map<uint32_t, hnode_req_ctx_t*> reqer;  // seq
//  private:
//   int connect_broker();
//   static void on_subscrib_recv_cb(void*);
//   static int on_status_req(const hmsg_t* hm, void* p);
//   static int on_request(const hmsg_t* hm, void* p);

//  public:
//   hnode(int);
//   ~hnode();

//   int publish(uint16_t msg_id, const void* d, uint32_t s,
//               uint16_t target_node_id = 0, uint32_t seq = 0);
//   int subscrib(uint16_t msg_id, subscrib_handler_t h, void* p);
//   int request(uint16_t node_id, uint16_t msg_id, const void* req,
//               uint32_t req_size, void* res, uint32_t res_size, int timeout);
// };

// }  // namespace hbus

#endif