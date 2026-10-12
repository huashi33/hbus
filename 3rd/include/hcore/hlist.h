#ifndef _HLIST_H_
#define _HLIST_H_
#include "hdef.h"

// LIST
#define HC_RET_BASE_LIST HC_RET_BASE_DS
typedef struct hlistitem_ {
  struct hlistitem_* next;
} hlistitem_t;
typedef struct hlist_ {
  hlistitem_t* head;
  size_t size;
} hlist_t;
typedef int (*hlist_compare_t)(hlistitem_t* i, hlistitem_t* target);
typedef int (*hlist_foreach_t)(hlistitem_t* i, int index);

// without mem free
HFC_EXPORT int hlist_clear(hlist_t*);
HFC_EXPORT int hlist_append(hlist_t* l, hlistitem_t* i);
HFC_EXPORT int hlist_insert(hlist_t* l, hlistitem_t* i, uint32_t index);
HFC_EXPORT hlistitem_t* hlist_at(hlist_t* l, uint32_t index);
HFC_EXPORT hlistitem_t* hlist_remove(hlist_t* l, uint32_t index);
// need set compare-func
HFC_EXPORT hlistitem_t* hlist_remove_first(hlist_t* l, hlistitem_t* tar,
                                           hlist_compare_t cmp);
HFC_EXPORT hlistitem_t* hlist_find_first(hlist_t* l, hlistitem_t* tar,
                                         hlist_compare_t cmp);
// need set foreach-func
HFC_EXPORT int hislist_foreach(hlist_t* l, hlist_foreach_t f);


#endif