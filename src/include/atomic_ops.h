// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_OPS_H
#define CEREGGII_ATOMIC_OPS_H

#include "Python.h"

// This header provides cross-platform low-level atomic operations
// similar to C11 atomics.
//
// Operations are sequentially consistent unless they have a suffix indicating
// otherwise. If in doubt, prefer the sequentially consistent operations.
//
// The "_relaxed" suffix for load and store operations indicates the "relaxed"
// memory order. They don't provide synchronization, but (roughly speaking)
// guarantee somewhat sane behavior for races instead of undefined behavior.
// In practice, they correspond to "normal" hardware load and store
// instructions, so they are almost as inexpensive as plain loads and stores
// in C.
//
// Note that atomic read-modify-write operations like CereggiiAtomic_add_* return
// the previous value of the atomic variable, not the new value.
//
// See https://en.cppreference.com/w/c/atomic for more information on C11
// atomics.
// See https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2055r0.pdf
// "A Relaxed Guide to memory_order_relaxed" for discussion of and common usage
// or relaxed atomics.
//
// Functions with pseudo Python code:
//
//   def CereggiiAtomic_load(obj):
//        return obj  # sequential consistency
//
//   def CereggiiAtomic_loadRelaxed(obj):
//       return obj  # relaxed consistency
//
//   def CereggiiAtomic_store(obj, value):
//       obj = value  # sequential consistency
//
//   def CereggiiAtomic_storeRelaxed(obj, value):
//       obj = value  # relaxed consistency
//
//   def CereggiiAtomic_Exchange(obj, value):
//       # sequential consistency
//       old_obj = obj
//       obj = value
//       return old_obj
//
//   def CereggiiAtomic_CompareExchange(obj, expected, updated):
//       # sequential consistency
//       if obj == expected:
//           obj = updated
//           return True
//       else:
//           expected = obj
//           return False
//
//   def CereggiiAtomic_add(obj, value):
//       # sequential consistency
//       old_obj = obj
//       obj += value
//       return old_obj
//
//   def CereggiiAtomic_and(obj, value):
//       # sequential consistency
//       old_obj = obj
//       obj &= value
//       return old_obj
//
//   def CereggiiAtomic_or(obj, value):
//       # sequential consistency
//       old_obj = obj
//       obj |= value
//       return old_obj
//
// Other functions:
//
//   def CereggiiAtomic_load_ptr_acquire(obj):
//       return obj  # acquire
//
//   def CereggiiAtomic_store_ptr_release(obj):
//       return obj  # release
//
//   def CereggiiAtomic_fence_seq_cst():
//       # sequential consistency
//       ...
//
//   def CereggiiAtomic_fence_release():
//       # release
//       ...

// --- CereggiiAtomic_Add --------------------------------------------------------
// Atomically adds `value` to `obj` and returns the previous value

int CereggiiAtomic_AddInt(int *obj, int value);

int8_t CereggiiAtomic_AddInt8(int8_t *obj, int8_t value);

int16_t CereggiiAtomic_AddInt16(int16_t *obj, int16_t value);

int32_t CereggiiAtomic_AddInt32(int32_t *obj, int32_t value);

int64_t CereggiiAtomic_AddInt64(int64_t *obj, int64_t value);

intptr_t CereggiiAtomic_AddIntPtr(intptr_t *obj, intptr_t value);

unsigned int CereggiiAtomic_AddUInt(unsigned int *obj, unsigned int value);

uint8_t CereggiiAtomic_AddUInt8(uint8_t *obj, uint8_t value);

uint16_t CereggiiAtomic_AddUInt16(uint16_t *obj, uint16_t value);

uint32_t CereggiiAtomic_AddUInt32(uint32_t *obj, uint32_t value);

uint64_t CereggiiAtomic_AddUInt64(uint64_t *obj, uint64_t value);

uintptr_t CereggiiAtomic_AddUIntPtr(uintptr_t *obj, uintptr_t value);

Py_ssize_t CereggiiAtomic_AddSsize(Py_ssize_t *obj, Py_ssize_t value);


// --- CereggiiAtomic_CompareExchange -------------------------------------------
// Performs an atomic compare-and-exchange.
//
// - If `*obj` and `*expected` are equal, store `updated` into `*obj`
//   and return 1 (success).
// - Otherwise, store the `*obj` current value into `*expected`
//   and return 0 (failure).
//
// These correspond to the C11 atomic_compare_exchange_strong() function.

