#ifndef _HCOMMON_H_
#define _HCOMMON_H_
#include <stdint.h>
// log
// typedef void (*hbus_log_func_t)(const char* ctx);
// typedef struct hbus_log_ {
//   uint8_t level;
//   hbus_log_func_t* log_cb;
// } hbus_log_t;

// #define HBUS_LOG_DEBUG(LEV,fmt,...) fprintf(stdout,fmt,__VA_ARGS__)
// #define HBUS_LOG_INFO(LEV,fmt,...) fprintf(stdout,fmt,__VA_ARGS__)
// #define HBUS_LOG_WARN(LEV,fmt,...) fprintf(stdout,fmt,__VA_ARGS__)
// #define HBUS_LOG_ERROR(LEV,fmt,...) fprintf(stdout,fmt,__VA_ARGS__)





uint64_t hcommon_clock_now_ns();





// namespace hbus{



// class hcommon{
// private:
//   hcommon(/* args */);
//   ~hcommon();
// public:
  
  
  
//   static uint64_t clock_now_ns();
// };



// }



#endif