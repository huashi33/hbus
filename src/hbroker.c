#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hcore/hhash.h"
#include "hmsg.h"
#include "hv/hloop.h"
#include "hv/hsocket.h"

typedef struct hsession_{
  hio_t* io;
  uint16_t node_id;
  uint16_t timeout_sec;
}hsession_t;

#define HSESSION_TIMEOUT_SEC 30
HIHASH_DEFINE(uint16_t, hsession_t);



static unpack_setting_t unpack_setting;
static hhash_hsession_t* sessions_map = NULL;  // key: node_id, value: hsession_t



// 客户端数据到达回调
static void on_read(hio_t* io, void* buf, int readbytes) {
  printf("recv [%d bytes]\n", readbytes);
  hmsg_t* msg = (hmsg_t*)buf;
  if(HBUS_APPID_BROKER != msg->to) {
    // forward to other node
    HIHASH_ITERATE it = HIHASH_FIND(sessions_map, msg->to);
    if (!HIHASH_ITERATE_VALID(sessions_map, it)) {
      printf("no session for node_id=%d\n", msg->to);
      return;
    }
    hsession_t* session = HIHASH_GETVPTR(sessions_map, it);
    printf("forward to node_id=%d, fd=%d\n", session->node_id, hio_fd(session->io));
    hio_write(session->io, buf, readbytes);
  }
  else{
    // process msg for broker
    printf("recv msg for broker, msg_id=%d, from=%d, seq=%d, payload_size=%d\n", 
        msg->msg_id, msg->from, msg->seq, msg->payload_size);
    if(HBUS_NODE_STATUS == msg->msg_id) {
      HIHASH_ITERATE it = HIHASH_FIND(sessions_map, msg->to);
      if (!HIHASH_ITERATE_VALID(sessions_map, it)) {
        hsession_t session = {io, msg->from, HSESSION_TIMEOUT_SEC};
        HIHASH_SET(sessions_map, msg->from, session);
      }
      else {
        hsession_t* session = HIHASH_GETVPTR(sessions_map, it);
        session->timeout_sec = HSESSION_TIMEOUT_SEC;
      }
      printf("node_id=%d, status=%s\n", msg->from, msg->payload);
    }
  }
  

}

// 客户端断开回调
static void on_close(hio_t* io) {
  printf("client disconnected! fd=%d\n", hio_fd(io));
}

// 新客户端接入回调
static void on_accept(hio_t* io) {
  // 1. 正确获取客户端 IP 字符串的写法
  char peeraddrstr[SOCKADDR_STRLEN] = {0};
  SOCKADDR_STR(hio_peeraddr(io), peeraddrstr);

  printf("client connected! fd=%d, addr: %s\n", hio_fd(io), peeraddrstr);

  // 2. 正确的 libhv 回调绑定函数名
  hio_setcb_read(io, on_read);
  hio_setcb_close(io, on_close);
  hio_set_unpack(io, &unpack_setting);




  // 开始读取
  hio_read_start(io);
}

int main(int argc, char** argv) {
  int port = 55000;
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


  HIHASH_INIT(sessions_map);
  printf("listen: 0.0.0.0:%d ...\n", port);

  // 3. 启动事件循环（进入主运行事件轮询，阻塞）
  hloop_run(loop);

  // 4. 退出后清理事件循环资源
  hloop_free(&loop);
  HIHASH_FINI(sessions_map);
  return 0;
}