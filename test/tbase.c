// #include "hbus.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "tcommon.h"

#include "hnode.h"
#define TPUB_APPID 100
#define TSUB_APPID 2
#define TTOPIC_ID 0x0100

uint64_t hcommon_clock_now_ns() {
  struct timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  return at.tv_sec * 1e9 + at.tv_nsec;
}
double hcommon_clock_now_ms() {
  struct timespec at;
  clock_gettime(CLOCK_MONOTONIC, &at);
  double now = at.tv_sec * 1e3 + at.tv_nsec/1e6;
  return now;
}

static int tpub(int delay,uint32_t msg_total){
  int r;
  int all_count = msg_total;
  hnode_t n = {TPUB_APPID};
  r = hnode_init(&n);
  while(HNODE_STATUS_CONNECTED != n.status){
    hv_usleep(10*1000);
  }
  uint64_t pay[12];
  memset(pay,0,sizeof(pay));
  while (all_count--){
    // n.send_heartbeat();
    pay[0]= hcommon_clock_now_ns();
    r = hnode_publish(&n,TTOPIC_ID,HMSG_APPID_EVERYONR,pay,sizeof(pay));
    // fprintf(stdout,"[%03d]pub\n",d.count);
    usleep(delay);
  }
  sleep(5);

  printf("[%d]publish:%d\n",r,msg_total);
  return 0;
}
static void tsub_handle(const hmsg_t* hm,void* p){
  uint64_t* pay = (uint64_t*)hm->payload;
  pay[5] = hcommon_clock_now_ns();

  for(int i = 0;i<5;++i){
    double cost_ns = (pay[i+1] - pay[i])/1e3;
    printf("[%d]:%.3f us\n",i,cost_ns);
  }

  double cost_ns = (pay[5] - pay[0])/1e3;
  printf("all:%.3f us\n",cost_ns);


  uint64_t tsend= *((uint64_t*)hm->payload);
  uint64_t trecv = hcommon_clock_now_ns();
  double cost = (trecv - tsend)/1e3;
  // printf("cost:%.3f us\n",cost);

  TPStatics_t* tps = (TPStatics_t*)p;
  ++tps->msg_total;
  tps->time_total += (hcommon_clock_now_ns() - tsend)/1e6;
  
  
  
  // TStatics_t* total = (TStatics_t*)p;
  // ++total->count;
  // total->a1 += (td->a2 - td->a1);
  // total->a2 += (td->a3 - td->a2);
  // total->a3 += (td->a3 - td->a1); 
  // printf("total->count:%d\n",td->count);
  // printf("%f = %f + %f\n",t,t1,t2);
  // return 0;
}
static int tsub(int node_id,int count){
  // int all_count = TPUB_COUNT_TOTAL;

  int r;
  TPStatics_t tps;
  memset(&tps,0,sizeof tps);
  hnode_t n = {node_id};
  r = hnode_init(&n);

  while(HNODE_STATUS_CONNECTED != n.status){
    sleep(1);
  }
  r = hnode_subscribe(&n,TTOPIC_ID,tsub_handle,&tps);
  // int count = 10;
  while (count--){
    sleep(1);
  }
  


  tpstatics_print(&tps);
  return 0;
}

// static int treq(uint16_t node_id,uint16_t server_node_id,int count){
//   int r;
//   hnode_t n;
//   r = hnode_init(&n,node_id);
//   // size_t size = (sizeof (hnode_status_t) )+100*sizeof(uint16_t);
//   // hnode_status_t *status = (hnode_status_t *)malloc(size);
//   uint64_t t1 = hcommon_clock_now_ns();
//   hbuf_t req, res;
//   hbuf_init(&res, 0);
//   r = hnode_request(&n,HBUS_NODE_STATUS,server_node_id,&req,&res,1000);
//   uint64_t t = hcommon_clock_now_ns() - t1;
//   double dt = t / 1e3;
//   fprintf(stdout,"cost-0:%.3f us\n",dt);

//   t1 = hcommon_clock_now_ns();
//   for(int i = 0;i<count;++i){
//     hbuf_clear(&res);
//     r = hnode_request(&n,HBUS_NODE_STATUS,server_node_id,&req,&res,1000); 
//   }
//   t = hcommon_clock_now_ns() - t1;
//   dt = t / 1e3;

//   hnode_status_t *status = (hnode_status_t *)res.data;
//   fprintf(stdout,"cost-%d:%.3f us\n",count,dt);
//   fprintf(stdout,"node_id:%hu\n",status->node_id);
//   fprintf(stdout,"sub_msg_id_count:%hu\n",status->sub_msg_id_count);
//   for (size_t i = 0; i < status->sub_msg_id_count; i++){
//     fprintf(stdout,"sub_msg_id[%zu]:%hu\n",i,status->sub_msg_id[i]);
//   }
//   // free(status);
//   hbuf_deinit(&res);
//   return 0;

// }
// static int trep(int node_id){

//   return 0;
// }


int main(int argc,char *argv[]){
  if('p' == argv[1][0]) {
    int interval = atoi(argv[2]);
    int count = atoi(argv[3]);
    return tpub(interval,count);
  }
  if('s' == argv[1][0]){
    printf("sub\n");
    int node_id = atoi(argv[2]);
    int count = atoi(argv[3]);
    return tsub(node_id,count);
  } 

  // if(0 == strcmp("rep",argv[1])){
  //   int node_id = atoi(argv[2]);
  //   return trep(node_id);
  // } 
  // if(0 == strcmp("req",argv[1])){
  //   int node_id = atoi(argv[2]);
  //   int server_node_id = atoi(argv[3]);
  //   int count = atoi(argv[4]);
  //   return treq(node_id,server_node_id,count);
  // } 
}