#ifndef _HMSG_H_
#define _HMSG_H_
#include <stdint.h>

#define HBUS_MSG_MAGIC 0x55AA

#define HBUS_APPID_BROKER 0xFFFF

// MSGTYPE
#define HBUS_MSGTYPE_SYS 0
#define HBUS_MSGTYPE_PUBLISH 1
#define HBUS_MSGTYPE_REQUEST 2
#define HBUS_MSGTYPE_RESPONSE 3
// CONTENT ID WHEN msg_type == HBUS_MSGTYPE_SYS
#define HBUS_SYS_HEARTBEAT 0
// CONTENT ID WHEN msg_type == HBUS_MSGTYPE_REQUEST
#define HBUS_SYS_STATUS 0


namespace hbus {

// when HBUS_MSGTYPE_PUBLISH==type,content_id is topic_id
// when HBUS_MSGTYPE_REQUEST==type,content_id is request_id
// when HBUS_MSGTYPE_RESPONSE==type,content_id is request_id
typedef struct hmsg_ {
  uint16_t magic;// 55aa
  uint16_t content_id;
  uint8_t version;
  uint8_t msg_type;
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

#endif