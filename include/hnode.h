#ifndef _HNODE_H_
#define _HNODE_H_
#include "nng/nng.h"
#include <stddef.h>
#include <string>
#include <unordered_map>
#include "hmsg.h"



namespace hbus{




typedef int (*subscrib_handler_t)(const hmsg_t* hmsg,void* p);
// typedef int (*response_handle_t)(int request_id,const void* d,size_t s);
typedef struct hsuber_{
  void* param;
  subscrib_handler_t h;
}hsuber_t;

class hnode{
private:
  uint16_t node_id;
  nng_socket pub_sock;
  
  
  nng_socket sub_sock;
  nng_aio *sub_aio;

  uint64_t heartbeat_lasttime;//ms
  nng_msg* nmsg_heartbeat;

  // std::string broker_url;
  std::unordered_map<int,hsuber_t> suber;
private:
  int connect_broker();
  static void on_subscrib_recv_cb(void*);
  static int on_status_req(const hmsg_t* hm, void* p);
public:
  hnode(int);
  ~hnode();


  int send_heartbeat();
  int publish(uint16_t topic_id,const void* d,uint32_t s);
  int subscrib(uint16_t topic_id,subscrib_handler_t h,void*);
  int spin_once();
  int spin();
  // int request(int request_id,const void* d,size_t s);
  // int response(int request_id);

};





}



#endif