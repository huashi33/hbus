#ifndef _HNODE_H_
#define _HNODE_H_
#include "nng/nng.h"
#include <stddef.h>
#include <string>
#include <unordered_map>




namespace hbus{



typedef int (*subscrib_handler_t)(const void* d,size_t s,void*);
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
  // std::string broker_url;
  std::unordered_map<int,hsuber_t> suber;
private:
  int connect_broker();
  int send_heartbeat();
public:
  hnode(int);
  ~hnode();


  int publish(int topic_id,const void* d,size_t s);
  int subscrib(int topic_id,subscrib_handler_t h,void*);
  int spin_once();
  int spin();
  // int request(int request_id,const void* d,size_t s);
  // int response(int request_id);

};





}



#endif