int CereggiiAtomic_CompareExchangeInt(int *obj, int expected, int updated);

int CereggiiAtomic_CompareExchangeInt8(int8_t *obj, int8_t expected, int8_t updated);

int CereggiiAtomic_CompareExchangeInt16(int16_t *obj, int16_t expected, int16_t updated);

int CereggiiAtomic_CompareExchangeInt32(int32_t *obj, int32_t expected, int32_t updated);

int CereggiiAtomic_CompareExchangeInt64(int64_t *obj, int64_t expected, int64_t updated);

int CereggiiAtomic_CompareExchangeInt128(__int128_t *obj, __int128_t expected, __int128_t updated);

int CereggiiAtomic_CompareExchangeIntPtr(intptr_t *obj, intptr_t expected, intptr_t updated);

int CereggiiAtomic_CompareExchangeUInt(unsigned int *obj, unsigned int expected, unsigned int updated);

int CereggiiAtomic_CompareExchangeUInt8(uint8_t *obj, uint8_t expected, uint8_t updated);

int CereggiiAtomic_CompareExchangeUInt16(uint16_t *obj, uint16_t expected, uint16_t updated);

int CereggiiAtomic_CompareExchangeUInt32(uint32_t *obj, uint32_t expected, uint32_t updated);

int CereggiiAtomic_CompareExchangeUInt64(uint64_t *obj, uint64_t expected, uint64_t updated);

int CereggiiAtomic_CompareExchangeUInt128(__uint128_t *obj, __uint128_t expected, __uint128_t updated);

int CereggiiAtomic_CompareExchangeUIntPtr(uintptr_t *obj, uintptr_t expected, uintptr_t updated);

int CereggiiAtomic_CompareExchangeSsize(Py_ssize_t *obj, Py_ssize_t expected, Py_ssize_t updated);

int CereggiiAtomic_CompareExchangePtr(void *obj, void *expected, void *value);


// --- CereggiiAtomic_Exchange ---------------------------------------------------
// Atomically replaces `*obj` with `value` and returns the previous value of `*obj`.

int CereggiiAtomic_ExchangeInt(int *obj, int value);

int8_t CereggiiAtomic_ExchangeInt8(int8_t *obj, int8_t value);

int16_t CereggiiAtomic_ExchangeInt16(int16_t *obj, int16_t value);

int32_t CereggiiAtomic_ExchangeInt32(int32_t *obj, int32_t value);

int64_t CereggiiAtomic_ExchangeInt64(int64_t *obj, int64_t value);

intptr_t CereggiiAtomic_ExchangeIntPtr(intptr_t *obj, intptr_t value);

unsigned int CereggiiAtomic_ExchangeUInt(unsigned int *obj, unsigned int value);

uint8_t CereggiiAtomic_ExchangeUInt8(uint8_t *obj, uint8_t value);

uint16_t CereggiiAtomic_ExchangeUInt16(uint16_t *obj, uint16_t value);

uint32_t CereggiiAtomic_ExchangeUInt32(uint32_t *obj, uint32_t value);

uint64_t CereggiiAtomic_ExchangeUInt64(uint64_t *obj, uint64_t value);

uintptr_t CereggiiAtomic_ExchangeUIntPtr(uintptr_t *obj, uintptr_t value);

Py_ssize_t CereggiiAtomic_ExchangeSsize(Py_ssize_t *obj, Py_ssize_t value);

void *CereggiiAtomic_ExchangePtr(void **obj, void *value);


// --- CereggiiAtomic_and --------------------------------------------------------
// Performs `*obj &= value` atomically and returns the previous value of `*obj`.

uint8_t CereggiiAtomic_AndUInt8(uint8_t *obj, uint8_t value);

uint16_t CereggiiAtomic_AndUInt16(uint16_t *obj, uint16_t value);

uint32_t CereggiiAtomic_AndUInt32(uint32_t *obj, uint32_t value);

uint64_t CereggiiAtomic_AndUInt64(uint64_t *obj, uint64_t value);

uintptr_t CereggiiAtomic_AndUIntPtr(uintptr_t *obj, uintptr_t value);


