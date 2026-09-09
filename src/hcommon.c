#define _POSIX_C_SOURCE 200809L
#include "hcommon.h"
#include <time.h>

uint64_t hcommon_clock_now_ns(){
  struct timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  return at.tv_sec*1e9+at.tv_nsec;
}



