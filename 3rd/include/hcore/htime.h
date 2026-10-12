#ifndef _HTIME_H_
#define _HTIME_H_
#include "hdef.h"


// htime 
typedef struct htime_{
  uint16_t  year;
  uint8_t   mon;
  uint8_t   day;
  uint8_t   hour;
  uint8_t   min;
  uint8_t   sec;
  uint16_t  millsec;
}htime_t;

// ms since 1970
int htime_utc(uint64_t *t);
int htime_utc2htime(const uint64_t t,htime_t* ht);
int htime_htime2utc(uint64_t *t,const htime_t* ht);



#endif