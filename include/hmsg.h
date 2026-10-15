#ifndef _HMSG_H_
#define _HMSG_H_
#include <stdint.h>

#define HMSG_MAGIC 0x55AA
#define HMSG_APPID_BROKER 0xFFFE
#define HMSG_APPID_EVERYONR 0xFFFF
#define HMSG_FLAG_REGISTER 0x00
#define HMSG_FLAG_DATA 0x01
#define HMSG_FLAG_SUB 0x02
#define HMSG_FLAG_UNSUB 0x03
#define HMSG_FLAG_REQUEST 0x04
#define HMSG_FLAG_RESPOND 0x05

//every msg's replay define by macro HMSG_REPLY
//so mgs_id must be <= INT16_MAX
// #define HMSG_REPLY(MSGID) (UINT16_MAX-MSGID)
#define HMSG_MSGID_CONNECT 0x0000




typedef struct hmsg_ {
  uint16_t magic;// 55aa
  uint16_t topic;
  uint16_t to;//0xFFFF means everyone
  uint8_t version;
  uint8_t flags;
  
  uint16_t crc_head;
  uint16_t from;
  uint32_t seq;

  uint64_t timestamp_us;

  uint32_t crc_payload;
  uint32_t payload_size;
  char payload[];
} hmsg_t;







#endif