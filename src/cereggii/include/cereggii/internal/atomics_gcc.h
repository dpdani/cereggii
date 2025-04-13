// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include <cereggii/internal/atomics_declarations.h>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-non-const-parameter"


// ----------------------------------------------------------------------------
// add: atomically fetch and add value to obj

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


// ----------------------------------------------------------------------------
// compare exchange:
//    1. atomically read the value of obj
//    2. if it is expected, then update it with desired and return 1;
//    3. otherwise, don't change it and return 0

inline int
CereggiiAtomic_CompareExchangeInt(int *obj, int expected, int desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeInt8(int8_t *obj, int8_t expected, int8_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeInt16(int16_t *obj, int16_t expected, int16_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeInt32(int32_t *obj, int32_t expected, int32_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeInt64(int64_t *obj, int64_t expected, int64_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeIntPtr(intptr_t *obj, intptr_t expected, intptr_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeUInt(unsigned int *obj, unsigned int expected, unsigned int desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeUInt8(uint8_t *obj, uint8_t expected, uint8_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeUInt16(uint16_t *obj, uint16_t expected, uint16_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeUInt32(uint32_t *obj, uint32_t expected, uint32_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeUInt64(uint64_t *obj, uint64_t expected, uint64_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangeUIntPtr(uintptr_t *obj, uintptr_t expected, uintptr_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

__attribute__((visibility("default"))) inline int
CereggiiAtomic_CompareExchangeSsize(Py_ssize_t *obj, Py_ssize_t expected, Py_ssize_t desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }

inline int
CereggiiAtomic_CompareExchangePtr(void **obj, void *expected, void *desired)
{ return __atomic_compare_exchange_n(obj, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); }


// ----------------------------------------------------------------------------
// exchange: atomically swap the current value V of obj with value and return V

inline int
CereggiiAtomic_ExchangeInt(int *obj, int value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline int8_t
CereggiiAtomic_ExchangeInt8(int8_t *obj, int8_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline int16_t
CereggiiAtomic_ExchangeInt16(int16_t *obj, int16_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline int32_t
CereggiiAtomic_ExchangeInt32(int32_t *obj, int32_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline int64_t
CereggiiAtomic_ExchangeInt64(int64_t *obj, int64_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline intptr_t
CereggiiAtomic_ExchangeIntPtr(intptr_t *obj, intptr_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline unsigned int
CereggiiAtomic_ExchangeUInt(unsigned int *obj, unsigned int value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline uint8_t
CereggiiAtomic_ExchangeUInt8(uint8_t *obj, uint8_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline uint16_t
CereggiiAtomic_ExchangeUInt16(uint16_t *obj, uint16_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_ACQ_REL); }

inline uint32_t
CereggiiAtomic_ExchangeUInt32(uint32_t *obj, uint32_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline uint64_t
CereggiiAtomic_ExchangeUInt64(uint64_t *obj, uint64_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline uintptr_t
CereggiiAtomic_ExchangeUIntPtr(uintptr_t *obj, uintptr_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline Py_ssize_t
CereggiiAtomic_ExchangeSsize(Py_ssize_t *obj, Py_ssize_t value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }

inline void *
CereggiiAtomic_ExchangePtr(void **obj, void *value)
{ return __atomic_exchange_n(obj, value, __ATOMIC_SEQ_CST); }


// ----------------------------------------------------------------------------
// and: atomically fetch and bitwise-and obj with value

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


// ----------------------------------------------------------------------------
// or: atomically fetch and bitwise-or obj with value

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
CereggiiAtomic_OrUIntPtr(uintptr_t *obj, uintptr_t value)
{ return __atomic_fetch_or(obj, value, __ATOMIC_SEQ_CST); }


// ----------------------------------------------------------------------------
// load: atomically read the value of obj

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
{ return __atomic_load_n((void * const *)obj, __ATOMIC_SEQ_CST); }


// ----------------------------------------------------------------------------
// relaxed load: plainly read the value of obj

inline int
CereggiiAtomic_LoadIntRelaxed(const int *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline char
CereggiiAtomic_LoadCharRelaxed(const char *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline unsigned char
CereggiiAtomic_LoadUCharRelaxed(const unsigned char *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline short
CereggiiAtomic_LoadShortRelaxed(const short *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline unsigned short
CereggiiAtomic_LoadUShortRelaxed(const unsigned short *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline long
CereggiiAtomic_LoadLongRelaxed(const long *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline float
CereggiiAtomic_LoadFloatRelaxed(const float *obj)
{ float ret; __atomic_load(obj, &ret, __ATOMIC_RELAXED); return ret; }

inline double
CereggiiAtomic_LoadDoubleRelaxed(const double *obj)
{ double ret; __atomic_load(obj, &ret, __ATOMIC_RELAXED); return ret; }

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
CereggiiAtomic_LoadIntPtrRelaxed(const intptr_t *obj)
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
CereggiiAtomic_LoadUIntPtrRelaxed(const uintptr_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline unsigned int
CereggiiAtomic_LoadUIntRelaxed(const unsigned int *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline Py_ssize_t
CereggiiAtomic_LoadSsizeRelaxed(const Py_ssize_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline void *
CereggiiAtomic_LoadPtrRelaxed(const void **obj)
{ return __atomic_load_n((void * const *)obj, __ATOMIC_RELAXED); }

inline unsigned long long
CereggiiAtomic_LoadULLongRelaxed(const unsigned long long *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }

inline long long
CereggiiAtomic_LoadLLongRelaxed(const long long *obj)
{ return __atomic_load_n(obj, __ATOMIC_RELAXED); }


// ----------------------------------------------------------------------------
// store: atomically write value into obj

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
CereggiiAtomic_StoreUIntPtr(uintptr_t *obj, uintptr_t value)
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


// ----------------------------------------------------------------------------
// relaxed store: plainly write value into obj

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
CereggiiAtomic_StoreIntPtrRelaxed(intptr_t *obj, intptr_t value)
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
CereggiiAtomic_StoreUIntPtrRelaxed(uintptr_t *obj, uintptr_t value)
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

inline void
CereggiiAtomic_StoreULLongRelaxed(unsigned long long *obj, unsigned long long value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreCharRelaxed(char *obj, char value)
{ __atomic_store_n(obj, value, __ATOMIC_RELEASE); }

inline void
CereggiiAtomic_StoreUCharRelaxed(unsigned char *obj, unsigned char value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreShortRelaxed(short *obj, short value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreUShortRelaxed(unsigned short *obj, unsigned short value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreLongRelaxed(long *obj, long value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreFloatRelaxed(float *obj, float value)
{ __atomic_store(obj, &value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreDoubleRelaxed(double *obj, double value)
{ __atomic_store(obj, &value, __ATOMIC_RELAXED); }

inline void
CereggiiAtomic_StoreLLongRelaxed(long long *obj, long long value)
{ __atomic_store_n(obj, value, __ATOMIC_RELAXED); }


// ----------------------------------------------------------------------------
// _Py_atomic_load_ptr_acquire / _Py_atomic_store_ptr_release

inline void *
CereggiiAtomic_LoadPtrAcquire(const void **obj)
{ return __atomic_load_n((void * const *)obj, __ATOMIC_ACQUIRE); }

inline uintptr_t
CereggiiAtomic_LoadUIntPtrAcquire(const uintptr_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_ACQUIRE); }

inline void
CereggiiAtomic_StorePtrRelease(void **obj, void *value)
{ __atomic_store_n(obj, value, __ATOMIC_RELEASE); }

inline void
CereggiiAtomic_StoreUIntPtrRelease(uintptr_t *obj, uintptr_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELEASE); }

inline void
CereggiiAtomic_StoreIntRelease(int *obj, int value)
{ __atomic_store_n(obj, value, __ATOMIC_RELEASE); }

inline void
CereggiiAtomic_StoreUIntRelease(unsigned int *obj, unsigned int value)
{ __atomic_store_n(obj, value, __ATOMIC_RELEASE); }

inline void
CereggiiAtomic_StoreSsizeRelease(Py_ssize_t *obj, Py_ssize_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELEASE); }

inline int
CereggiiAtomic_LoadIntAcquire(const int *obj)
{ return __atomic_load_n(obj, __ATOMIC_ACQUIRE); }

inline void
CereggiiAtomic_StoreUInt32Release(uint32_t *obj, uint32_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELEASE); }

inline void
CereggiiAtomic_StoreUInt64Release(uint64_t *obj, uint64_t value)
{ __atomic_store_n(obj, value, __ATOMIC_RELEASE); }

inline uint64_t
CereggiiAtomic_LoadUInt64Acquire(const uint64_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_ACQUIRE); }

inline uint32_t
CereggiiAtomic_LoadUInt32Acquire(const uint32_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_ACQUIRE); }

inline Py_ssize_t
CereggiiAtomic_LoadSsizeAcquire(const Py_ssize_t *obj)
{ return __atomic_load_n(obj, __ATOMIC_ACQUIRE); }


// ----------------------------------------------------------------------------
// fence: separate regions of code

inline void
CereggiiAtomic_FenceSeqCst(void)
{ __atomic_thread_fence(__ATOMIC_SEQ_CST); }

inline void
CereggiiAtomic_FenceAcquire(void)
{ __atomic_thread_fence(__ATOMIC_ACQUIRE); }

inline void
CereggiiAtomic_FenceRelease(void)
{ __atomic_thread_fence(__ATOMIC_RELEASE); }
