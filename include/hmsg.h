#ifndef _HMSG_H_
#define _HMSG_H_
#include <stdint.h>

#define HBUS_MSG_MAGIC 0x55AA

#define HBUS_APPID_BROKER 0xFFFF

//every msg's replay define by macro HBUS_MSG_REPLY
//so mgs_id must be <= INT16_MAX
#define HBUS_MSG_REPLY(MSGID) (UINT16_MAX-MSGID)
#define HBUS_NODE_STATUS 0x0000



namespace hbus {

typedef struct hmsg_ {
  uint16_t magic;// 55aa
  uint16_t msg_id;
  uint8_t version;
  uint8_t align;
  uint16_t crc_head;

  uint16_t from;
  uint16_t to;
  uint32_t seq;

  uint64_t timestamp_us;

  uint32_t crc_payload;
  uint32_t payload_size;
  char payload[];
} hmsg_t;



}  // namespace hbus

// pub/sub
#define HBUS_MSG_PUBSUB_HEAD_SIZE (sizeof(hmsg_t::magic) + sizeof(hmsg_t::msg_id))



#endif