#ifndef _HCOMMON_H_
#define _HCOMMON_H_
#include <stdint.h>












namespace hbus{



class hcommon{
private:
  hcommon(/* args */);
  ~hcommon();
public:
  
  
  
  static uint64_t clock_now_ns();
};



}



#endif