// #include "hbus.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "hcore/hbuf.h"
#include "tcommon.h"

#include "hnode.h"
#define TPUB_APPID 100
#define TSUB_APPID 2
#define TTOPIC_ID 0x0001


#define HBROKER_LISTEN_PORT 55000
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
  hnode_t *n = hnode_init(TPUB_APPID, "127.0.0.1", HBROKER_LISTEN_PORT);
  while(HNODE_STATUS_CONNECTED != hnode_status(n)){
    hv_usleep(10*1000);
    printf("wait to connected\n");
  }
  uint64_t pay[12];
  memset(pay,0,sizeof(pay));
  while (all_count--){
    // n.send_heartbeat();
    pay[0]= hcommon_clock_now_ns();
    r = hnode_publish(n,TTOPIC_ID,HMSG_APPID_EVERYONR,pay,sizeof(pay));
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
  hnode_t *n = hnode_init(node_id,"127.0.0.1",HBROKER_LISTEN_PORT);

  while(HNODE_STATUS_CONNECTED != hnode_status(n)){
    sleep(1);
  }
  r = hnode_subscribe(n,TTOPIC_ID,tsub_handle,&tps);
  // int count = 10;
  while (count--){
    sleep(1);
  }
  tpstatics_print(&tps);
  return 0;
}

static int treq(uint16_t node_id,uint16_t server_node_id,int count){
  int r;
  hnode_t *n = hnode_init(node_id, "127.0.0.1",HBROKER_LISTEN_PORT);
  while(HNODE_STATUS_CONNECTED != hnode_status(n)){
    sleep(1);
  }
  // r = hnode_init(&n,node_id);
  // size_t size = (sizeof (hnode_status_t) )+100*sizeof(uint16_t);
  // hnode_status_t *status = (hnode_status_t *)malloc(size);
  uint64_t t1 = hcommon_clock_now_ns();
  hbuf_t req, res;
  hbuf_init(&res, 0);
  r = hnode_request(n,HMSG_MSGID_CONNECT,server_node_id,&req,&res,1000);
  uint64_t t = hcommon_clock_now_ns() - t1;
  double dt = t / 1e3;
  fprintf(stdout,"cost-0:%.3f us\n",dt);
  fprintf(stdout,"repond:%s\n",res.data);

  t1 = hcommon_clock_now_ns();
  for(int i = 0;i<count;++i){
    hbuf_clear(&res);
    r = hnode_request(n,HMSG_MSGID_CONNECT,server_node_id,&req,&res,1000); 
  }
  t = hcommon_clock_now_ns() - t1;
  dt = t / 1e3;

  
  fprintf(stdout,"cost-%d:%.3f us\n",count,dt);
  fprintf(stdout,"repond:%s\n",res.data);

  // free(status);
  hbuf_fini(&res);
  return 0;

}


static void t_respond_connect(const hmsg_t* msg,void* p){
  hnode_t* node = (hnode_t*)p;

  
  hmsg_t* respond = (hmsg_t*)calloc(1,sizeof(hmsg_t)+ 8);
  respond->topic = msg->topic;
  respond->from = hnode_id(node);
  respond->to = msg->from;
  respond->flags = HMSG_FLAG_RESPOND;
  respond->payload_size = 2;
  memcpy(respond->payload, "OK", 2);


  
  hnode_publish_in_threadio(node,respond,sizeof(hmsg_t)+ 8);
}
static int trep(int node_id){
  hnode_t *n = hnode_init(node_id, "127.0.0.1",HBROKER_LISTEN_PORT);
  while(HNODE_STATUS_CONNECTED != hnode_status(n)){
    sleep(1);
  }

  hnode_subscribe(n, HMSG_MSGID_CONNECT, t_respond_connect, n);
  
  while(1){
    sleep(1);
  }
  return 0;
}


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

  if(0 == strcmp("rep",argv[1])){
    int node_id = atoi(argv[2]);
    return trep(node_id);
  } 
  if(0 == strcmp("req",argv[1])){
    int node_id = atoi(argv[2]);
    int server_node_id = atoi(argv[3]);
    int count = atoi(argv[4]);
    return treq(node_id,server_node_id,count);
  } 
}