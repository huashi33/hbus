#include "hnode.h"

#include <stdio.h>
#include <string.h>

#include "hcommon.h"
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

  // nng_recv_aio(sub_sock, sub_aio);
  return subscrib(HBUS_NODE_STATUS, hnode::on_status_req, this);
}

void hnode::on_subscrib_recv_cb(void* arg) {
  // fprintf(stderr, "on_subscrib_recv_cb in\n");
  hnode* node = (hnode*)arg;
  int rv = nng_aio_result(node->sub_aio);
  if (rv == NNG_ECLOSED) return;
  if (rv != 0) {
    fprintf(stderr, "[sub] <- error: %s\n", nng_strerror(rv));
    return;
  }
  nng_msg* nmsg = nng_aio_get_msg(node->sub_aio);
  size_t l = nng_msg_len(nmsg);
  // fprintf(stdout,"[%zu] <- nng_msg\n", l);

  if (sizeof(hbus::hmsg_t) > l) {
    fprintf(stderr, "[sub] <- len error: %zu\n", l);
    nng_msg_free(nmsg);
    nng_recv_aio(node->sub_sock, node->sub_aio);
    return;
  }

  hbus::hmsg_t* hm = (hbus::hmsg_t*)nng_msg_body(nmsg);
  // fprintf(stdout,"<- msg_id(%d) %d->%d\n",hm->msg_id,hm->from,hm->to);

  // TODO:check crc

  // boardcast || own
  if (0 == hm->to || node->node_id == hm->to) {
    auto it = node->suber.find(hm->msg_id);
    if (node->suber.end() != it) {
      // TODO:Thread pool
      it->second.h(hm, it->second.param);
    } else {
      fprintf(stderr,
              "[sub]msg_id error:node(%d) <- msg_id(%d),from %d to %d\n",
              node->node_id, hm->msg_id, hm->from, hm->to);
    }
  } else {
    fprintf(stderr, "[sub]to error:node(%d) <- msg_id(%d),from %d to %d\n",
            node->node_id, hm->msg_id, hm->from, hm->to);
  }
  nng_recv_aio(node->sub_sock, node->sub_aio);
}

int hnode::on_status_req(const hmsg_t* hm, void* p) {
  hnode* n = (hnode*)p;
  nng_msg* nmsg_heartbeat;
  nng_msg_alloc(&nmsg_heartbeat, 0);

  hbus::hmsg_t hms = {.magic = HBUS_MSG_MAGIC,
                      .msg_id = HBUS_MSG_REPLY(HBUS_NODE_STATUS),
                      .to = hm->from,
                      .align = 0,
                      .from = n->node_id,
                      .seq = hm->seq};

  hms.payload_size =
      sizeof(hnode_status_t) + n->suber.size() * sizeof(uint16_t);
  nng_msg_append(nmsg_heartbeat, &hms, sizeof hms);
  hnode_status_t status = {.node_id = n->node_id,
                           .sub_msg_id_count = (uint16_t)n->suber.size()};
  nng_msg_append(nmsg_heartbeat, &status, sizeof status);
  for (auto& s : n->suber) {
    uint16_t msg_id = s.first;
    nng_msg_append(nmsg_heartbeat, &msg_id, sizeof msg_id);
  }

  nng_sendmsg(n->pub_sock, nmsg_heartbeat, 0);

  fprintf(stderr, "-> msg_id(%d) %d->%d\n", hms.msg_id, hms.from, hms.to);
  return 0;
}

int hnode::on_request(const hmsg_t* hm, void* p) {
  // muti-thread
  hnode* n = (hnode*)p;
  auto it = n->reqer.find(hm->seq);
  if (n->reqer.end() != it) {
    hnode_req_ctx_t* ctx = it->second;
    fprintf(stdout, "replay: msg_id(%d) %d->%d:%u\n", hm->msg_id, hm->from,
            hm->to, hm->payload_size);
    if (ctx->res_size >= hm->payload_size) {
      memcpy(ctx->respond, hm->payload, hm->payload_size);
      ctx->status = 2;
    } else {
      // TODO
    }
    ctx->cv.notify_one();
  }
  fprintf(stdout, "on_rep out: msg_id(%d) %d->%d\n", hm->msg_id, hm->from,
          hm->to);
  return 0;
}

