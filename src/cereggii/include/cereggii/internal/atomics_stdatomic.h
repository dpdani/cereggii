// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include <stdatomic.h>
#include <cereggii/internal/atomics_declarations.h>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "readability-non-const-parameter"


// ----------------------------------------------------------------------------
// add: atomically fetch and add value to obj

inline int
CereggiiAtomic_AddInt(int *obj, int value)
{ return atomic_fetch_add_explicit((_Atomic(int) *)obj, value, memory_order_seq_cst); }

inline int8_t
CereggiiAtomic_AddInt8(int8_t *obj, int8_t value)
{ return atomic_fetch_add_explicit((_Atomic(int8_t) *)obj, value, memory_order_seq_cst); }

inline int16_t
CereggiiAtomic_AddInt16(int16_t *obj, int16_t value)
{ return atomic_fetch_add_explicit((_Atomic(int16_t) *)obj, value, memory_order_seq_cst); }

inline int32_t
CereggiiAtomic_AddInt32(int32_t *obj, int32_t value)
{ return atomic_fetch_add_explicit((_Atomic(int32_t) *)obj, value, memory_order_seq_cst); }

inline int64_t
CereggiiAtomic_AddInt64(int64_t *obj, int64_t value)
{ return atomic_fetch_add_explicit((_Atomic(int64_t) *)obj, value, memory_order_seq_cst); }

inline intptr_t
CereggiiAtomic_AddIntPtr(intptr_t *obj, intptr_t value)
{ return atomic_fetch_add_explicit((_Atomic(intptr_t) *)obj, value, memory_order_seq_cst); }

inline unsigned int
CereggiiAtomic_AddUInt(unsigned int *obj, unsigned int value)
{ return atomic_fetch_add_explicit((_Atomic(unsigned int) *) obj, value, memory_order_seq_cst); }

inline uint8_t
CereggiiAtomic_AddUInt8(uint8_t *obj, uint8_t value)
{ return atomic_fetch_add_explicit((_Atomic(uint8_t) *)obj, value, memory_order_seq_cst); }

inline uint16_t
CereggiiAtomic_AddUInt16(uint16_t *obj, uint16_t value)
{ return atomic_fetch_add_explicit((_Atomic(uint16_t) *)obj, value, memory_order_seq_cst); }

inline uint32_t
CereggiiAtomic_AddUInt32(uint32_t *obj, uint32_t value)
{ return atomic_fetch_add_explicit((_Atomic(uint32_t) *)obj, value, memory_order_seq_cst); }

inline uint64_t
CereggiiAtomic_AddUInt64(uint64_t *obj, uint64_t value)
{ return atomic_fetch_add_explicit((_Atomic(uint64_t) *)obj, value, memory_order_seq_cst); }

inline uintptr_t
CereggiiAtomic_AddUIntPtr(uintptr_t *obj, uintptr_t value)
{ return atomic_fetch_add_explicit((_Atomic(uintptr_t) *)obj, value, memory_order_seq_cst); }

inline Py_ssize_t
CereggiiAtomic_AddSsize(Py_ssize_t *obj, Py_ssize_t value)
{ return atomic_fetch_add_explicit((_Atomic(Py_ssize_t) *)obj, value, memory_order_seq_cst); }


// ----------------------------------------------------------------------------
// compare exchange:
//    1. atomically read the value of obj
//    2. if it is expected, then update it with desired and return 1;
//    3. otherwise, don't change it and return 0

