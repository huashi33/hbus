#include "hbus.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "tcommon.h"
#include "nng/supplemental/util/platform.h"
#include "hcommon.h"
#define TPUB_APPID 100
#define TSUB_APPID 2
#define TTOPIC_ID 0x0100




static int tpub(int delay,uint32_t msg_total){
  int r;
  int all_count = msg_total;
  hbus::hnode n(TPUB_APPID);
  while (all_count--){
    // n.send_heartbeat();
    uint64_t now= hbus::hcommon::clock_now_ns();
    r = n.publish(TTOPIC_ID,&now,sizeof(now));
    // fprintf(stdout,"[%03d]pub\n",d.count);
    usleep(delay);
  }
  sleep(5);

  printf("[%d]publish:%d\n",r,msg_total);
  return 0;
}
static int tsub_handle(const hbus::hmsg_t* hm,void* p){
  uint64_t tsend = *((uint64_t*)hm->payload);



  TPStatics_t* tps = (TPStatics_t*)p;
  ++tps->msg_total;
  tps->time_total += (hbus::hcommon::clock_now_ns() - tsend)/1e6;
  
  // TStatics_t* total = (TStatics_t*)p;
  // ++total->count;
  // total->a1 += (td->a2 - td->a1);
  // total->a2 += (td->a3 - td->a2);
  // total->a3 += (td->a3 - td->a1); 
  // printf("total->count:%d\n",td->count);
  // printf("%f = %f + %f\n",t,t1,t2);
  return 0;
}
static int tsub(int node_id){
  // int all_count = TPUB_COUNT_TOTAL;

  int r;
  TPStatics_t tps;
  memset(&tps,0,sizeof tps);
  hbus::hnode n(node_id);
  r = n.subscrib(TTOPIC_ID,tsub_handle,&tps);

  int count = 10;
  while (count--){
    sleep(1);
  }
  


  tpstatics_print(&tps);
  return 0;
}
int main(int argc,char *argv[]){
  if('p' == argv[1][0]) {
    int interval = atoi(argv[2]);
    int count = atoi(argv[3]);
    return tpub(interval,count);
  }
  if('s' == argv[1][0]){
    int node_id = atoi(argv[2]);
    return tsub(node_id);
  } 
}