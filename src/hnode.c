#include "hnode.h"
#include <stdio.h>
#include <string.h>

#include "hcore/hlog.h"
#include "hcommon.h"
#include "hmsg.h"
#include "nng/protocol/pubsub0/pub.h"
#include "nng/protocol/pubsub0/sub.h"
#include "nng/supplemental/util/platform.h"
#define HBUS_BROKER_PUB_URL "tcp://127.0.0.1:5555"
#define HBUS_BROKER_SUB_URL "tcp://127.0.0.1:5556"
// #define HBUS_BROKER_PUB_URL "ipc:///tmp/sub"
// #define HBUS_BROKER_SUB_URL "ipc:///tmp/pub"

static void hnode_print_suber(hnode_t* n) {
  fprintf(stdout, "hnode(%d) suber:\n", n->node_id);
  HIHASH_ITERATE it = HIHASH_ITERATE_BEGIN(n->suber);
  while (HIHASH_ITERATE_VALID(n->suber, it)) {
    uint16_t msg_id = HIHASH_GETK(n->suber, it);
    fprintf(stdout, "msg_id:%d\n", msg_id);
    it = HIHASH_ITERATE_NEXT(it);
  }
}
static void hnode_print_reqer(hnode_t* n) {
  fprintf(stdout, "hnode(%d) reqer:\n", n->node_id);
  HIHASH_ITERATE it = HIHASH_ITERATE_BEGIN(n->reqer);
  while (HIHASH_ITERATE_VALID(n->reqer, it)) {
    uint32_t seq = HIHASH_GETK(n->reqer, it);
    uintptr_t ctx = HIHASH_GETV(n->reqer, it);
    fprintf(stdout, "seq(%d):%p\n", seq, (void*)ctx);
    it = HIHASH_ITERATE_NEXT(it);
  }
}
static int hnode_on_status_req(const hmsg_t* hm, void* p) {
  hnode_t* n = (hnode_t*)p;
  nng_msg* nmsg_heartbeat;
  nng_msg_alloc(&nmsg_heartbeat, 0);

  hmsg_t hms = {.magic = HBUS_MSG_MAGIC,
                      .msg_id = HBUS_MSG_REPLY(HBUS_NODE_STATUS),
                      .to = hm->from,
                      .align = 0,
                      .from = n->node_id,
                      .seq = hm->seq};

  hms.payload_size =
      sizeof(hnode_status_t) + HIHASH_SIZE(n->suber) * sizeof(uint16_t);
  nng_msg_append(nmsg_heartbeat, &hms, sizeof hms);
  hnode_status_t status = {.node_id = n->node_id,
                           .sub_msg_id_count = (uint16_t)HIHASH_SIZE(n->suber)};
  nng_msg_append(nmsg_heartbeat, &status, sizeof status);
  // for (auto& s : n->suber) {
  //   uint16_t msg_id = s.first;
  //   nng_msg_append(nmsg_heartbeat, &msg_id, sizeof msg_id);
  // }
  HIHASH_ITERATE it = HIHASH_ITERATE_BEGIN(n->suber);
  while (HIHASH_ITERATE_VALID(n->suber, it)) {
    uint16_t msg_id = HIHASH_GETK(n->suber, it);
    nng_msg_append(nmsg_heartbeat, &msg_id, sizeof msg_id);
    it = HIHASH_ITERATE_NEXT(it);
  }




  nng_sendmsg(n->pub_sock, nmsg_heartbeat, 0);

  // fprintf(stderr, "hnode(%d) -> msg_id(%d) -> %d\n", n->node_id, hms.msg_id, hms.to);
  return 0;
}

static int hnode_connect_broker(hnode_t* n) {
  int rv;
  // 连接到发布端
  if ((rv = nng_dial(n->pub_sock, HBUS_BROKER_PUB_URL, NULL, 0)) != 0) {
    fprintf(stderr, "dial: %s\n", nng_strerror(rv));
    nng_close(n->pub_sock);
    return 1;
  }

  // 连接到发布端
  if ((rv = nng_dial(n->sub_sock, HBUS_BROKER_SUB_URL, NULL, 0)) != 0) {
    fprintf(stderr, "dial: %s\n", nng_strerror(rv));
    nng_close(n->sub_sock);
    return 1;
  }

  rv = hnode_subscribe(n, HBUS_NODE_STATUS, hnode_on_status_req, n);
  return rv;
}