inline int
CereggiiAtomic_CompareExchangeInt(int *obj, int expected, int desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(int) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeInt8(int8_t *obj, int8_t expected, int8_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(int8_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeInt16(int16_t *obj, int16_t expected, int16_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(int16_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeInt32(int32_t *obj, int32_t expected, int32_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(int32_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeInt64(int64_t *obj, int64_t expected, int64_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(int64_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeIntPtr(intptr_t *obj, intptr_t expected, intptr_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(intptr_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeUInt(unsigned int *obj, unsigned int expected, unsigned int desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(unsigned int) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeUInt8(uint8_t *obj, uint8_t expected, uint8_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(uint8_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeUInt16(uint16_t *obj, uint16_t expected, uint16_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(uint16_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeUInt32(uint32_t *obj, uint32_t expected, uint32_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(uint32_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeUInt64(uint64_t *obj, uint64_t expected, uint64_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(uint64_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeUIntPtr(uintptr_t *obj, uintptr_t expected, uintptr_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(uintptr_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangeSsize(Py_ssize_t *obj, Py_ssize_t expected, Py_ssize_t desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(Py_ssize_t) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }

inline int
CereggiiAtomic_CompareExchangePtr(void **obj, void *expected, void *desired)
{ return atomic_compare_exchange_strong_explicit((_Atomic(void *) *) obj, &expected, desired, memory_order_seq_cst, memory_order_seq_cst); }


// ----------------------------------------------------------------------------
// exchange: atomically swap the current value V of obj with value and return V

inline int
CereggiiAtomic_ExchangeInt(int *obj, int value)
{ return atomic_exchange_explicit((_Atomic(int) *) obj, value, memory_order_seq_cst); }

inline int8_t
CereggiiAtomic_ExchangeInt8(int8_t *obj, int8_t value)
{ return atomic_exchange_explicit((_Atomic(int8_t) *) obj, value, memory_order_seq_cst); }

inline int16_t
CereggiiAtomic_ExchangeInt16(int16_t *obj, int16_t value)
{ return atomic_exchange_explicit((_Atomic(int16_t) *) obj, value, memory_order_seq_cst); }

inline int32_t
CereggiiAtomic_ExchangeInt32(int32_t *obj, int32_t value)
{ return atomic_exchange_explicit((_Atomic(int32_t) *) obj, value, memory_order_seq_cst); }

inline int64_t
CereggiiAtomic_ExchangeInt64(int64_t *obj, int64_t value)
{ return atomic_exchange_explicit((_Atomic(int64_t) *) obj, value, memory_order_seq_cst); }

inline intptr_t
CereggiiAtomic_ExchangeIntPtr(intptr_t *obj, intptr_t value)
{ return atomic_exchange_explicit((_Atomic(intptr_t) *) obj, value, memory_order_seq_cst); }

inline unsigned int
CereggiiAtomic_ExchangeUInt(unsigned int *obj, unsigned int value)
{ return atomic_exchange_explicit((_Atomic(unsigned int) *) obj, value, memory_order_seq_cst); }

inline uint8_t
CereggiiAtomic_ExchangeUInt8(uint8_t *obj, uint8_t value)
{ return atomic_exchange_explicit((_Atomic(uint8_t) *) obj, value, memory_order_seq_cst); }

inline uint16_t
CereggiiAtomic_ExchangeUInt16(uint16_t *obj, uint16_t value)
{ return atomic_exchange_explicit((_Atomic(uint16_t) *) obj, value, memory_order_seq_cst); }

inline uint32_t
CereggiiAtomic_ExchangeUInt32(uint32_t *obj, uint32_t value)
{ return atomic_exchange_explicit((_Atomic(uint32_t) *) obj, value, memory_order_seq_cst); }

inline uint64_t
CereggiiAtomic_ExchangeUInt64(uint64_t *obj, uint64_t value)
{ return atomic_exchange_explicit((_Atomic(uint64_t) *) obj, value, memory_order_seq_cst); }

inline uintptr_t
CereggiiAtomic_ExchangeUIntPtr(uintptr_t *obj, uintptr_t value)
{ return atomic_exchange_explicit((_Atomic(uintptr_t) *) obj, value, memory_order_seq_cst); }

inline Py_ssize_t
CereggiiAtomic_ExchangeSsize(Py_ssize_t *obj, Py_ssize_t value)
{ return atomic_exchange_explicit((_Atomic(Py_ssize_t) *) obj, value, memory_order_seq_cst); }

inline void *
CereggiiAtomic_ExchangePtr(void **obj, void *value)
{ return atomic_exchange_explicit((_Atomic(void *) *)obj, value, memory_order_seq_cst); }


// ----------------------------------------------------------------------------
// and: atomically fetch and bitwise-and obj with value

inline uint8_t
CereggiiAtomic_AndUInt8(uint8_t *obj, uint8_t value)
{ return atomic_fetch_and_explicit((_Atomic(uint8_t) *) obj, value, memory_order_seq_cst); }

inline uint16_t
CereggiiAtomic_AndUInt16(uint16_t *obj, uint16_t value)
{ return atomic_fetch_and_explicit((_Atomic(uint16_t) *) obj, value, memory_order_seq_cst); }

inline uint32_t
CereggiiAtomic_AndUInt32(uint32_t *obj, uint32_t value)
{ return atomic_fetch_and_explicit((_Atomic(uint32_t) *) obj, value, memory_order_seq_cst); }

inline uint64_t
CereggiiAtomic_AndUInt64(uint64_t *obj, uint64_t value)
{ return atomic_fetch_and_explicit((_Atomic(uint64_t) *) obj, value, memory_order_seq_cst); }

inline uintptr_t
CereggiiAtomic_AndUIntPtr(uintptr_t *obj, uintptr_t value)
{ return atomic_fetch_and_explicit((_Atomic(uintptr_t) *) obj, value, memory_order_seq_cst); }


// ----------------------------------------------------------------------------
// or: atomically fetch and bitwise-or obj with value

inline uint8_t
CereggiiAtomic_OrUInt8(uint8_t *obj, uint8_t value)
{ return atomic_fetch_or_explicit((_Atomic(uint8_t) *) obj, value, memory_order_seq_cst); }

inline uint16_t
CereggiiAtomic_OrUInt16(uint16_t *obj, uint16_t value)
{ return atomic_fetch_or_explicit((_Atomic(uint16_t) *) obj, value, memory_order_seq_cst); }

inline uint32_t
CereggiiAtomic_OrUInt32(uint32_t *obj, uint32_t value)
{ return atomic_fetch_or_explicit((_Atomic(uint32_t) *) obj, value, memory_order_seq_cst); }

inline uint64_t
CereggiiAtomic_OrUInt64(uint64_t *obj, uint64_t value)
{ return atomic_fetch_or_explicit((_Atomic(uint64_t) *) obj, value, memory_order_seq_cst); }

inline uintptr_t
CereggiiAtomic_OrUIntPtr(uintptr_t *obj, uintptr_t value)
{ return atomic_fetch_or_explicit((_Atomic(uintptr_t) *) obj, value, memory_order_seq_cst); }


// ----------------------------------------------------------------------------
// load: atomically read the value of obj

inline int
CereggiiAtomic_LoadInt(const int *obj)
{ return atomic_load_explicit((_Atomic(int) *) obj, memory_order_seq_cst); }

inline int8_t
CereggiiAtomic_LoadInt8(const int8_t *obj)
{ return atomic_load_explicit((_Atomic(int8_t) *) obj, memory_order_seq_cst); }

inline int16_t
CereggiiAtomic_LoadInt16(const int16_t *obj)
{ return atomic_load_explicit((_Atomic(int16_t) *) obj, memory_order_seq_cst); }

inline int32_t
CereggiiAtomic_LoadInt32(const int32_t *obj)
{ return atomic_load_explicit((_Atomic(int32_t) *) obj, memory_order_seq_cst); }

inline int64_t
CereggiiAtomic_LoadInt64(const int64_t *obj)
{ return atomic_load_explicit((_Atomic(int64_t) *) obj, memory_order_seq_cst); }

inline intptr_t
CereggiiAtomic_LoadIntPtr(const intptr_t *obj)
{ return atomic_load_explicit((_Atomic(intptr_t) *) obj, memory_order_seq_cst); }

inline uint8_t
CereggiiAtomic_LoadUInt8(const uint8_t *obj)
{ return atomic_load_explicit((_Atomic(uint8_t) *) obj, memory_order_seq_cst); }

inline uint16_t
CereggiiAtomic_LoadUInt16(const uint16_t *obj)
{ return atomic_load_explicit((_Atomic(uint16_t) *) obj, memory_order_seq_cst); }

inline uint32_t
CereggiiAtomic_LoadUInt32(const uint32_t *obj)
{ return atomic_load_explicit((_Atomic(uint32_t) *) obj, memory_order_seq_cst); }

inline uint64_t
CereggiiAtomic_LoadUInt64(const uint64_t *obj)
{ return atomic_load_explicit((_Atomic(uint64_t) *) obj, memory_order_seq_cst); }

inline uintptr_t
CereggiiAtomic_LoadUIntPtr(const uintptr_t *obj)
{ return atomic_load_explicit((_Atomic(uintptr_t) *) obj, memory_order_seq_cst); }

inline unsigned int
CereggiiAtomic_LoadUInt(const unsigned int *obj)
{ return atomic_load_explicit((_Atomic(unsigned int) *) obj, memory_order_seq_cst); }

inline Py_ssize_t
CereggiiAtomic_LoadSsize(const Py_ssize_t *obj)
{ return atomic_load_explicit((_Atomic(Py_ssize_t) *) obj, memory_order_seq_cst); }

inline void *
CereggiiAtomic_LoadPtr(const void **obj)
{ return (void *) atomic_load_explicit((_Atomic(void *) *) obj, memory_order_seq_cst); }


// ----------------------------------------------------------------------------
// relaxed load: plainly read the value of obj

inline int
CereggiiAtomic_LoadIntRelaxed(const int *obj)
{ return atomic_load_explicit((_Atomic(int) *)obj, memory_order_relaxed); }

inline char
CereggiiAtomic_LoadCharRelaxed(const char *obj)
{ return atomic_load_explicit((_Atomic(char) *) obj, memory_order_relaxed); }

inline unsigned char
CereggiiAtomic_LoadUCharRelaxed(const unsigned char *obj)
{ return atomic_load_explicit((_Atomic(unsigned char) *) obj, memory_order_relaxed); }

inline short
CereggiiAtomic_LoadShortRelaxed(const short *obj)
{ return atomic_load_explicit((_Atomic(short) *) obj, memory_order_relaxed); }

inline unsigned short
CereggiiAtomic_LoadUShortRelaxed(const unsigned short *obj)
{ return atomic_load_explicit((_Atomic(unsigned short) *) obj, memory_order_relaxed); }

inline long
CereggiiAtomic_LoadLongRelaxed(const long *obj)
{ return atomic_load_explicit((_Atomic(long) *) obj, memory_order_relaxed); }

inline float
CereggiiAtomic_LoadFloatRelaxed(const float *obj)
{ return atomic_load_explicit((_Atomic(float) *) obj, memory_order_relaxed); }

inline double
CereggiiAtomic_LoadDoubleRelaxed(const double *obj)
{ return atomic_load_explicit((_Atomic(double) *) obj, memory_order_relaxed); }

inline int8_t
CereggiiAtomic_LoadInt8Relaxed(const int8_t *obj)
{ return atomic_load_explicit((_Atomic(int8_t) *) obj, memory_order_relaxed); }

inline int16_t
CereggiiAtomic_LoadInt16Relaxed(const int16_t *obj)
{ return atomic_load_explicit((_Atomic(int16_t) *) obj, memory_order_relaxed); }

inline int32_t
CereggiiAtomic_LoadInt32Relaxed(const int32_t *obj)
{ return atomic_load_explicit((_Atomic(int32_t) *) obj, memory_order_relaxed); }

inline int64_t
CereggiiAtomic_LoadInt64Relaxed(const int64_t *obj)
{ return atomic_load_explicit((_Atomic(int64_t) *) obj, memory_order_relaxed); }

inline intptr_t
CereggiiAtomic_LoadIntPtrRelaxed(const intptr_t *obj)
{ return atomic_load_explicit((_Atomic(intptr_t) *) obj, memory_order_relaxed); }

inline uint8_t
CereggiiAtomic_LoadUInt8Relaxed(const uint8_t *obj)
{ return atomic_load_explicit((_Atomic(uint8_t) *) obj, memory_order_relaxed); }

inline uint16_t
CereggiiAtomic_LoadUInt16Relaxed(const uint16_t *obj)
{ return atomic_load_explicit((_Atomic(uint16_t) *) obj, memory_order_relaxed); }

inline uint32_t
CereggiiAtomic_LoadUInt32Relaxed(const uint32_t *obj)
{ return atomic_load_explicit((_Atomic(uint32_t) *) obj, memory_order_relaxed); }

inline uint64_t
CereggiiAtomic_LoadUInt64Relaxed(const uint64_t *obj)
{ return atomic_load_explicit((_Atomic(uint64_t) *) obj, memory_order_relaxed); }

inline uintptr_t
CereggiiAtomic_LoadUIntPtrRelaxed(const uintptr_t *obj)
{ return atomic_load_explicit((_Atomic(uintptr_t) *) obj, memory_order_relaxed); }

inline unsigned int
CereggiiAtomic_LoadUIntRelaxed(const unsigned int *obj)
{ return atomic_load_explicit((_Atomic(unsigned int) *) obj, memory_order_relaxed); }

inline Py_ssize_t
CereggiiAtomic_LoadSsizeRelaxed(const Py_ssize_t *obj)
{ return atomic_load_explicit((_Atomic(Py_ssize_t) *) obj, memory_order_relaxed); }

inline void *
CereggiiAtomic_LoadPtrRelaxed(const void **obj)
{ return atomic_load_explicit((_Atomic(void*)*) obj, memory_order_relaxed); }

inline unsigned long long
CereggiiAtomic_LoadULLongRelaxed(const unsigned long long *obj)
{ return atomic_load_explicit((_Atomic(unsigned long long) *) obj, memory_order_relaxed); }

inline long long
CereggiiAtomic_LoadLLongRelaxed(const long long *obj)
{ return atomic_load_explicit((_Atomic(long long) *) obj, memory_order_relaxed); }


// ----------------------------------------------------------------------------
// store: atomically write value into obj

inline void
CereggiiAtomic_StoreInt(int *obj, int value)
{ atomic_store_explicit((_Atomic(int) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreInt8(int8_t *obj, int8_t value)
{ atomic_store_explicit((_Atomic(int8_t) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreInt16(int16_t *obj, int16_t value)
{ atomic_store_explicit((_Atomic(int16_t) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreInt32(int32_t *obj, int32_t value)
{ atomic_store_explicit((_Atomic(int32_t) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreInt64(int64_t *obj, int64_t value)
{ atomic_store_explicit((_Atomic(int64_t) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreIntPtr(intptr_t *obj, intptr_t value)
{ atomic_store_explicit((_Atomic(intptr_t) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreUInt8(uint8_t *obj, uint8_t value)
{ atomic_store_explicit((_Atomic(uint8_t) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreUInt16(uint16_t *obj, uint16_t value)
{ atomic_store_explicit((_Atomic(uint16_t) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreUInt32(uint32_t *obj, uint32_t value)
{ atomic_store_explicit((_Atomic(uint32_t) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreUInt64(uint64_t *obj, uint64_t value)
{ atomic_store_explicit((_Atomic(uint64_t) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreUIntPtr(uintptr_t *obj, uintptr_t value)
{ atomic_store_explicit((_Atomic(uintptr_t) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreUInt(unsigned int *obj, unsigned int value)
{ atomic_store_explicit((_Atomic(unsigned int) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StorePtr(void **obj, void *value)
{ atomic_store_explicit((_Atomic(void *) *) obj, value, memory_order_seq_cst); }

inline void
CereggiiAtomic_StoreSsize(Py_ssize_t *obj, Py_ssize_t value)
{ atomic_store_explicit((_Atomic(Py_ssize_t) *) obj, value, memory_order_seq_cst); }


// ----------------------------------------------------------------------------
// relaxed store: plainly write value into obj

inline void
CereggiiAtomic_StoreIntRelaxed(int *obj, int value)
{ atomic_store_explicit((_Atomic(int) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreInt8Relaxed(int8_t *obj, int8_t value)
{ atomic_store_explicit((_Atomic(int8_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreInt16Relaxed(int16_t *obj, int16_t value)
{ atomic_store_explicit((_Atomic(int16_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreInt32Relaxed(int32_t *obj, int32_t value)
{ atomic_store_explicit((_Atomic(int32_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreInt64Relaxed(int64_t *obj, int64_t value)
{ atomic_store_explicit((_Atomic(int64_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreIntPtrRelaxed(intptr_t *obj, intptr_t value)
{ atomic_store_explicit((_Atomic(intptr_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreUInt8Relaxed(uint8_t *obj, uint8_t value)
{ atomic_store_explicit((_Atomic(uint8_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreUInt16Relaxed(uint16_t *obj, uint16_t value)
{ atomic_store_explicit((_Atomic(uint16_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreUInt32Relaxed(uint32_t *obj, uint32_t value)
{ atomic_store_explicit((_Atomic(uint32_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreUInt64Relaxed(uint64_t *obj, uint64_t value)
{ atomic_store_explicit((_Atomic(uint64_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreUIntPtrRelaxed(uintptr_t *obj, uintptr_t value)
{ atomic_store_explicit((_Atomic(uintptr_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreUIntRelaxed(unsigned int *obj, unsigned int value)
{ atomic_store_explicit((_Atomic(unsigned int) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StorePtrRelaxed(void **obj, void *value)
{ atomic_store_explicit((_Atomic(void *) *)(void **)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreSsizeRelaxed(Py_ssize_t *obj, Py_ssize_t value)
{ atomic_store_explicit((_Atomic(Py_ssize_t) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreULLongRelaxed(unsigned long long *obj,
                                unsigned long long value)
{ atomic_store_explicit((_Atomic(unsigned long long) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreCharRelaxed(char *obj, char value)
{ atomic_store_explicit((_Atomic(char) *)obj, value, __ATOMIC_RELEASE); }

inline void
CereggiiAtomic_StoreUCharRelaxed(unsigned char *obj, unsigned char value)
{ atomic_store_explicit((_Atomic(unsigned char) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreShortRelaxed(short *obj, short value)
{ atomic_store_explicit((_Atomic(short) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreUShortRelaxed(unsigned short *obj, unsigned short value)
{ atomic_store_explicit((_Atomic(unsigned short) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreLongRelaxed(long *obj, long value)
{ atomic_store_explicit((_Atomic(long) *)obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreFloatRelaxed(float *obj, float value)
{ atomic_store_explicit((_Atomic(float) *) obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreDoubleRelaxed(double *obj, double value)
{ atomic_store_explicit((_Atomic(double) *) obj, value, memory_order_relaxed); }

inline void
CereggiiAtomic_StoreLLongRelaxed(long long *obj, long long value)
{ atomic_store_explicit((_Atomic(long long) *)obj, value, memory_order_relaxed); }


// ----------------------------------------------------------------------------
// _Py_atomic_load_ptr_acquire / _Py_atomic_store_ptr_release

inline void *
CereggiiAtomic_LoadPtrAcquire(const void **obj)
{ return (void *) atomic_load_explicit((_Atomic(void *) *) obj, memory_order_acquire); }

inline uintptr_t
CereggiiAtomic_LoadUIntPtrAcquire(const uintptr_t *obj)
{ return (uintptr_t)atomic_load_explicit((_Atomic(uintptr_t) *) obj, memory_order_acquire); }

inline void
CereggiiAtomic_StorePtrRelease(void **obj, void *value)
{ atomic_store_explicit((_Atomic(void *) *) obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreUIntPtrRelease(uintptr_t *obj, uintptr_t value)
{ atomic_store_explicit((_Atomic(uintptr_t) *) obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreIntRelease(int *obj, int value)
{ atomic_store_explicit((_Atomic(int) *) obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreUIntRelease(unsigned int *obj, unsigned int value)
{ atomic_store_explicit((_Atomic(unsigned int) *) obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreSsizeRelease(Py_ssize_t *obj, Py_ssize_t value)
{ atomic_store_explicit((_Atomic(Py_ssize_t) *) obj, value, memory_order_release); }

inline int
CereggiiAtomic_LoadIntAcquire(const int *obj)
{ return atomic_load_explicit((_Atomic(int) *) obj, memory_order_acquire); }

inline void
CereggiiAtomic_StoreUInt32Release(uint32_t *obj, uint32_t value)
{ atomic_store_explicit((_Atomic(uint32_t) *) obj, value, memory_order_release); }

inline void
CereggiiAtomic_StoreUInt64Release(uint64_t *obj, uint64_t value)
{ atomic_store_explicit((_Atomic(uint64_t) *) obj, value, memory_order_release); }

inline uint64_t
CereggiiAtomic_LoadUInt64Acquire(const uint64_t *obj)
{ return atomic_load_explicit((_Atomic(uint64_t) *) obj, memory_order_acquire); }

inline uint32_t
CereggiiAtomic_LoadUInt32Acquire(const uint32_t *obj)
{ return atomic_load_explicit((_Atomic(uint32_t) *) obj, memory_order_acquire); }

inline Py_ssize_t
CereggiiAtomic_LoadSsizeAcquire(const Py_ssize_t *obj)
{ return atomic_load_explicit((_Atomic(Py_ssize_t) *) obj, memory_order_acquire); }


// ----------------------------------------------------------------------------
// fence: separate regions of code

inline void
CereggiiAtomic_FenceSeqCst(void)
{ atomic_thread_fence(memory_order_seq_cst); }

inline void
CereggiiAtomic_FenceAcquire(void)
{ atomic_thread_fence(memory_order_acquire); }

inline void
CereggiiAtomic_FenceRelease(void)
{ atomic_thread_fence(memory_order_release); }


#pragma clang diagnostic pop
