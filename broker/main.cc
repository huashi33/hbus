#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <unordered_map>

#include "hbtype.h"
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

typedef struct TStatics_ {
  int count;
  int aaaa;
  double a1;
  double a2;
  double a3;
  char algn[256 - 32];
} TStatics_t;

typedef struct hbusbroker_ {
  nng_socket pub_sock;
  nng_aio* pub_aio;

  nng_socket sub_sock;
  nng_aio* sub_aio;
  std::unordered_map<uint16_t, nodeinfo_t> nodes;
} hbusbroker_t;

static void hb_print_nodeinfo(hbusbroker_t* b, std::string& ret) {
  char node_status[256];
  for (auto& n : b->nodes) {
    nodeinfo_t& ni = n.second;
    snprintf(node_status, sizeof(node_status) - 1, "%d:hb %u\n", ni.node_id,
             ni.watchdog);
    ret.append(node_status);
  }
}
static void hb_process_msg(hbusbroker_t* b, hbus::hmsg_t* msg);
static void hb_sub_cb(void* arg) {
  hbusbroker_t* b = (hbusbroker_t*)arg;

  int rv = nng_aio_result(b->sub_aio);
  if (rv == NNG_ECLOSED) return;
  if (rv != 0) {
    fprintf(stderr, "[sub] recv error: %s\n", nng_strerror(rv));
    return;
  }

  nng_msg* msg = nng_aio_get_msg(b->sub_aio);
  size_t l = nng_msg_len(msg);
  // fprintf(stdout,"[%zu] recv nng_msg\n", l);

  if(sizeof (hbus::hmsg_t) > l){
    fprintf(stderr, "[sub] recv len error: %zu\n", l);
    nng_msg_free(msg);
    nng_recv_aio(b->sub_sock, b->sub_aio);
    return;
  }
  hbus::hmsg_t* m = (hbus::hmsg_t*)nng_msg_body(msg);
  // fprintf(stdout,"recv from %d:%u\n",m->from,m->payload_size);
  if (HBUS_APPID_BROKER == m->to) {
    hb_process_msg(b, m);
    nng_msg_free(msg);
    nng_recv_aio(b->sub_sock, b->sub_aio);
    return;
  }

  TStatics_t* td = (TStatics_t*)m->payload;
  td->a2 = hbus::hcommon::clock_now_us();
  // if (nng_aio_busy(b->pub_aio)) {
  //     fprintf(stderr, "pub aio busy\n");
  // }
  nng_aio_set_msg(b->pub_aio, msg);
  nng_send_aio(b->pub_sock, b->pub_aio);
  // 继续异步接收
  nng_recv_aio(b->sub_sock, b->sub_aio);
}
static void hb_pub_cb(void* arg) {
  hbusbroker_t* b = (hbusbroker_t*)arg;
  // nng_msg* msg = nng_aio_get_msg(b->pub_aio);
  // nng_msg_free(msg);

  int rv = nng_aio_result(b->pub_aio);
  if (rv == NNG_ECLOSED) return;
  if (rv != 0) {
    fprintf(stderr, "[sub] recv error: %s\n", nng_strerror(rv));
    return;
  }

  static uint32_t count = 0;
  ++count;
  // fprintf(stdout, "pub-count:%u\n", count);
}

static int hb_init(hbusbroker_t* b) {
  int rv;
  if ((rv = nng_sub0_open(&b->sub_sock)) != 0) {
    fprintf(stderr, "sub open: %s\n", nng_strerror(rv));
    return 1;
  }

  // 订阅所有主题（空前缀）
  uint16_t magic = HBUS_MSG_MAGIC;
  if ((rv = nng_setopt(b->sub_sock, NNG_OPT_SUB_SUBSCRIBE, &magic, sizeof magic)) != 0) {
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
static void hb_process_sys(hbusbroker_t* b, hbus::hmsg_t* msg) {
  if (HBUS_SYS_HEARTBEAT == msg->content_id) {
    auto t = b->nodes.find(msg->from);
    if (b->nodes.end() == t) {
      // init nodeinfo
      nodeinfo_t n;
      n.node_id = msg->from;
      n.watchdog = NODE_WATCHDOG_INIT;
      auto i = b->nodes.emplace(n.node_id, n);
      t = i.first;
      fprintf(stdout, "node add:%d\n", msg->from);
    }
    t->second.watchdog = NODE_WATCHDOG_INIT;
  }
}
static void hb_process_request(hbusbroker_t* b, hbus::hmsg_t* msg) {
  if (HBUS_SYS_STATUS == msg->content_id) {
    std::string ret;
    hb_print_nodeinfo(b, ret);
    // send back
    fprintf(stderr, " hb_process_request\n%s", ret.c_str());
  }
}
static void hb_process_msg(hbusbroker_t* b, hbus::hmsg_t* msg) {
  if (HBUS_MSGTYPE_SYS == msg->msg_type) {
    hb_process_sys(b, msg);
  } else if (HBUS_MSGTYPE_REQUEST == msg->msg_type) {
    hb_process_request(b, msg);
  }
}
static void hb_process_nodes(hbusbroker_t* b) {
  static uint64_t time_last = hbus::hcommon::clock_now_us();
  uint64_t time_now = hbus::hcommon::clock_now_us();
  if (1 * 1e6 > time_now - time_last) {
    return;
  }
  time_last = time_now;
  for (auto& n : b->nodes) {
    if (!n.second.watchdog) {
      std::string s;
      hb_print_nodeinfo(b, s);
      // fprintf(stderr,"%d watchdog down\n",n.first);
      fprintf(stderr, "%s", s.c_str());
      continue;
    }
    --n.second.watchdog;
  }
}
static int hb_exec(hbusbroker_t* b) {
  for (;;) {
    int rv;
    char* buf = NULL;
    size_t sz;
    if (0 == (rv = nng_recv(b->sub_sock, &buf, &sz, NNG_FLAG_ALLOC))) {
      hbus::hmsg_t* msg = (hbus::hmsg_t*)buf;
      if (HBUS_APPID_BROKER == msg->to) {
        hb_process_msg(b, msg);
        continue;
      }

      TStatics_t* td = (TStatics_t*)msg->payload;
      td->a2 = hbus::hcommon::clock_now_us();
      rv = nng_send(b->pub_sock, buf, sz, 0);
    }

    // fprintf(stderr, "recv: %s\n", nng_strerror(rv));
    nng_free(buf, sz);
    hb_process_nodes(b);
    usleep(1000 * 1);
  }
  return 0;
}

int main(int argc, char* argv[]) {
  int rv;
  hbusbroker_t b;
  rv = hb_init(&b);
  // rv = hb_exec(&b);

  for (;;) {
    hb_process_nodes(&b);
    nng_msleep(1000);
  }
  return rv;
}