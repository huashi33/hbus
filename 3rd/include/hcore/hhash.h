#ifndef _HHASH_H_
#define _HHASH_H_
#include "hdef.h"
#include "stb_ds.h"
// HASH
#define HC_RET_BASE_HASH HC_RET_BASE_DS


// define
#define HHASH_ENTRY(TK, TV)      \
  typedef struct hhash_##TV##_ { \
    TK key;                      \
    TV value;                    \
  } HHASH_TYPEDEF(TK, TV);
#define HHASH_DEFINE(TK, TV) HHASH_ENTRY(TK, TV)
// iterater
#define HHASH_TYPEDEF(TK, TV) hhash_##TV
#define HHASH_ITERATE ptrdiff_t
#define HHASH_ITERATE_VALID(h, IT) (0 <= (IT) && (IT) < hmlen(h))
#define HHASH_ITERATE_BEGIN(h) 0
#define HHASH_ITERATE_NEXT(IT) ((IT) + 1)
// get
#define HHASH_GETV(h, it) (h[it].value)
#define HHASH_GETVPTR(h, it) (&h[it].value)
#define HHASH_GETK(h, it) (h[it].key)
#define HHASH_GETKPTR(h, it) (&h[it].key)
// char*
#define HSHASH_DEFINE(TK, TV) HHASH_DEFINE(TK, TV)
#define HSHASH_TYPEDEF(TK, TV) HHASH_TYPEDEF(TK, TV)
#define HSHASH_INIT(h) sh_new_strdup(h)
#define HSHASH_FINI(h) shfree(h)
#define HSHASH_SET(h, k, v) shput(h, k, v)
#define HSHASH_DEL(h, k) shdel(h, k)
#define HSHASH_SIZE(h) shlen(h)
#define HSHASH_EXIST(h, k) (shgeti(h, k)) >= 0 ? HC_RET_OK : HC_RET_NOTFOUND
#define HSHASH_FIND(h, k) shgeti(h, k)
#define HSHASH_GETV(h, it) HHASH_GETV(h, it)
#define HSHASH_GETVPTR(h, it) HHASH_GETVPTR(h, it)
#define HSHASH_GETK(h, it) HHASH_GETK(h, it)
#define HSHASH_GETKPTR(h, it) HHASH_GETKPTR(h, it)
#define HSHASH_ITERATE HHASH_ITERATE
#define HSHASH_ITERATE_VALID(h, IT) HHASH_ITERATE_VALID(h, IT)
#define HSHASH_ITERATE_BEGIN(h) HHASH_ITERATE_BEGIN(h)
#define HSHASH_ITERATE_NEXT(IT) HHASH_ITERATE_NEXT(IT)
// int
#define HIHASH_DEFINE(TK, TV) HHASH_DEFINE(TK, TV)
#define HIHASH_TYPEDEF(TK, TV) HHASH_TYPEDEF(TK, TV)
#define HIHASH_INIT(h) h=NULL
#define HIHASH_FINI(h) hmfree(h)
#define HIHASH_SET(h, k, v) hmput(h, k, v)
#define HIHASH_DEL(h, k) hmdel(h, k)
#define HIHASH_SIZE(h) (h ? hmlen(h) : 0)
#define HIHASH_EXIST(h, k) (hmgeti(h, k)) >= 0 ? HC_RET_OK : HC_RET_NOTFOUND
#define HIHASH_FIND(h, k) hmgeti(h, k)
#define HIHASH_GETV(h, it) HHASH_GETV(h, it)
#define HIHASH_GETVPTR(h, it) HHASH_GETVPTR(h, it)
#define HIHASH_GETK(h, it) HHASH_GETK(h, it)
#define HIHASH_GETKPTR(h, it) HHASH_GETKPTR(h, it)
#define HIHASH_ITERATE HHASH_ITERATE
#define HIHASH_ITERATE_VALID(h, IT) HHASH_ITERATE_VALID(h, IT)
#define HIHASH_ITERATE_BEGIN(h) HHASH_ITERATE_BEGIN(h)
#define HIHASH_ITERATE_NEXT(IT) HHASH_ITERATE_NEXT(IT)
#endif
