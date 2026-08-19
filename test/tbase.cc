#include "hbus.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#define TPUB_APPID 1
#define TSUB_APPID 2
#define TTOPIC_ID 10010
#define TPUB_COUNT_TOTAL 1000

typedef struct TStatics_{
  int count;
  int aaaa;
  double a1;
  double a2;
  double a3;
  char algn[256];
}TStatics_t;

static double time_now(){
  timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  return at.tv_sec*1e6+at.tv_nsec/1e3;
}

static int tpub(){
  int r;
  int all_count = TPUB_COUNT_TOTAL;
  hbus::hnode n(TPUB_APPID);
  TStatics_t d;
  memset(&d,0,sizeof d);
  while (all_count--){
    d.a1 = time_now();
    ++d.count;
    r = n.publish(TTOPIC_ID,&d,sizeof(d));
    usleep(200);
  }
  

  printf("[%d]publish:%d\n",r,TPUB_COUNT_TOTAL);
  return 0;
}
static int tsub_handle(const void* d,size_t s,void* p){
  TStatics_t* td = (TStatics_t*)d;
  td->a3 = time_now();
  
  TStatics_t* total = (TStatics_t*)p;
  ++total->count;
  total->a1 += (td->a2 - td->a1);
  total->a2 += (td->a3 - td->a2);
  total->a3 += (td->a3 - td->a1); 
  printf("total->count:%d\n",td->count);
  // printf("%f = %f + %f\n",t,t1,t2);
  return 0;
}
static int tsub(){
  int all_count = TPUB_COUNT_TOTAL;

  int r;
  TStatics_t total;
  memset(&total,0,sizeof total);
  hbus::hnode n(TSUB_APPID);
  r = n.subscrib(TTOPIC_ID,tsub_handle,&total);
  while (TPUB_COUNT_TOTAL > total.count)  {
    n.spin_once();
  }


  printf("count:%d\n",total.count);
  printf("step1:%f us\n",total.a1);
  printf("step2:%f us\n",total.a2);
  printf("total:%f us\n",total.a3);
  printf("single:%f us\n",total.a3/total.count);
  printf("rps:%f /s\n",total.count/total.a3*1e6);
  return 0;
}
int main(int argc,char *argv[]){
  if('p' == argv[1][0]) return tpub();

  if('s' == argv[1][0]) return tsub();
}