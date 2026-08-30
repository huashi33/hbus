#ifndef _HNODE_H_
#define _HNODE_H_
#include "nng/nng.h"
#include <stddef.h>
#include <string>
#include <unordered_map>
#include "hmsg.h"
#include <atomic>
#include <condition_variable>

namespace hbus{




typedef int (*subscrib_handler_t)(const hmsg_t* hmsg,void* p);
// typedef int (*response_handle_t)(int request_id,const void* d,size_t s);
typedef struct hsuber_{
  void* param;
  subscrib_handler_t h;
}hsuber_t;

typedef struct hnode_status_ {
  uint16_t node_id;
  uint16_t sub_msg_id_count;
  uint16_t sub_msg_id[];
} hnode_status_t;

typedef struct hnode_req_ctx_ {
  uint32_t seq;
  uint16_t status;//0 idle;1:wait;2:responded
  uint16_t msg_id;
  void* respond;
  uint32_t res_size;
  std::mutex mtx;
  std::condition_variable cv;
} hnode_req_ctx_t;

class hnode{
private:
  uint16_t node_id;
  nng_socket pub_sock;
  
  
  nng_socket sub_sock;
  nng_aio *sub_aio;

  uint64_t heartbeat_lasttime;//ms
  nng_msg* nmsg_heartbeat;

  std::atomic<uint32_t> req_seq;
  std::unordered_map<uint16_t,hsuber_t> suber;//msg_id
  std::unordered_map<uint32_t,hnode_req_ctx_t*> reqer;//seq
private:
  int connect_broker();
  static void on_subscrib_recv_cb(void*);
  static int on_status_req(const hmsg_t* hm, void* p);
  static int on_req(const hmsg_t* hm, void* p);
public:
  hnode(int);
  ~hnode();


  int send_heartbeat();
  int publish(uint16_t msg_id,const void* d,uint32_t s,uint16_t target_node_id=0);
  int subscrib(uint16_t msg_id,subscrib_handler_t h,void* p);
  // int spin_once();
  // int spin();
  int request(uint16_t node_id,
              uint16_t msg_id,
              const void* req,
              uint32_t req_size,
              void* res,
              uint32_t res_size,
              int timeout);


};





}



#endif