// --- CereggiiAtomic_or ---------------------------------------------------------
// Performs `*obj |= value` atomically and returns the previous value of `*obj`.

uint8_t CereggiiAtomic_OrUInt8(uint8_t *obj, uint8_t value);

uint16_t CereggiiAtomic_OrUInt16(uint16_t *obj, uint16_t value);

uint32_t CereggiiAtomic_OrUInt32(uint32_t *obj, uint32_t value);

uint64_t CereggiiAtomic_OrUInt64(uint64_t *obj, uint64_t value);

uintptr_t CereggiiAtomic_OrUIntPtr(uintptr_t *obj, uintptr_t value);


// --- CereggiiAtomic_load -------------------------------------------------------
// Atomically loads `*obj` (sequential consistency)

int CereggiiAtomic_LoadInt(const int *obj);

int8_t CereggiiAtomic_LoadInt8(const int8_t *obj);

int16_t CereggiiAtomic_LoadInt16(const int16_t *obj);

int32_t CereggiiAtomic_LoadInt32(const int32_t *obj);

int64_t CereggiiAtomic_LoadInt64(const int64_t *obj);

intptr_t CereggiiAtomic_LoadIntPtr(const intptr_t *obj);

uint8_t CereggiiAtomic_LoadUInt8(const uint8_t *obj);

uint16_t CereggiiAtomic_LoadUInt16(const uint16_t *obj);

uint32_t CereggiiAtomic_LoadUInt32(const uint32_t *obj);

uint64_t CereggiiAtomic_LoadUInt64(const uint64_t *obj);

uintptr_t CereggiiAtomic_LoadUIntPtr(const uintptr_t *obj);

unsigned int CereggiiAtomic_LoadUInt(const unsigned int *obj);

Py_ssize_t CereggiiAtomic_LoadSsize(const Py_ssize_t *obj);

void *CereggiiAtomic_LoadPtr(const void **obj);


// --- CereggiiAtomic_LoadRelaxed -----------------------------------------------
// Loads `*obj` (relaxed consistency, i.e., no ordering)

int CereggiiAtomic_LoadIntRelaxed(const int *obj);

int8_t CereggiiAtomic_LoadInt8Relaxed(const int8_t *obj);

int16_t CereggiiAtomic_LoadInt16Relaxed(const int16_t *obj);

int32_t CereggiiAtomic_LoadInt32Relaxed(const int32_t *obj);

int64_t CereggiiAtomic_LoadInt64Relaxed(const int64_t *obj);

intptr_t CereggiiAtomic_LoadIntptrRelaxed(const intptr_t *obj);

uint8_t CereggiiAtomic_LoadUInt8Relaxed(const uint8_t *obj);

uint16_t CereggiiAtomic_LoadUInt16Relaxed(const uint16_t *obj);

uint32_t CereggiiAtomic_LoadUInt32Relaxed(const uint32_t *obj);

uint64_t CereggiiAtomic_LoadUInt64Relaxed(const uint64_t *obj);

uintptr_t CereggiiAtomic_LoadUIntptrRelaxed(const uintptr_t *obj);

unsigned int CereggiiAtomic_LoadUIntRelaxed(const unsigned int *obj);

Py_ssize_t CereggiiAtomic_LoadSsizeRelaxed(const Py_ssize_t *obj);

void *CereggiiAtomic_LoadPtrRelaxed(const void *obj);


// --- CereggiiAtomic_store ------------------------------------------------------
// Atomically performs `*obj = value` (sequential consistency)

void CereggiiAtomic_StoreInt(int *obj, int value);

void CereggiiAtomic_StoreInt8(int8_t *obj, int8_t value);

void CereggiiAtomic_StoreInt16(int16_t *obj, int16_t value);

void CereggiiAtomic_StoreInt32(int32_t *obj, int32_t value);

void CereggiiAtomic_StoreInt64(int64_t *obj, int64_t value);

void CereggiiAtomic_StoreIntPtr(intptr_t *obj, intptr_t value);

void CereggiiAtomic_StoreUInt8(uint8_t *obj, uint8_t value);

void CereggiiAtomic_StoreUInt16(uint16_t *obj, uint16_t value);

void CereggiiAtomic_StoreUInt32(uint32_t *obj, uint32_t value);

void CereggiiAtomic_StoreUInt64(uint64_t *obj, uint64_t value);

