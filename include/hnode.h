#ifndef _HNODE_H_
#define _HNODE_H_
#include <stdint.h>


typedef struct hnode_ {
  uint16_t id;
  uint16_t status;
} hnode_t;


int hnode_connect(hnode_t* node, const char* ip, uint16_t port);
int hnode_disconnect(hnode_t* node);

int hnode_publish(hnode_t* node, uint16_t msg_id, const char* payload, uint32_t payload_size);
int hnode_publish_to(hnode_t* node, uint16_t to_node_id, uint16_t msg_id, const char* payload, uint32_t payload_size);
int hnode_subscribe(hnode_t* node, uint16_t msg_id);
int hnode_unsubscribe(hnode_t* node, uint16_t msg_id);
int hnode_request(hnode_t* node, uint16_t to_node_id, uint16_t msg_id, const char* payload, uint32_t payload_size);


#endif