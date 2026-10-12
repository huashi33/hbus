#include "hnode.h"



int hnode_connect(hnode_t* node, const char* ip, uint16_t port){
  return 0;
}
int hnode_disconnect(hnode_t* node){
  return 0;
}
int hnode_publish(hnode_t* node, uint16_t msg_id, const char* payload, uint32_t payload_size){
  return 0;
}
int hnode_publish_to(hnode_t* node, uint16_t to_node_id, uint16_t msg_id, const char* payload, uint32_t payload_size){
  return 0;
}
int hnode_subscribe(hnode_t* node, uint16_t msg_id){
  return 0;
}
int hnode_unsubscribe(hnode_t* node, uint16_t msg_id){
  return 0;
}
int hnode_request(hnode_t* node, uint16_t to_node_id, uint16_t msg_id, const char* payload, uint32_t payload_size){
  return 0;
}