hnode::hnode(int node_id) {
  printf("hnode ctor\n");
  // init param
  this->node_id = node_id;
  req_seq.store(0, std::memory_order::memory_order_relaxed);
  nng_aio_alloc(&sub_aio, hnode::on_subscrib_recv_cb, this);

  // init sock
  int rv;
  if ((rv = nng_sub0_open(&sub_sock)) != 0) {
    fprintf(stderr, "sub open: %s\n", nng_strerror(rv));
  }
  if ((rv = nng_pub0_open(&pub_sock)) != 0) {
    fprintf(stderr, "pub open: %s\n", nng_strerror(rv));
  }

  // start recv
  connect_broker();
}
hnode::~hnode() {
  printf("hnode dtor\n");
  nng_close(pub_sock);
  nng_close(sub_sock);
}

int hnode::publish(uint16_t topic_id, const void* d, uint32_t s,
                   uint16_t target_node_id, uint32_t seq) {
  // sizeof(hmsg_t)
  nng_msg* msg;
  nng_msg_alloc(&msg, 0);
  hbus::hmsg_t hm = {.magic = HBUS_MSG_MAGIC,
                     .msg_id = topic_id,
                     .to = target_node_id,
                     .align = 0,
                     .from = node_id,
                     .seq = seq,
                     .payload_size = s};
  nng_msg_append(msg, &hm, sizeof hm);
  nng_msg_append(msg, d, s);
  int rv = nng_sendmsg(pub_sock, msg, 0);
  if (rv) {
    nng_msg_free(msg);
  }
  return rv;
}
int hnode::subscrib(uint16_t topic_id, subscrib_handler_t h, void* param) {
  // multi thread
  if (!suber.size()) {
    // lazy subscrib:start <- when first subscrib
    nng_recv_aio(sub_sock, sub_aio);
    fprintf(stdout, "node_id(%d),start recv\n", node_id);
  }

  int rv;
  hmsg_t msg = {.magic = HBUS_MSG_MAGIC,
                .msg_id = topic_id,
                .to = 0,
                .version = 0,
                .align = 0};
  if (rv = nng_socket_set(sub_sock, NNG_OPT_SUB_SUBSCRIBE, &msg,
                          HBUS_MSG_PUBSUB_HEAD_SIZE)) {
    fprintf(stderr, "set subscribe: %s\n", nng_strerror(rv));
    nng_close(sub_sock);
    return rv;
  }
  msg = {.magic = HBUS_MSG_MAGIC,
         .msg_id = topic_id,
         .to = node_id,
         .version = 0,
         .align = 0};
  if (rv = nng_socket_set(sub_sock, NNG_OPT_SUB_SUBSCRIBE, &msg,
                          HBUS_MSG_PUBSUB_HEAD_SIZE)) {
    fprintf(stderr, "set subscribe: %s\n", nng_strerror(rv));
    nng_close(sub_sock);
    return rv;
  }

  suber[topic_id] = {param, h};
  fprintf(stdout, "node_id(%d) subscrib topic_id(%d)\n", node_id, topic_id);
  return rv;
}

int hnode::request(uint16_t target_node_id, uint16_t msg_id, const void* req,
                   uint32_t req_size, void* res, uint32_t res_size,
                   int timeout) {
  // check if subscrib replay
  uint16_t msg_id_replay = HBUS_MSG_REPLY(msg_id);
  auto it = suber.find(msg_id_replay);
  if (suber.end() == it) {
    subscrib(msg_id_replay, hnode::on_request, this);
  }

  // init ctx,TODO: req ctx pool
  hnode_req_ctx_t* ctx = new hnode_req_ctx_t();
  ctx->seq = req_seq.fetch_add(1, std::memory_order::memory_order_relaxed);
  ctx->msg_id = msg_id_replay;
  ctx->respond = res;
  ctx->res_size = res_size;
  ctx->status = 1;
  reqer[ctx->seq] = ctx;

  // publish request
  publish(msg_id, req, req_size, target_node_id, ctx->seq);
  fprintf(stdout, "request(%u): msg_id:%d,req_size:%u,target_node_id:%d\n",
          ctx->seq, msg_id, req_size, target_node_id);

  // wait
  std::unique_lock<std::mutex> l(ctx->mtx);
  bool ret = ctx->cv.wait_for(l, std::chrono::milliseconds(timeout),
                              [&] { return 2 == ctx->status; });

  fprintf(stdout, "ret:%d\n", ret);
  // release resource
  reqer.erase(ctx->seq);
  ctx->status = 0;
  delete ctx;

  int r = ret ? 0 : 1;
  return r;
}

}  // namespace hbus