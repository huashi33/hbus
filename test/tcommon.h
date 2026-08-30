#ifndef _TCOMMON_H_
#define _TCOMMON_H_
#include <stdint.h>
#include <stdio.h>
typedef struct TPStatics_{
  uint64_t msg_total;       //msg num count
  double msg_avg_ps;  //msg num pre second
  double time_total; //ms
  double time_avg_pm;   //ms pre msg
}TPStatics_t;

static inline void tpstatics_print(TPStatics_t *tps){
  // tps->time_total /= 1e6;
  tps->msg_avg_ps = tps->msg_total*1e3 / tps->time_total;
  tps->time_avg_pm = tps->time_total / tps->msg_total;

  fprintf(stdout,"msg_total:%lu\n",tps->msg_total);
  fprintf(stdout,"msg_avg_ps:%.3f /s\n",tps->msg_avg_ps);
  fprintf(stdout,"time_total:%.3f ms\n",tps->time_total);
  fprintf(stdout,"time_avg_pm:%.3f ms\n",tps->time_avg_pm);
}

#endif