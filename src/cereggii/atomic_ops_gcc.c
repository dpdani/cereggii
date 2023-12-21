// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_ops.h"

#ifdef _CEREGGII_ATOMIC_OPS_GCC

// This implementation is preferred for GCC compatible compilers, such as Clang.
// These functions are available in GCC 4.8+ without needing to compile with
// --std=c11 or --std=gnu11.


#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-non-const-parameter"


inline int
CereggiiAtomic_AddInt(int *obj, int value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline int8_t
CereggiiAtomic_AddInt8(int8_t *obj, int8_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline int16_t
CereggiiAtomic_AddInt16(int16_t *obj, int16_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline int32_t
CereggiiAtomic_AddInt32(int32_t *obj, int32_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline int64_t
CereggiiAtomic_AddInt64(int64_t *obj, int64_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline intptr_t
CereggiiAtomic_AddIntPtr(intptr_t *obj, intptr_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline unsigned int
CereggiiAtomic_AddUInt(unsigned int *obj, unsigned int value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline uint8_t
CereggiiAtomic_AddUInt8(uint8_t *obj, uint8_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline uint16_t
CereggiiAtomic_AddUInt16(uint16_t *obj, uint16_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline uint32_t
CereggiiAtomic_AddUInt32(uint32_t *obj, uint32_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline uint64_t
CereggiiAtomic_AddUInt64(uint64_t *obj, uint64_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline uintptr_t
CereggiiAtomic_AddUIntPtr(uintptr_t *obj, uintptr_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }

inline Py_ssize_t
CereggiiAtomic_AddSsize(Py_ssize_t *obj, Py_ssize_t value)
{ return __atomic_fetch_add(obj, value, __ATOMIC_SEQ_CST); }


inline int
CereggiiAtomic_CompareExchangeInt(int *obj, int expected, int desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeInt8(int8_t *obj, int8_t expected, int8_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeInt16(int16_t *obj, int16_t expected, int16_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeInt32(int32_t *obj, int32_t expected, int32_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeInt64(int64_t *obj, int64_t expected, int64_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeIntPtr(intptr_t *obj, intptr_t expected, intptr_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeUInt(unsigned int *obj, unsigned int expected, unsigned int desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeUInt8(uint8_t *obj, uint8_t expected, uint8_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeUInt16(uint16_t *obj, uint16_t expected, uint16_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeUInt32(uint32_t *obj, uint32_t expected, uint32_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeUInt64(uint64_t *obj, uint64_t expected, uint64_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeUIntPtr(uintptr_t *obj, uintptr_t expected, uintptr_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangeSsize(Py_ssize_t *obj, Py_ssize_t expected, Py_ssize_t desired)
{
    return __atomic_compare_exchange_n(obj, &expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}

inline int
CereggiiAtomic_CompareExchangePtr(void **obj, void *expected, void *desired)
{
    return __atomic_compare_exchange_n(obj, expected, desired, 0,
                                       __ATOMIC_RELAXED, __ATOMIC_RELAXED);
}


inline int
CereggiiAtomic_ExchangeInt(int *obj, int value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline int8_t
CereggiiAtomic_ExchangeInt8(int8_t *obj, int8_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline int16_t
CereggiiAtomic_ExchangeInt16(int16_t *obj, int16_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline int32_t
CereggiiAtomic_ExchangeInt32(int32_t *obj, int32_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline int64_t
CereggiiAtomic_ExchangeInt64(int64_t *obj, int64_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline intptr_t
CereggiiAtomic_ExchangeIntPtr(intptr_t *obj, intptr_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline unsigned int
CereggiiAtomic_ExchangeUInt(unsigned int *obj, unsigned int value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline uint8_t
CereggiiAtomic_ExchangeUInt8(uint8_t *obj, uint8_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline uint16_t
CereggiiAtomic_ExchangeUInt16(uint16_t *obj, uint16_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline uint32_t
CereggiiAtomic_ExchangeUInt32(uint32_t *obj, uint32_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline uint64_t
CereggiiAtomic_ExchangeUInt64(uint64_t *obj, uint64_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline uintptr_t
CereggiiAtomic_ExchangeUIntPtr(uintptr_t *obj, uintptr_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline Py_ssize_t
CereggiiAtomic_ExchangeSsize(Py_ssize_t *obj, Py_ssize_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }

inline void *
CereggiiAtomic_ExchangePtr(void **obj, void *value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_RELAXED); }


inline uint8_t
CereggiiAtomic_AndUInt8(uint8_t *obj, uint8_t value)
{ return __atomic_fetch_and(obj, value, __ATOMIC_SEQ_CST); }

inline uint16_t
CereggiiAtomic_AndUInt16(uint16_t *obj, uint16_t value)
{ return __atomic_fetch_and(obj, value, __ATOMIC_SEQ_CST); }

inline uint32_t
CereggiiAtomic_AndUInt32(uint32_t *obj, uint32_t value)
{ return __atomic_fetch_and(obj, value, __ATOMIC_SEQ_CST); }

inline uint64_t
CereggiiAtomic_AndUInt64(uint64_t *obj, uint64_t value)
{ return __atomic_fetch_and(obj, value, __ATOMIC_SEQ_CST); }

inline uintptr_t
CereggiiAtomic_AndUIntPtr(uintptr_t *obj, uintptr_t value)
{ return __atomic_fetch_and(obj, value, __ATOMIC_SEQ_CST); }


inline uint8_t
CereggiiAtomic_OrUInt8(uint8_t *obj, uint8_t value)
{ return __atomic_fetch_or(obj, value, __ATOMIC_SEQ_CST); }

inline uint16_t
CereggiiAtomic_OrUInt16(uint16_t *obj, uint16_t value)
{ return __atomic_fetch_or(obj, value, __ATOMIC_SEQ_CST); }

inline uint32_t
CereggiiAtomic_OrUInt32(uint32_t *obj, uint32_t value)
{ return __atomic_fetch_or(obj, value, __ATOMIC_SEQ_CST); }

inline uint64_t
CereggiiAtomic_OrUInt64(uint64_t *obj, uint64_t value)
{ return __atomic_fetch_or(obj, value, __ATOMIC_SEQ_CST); }

inline uintptr_t
CereggiiAtomic_OrUIntptr(uintptr_t *obj, uintptr_t value)
{ return __atomic_fetch_or(obj, value, __ATOMIC_SEQ_CST); }


inline int
CereggiiAtomic_LoadInt(const int *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline int8_t
CereggiiAtomic_LoadInt8(const int8_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline int16_t
CereggiiAtomic_LoadInt16(const int16_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline int32_t
CereggiiAtomic_LoadInt32(const int32_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline int64_t
CereggiiAtomic_LoadInt64(const int64_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline intptr_t
CereggiiAtomic_LoadIntPtr(const intptr_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline uint8_t
CereggiiAtomic_LoadUInt8(const uint8_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline uint16_t
CereggiiAtomic_LoadUInt16(const uint16_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline uint32_t
CereggiiAtomic_LoadUInt32(const uint32_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline uint64_t
CereggiiAtomic_LoadUInt64(const uint64_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline uintptr_t
CereggiiAtomic_LoadUIntPtr(const uintptr_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline unsigned int
CereggiiAtomic_LoadUInt(const unsigned int *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline Py_ssize_t
CereggiiAtomic_LoadSsize(const Py_ssize_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_SEQ_CST); }

inline void *
CereggiiAtomic_LoadPtr(const void **obj)
{ return (void *) __atomic_load_n(obj, __ATOMIC_SEQ_CST); }


inline int
CereggiiAtomic_LoadIntRelaxed(const int *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline int8_t
CereggiiAtomic_LoadInt8Relaxed(const int8_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline int16_t
CereggiiAtomic_LoadInt16Relaxed(const int16_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline int32_t
CereggiiAtomic_LoadInt32Relaxed(const int32_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline int64_t
CereggiiAtomic_LoadInt64Relaxed(const int64_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline intptr_t
CereggiiAtomic_LoadIntptrRelaxed(const intptr_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline uint8_t
CereggiiAtomic_LoadUInt8Relaxed(const uint8_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline uint16_t
CereggiiAtomic_LoadUInt16Relaxed(const uint16_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline uint32_t
CereggiiAtomic_LoadUInt32Relaxed(const uint32_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline uint64_t
CereggiiAtomic_LoadUInt64Relaxed(const uint64_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline uintptr_t
CereggiiAtomic_LoadUIntptrRelaxed(const uintptr_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline unsigned int
CereggiiAtomic_LoadUIntRelaxed(const unsigned int *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline Py_ssize_t
CereggiiAtomic_LoadSsizeRelaxed(const Py_ssize_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline void *
CereggiiAtomic_LoadPtrRelaxed(const void *obj)
{ return (void *) __atomic_load_n((const void **) obj, __ATOMIC_RELAXED); }


inline void
CereggiiAtomic_StoreInt(int *obj, int value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreInt8(int8_t *obj, int8_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreInt16(int16_t *obj, int16_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreInt32(int32_t *obj, int32_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreInt64(int64_t *obj, int64_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreIntPtr(intptr_t *obj, intptr_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreUInt8(uint8_t *obj, uint8_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreUInt16(uint16_t *obj, uint16_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreUInt32(uint32_t *obj, uint32_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreUInt64(uint64_t *obj, uint64_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreUIntptr(uintptr_t *obj, uintptr_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreUInt(unsigned int *obj, unsigned int value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StorePtr(void **obj, void *value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_StoreSsize(Py_ssize_t *obj, Py_ssize_t value)
{ __atomic_store_n(obj, value, __ATOMIC_SEQ_CST); }


inline void
CereggiiAtomic_StoreIntRelaxed(int *obj, int value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreInt8Relaxed(int8_t *obj, int8_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreInt16Relaxed(int16_t *obj, int16_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreInt32Relaxed(int32_t *obj, int32_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreInt64Relaxed(int64_t *obj, int64_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreIntptrRelaxed(intptr_t *obj, intptr_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreUInt8Relaxed(uint8_t *obj, uint8_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreUInt16Relaxed(uint16_t *obj, uint16_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreUInt32Relaxed(uint32_t *obj, uint32_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreUInt64Relaxed(uint64_t *obj, uint64_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreUIntptrRelaxed(uintptr_t *obj, uintptr_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreUIntRelaxed(unsigned int *obj, unsigned int value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StorePtrRelaxed(void **obj, void *value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreSsizeRelaxed(Py_ssize_t *obj, Py_ssize_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }


inline void *
CereggiiAtomic_LoadPtrAcquire(const void **obj)
{ return (void *) __atomic_load_n(obj, __ATOMIC_ACQUIRE); }

inline void
CereggiiAtomic_StorePtrRelease(void **obj, void *value)
{ __atomic_store_n(obj, value, __ATOMIC_RELEASE); }

inline void
CereggiiAtomic_StoreIntRelease(int *obj, int value)
{ __atomic_store_n(obj, value, __ATOMIC_RELEASE); }

inline int
CereggiiAtomic_LoadIntAcquire(const int *obj)
{ return __atomic_load_n(obj, __ATOMIC_ACQUIRE); }


inline void
CereggiiAtomic_FenceSeqCst(void)
{ __atomic_thread_fence(__ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_FenceRelease(void)
{ __atomic_thread_fence(__ATOMIC_RELEASE); }

#endif //_CEREGGII_ATOMIC_OPS_GCC

#pragma clang diagnostic pop