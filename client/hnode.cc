#include "hnode.h"

#include <stdio.h>
#include <string.h>

#include "hmsg.h"
#include "nng/protocol/pubsub0/pub.h"
#include "nng/protocol/pubsub0/sub.h"
#include "nng/supplemental/util/platform.h"
#define HBUS_BROKER_PUB_URL "tcp://127.0.0.1:5555"
#define HBUS_BROKER_SUB_URL "tcp://127.0.0.1:5556"
// #define HBUS_BROKER_PUB_URL "ipc:///tmp/sub"
// #define HBUS_BROKER_SUB_URL "ipc:///tmp/pub"

namespace hbus {

int hnode::connect_broker() {
  int rv;
  // 连接到发布端
  if ((rv = nng_dial(pub_sock, HBUS_BROKER_PUB_URL, NULL, 0)) != 0) {
    fprintf(stderr, "dial: %s\n", nng_strerror(rv));
    nng_close(pub_sock);
    return 1;
  }

  // 连接到发布端
  if ((rv = nng_dial(sub_sock, HBUS_BROKER_SUB_URL, NULL, 0)) != 0) {
    fprintf(stderr, "dial: %s\n", nng_strerror(rv));
    nng_close(pub_sock);
    return 1;
  }
  return rv;
}

int hnode::send_heartbeat() {
  hmsg_t msg = {.msg_type = HBUS_MSGTYPE_CONTROL,
                .content_id = HBUS_CONTROL_HEARTBEAT,
                .from = node_id,
                .to = HBUS_APPID_BROKER};
  int rv = nng_send(pub_sock, &msg, sizeof msg, 0);
  return rv;
}

hnode::hnode(int) {
  printf("hnode ctor\n");
  // broker_url.assign(url);
  int rv;
  if ((rv = nng_sub0_open(&sub_sock)) != 0) {
    fprintf(stderr, "sub open: %s\n", nng_strerror(rv));
  }
  if ((rv = nng_setopt(sub_sock, NNG_OPT_SUB_SUBSCRIBE, "", 0)) != 0) {
    fprintf(stderr, "set subscribe: %s\n", nng_strerror(rv));
    nng_close(sub_sock);
  }

  if ((rv = nng_pub0_open(&pub_sock)) != 0) {
    fprintf(stderr, "pub open: %s\n", nng_strerror(rv));
  }

  connect_broker();
}
hnode::~hnode() {
  printf("hnode dtor\n");
  nng_close(pub_sock);
  nng_close(sub_sock);
}

int hnode::publish(int topic_id, const void* d, size_t s) {
  // sizeof(hmsg_t)
  size_t total_size = sizeof(hmsg_t) + s;
  hbus::hmsg_t* msg = (hbus::hmsg_t*)malloc(total_size);
  msg->content_id = topic_id;
  msg->payload_size = s;
  memcpy(msg->payload, d, s);
  int rv = nng_send(pub_sock, msg, total_size, 0);
  free(msg);
  // send_heartbeat();
  return rv;
}
int hnode::subscrib(int topic_id, subscrib_handler_t h, void* param) {
  suber[topic_id] = {param, h};
  return 0;
}

int hnode::spin_once() {
  int rv;
  char* buf = NULL;
  size_t sz;
  send_heartbeat();
  if (0 == (rv = nng_recv(sub_sock, &buf, &sz, NNG_FLAG_ALLOC))) {
    hbus::hmsg_t* msg = (hbus::hmsg_t*)buf;
    auto v = suber.find(msg->content_id);
    if (suber.end() != v) {
      hsuber_t s = v->second;
      s.h(msg->payload, msg->payload_size, s.param);
    }
  }
  // fprintf(stderr, "recv: %s\n", nng_strerror(rv));
  nng_free(buf, sz);
  
  return rv;
}
int hnode::spin() { return 0; }
}  // namespace hbus