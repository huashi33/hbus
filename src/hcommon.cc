#include "hcommon.h"
#include <time.h>
namespace hbus{


hcommon::hcommon(){}
hcommon::~hcommon(){}

uint64_t hcommon::clock_now_ns(){
  timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  return at.tv_sec*1e9+at.tv_nsec;
}

}

