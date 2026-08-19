#include "hcommon.h"
#include <time.h>
namespace hbus{


hcommon::hcommon(){}
hcommon::~hcommon(){}

uint64_t hcommon::clock_now_us(){
  timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  return at.tv_sec*1e6+at.tv_nsec/1e3;
}

}

