#ifndef _HMSG_H_
#define _HMSG_H_
#include <stdint.h>

#define HMSG_MAGIC 0x55AA
#define HMSG_APPID_BROKER 0xFFFE
#define HMSG_APPID_EVERYONR 0xFFFF
// MSG CMD
#define HMSG_FCMD_REGISTER 0x00
#define HMSG_FCMD_DATA 0x10
#define HMSG_FCMD_SUB 0x20
#define HMSG_FCMD_UNSUB 0x30
// MSG DATA
#define HMSG_FDATA_PUBSUB 0x00
#define HMSG_FDATA_REQUEST 0x01
#define HMSG_FDATA_REPOND 0x02

//every msg's replay define by macro HMSG_REPLY
//so mgs_id must be <= INT16_MAX
// #define HMSG_REPLY(MSGID) (UINT16_MAX-MSGID)
#define HMSG_MSGID_STATUS 0x0000




typedef struct hmsg_ {
  uint16_t magic;// 55aa
  uint16_t crc_head;
  uint8_t version;
  uint8_t flags;
  uint16_t topic;
  
  uint16_t from;
  uint16_t to;//0xFFFF means everyone
  uint32_t seq;

  uint64_t timestamp_us;

  uint32_t crc_payload;
  uint32_t payload_size;
  char payload[];
} hmsg_t;


#define HMSG_FLAGS(FCMD,FDATA) ((FCMD)|(FDATA))
#define HMSG_FLAGS_CMD(FLAGS) ((FLAGS)&0xF0)
#define HMSG_FLAGS_DATA(FLAGS) ((FLAGS)&0x0F)

#define HMSG_TOPICEX(VER,FLAGS,TPC) ((VER<<24)|(FLAGS<<16)|(TPC))
#define HMSG_TOPICEX_INMSG(msg) HMSG_TOPICEX(msg->version,msg->flags,msg->topic)



#endif