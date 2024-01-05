// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_ops.h"

#include <stdatomic.h>


#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-non-const-parameter"


inline int
CereggiiAtomic_AddInt(int *obj, int value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline int8_t
CereggiiAtomic_AddInt8(int8_t *obj, int8_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline int16_t
CereggiiAtomic_AddInt16(int16_t *obj, int16_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline int32_t
CereggiiAtomic_AddInt32(int32_t *obj, int32_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline int64_t
CereggiiAtomic_AddInt64(int64_t *obj, int64_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline intptr_t
CereggiiAtomic_AddIntPtr(intptr_t *obj, intptr_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline unsigned int
CereggiiAtomic_AddUInt(unsigned int *obj, unsigned int value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline uint8_t
CereggiiAtomic_AddUInt8(uint8_t *obj, uint8_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline uint16_t
CereggiiAtomic_AddUInt16(uint16_t *obj, uint16_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline uint32_t
CereggiiAtomic_AddUInt32(uint32_t *obj, uint32_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline uint64_t
CereggiiAtomic_AddUInt64(uint64_t *obj, uint64_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline uintptr_t
CereggiiAtomic_AddUIntPtr(uintptr_t *obj, uintptr_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }

inline Py_ssize_t
CereggiiAtomic_AddSsize(Py_ssize_t *obj, Py_ssize_t value)
{ return atomic_fetch_add_explicit(obj, value, memory_order_acq_rel); }


inline int
CereggiiAtomic_CompareExchangeInt(int *obj, int expected, int desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeInt8(int8_t *obj, int8_t expected, int8_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeInt16(int16_t *obj, int16_t expected, int16_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeInt32(int32_t *obj, int32_t expected, int32_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeInt64(int64_t *obj, int64_t expected, int64_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeInt128(__int128_t *obj, __int128_t expected, __int128_t desired)
{
    return __sync_bool_compare_and_swap_16(obj, expected, desired);
}

inline int
CereggiiAtomic_CompareExchangeIntPtr(intptr_t *obj, intptr_t expected, intptr_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeUInt(unsigned int *obj, unsigned int expected, unsigned int desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeUInt8(uint8_t *obj, uint8_t expected, uint8_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeUInt16(uint16_t *obj, uint16_t expected, uint16_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeUInt32(uint32_t *obj, uint32_t expected, uint32_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeUInt64(uint64_t *obj, uint64_t expected, uint64_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeUInt128(__uint128_t *obj, __uint128_t expected, __uint128_t desired)
{
    return __sync_bool_compare_and_swap_16(obj, expected, desired);
}

inline int
CereggiiAtomic_CompareExchangeUIntPtr(uintptr_t *obj, uintptr_t expected, uintptr_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangeSsize(Py_ssize_t *obj, Py_ssize_t expected, Py_ssize_t desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel, memory_order_acquire);
}

inline int
CereggiiAtomic_CompareExchangePtr(void **obj, void *expected, void *desired)
{
    return atomic_compare_exchange_strong_explicit(obj, &expected, desired, memory_order_acq_rel,
                                                   memory_order_acquire);
}


inline int
CereggiiAtomic_ExchangeInt(int *obj, int value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline int8_t
CereggiiAtomic_ExchangeInt8(int8_t *obj, int8_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline int16_t
CereggiiAtomic_ExchangeInt16(int16_t *obj, int16_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline int32_t
CereggiiAtomic_ExchangeInt32(int32_t *obj, int32_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline int64_t
CereggiiAtomic_ExchangeInt64(int64_t *obj, int64_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline intptr_t
CereggiiAtomic_ExchangeIntPtr(intptr_t *obj, intptr_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline unsigned int
CereggiiAtomic_ExchangeUInt(unsigned int *obj, unsigned int value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline uint8_t
CereggiiAtomic_ExchangeUInt8(uint8_t *obj, uint8_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline uint16_t
CereggiiAtomic_ExchangeUInt16(uint16_t *obj, uint16_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline uint32_t
CereggiiAtomic_ExchangeUInt32(uint32_t *obj, uint32_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline uint64_t
CereggiiAtomic_ExchangeUInt64(uint64_t *obj, uint64_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline uintptr_t
CereggiiAtomic_ExchangeUIntPtr(uintptr_t *obj, uintptr_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline Py_ssize_t
CereggiiAtomic_ExchangeSsize(Py_ssize_t *obj, Py_ssize_t value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }

inline void *
CereggiiAtomic_ExchangePtr(void **obj, void *value)
{ return atomic_exchange_explicit(obj, value, memory_order_acq_rel); }


inline uint8_t
CereggiiAtomic_AndUInt8(uint8_t *obj, uint8_t value)
{ return atomic_fetch_and_explicit(obj, value, memory_order_acq_rel); }

inline uint16_t
CereggiiAtomic_AndUInt16(uint16_t *obj, uint16_t value)
{ return atomic_fetch_and_explicit(obj, value, memory_order_acq_rel); }

inline uint32_t
CereggiiAtomic_AndUInt32(uint32_t *obj, uint32_t value)
{ return atomic_fetch_and_explicit(obj, value, memory_order_acq_rel); }

inline uint64_t
CereggiiAtomic_AndUInt64(uint64_t *obj, uint64_t value)
{ return atomic_fetch_and_explicit(obj, value, memory_order_acq_rel); }

inline uintptr_t
CereggiiAtomic_AndUIntPtr(uintptr_t *obj, uintptr_t value)
{ return atomic_fetch_and_explicit(obj, value, memory_order_acq_rel); }


inline uint8_t
CereggiiAtomic_OrUInt8(uint8_t *obj, uint8_t value)
{ return atomic_fetch_or_explicit(obj, value, memory_order_acq_rel); }

inline uint16_t
CereggiiAtomic_OrUInt16(uint16_t *obj, uint16_t value)
{ return atomic_fetch_or_explicit(obj, value, memory_order_acq_rel); }

inline uint32_t
CereggiiAtomic_OrUInt32(uint32_t *obj, uint32_t value)
{ return atomic_fetch_or_explicit(obj, value, memory_order_acq_rel); }

inline uint64_t
CereggiiAtomic_OrUInt64(uint64_t *obj, uint64_t value)
{ return atomic_fetch_or_explicit(obj, value, memory_order_acq_rel); }

inline uintptr_t
CereggiiAtomic_OrUIntPtr(uintptr_t *obj, uintptr_t value)
{ return atomic_fetch_or_explicit(obj, value, memory_order_acq_rel); }


inline int
CereggiiAtomic_LoadInt(const int *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline int8_t
CereggiiAtomic_LoadInt8(const int8_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline int16_t
CereggiiAtomic_LoadInt16(const int16_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline int32_t
CereggiiAtomic_LoadInt32(const int32_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline int64_t
CereggiiAtomic_LoadInt64(const int64_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline intptr_t
CereggiiAtomic_LoadIntPtr(const intptr_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline uint8_t
CereggiiAtomic_LoadUInt8(const uint8_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline uint16_t
CereggiiAtomic_LoadUInt16(const uint16_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline uint32_t
CereggiiAtomic_LoadUInt32(const uint32_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline uint64_t
CereggiiAtomic_LoadUInt64(const uint64_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline uintptr_t
CereggiiAtomic_LoadUIntPtr(const uintptr_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline unsigned int
CereggiiAtomic_LoadUInt(const unsigned int *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline Py_ssize_t
CereggiiAtomic_LoadSsize(const Py_ssize_t *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }

inline void *
CereggiiAtomic_LoadPtr(const void **obj)
{ return (void *) atomic_load_explicit(obj, memory_order_acquire); }


inline void
CereggiiAtomic_StoreInt(int *obj, int value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreInt8(int8_t *obj, int8_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreInt16(int16_t *obj, int16_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreInt32(int32_t *obj, int32_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreInt64(int64_t *obj, int64_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreIntPtr(intptr_t *obj, intptr_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreUInt8(uint8_t *obj, uint8_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreUInt16(uint16_t *obj, uint16_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreUInt32(uint32_t *obj, uint32_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreUInt64(uint64_t *obj, uint64_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreUIntPtr(uintptr_t *obj, uintptr_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreUInt(unsigned int *obj, unsigned int value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StorePtr(void **obj, void *value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreSsize(Py_ssize_t *obj, Py_ssize_t value)
{ atomic_store_explicit(obj, value, memory_order_release); }


inline void *
CereggiiAtomic_LoadPtrAcquire(const void **obj)
{ return (void *) atomic_load_explicit(obj, memory_order_acquire); }

inline void
CereggiiAtomic_StorePtrRelease(void **obj, void *value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreIntRelease(int *obj, int value)
{ atomic_store_explicit(obj, value, memory_order_release); }

inline int
CereggiiAtomic_LoadIntAcquire(const int *obj)
{ return atomic_load_explicit(obj, memory_order_acquire); }


inline void
CereggiiAtomic_FenceSeqCst(void)
{ atomic_thread_fence(memory_order_seq_cst); }

inline void
CereggiiAtomic_FenceRelease(void)
{ atomic_thread_fence(memory_order_release); }


#pragma clang diagnostic pop
