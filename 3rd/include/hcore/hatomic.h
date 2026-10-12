#ifndef __HATOMIC_H__
#define __HATOMIC_H__
#include <stdint.h>

#define hatomic_int32_t int32_t
#define hatomic_uint32_t uint32_t
#define hatomic_int64_t int64_t
#define hatomic_uint64_t uint64_t

#define hatomic_inc(v,n) __atomic_fetch_add(v, n, __ATOMIC_RELAXED)
#define hatomic_dec(v,n) __atomic_fetch_sub(v, n, __ATOMIC_RELAXED)
#define hatomic_load(v) __atomic_load_n(v, __ATOMIC_ACQUIRE)
#define hatomic_store(v, x) __atomic_store_n(v, x, __ATOMIC_RELEASE)




#endif // __HATOMIC_H__