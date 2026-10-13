#include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include "hmsg.h"
#include "hcore/hhash.h"
#include "hcore/hbuf.h"
#include "hv/hloop.h"
#include "hv/hsocket.h"




typedef struct hsession_{
  hio_t* io;
  uint16_t node_id;
  uint16_t timeout_sec;
}hsession_t;
HIHASH_DEFINE(uint16_t, hbuf_t);

#define HSESSION_TIMEOUT_SEC 30
// #define HROKER_LOG(FMT,...) printf(FMT,##__VA_ARGS__)
#define HROKER_LOG(FMT,...) printf(FMT,##__VA_ARGS__)
#define HBROKER_LISTEN_PORT 55000



static unpack_setting_t unpack_setting;
static hhash_hbuf_t* map_topic_nodes = NULL;  // key: topic, value: hbuf_t(session_t)
static hhash_hbuf_t* map_node_topics = NULL;  // key: node_id, value: hbuf_t(topic)

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
// 客户端数据到达回调
static void on_read(hio_t* io, void* buf, int readbytes) {
    hmsg_t* msg = (hmsg_t*)buf;
    // HROKER_LOG("< [%d](%d)-(%d)->(%d)\n",readbytes,msg->from,msg->msg_id,msg->to);
    if(HMSG_FLAG_DATA == msg->flags) {
      // route to node
uint64_t* pay = (uint64_t*)msg->payload;
pay[2] = hcommon_clock_now_ns();
      HIHASH_ITERATE it = HIHASH_FIND(map_topic_nodes, msg->msg_id);
      if(HIHASH_ITERATE_VALID(map_topic_nodes, it)) {
        hbuf_t* sbuf = HIHASH_GETVPTR(map_topic_nodes, it);
        size_t session_size = sbuf->len / sizeof(hsession_t);
        hsession_t* sessions = (hsession_t*)sbuf->data;
        int send_num = 0;
        for(size_t i=0; i<session_size; i++) {
          if(msg->to & sessions[i].node_id){
            ++send_num;
            // HROKER_LOG("> forward:(%d)(%d)(%d,%d)\n",
            //   msg->from,msg->msg_id, sessions[i].node_id,hio_fd(sessions[i].io));
            pay[3] = hcommon_clock_now_ns();
            hio_write(sessions[i].io, buf, readbytes);
          }
        }
        HROKER_LOG("> forward:byte(%d) num:%d\n",readbytes,send_num);
      }
      else{
        HROKER_LOG("< not-found,(%d)(%d)(%d)\n",
          msg->from,msg->msg_id,msg->to);
      }
    }
    else if(HMSG_FLAG_SUB == msg->flags) {
      // add sub
      hsession_t s = {io,msg->from};
      HIHASH_ITERATE it = HIHASH_FIND(map_topic_nodes, msg->msg_id);
      if(!HIHASH_ITERATE_VALID(map_topic_nodes, it)){
        HROKER_LOG("node(%d,%d):+ topic(%d)\n",msg->from,hio_fd(io),msg->msg_id);
        hbuf_t b;
        hbuf_init(&b, sizeof(hsession_t)*8);
        hbuf_push(&b, &s, sizeof(hsession_t));
        HIHASH_SET(map_topic_nodes, msg->msg_id, b);
        return;
      }

      hbuf_t* sbuf = HIHASH_GETVPTR(map_topic_nodes, it);
      size_t session_num = sbuf->len / sizeof(hsession_t);
      hsession_t* sessions = (hsession_t*)sbuf->data;
      for(size_t i=0; i<session_num; i++) {
        HROKER_LOG(">-(%d)- %d\n",msg->msg_id, sessions[i].node_id);
        if(msg->from == sessions[i].node_id) {
          HROKER_LOG("node(%d,%d) sub,already-sub,topic(%d):\n",msg->from,hio_fd(io),msg->msg_id);
          return;
        }
      }

      HROKER_LOG("node(%d,%d):+ topic(%d)\n",msg->from,hio_fd(io),msg->msg_id);
      hbuf_push(sbuf, &s, sizeof(hsession_t));
    }
    else if(HMSG_FLAG_UNSUB == msg->flags) {
      // remove sub
      HIHASH_ITERATE it = HIHASH_FIND(map_topic_nodes, msg->msg_id);
      if(!HIHASH_ITERATE_VALID(map_topic_nodes, it)){
        HROKER_LOG("node(%d,%d):never sub topic(%d)\n",msg->from,hio_fd(io),msg->msg_id);
        return;
      }

      hbuf_t* sbuf = HIHASH_GETVPTR(map_topic_nodes, it);
      size_t session_num = sbuf->len / sizeof(hsession_t);
      hsession_t* sessions = (hsession_t*)sbuf->data;
      for(size_t i=0; i<session_num; i++) {
        HROKER_LOG(">-(%d)- %d\n",msg->msg_id, sessions[i].node_id);
        if(msg->from == sessions[i].node_id) {
          HROKER_LOG("node(%d,%d):- topic(%d)\n",msg->from,hio_fd(io),msg->msg_id);
          // TODO memmove
          return;
        }
      }

      HROKER_LOG("node(%d,%d):never sub topic(%d)\n",msg->from,hio_fd(io),msg->msg_id);
      return;
    }
    else if(HMSG_FLAG_REGISTER == msg->flags) {
      // hsession_t s = {io,msg->from};
      // hio_set_context(io, node);


      // HIHASH_ITERATE it = HIHASH_FIND(map_topic_nodes, msg->msg_id);
      // if(!HIHASH_ITERATE_VALID(map_topic_nodes, it)){
      //   HROKER_LOG("node(%d,%d):+ topic(%d)\n",msg->from,hio_fd(io),msg->msg_id);
      //   hbuf_t b;
      //   hbuf_init(&b, sizeof(hsession_t)*8);
      //   hbuf_push(&b, &s, sizeof(hsession_t));
      //   HIHASH_SET(map_topic_nodes, msg->msg_id, b);
      //   return;
      // }
    }
    else{
      HROKER_LOG("unknown msg->flags=%d\n", msg->flags);
    }

    

    
}