void CereggiiAtomic_StoreUIntPtr(uintptr_t *obj, uintptr_t value);

void CereggiiAtomic_StoreUInt(unsigned int *obj, unsigned int value);

void CereggiiAtomic_StorePtr(void **obj, void *value);

void CereggiiAtomic_StoreSsize(Py_ssize_t *obj, Py_ssize_t value);


// --- CereggiiAtomic_storeRelaxed ----------------------------------------------
// Stores `*obj = value` (relaxed consistency, i.e., no ordering)

void CereggiiAtomic_StoreIntRelaxed(int *obj, int value);

void CereggiiAtomic_StoreInt8Relaxed(int8_t *obj, int8_t value);

void CereggiiAtomic_StoreInt16Relaxed(int16_t *obj, int16_t value);

void CereggiiAtomic_StoreInt32Relaxed(int32_t *obj, int32_t value);

void CereggiiAtomic_StoreInt64Relaxed(int64_t *obj, int64_t value);

void CereggiiAtomic_StoreIntptrRelaxed(intptr_t *obj, intptr_t value);

void CereggiiAtomic_StoreUInt8Relaxed(uint8_t *obj, uint8_t value);

void CereggiiAtomic_StoreUInt16Relaxed(uint16_t *obj, uint16_t value);

void CereggiiAtomic_StoreUInt32Relaxed(uint32_t *obj, uint32_t value);

void CereggiiAtomic_StoreUInt64Relaxed(uint64_t *obj, uint64_t value);

void CereggiiAtomic_StoreUIntptrRelaxed(uintptr_t *obj, uintptr_t value);

void CereggiiAtomic_StoreUIntRelaxed(unsigned int *obj, unsigned int value);

void CereggiiAtomic_StorePtrRelaxed(void **obj, void *value);

void CereggiiAtomic_StoreSsizeRelaxed(Py_ssize_t *obj, Py_ssize_t value);


// --- CereggiiAtomic_load_ptr_acquire / CereggiiAtomic_store_ptr_release ------------

// Loads `*obj` (acquire operation)
void *CereggiiAtomic_LoadPtrAcquire(const void **obj);

// Stores `*obj = value` (release operation)
void CereggiiAtomic_StorePtrRelease(void **obj, void *value);

void CereggiiAtomic_StoreIntRelease(int *obj, int value);

int CereggiiAtomic_LoadIntAcquire(const int *obj);


// --- CereggiiAtomic_fence ------------------------------------------------------

// Sequential consistency fence. C11 fences have complex semantics. When
// possible, use the atomic operations on variables defined above, which
// generally do not require explicit use of a fence.
// See https://en.cppreference.com/w/cpp/atomic/atomic_thread_fence
void CereggiiAtomic_FenceSeqCst(void);

// Release fence
void CereggiiAtomic_FenceRelease(void);


// --- aliases ---------------------------------------------------------------

#if SIZEOF_LONG == 8
# define CereggiiAtomic_LoadULong(p) \
    CereggiiAtomic_LoadUInt64((uint64_t *)p)
# define CereggiiAtomic_LoadULongRelaxed(p) \
    CereggiiAtomic_LoadUInt64Relaxed((uint64_t *)p)
# define CereggiiAtomic_StoreULong(p, v) \
    CereggiiAtomic_StoreUInt64((uint64_t *)p, v)
# define CereggiiAtomic_StoreULongRelaxed(p, v) \
    CereggiiAtomic_StoreUInt64Relaxed((uint64_t *)p, v)
#elif SIZEOF_LONG == 4
# define CereggiiAtomic_LoadULong(p) \
    CereggiiAtomic_LoadUInt32((uint32_t *)p)
# define CereggiiAtomic_LoadULongRelaxed(p) \
    CereggiiAtomic_LoadUInt32Relaxed((uint32_t *)p)
# define CereggiiAtomic_StoreULong(p, v) \
    CereggiiAtomic_StoreUInt32((uint32_t *)p, v)
# define CereggiiAtomic_StoreULongRelaxed(p, v) \
    CereggiiAtomic_StoreUInt32Relaxed((uint32_t *)p, v)
#else
# error "long must be 4 or 8 bytes in size"
#endif  // SIZEOF_LONG

#endif //CEREGGII_ATOMIC_OPS_H