static void hnode_on_subscrib_recv_cb(void* arg) {
  // fprintf(stderr, "on_subscrib_recv_cb in\n");
  hnode_t* node = (hnode_t*)arg;
  int rv = nng_aio_result(node->sub_aio);
  if (rv == NNG_ECLOSED) return;
  if (rv != 0) {
    fprintf(stderr, "hnode(%d) sub cb error: %s\n", node->node_id, nng_strerror(rv));
    return;
  }
  nng_msg* nmsg = nng_aio_get_msg(node->sub_aio);
  size_t l = nng_msg_len(nmsg);
  // fprintf(stdout,"[%zu] <- nng_msg\n", l);

  if (sizeof(hmsg_t) > l) {
    fprintf(stderr, "hnode(%d) sub len error: %zu\n", node->node_id, l);
    nng_msg_free(nmsg);
    nng_recv_aio(node->sub_sock, node->sub_aio);
    return;
  }

  hmsg_t* hm = (hmsg_t*)nng_msg_body(nmsg);
  // fprintf(stdout,"<- msg_id(%d) %d->%d\n",hm->msg_id,hm->from,hm->to);

  // TODO:check crc

  // boardcast || own
  if (0 == hm->to || node->node_id == hm->to) {

    // hbus_suber_t suber = HIHASH_GET(node->suber, hm->msg_id);
    HIHASH_ITERATE it = HIHASH_FIND(node->suber, hm->msg_id);
    // auto it = node->suber.find(hm->msg_id);
    if (HIHASH_ITERATE_VALID(node->suber, it)) {
      // TODO:Thread pool
      hbus_suber_t suber = HIHASH_GETV(node->suber, it);
      suber.h(hm, suber.param);
    } else {
      fprintf(stderr,
              "hnode(%d)msgid error: msg_id(%d),from %d to %d\n",
              node->node_id, hm->msg_id, hm->from, hm->to);
    }
  } else {
    fprintf(stderr, "hnode(%d)to error: msg_id(%d),from %d to %d\n",
            node->node_id, hm->msg_id, hm->from, hm->to);
  }
  nng_recv_aio(node->sub_sock, node->sub_aio);
}


static int hnode_on_request(const hmsg_t* hm, void* p) {
  // hnode_print_reqer((hnode_t*)p);
  // muti-thread
  hnode_t* n = (hnode_t*)p;
  // auto it = n->reqer.find(hm->seq);
  // uintptr_t ctx_ptr = HIHASH_GET(n->reqer, hm->seq);
  HIHASH_ITERATE it = HIHASH_FIND(n->reqer, hm->seq);
  if (HIHASH_ITERATE_VALID(n->reqer, it)) {
    uintptr_t ctx_ptr = HIHASH_GETV(n->reqer, it);
    hnode_req_ctx_t* ctx = (hnode_req_ctx_t*)ctx_ptr;
    // hnode_req_ctx_t* ctx = it->second;
    // fprintf(stdout, "replay: msg_id(%d) %d->%d:%u\n", hm->msg_id, hm->from,
    //         hm->to, hm->payload_size);
    hbuf_push(ctx->replay, hm->payload, hm->payload_size);
    ctx->status = 2;
    // if (ctx->replay >= hm->payload_size) {
    //   memcpy(ctx->respond, hm->payload, hm->payload_size);
    //   ctx->status = 2;
    // } else {
    //   // TODO
    // }
    nng_cv_wake1(ctx->cv);
    // ctx->cv.notify_one();
  }
  // fprintf(stdout, "on_rep out: msg_id(%d) %d->%d\n", hm->msg_id, hm->from,
  //         hm->to);
  return 0;
}

static void hnode_req_ctx_init(hnode_req_ctx_t* ctx,uint16_t msg_id_replay,uint32_t seq) {
  ctx->seq = seq;
  ctx->status = 1;
  ctx->msg_id_replay = msg_id_replay;
  // hbuf_init(&ctx->replay, 0);
  nng_mtx_alloc(&ctx->mtx);
  nng_cv_alloc(&ctx->cv, ctx->mtx);
  // return ctx;
}
static int hnode_req_ctx_fini(hnode_req_ctx_t* ctx){
  // hbuf_deinit(&ctx->replay);
  nng_mtx_free(ctx->mtx);
  nng_cv_free(ctx->cv);
  return 0;
}

int hnode_init(hnode_t* n, int node_id) {
  printf("hnode ctor\n");
  // init param
  n->node_id = node_id;
  hatomic_store(&n->req_seq, 0);
  nng_aio_alloc(&n->sub_aio, hnode_on_subscrib_recv_cb, n);
  HIHASH_INIT(n->suber);
  HIHASH_INIT(n->reqer);
  // init sock
  int rv;
  if ((rv = nng_sub0_open(&n->sub_sock)) != 0) {
    fprintf(stderr, "sub open: %s\n", nng_strerror(rv));
  }
  if ((rv = nng_pub0_open(&n->pub_sock)) != 0) {
    fprintf(stderr, "pub open: %s\n", nng_strerror(rv));
  }

  // start recv
  
  return hnode_connect_broker(n);
}
int hnode_fini(hnode_t* n) {
  printf("hnode dtor\n");
  HIHASH_FINI(n->suber);
  HIHASH_FINI(n->reqer);
  nng_close(n->pub_sock);
  nng_close(n->sub_sock);
  return 0;
}

