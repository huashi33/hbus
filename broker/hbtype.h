#ifndef _HSERVERTYPE_H_
#define _HSERVERTYPE_H_
#include <stdint.h>
// #include <vector>


#define NODE_WATCHDOG_INIT 10 // second

typedef struct nodeinfo_{
  uint16_t node_id;
  uint32_t watchdog;
  // std::vector<uint16_t> topic_pub;
  // std::vector<uint16_t> topic_sub;

}nodeinfo_t;






#endif