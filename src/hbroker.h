#ifndef _HBROKER_H_
#define _HBROKER_H_
#include <stdint.h>
// #include <vector>


#define NODE_WATCHDOG_INIT 10 // second

typedef struct nodeinfo_{
  uint16_t node_id;
  uint32_t watchdog;

}nodeinfo_t;






#endif