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
    nng_close(sub_sock);
    return 1;
  }
  nng_recv_aio(sub_sock, sub_aio);
  return rv;
}

void hnode::on_recv_cb(void* arg) {
  hnode* node = (hnode*)arg;
  int rv = nng_aio_result(node->sub_aio);
  if (rv == NNG_ECLOSED) return;
  if (rv != 0) {
    fprintf(stderr, "[sub] recv error: %s\n", nng_strerror(rv));
    return;
  }
  nng_msg* msg = nng_aio_get_msg(node->sub_aio);
  size_t l = nng_msg_len(msg);
  // fprintf(stdout,"[%zu] recv nng_msg\n", l);

  if (sizeof(hbus::hmsg_t) > l) {
    fprintf(stderr, "[sub] recv len error: %zu\n", l);
    nng_msg_free(msg);
    nng_recv_aio(node->sub_sock, node->sub_aio);
    return;
  }

  hbus::hmsg_t* m = (hbus::hmsg_t*)nng_msg_body(msg);
  auto it = node->suber.find(m->content_id);
  if (node->suber.end() != it) {
    it->second.h(m->payload, m->payload_size, it->second.param);
  }

  nng_recv_aio(node->sub_sock, node->sub_aio);
}

int hnode::send_heartbeat() {
  nng_msg* msg;
  nng_msg_alloc(&msg, 0);
  hmsg_t hm = {.magic = HBUS_MSG_MAGIC,
               .content_id = HBUS_SYS_HEARTBEAT,
               .msg_type = HBUS_MSGTYPE_SYS,
               .from = node_id,
               .to = HBUS_APPID_BROKER,
               .payload_size = 0};
  // int rv = nng_send(pub_sock, &msg, sizeof msg, 0);
  nng_msg_append(msg, &hm, sizeof hm);
  nng_sendmsg(pub_sock, msg, 0);
  return 0;
}

hnode::hnode(int node_id) {
  this->node_id = node_id;
  printf("hnode ctor\n");
  // broker_url.assign(url);
  int rv;
  if ((rv = nng_sub0_open(&sub_sock)) != 0) {
    fprintf(stderr, "sub open: %s\n", nng_strerror(rv));
  }
  // if ((rv = nng_setopt(sub_sock, NNG_OPT_SUB_SUBSCRIBE, "", 0)) != 0) {
  //   fprintf(stderr, "set subscribe: %s\n", nng_strerror(rv));
  //   nng_close(sub_sock);
  // }
  nng_aio_alloc(&sub_aio, hnode::on_recv_cb, this);

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

int hnode::publish(uint16_t topic_id, const void* d, uint32_t s) {
  // sizeof(hmsg_t)
  nng_msg* msg;
  nng_msg_alloc(&msg, 0);
  hbus::hmsg_t hm = {.magic = HBUS_MSG_MAGIC,
                     .content_id = topic_id,
                     .msg_type = HBUS_MSGTYPE_PUBLISH,
                     .from = node_id,
                     .to = 0,
                     .payload_size = s};
  nng_msg_append(msg, &hm, sizeof hm);
  nng_msg_append(msg, d, s);
  nng_sendmsg(pub_sock, msg, 0);

  // size_t total_size = sizeof(hmsg_t) + s;
  // hbus::hmsg_t* msg = (hbus::hmsg_t*)malloc(total_size);
  // msg->from = node_id;
  // msg->to = 0;
  // msg->msg_type = HBUS_MSGTYPE_PUBLISH;
  // msg->content_id = topic_id;
  // msg->payload_size = s;
  // memcpy(msg->payload, d, s);
  // int rv = nng_send(pub_sock, msg, total_size, 0);

  // free(msg);
  // send_heartbeat();
  return 0;
}
int hnode::subscrib(uint16_t topic_id, subscrib_handler_t h, void* param) {
  int rv;
  hmsg_t msg = {.magic = HBUS_MSG_MAGIC, .content_id = topic_id};

  if ((rv = nng_setopt(sub_sock, NNG_OPT_SUB_SUBSCRIBE, &msg,
                       sizeof(hmsg_t::magic) + sizeof(hmsg_t::content_id))) !=
      0) {
    fprintf(stderr, "set subscribe: %s\n", nng_strerror(rv));
    nng_close(sub_sock);
    return rv;
  }

  suber[topic_id] = {param, h};
  return rv;
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