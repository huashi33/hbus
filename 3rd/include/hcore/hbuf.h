#ifndef _HBUF_H_
#define _HBUF_H_
#include "hdef.h"


// BUF
#define HC_RET_BUF_BASE HC_RET_BASE_DS



typedef struct hbuf_ {
  size_t len;
  size_t size;
  uint8_t* data;
} hbuf_t;

HFC_EXPORT int hbuf_init(hbuf_t* b, size_t s);
HFC_EXPORT int hbuf_fini(hbuf_t* b);
HFC_EXPORT int hbuf_push(hbuf_t* b, const void* d, size_t dSize);
HFC_EXPORT int hbuf_remove(hbuf_t* b, size_t index, size_t dSize);
HFC_EXPORT int hbuf_clear(hbuf_t* b);
HFC_EXPORT int hbuf_grow(hbuf_t* b, size_t wish_size);

#define HBUF_ITEM_COUNT(BUF,TYPE) ((BUF)->len/sizeof(TYPE))
#define HBUF_ITEM_AT(BUF,TYPE,INDEX) (((TYPE*)(BUF)->data) + INDEX)
#define HBUF_ITEM_SET(BUF,TYPE,V,INDEX)  ((TYPE*)((BUF)->data))[INDEX] = (V)
//notice: out of range
#define HBUF_PTR_COUNT(BUF) HBUF_ITEM_COUNT(BUF,intptr_t)
#define HBUF_PTR_AT(BUF,TYPE,INDEX) (TYPE*)(*HBUF_ITEM_AT(BUF,intptr_t,INDEX))
#define HBUF_PTR_SET(BUF,PTR,INDEX)  HBUF_ITEM_SET(BUF,intptr_t,(intptr_t)(PTR),INDEX)
#define HBUF_PTR_PUSH(BUF,PTR) hbuf_push(BUF,&(PTR),sizeof(intptr_t))



// HFC_EXPORT int hbuf_print(hbuf_t* b, size_t ofs, size_t size);




#endif