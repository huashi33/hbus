#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "hcore/hds.h"
#include "hbroker.h"
#include "hcommon.h"
#include "hmsg.h"
#include "nng/nng.h"
#include "nng/protocol/pubsub0/pub.h"
#include "nng/protocol/pubsub0/sub.h"
#include "nng/supplemental/util/platform.h"

#define HBUS_BROKER_SUB_URL "tcp://127.0.0.1:5555"
#define HBUS_BROKER_PUB_URL "tcp://127.0.0.1:5556"

// #define HBUS_BROKER_SUB_URL "ipc:///tmp/sub"
// #define HBUS_BROKER_PUB_URL "ipc:///tmp/pub"


HIHASH_DEFINE(uint16_t, nodeinfo_t)

typedef struct hbusbroker_ {
  nng_socket pub_sock;
  nng_aio* pub_aio;

  nng_socket sub_sock;
  nng_aio* sub_aio;
  HIHASH_TYPEDEF(uint16_t, nodeinfo_t) *nodes;
} hbusbroker_t;

static void hb_print_nodeinfo(nodeinfo_t* n) {
  char node_status[256];
  snprintf(node_status, sizeof(node_status) - 1, "%d:wd %u\n", n->node_id,n->watchdog);
  node_status[sizeof(node_status) - 1] = 0;
  fprintf(stdout, "%s", node_status);
}
static void hb_process_sys(hbusbroker_t* b, hmsg_t* msg) {
  if (HBUS_MSG_REPLY(HBUS_NODE_STATUS) == msg->msg_id) {
    HIHASH_ITERATE it = HIHASH_FIND(b->nodes, msg->from);
    if (!HIHASH_ITERATE_VALID(b->nodes, it)) {
      // init nodeinfo
      nodeinfo_t n;
      n.node_id = msg->from;
      n.watchdog = NODE_WATCHDOG_INIT;
      HIHASH_SET(b->nodes, n.node_id, n);
      fprintf(stdout, "node add:%d\n", msg->from);
    }
    else{
      nodeinfo_t *n = HIHASH_GETVPTR(b->nodes, it);
      n->watchdog = NODE_WATCHDOG_INIT;
      fprintf(stdout, "node watchdog updated:%d\n", msg->from);
    }
  }
}
static void hb_sub_cb(void* arg) {
  hbusbroker_t* b = (hbusbroker_t*)arg;

  int rv = nng_aio_result(b->sub_aio);
  if (rv == NNG_ECLOSED) return;
  if (rv != 0) {
    fprintf(stderr, "hbroker sub cb error: %s\n", nng_strerror(rv));
    return;
  }

  nng_msg* msg = nng_aio_get_msg(b->sub_aio);
  size_t l = nng_msg_len(msg);
  // fprintf(stdout,"[%zu] <- nng_msg\n", l);

  if(sizeof (hmsg_t) > l){
    fprintf(stderr, "hbroker sub len error: %zu\n", l);
    nng_msg_free(msg);
    nng_recv_aio(b->sub_sock, b->sub_aio);
    return;
  }
  hmsg_t* hm = (hmsg_t*)nng_msg_body(msg);
  // fprintf(stdout,"hbroker <- msg_id(%d) %d->%d\n",hm->msg_id,hm->from,hm->to);
  if (HBUS_APPID_BROKER == hm->to) {
    hb_process_sys(b, hm);
    nng_msg_free(msg);
    nng_recv_aio(b->sub_sock, b->sub_aio);
    return;
  }


  nng_aio_set_msg(b->pub_aio, msg);
  nng_send_aio(b->pub_sock, b->pub_aio);
  // 继续异步接收
  nng_recv_aio(b->sub_sock, b->sub_aio);
}
static void hb_pub_cb(void* arg) {
  hbusbroker_t* b = (hbusbroker_t*)arg;
  // nng_msg* msg = nng_aio_get_msg(b->pub_aio);
  // hbus::hmsg_t* hm = (hbus::hmsg_t*)nng_msg_body(msg);
  // nng_msg_free(msg);

  int rv = nng_aio_result(b->pub_aio);
  if (rv == NNG_ECLOSED) return;
  if (rv != 0) {
    fprintf(stderr, "hbroker pub cb error: %s\n", nng_strerror(rv));
    return;
  }

  static uint32_t count = 0;
  ++count;
  
  // fprintf(stdout, "pub-count:%u\n", count);
}