int hnode_publish(hnode_t* n, uint16_t topic_id, const void* d, uint32_t s,
                         uint16_t target_node_id, uint32_t seq) {
  // sizeof(hmsg_t)
  nng_msg* msg;
  nng_msg_alloc(&msg, 0);
  hmsg_t hm = {.magic = HBUS_MSG_MAGIC,
                     .msg_id = topic_id,
                     .to = target_node_id,
                     .align = 0,
                     .from = n->node_id,
                     .seq = seq,
                     .payload_size = s};
  nng_msg_append(msg, &hm, sizeof hm);
  nng_msg_append(msg, d, s);
  int rv = nng_sendmsg(n->pub_sock, msg, 0);
  if (rv) {
    nng_msg_free(msg);
  }
  return rv;
}
int hnode_subscribe(hnode_t* n, uint16_t topic_id, hbus_subscrib_handler_t h, void* param) {
  // multi thread
  if (!HIHASH_SIZE(n->suber)) {
    // lazy subscrib:start <- when first subscrib
    nng_recv_aio(n->sub_sock, n->sub_aio);
    fprintf(stdout, "node_id(%d),start recv\n", n->node_id);
  }

  int rv;
  hmsg_t msg = {.magic = HBUS_MSG_MAGIC,
                .msg_id = topic_id,
                .to = 0,
                .version = 0,
                .align = 0};
  if (rv = nng_socket_set(n->sub_sock, NNG_OPT_SUB_SUBSCRIBE, &msg,
                          HBUS_MSG_PUBSUB_HEAD_SIZE)) {
    fprintf(stderr, "set subscribe: %s\n", nng_strerror(rv));
    nng_close(n->sub_sock);
    return rv;
  }
  msg.to = n->node_id;
  if (rv = nng_socket_set(n->sub_sock, NNG_OPT_SUB_SUBSCRIBE, &msg,
                          HBUS_MSG_PUBSUB_HEAD_SIZE)) {
    fprintf(stderr, "set subscribe: %s\n", nng_strerror(rv));
    nng_close(n->sub_sock);
    return rv;
  }

  hbus_suber_t suber = {param, h};
  HIHASH_SET(n->suber, topic_id, suber);
  // n->suber[topic_id] = {param, h};
  fprintf(stdout, "node_id(%d) subscrib topic_id(%d)\n", n->node_id, topic_id);
  return rv;
}

int hnode_request(hnode_t* n, uint16_t msg_id,uint16_t target_node_id,  hbuf_t* req,
   hbuf_t* res,int timeout) {
  // check if subscrib replay
  uint16_t msg_id_replay = HBUS_MSG_REPLY(msg_id);
  // auto it = suber.find(msg_id_replay);
  HIHASH_ITERATE it = HIHASH_FIND(n->suber, msg_id_replay);
  if (!HIHASH_ITERATE_VALID(n->suber, it)) {
    hnode_subscribe(n, msg_id_replay, hnode_on_request, n);
  }
  // hbus_suber_t* suber = HIHASH_GET(n->suber, msg_id_replay);
  // if (!suber) {
  //   hnode_subscrib(n, msg_id_replay, hnode_on_request, n);
  // }

  // init ctx,TODO: req ctx pool
  hnode_req_ctx_t ctx;
  hnode_req_ctx_init(&ctx,msg_id_replay, hatomic_inc(&n->req_seq, 1));
  ctx.replay = res;
  HIHASH_SET(n->reqer, ctx.seq, (uintptr_t)&ctx);

  // hnode_print_reqer(n);
  // publish request
  hnode_publish(n, msg_id, req->data, req->len, target_node_id, ctx.seq);
  // fprintf(stdout, "request(%u): msg_id:%d,req_size:%zu,target_node_id:%d\n",
  //         ctx.seq, msg_id, req->len, target_node_id);

  // wait
  // std::unique_lock<std::mutex> l(ctx->mtx);
  // bool ret = ctx->cv.wait_for(l, std::chrono::milliseconds(timeout),
  //                             [&] { return 2 == ctx->status; });
  
  nng_mtx_lock(ctx.mtx);
  nng_time expire = nng_clock() + timeout;
  int ret = 0;
  while (2 != ctx.status) {
    ret = nng_cv_until(ctx.cv, expire);
    if (ret == NNG_ETIMEDOUT) {
      break;
    }
  }
  HIHASH_DEL(n->reqer, ctx.seq);
  nng_mtx_unlock(ctx.mtx);

  // fprintf(stdout, "request ret:%d\n", ret);
  // release resource
  // reqer.erase(ctx.seq);
  // ctx->status = 0;
  // delete ctx;
  
  hnode_req_ctx_fini(&ctx);

  int r = ret ? 0 : 1;
  return r;
}

// }  // namespace hbus