// 客户端断开回调
static void on_close(hio_t* io) {
  double now_ms = hcommon_clock_now_ms();
  HROKER_LOG("[%.3f]fd(%d) disconnected!\n",now_ms, hio_fd(io));

  // TODO,unsub node in every topic

}

// 新客户端接入回调
static void on_accept(hio_t* io) {
  // 1. 正确获取客户端 IP 字符串的写法
  char peeraddrstr[SOCKADDR_STRLEN] = {0};
  SOCKADDR_STR(hio_peeraddr(io), peeraddrstr);

  double now_ms = hcommon_clock_now_ms();
  HROKER_LOG("[%.3f]fd(%d) connected!, addr: %s\n",now_ms, hio_fd(io), peeraddrstr);

  // 2. 正确的 libhv 回调绑定函数名
  hio_setcb_read(io, on_read);
  hio_setcb_close(io, on_close);
  hio_set_unpack(io, &unpack_setting);




  // 开始读取
  hio_read_start(io);
}

int main(int argc, char** argv) {
  printf("hello \n");
  int port = HBROKER_LISTEN_PORT;
  if (argc > 1) {
    port = atoi(argv[1]);
  }

  // 1. 创建底层事件循环核心
  hloop_t* loop = hloop_new(0);

  // 2. 创建 TCP 服务端监听
  hio_t* listenio = hloop_create_tcp_server(loop, "0.0.0.0", port, on_accept);
  if (listenio == NULL) {
    fprintf(stderr, "监听端口 %d 失败!\n", port);
    return -1;
  }

  memset(&unpack_setting, 0, sizeof(unpack_setting_t));
  unpack_setting.package_max_length = 1024 * 1024;  // 单个包最大长度限制
  unpack_setting.mode = UNPACK_BY_LENGTH_FIELD;
  unpack_setting.length_field_offset =
      offsetof(hmsg_t, payload_size);  // 长度字段在包头部的偏移量
  unpack_setting.length_field_bytes = sizeof(
      ((hmsg_t*)0)->payload_size);  // 长度字段占几个字节（如 2 字节代表长度）
  unpack_setting.length_field_coding = ENCODE_BY_LITTEL_ENDIAN;  // 大端或小端
  unpack_setting.body_offset =
      sizeof(hmsg_t);  // 消息体起始偏移量（跳过长度字段本身）


  HIHASH_INIT(map_topic_nodes);
  printf("listen: 0.0.0.0:%d ...\n", port);

  // 3. 启动事件循环（进入主运行事件轮询，阻塞）
  hloop_run(loop);

  // 4. 退出后清理事件循环资源
  hloop_free(&loop);
  HIHASH_FINI(map_topic_nodes);
  return 0;
}