static int hb_init(hbusbroker_t* b) {
  b->nodes = NULL;

  int rv;
  if ((rv = nng_sub0_open(&b->sub_sock)) != 0) {
    fprintf(stderr, "sub open: %s\n", nng_strerror(rv));
    return 1;
  }

  // 订阅所有MSG_ID
  uint16_t magic = HBUS_MSG_MAGIC;
  if ((rv = nng_socket_set(b->sub_sock, NNG_OPT_SUB_SUBSCRIBE, &magic, sizeof magic)) != 0) {
    fprintf(stderr, "set subscribe: %s\n", nng_strerror(rv));
    nng_close(b->sub_sock);
    return 1;
  }

  if ((rv = nng_listen(b->sub_sock, HBUS_BROKER_SUB_URL, NULL, 0)) != 0) {
    fprintf(stderr, "listen: %s\n", nng_strerror(rv));
    nng_close(b->sub_sock);
    return 1;
  }

  nng_aio_alloc(&b->sub_aio, hb_sub_cb, b);

  if ((rv = nng_pub0_open(&b->pub_sock)) != 0) {
    fprintf(stderr, "pub open: %s\n", nng_strerror(rv));
    return 1;
  }
  nng_aio_alloc(&b->pub_aio, hb_pub_cb, b);

  if ((rv = nng_listen(b->pub_sock, HBUS_BROKER_PUB_URL, NULL, 0)) != 0) {
    fprintf(stderr, "listen: %s\n", nng_strerror(rv));
    nng_close(b->pub_sock);
    return 1;
  }

  nng_recv_aio(b->sub_sock, b->sub_aio);
  return 0;
}



static void hb_process_nodes(hbusbroker_t* b) {
  // static uint64_t time_last = hbus::hcommon::clock_now_ns();
  // uint64_t time_now = hbus::hcommon::clock_now_ns();
  // if (1 * 1e6 > time_now - time_last) {
  //   return;
  // } 
  // time_last = time_now;
  if(0 == HIHASH_SIZE(b->nodes)){
    return;
  }

  hbuf_t buf_torm;
  hbuf_init(&buf_torm, 0);
  HIHASH_ITERATE it = HIHASH_ITERATE_BEGIN(b->nodes);
  while (HIHASH_ITERATE_VALID(b->nodes, it)) {
    nodeinfo_t* ni = HIHASH_GETVPTR(b->nodes, it);
    hb_print_nodeinfo(ni);
    if (!ni->watchdog) {
      hbuf_push(&buf_torm, &ni->node_id, sizeof ni->node_id);
    } else {
      --ni->watchdog;
    }
    it = HIHASH_ITERATE_NEXT(it);
  }
  size_t count = buf_torm.len / sizeof(uint16_t);
  uint16_t* p = (uint16_t*)buf_torm.data;
  for (size_t i = 0; i < count; i++){
    uint16_t node_id = p[i];
    HIHASH_DEL(b->nodes, node_id);
    fprintf(stderr, "node remove:%d\n", node_id);
  }
  hbuf_deinit(&buf_torm);

}

// request node's status 
static void hb_send_status_req(hbusbroker_t* b){
  nng_msg* nmsg_heartbeat = NULL;
  if(!nmsg_heartbeat){
    nng_msg_alloc(&nmsg_heartbeat, 0);
    hmsg_t hm = {.magic = HBUS_MSG_MAGIC,
                .msg_id = HBUS_NODE_STATUS,
                .to = 0,//all
                .align = 0,
                .from = HBUS_APPID_BROKER,
                .payload_size = 0};
    nng_msg_append(nmsg_heartbeat, &hm, sizeof hm);
  }

  nng_sendmsg(b->pub_sock, nmsg_heartbeat, 0);
}
int main(int argc, char* argv[]) {
  int rv;
  hbusbroker_t b;
  rv = hb_init(&b);

  for (;;) {
    hb_process_nodes(&b);
    hb_send_status_req(&b);
    nng_msleep(5000);

  }
  return rv;
}