// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include <intrin.h>
#include <assert.h>
#include "cereggii/internal/atomics_declarations.h"


#define ASSERT_CAN_CAST_OBJ_TO(TYPE) \
    static_assert(sizeof(*obj) == sizeof(TYPE), "no suitable atomics implementation")


// ----------------------------------------------------------------------------
// add: atomically fetch and add value to obj

inline int8_t
CereggiiAtomic_AddInt8(int8_t *obj, int8_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(char);
    return (int8_t)_InterlockedExchangeAdd8((volatile char *)obj, (char)value);
}

inline int16_t
CereggiiAtomic_AddInt16(int16_t *obj, int16_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(short);
    return (int16_t)_InterlockedExchangeAdd16((volatile short *)obj, (short)value);
}

inline int32_t
CereggiiAtomic_AddInt32(int32_t *obj, int32_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(long);
    return (int32_t)_InterlockedExchangeAdd((volatile long *)obj, (long)value);
}

inline int64_t
CereggiiAtomic_AddInt64(int64_t *obj, int64_t value)
{
#if defined(_M_X64) || defined(_M_ARM64)
    ASSERT_CAN_CAST_OBJ_TO(__int64);
    return (int64_t)_InterlockedExchangeAdd64((volatile __int64 *)obj, (__int64)value);
#else
    int64_t old_value = CereggiiAtomic_LoadInt64Relaxed(obj);
    for (;;) {
        int64_t new_value = old_value + value;
        if (CereggiiAtomic_CompareExchangeInt64(obj, old_value, new_value)) {
            return old_value;
        }
    }
#endif
}


inline uint8_t
CereggiiAtomic_AddUInt8(uint8_t *obj, uint8_t value)
{
    return (uint8_t)CereggiiAtomic_AddInt8((int8_t *)obj, (int8_t)value);
}

inline uint16_t
CereggiiAtomic_AddUInt16(uint16_t *obj, uint16_t value)
{
    return (uint16_t)CereggiiAtomic_AddInt16((int16_t *)obj, (int16_t)value);
}

inline uint32_t
CereggiiAtomic_AddUInt32(uint32_t *obj, uint32_t value)
{
    return (uint32_t)CereggiiAtomic_AddInt32((int32_t *)obj, (int32_t)value);
}

inline int
CereggiiAtomic_AddInt(int *obj, int value)
{
    ASSERT_CAN_CAST_OBJ_TO(int32_t);
    return (int)CereggiiAtomic_AddInt32((int32_t *)obj, (int32_t)value);
}

inline unsigned int
CereggiiAtomic_AddUInt(unsigned int *obj, unsigned int value)
{
    ASSERT_CAN_CAST_OBJ_TO(int32_t);
    return (unsigned int)CereggiiAtomic_AddInt32((int32_t *)obj, (int32_t)value);
}

inline uint64_t
CereggiiAtomic_AddUInt64(uint64_t *obj, uint64_t value)
{
    return (uint64_t)CereggiiAtomic_AddInt64((int64_t *)obj, (int64_t)value);
}

inline intptr_t
CereggiiAtomic_AddIntPtr(intptr_t *obj, intptr_t value)
{
#if SIZEOF_VOID_P == 8
    ASSERT_CAN_CAST_OBJ_TO(int64_t);
    return (intptr_t)CereggiiAtomic_AddInt64((int64_t *)obj, (int64_t)value);
#else
    ASSERT_CAN_CAST_OBJ_TO(int32_t);
    return (intptr_t)CereggiiAtomic_AddInt32((int32_t *)obj, (int32_t)value);
#endif
}

inline uintptr_t
CereggiiAtomic_AddUIntPtr(uintptr_t *obj, uintptr_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(intptr_t);
    return (uintptr_t)CereggiiAtomic_AddIntPtr((intptr_t *)obj, (intptr_t)value);
}

inline Py_ssize_t
CereggiiAtomic_AddSsize(Py_ssize_t *obj, Py_ssize_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(intptr_t);
    return (Py_ssize_t)CereggiiAtomic_AddIntPtr((intptr_t *)obj, (intptr_t)value);
}


// ----------------------------------------------------------------------------
// compare exchange:
//    1. atomically read the value of obj
//    2. if it is expected, then update it with desired and return 1;
//    3. otherwise, don't change it and return 0

inline int
CereggiiAtomic_CompareExchangeInt8(int8_t *obj, int8_t expected, int8_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(char);
    int8_t initial = (int8_t)_InterlockedCompareExchange8(
                                       (volatile char *)obj,
                                       (char)value,
                                       (char)expected);
    return initial == expected;
}

inline int
CereggiiAtomic_CompareExchangeInt16(int16_t *obj, int16_t expected, int16_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(short);
    int16_t initial = (int16_t)_InterlockedCompareExchange16(
                                       (volatile short *)obj,
                                       (short)value,
                                       (short)expected);
    return initial == expected;
}

inline int
CereggiiAtomic_CompareExchangeInt32(int32_t *obj, int32_t expected, int32_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(long);
    int32_t initial = (int32_t)_InterlockedCompareExchange(
                                       (volatile long *)obj,
                                       (long)value,
                                       (long)expected);
    return initial == expected;
}

inline int
CereggiiAtomic_CompareExchangeInt64(int64_t *obj, int64_t expected, int64_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(__int64);
    int64_t initial = (int64_t)_InterlockedCompareExchange64(
                                       (volatile __int64 *)obj,
                                       (__int64)value,
                                       (__int64)expected);
    return initial == expected;
}

inline int
CereggiiAtomic_CompareExchangePtr(void **obj, void *expected, void *value)
{
    void *initial = _InterlockedCompareExchangePointer(
                                       obj,
                                       value,
                                       expected);
    return initial == expected;
}


inline int
CereggiiAtomic_CompareExchangeUInt8(uint8_t *obj, uint8_t expected, uint8_t value)
{
    return CereggiiAtomic_CompareExchangeInt8((int8_t *)obj,
                                              (int8_t)expected,
                                              (int8_t)value);
}

inline int
CereggiiAtomic_CompareExchangeUInt16(uint16_t *obj, uint16_t expected, uint16_t value)
{
    return CereggiiAtomic_CompareExchangeInt16((int16_t *)obj,
                                               (int16_t)expected,
                                               (int16_t)value);
}

inline int
CereggiiAtomic_CompareExchangeUInt32(uint32_t *obj, uint32_t expected, uint32_t value)
{
    return CereggiiAtomic_CompareExchangeInt32((int32_t *)obj,
                                               (int32_t)expected,
                                               (int32_t)value);
}

inline int
CereggiiAtomic_CompareExchangeInt(int *obj, int expected, int value)
{
    return CereggiiAtomic_CompareExchangeInt32((int32_t *)obj,
                                               (int32_t)expected,
                                               (int32_t)value);
}

inline int
CereggiiAtomic_CompareExchangeUInt(unsigned int *obj, unsigned int expected, unsigned int value)
{
    ASSERT_CAN_CAST_OBJ_TO(int32_t);
    return CereggiiAtomic_CompareExchangeInt32((int32_t *)obj,
                                               (int32_t)expected,
                                               (int32_t)value);
}

inline int
CereggiiAtomic_CompareExchangeUInt64(uint64_t *obj, uint64_t expected, uint64_t value)
{
    return CereggiiAtomic_CompareExchangeInt64((int64_t *)obj,
                                               (int64_t)expected,
                                               (int64_t)value);
}

inline int
CereggiiAtomic_CompareExchangeIntPtr(intptr_t *obj, intptr_t expected, intptr_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(void*);
    return CereggiiAtomic_CompareExchangePtr((void**)obj,
                                             (void*)expected,
                                             (void*)value);
}

inline int
CereggiiAtomic_CompareExchangeUIntPtr(uintptr_t *obj, uintptr_t expected, uintptr_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(void*);
    return CereggiiAtomic_CompareExchangePtr((void**)obj,
                                             (void*)expected,
                                             (void*)value);
}

inline int
CereggiiAtomic_CompareExchangeSsize(Py_ssize_t *obj, Py_ssize_t expected, Py_ssize_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(void*);
    return CereggiiAtomic_CompareExchangePtr((void**)obj,
                                             (void*)expected,
                                             (void*)value);
}


// --- _Py_atomic_exchange ---------------------------------------------------

inline int8_t
CereggiiAtomic_ExchangeInt8(int8_t *obj, int8_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(char);
    return (int8_t)_InterlockedExchange8((volatile char *)obj, (char)value);
}

inline int16_t
CereggiiAtomic_ExchangeInt16(int16_t *obj, int16_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(short);
    return (int16_t)_InterlockedExchange16((volatile short *)obj, (short)value);
}

inline int32_t
CereggiiAtomic_ExchangeInt32(int32_t *obj, int32_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(long);
    return (int32_t)_InterlockedExchange((volatile long *)obj, (long)value);
}

inline int64_t
CereggiiAtomic_ExchangeInt64(int64_t *obj, int64_t value)
{
#if defined(_M_X64) || defined(_M_ARM64)
    ASSERT_CAN_CAST_OBJ_TO(__int64);
    return (int64_t)_InterlockedExchange64((volatile __int64 *)obj, (__int64)value);
#else
    int64_t old_value = CereggiiAtomic_LoadInt64Relaxed(obj);
    for (;;) {
        if (CereggiiAtomic_CompareExchangeInt64(obj, old_value, value)) {
            return old_value;
        }
    }
#endif
}

inline void*
CereggiiAtomic_ExchangePtr(void **obj, void *value)
{
    return (void*)_InterlockedExchangePointer((void * volatile *)obj, (void *)value);
}


inline uint8_t
CereggiiAtomic_ExchangeUInt8(uint8_t *obj, uint8_t value)
{
    return (uint8_t)CereggiiAtomic_ExchangeInt8((int8_t *)obj,
                                                (int8_t)value);
}

inline uint16_t
CereggiiAtomic_ExchangeUInt16(uint16_t *obj, uint16_t value)
{
    return (uint16_t)CereggiiAtomic_ExchangeInt16((int16_t *)obj,
                                                  (int16_t)value);
}

inline uint32_t
CereggiiAtomic_ExchangeUInt32(uint32_t *obj, uint32_t value)
{
    return (uint32_t)CereggiiAtomic_ExchangeInt32((int32_t *)obj,
                                                  (int32_t)value);
}

inline int
CereggiiAtomic_ExchangeInt(int *obj, int value)
{
    ASSERT_CAN_CAST_OBJ_TO(int32_t);
    return (int)CereggiiAtomic_ExchangeInt32((int32_t *)obj,
                                             (int32_t)value);
}

inline unsigned int
CereggiiAtomic_ExchangeUInt(unsigned int *obj, unsigned int value)
{
    ASSERT_CAN_CAST_OBJ_TO(int32_t);
    return (unsigned int)CereggiiAtomic_ExchangeInt32((int32_t *)obj,
                                                      (int32_t)value);
}

inline uint64_t
CereggiiAtomic_ExchangeUInt64(uint64_t *obj, uint64_t value)
{
    return (uint64_t)CereggiiAtomic_ExchangeInt64((int64_t *)obj,
                                                  (int64_t)value);
}

inline intptr_t
CereggiiAtomic_ExchangeIntPtr(intptr_t *obj, intptr_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(void*);
    return (intptr_t)CereggiiAtomic_ExchangePtr((void**)obj,
                                                (void*)value);
}

inline uintptr_t
CereggiiAtomic_ExchangeUIntPtr(uintptr_t *obj, uintptr_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(void*);
    return (uintptr_t)CereggiiAtomic_ExchangePtr((void**)obj,
                                                 (void*)value);
}

inline Py_ssize_t
CereggiiAtomic_ExchangeSsize(Py_ssize_t *obj, Py_ssize_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(void*);
    return (Py_ssize_t)CereggiiAtomic_ExchangePtr((void**)obj,
                                                  (void*)value);
}


// --- _Py_atomic_and --------------------------------------------------------

inline uint8_t
CereggiiAtomic_AndUInt8(uint8_t *obj, uint8_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(char);
    return (uint8_t)_InterlockedAnd8((volatile char *)obj, (char)value);
}

inline uint16_t
CereggiiAtomic_AndUInt16(uint16_t *obj, uint16_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(short);
    return (uint16_t)_InterlockedAnd16((volatile short *)obj, (short)value);
}

inline uint32_t
CereggiiAtomic_AndUInt32(uint32_t *obj, uint32_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(long);
    return (uint32_t)_InterlockedAnd((volatile long *)obj, (long)value);
}

inline uint64_t
CereggiiAtomic_AndUInt64(uint64_t *obj, uint64_t value)
{
#if defined(_M_X64) || defined(_M_ARM64)
    ASSERT_CAN_CAST_OBJ_TO(__int64);
    return (uint64_t)_InterlockedAnd64((volatile __int64 *)obj, (__int64)value);
#else
    uint64_t old_value = CereggiiAtomic_LoadUInt64Relaxed(obj);
    for (;;) {
        uint64_t new_value = old_value & value;
        if (CereggiiAtomic_CompareExchangeUInt64(obj, &old_value, new_value)) {
            return old_value;
        }
    }
#endif
}

inline uintptr_t
CereggiiAtomic_AndUIntPtr(uintptr_t *obj, uintptr_t value)
{
#if SIZEOF_VOID_P == 8
    ASSERT_CAN_CAST_OBJ_TO(uint64_t);
    return (uintptr_t)CereggiiAtomic_AndUInt64((uint64_t *)obj,
                                               (uint64_t)value);
#else
    ASSERT_CAN_CAST_OBJ_TO(uint32_t);
    return (uintptr_t)CereggiiAtomic_AndUInt32((uint32_t *)obj,
                                               (uint32_t)value);
#endif
}


// --- _Py_atomic_or ---------------------------------------------------------

inline uint8_t
CereggiiAtomic_OrUInt8(uint8_t *obj, uint8_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(char);
    return (uint8_t)_InterlockedOr8((volatile char *)obj, (char)value);
}

inline uint16_t
CereggiiAtomic_OrUInt16(uint16_t *obj, uint16_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(short);
    return (uint16_t)_InterlockedOr16((volatile short *)obj, (short)value);
}

inline uint32_t
CereggiiAtomic_OrUInt32(uint32_t *obj, uint32_t value)
{
    ASSERT_CAN_CAST_OBJ_TO(long);
    return (uint32_t)_InterlockedOr((volatile long *)obj, (long)value);
}

inline uint64_t
CereggiiAtomic_OrUInt64(uint64_t *obj, uint64_t value)
{
#if defined(_M_X64) || defined(_M_ARM64)
    ASSERT_CAN_CAST_OBJ_TO(__int64);
    return (uint64_t)_InterlockedOr64((volatile __int64 *)obj, (__int64)value);
#else
    uint64_t old_value = CereggiiAtomic_LoadUInt64Relaxed(obj);
    for (;;) {
        uint64_t new_value = old_value | value;
        if (CereggiiAtomic_CompareExchangeUInt64(obj, &old_value, new_value)) {
            return old_value;
        }
    }
#endif
}


inline uintptr_t
CereggiiAtomic_OrUIntPtr(uintptr_t *obj, uintptr_t value)
{
#if SIZEOF_VOID_P == 8
    ASSERT_CAN_CAST_OBJ_TO(uint64_t);
    return (uintptr_t)CereggiiAtomic_OrUInt64((uint64_t *)obj,
                                              (uint64_t)value);
#else
    ASSERT_CAN_CAST_OBJ_TO(uint32_t);
    return (uintptr_t)CereggiiAtomic_OrUInt32((uint32_t *)obj,
                                              (uint32_t)value);
#endif
}


// --- _Py_atomic_load -------------------------------------------------------

inline uint8_t
CereggiiAtomic_LoadUInt8(const uint8_t *obj)
{
#if defined(_M_X64) || defined(_M_IX86)
    return *(volatile uint8_t *)obj;
#elif defined(_M_ARM64)
    return (uint8_t)__ldar8((unsigned __int8 volatile *)obj);
#else
#  error "no implementation of CereggiiAtomic_LoadUInt8"
#endif
}

inline uint16_t
CereggiiAtomic_LoadUInt16(const uint16_t *obj)
{
#if defined(_M_X64) || defined(_M_IX86)
    return *(volatile uint16_t *)obj;
#elif defined(_M_ARM64)
    return (uint16_t)__ldar16((unsigned __int16 volatile *)obj);
#else
#  error "no implementation of CereggiiAtomic_LoadUInt16"
#endif
}

inline uint32_t
CereggiiAtomic_LoadUInt32(const uint32_t *obj)
{
#if defined(_M_X64) || defined(_M_IX86)
    return *(volatile uint32_t *)obj;
#elif defined(_M_ARM64)
    return (uint32_t)__ldar32((unsigned __int32 volatile *)obj);
#else
#  error "no implementation of CereggiiAtomic_LoadUInt32"
#endif
}

inline uint64_t
CereggiiAtomic_LoadUInt64(const uint64_t *obj)
{
#if defined(_M_X64) || defined(_M_IX86)
    return *(volatile uint64_t *)obj;
#elif defined(_M_ARM64)
    return (uint64_t)__ldar64((unsigned __int64 volatile *)obj);
#else
#  error "no implementation of CereggiiAtomic_LoadUInt64"
#endif
}

inline int8_t
CereggiiAtomic_LoadInt8(const int8_t *obj)
{
    return (int8_t)CereggiiAtomic_LoadUInt8((const uint8_t *)obj);
}

inline int16_t
CereggiiAtomic_LoadInt16(const int16_t *obj)
{
    return (int16_t)CereggiiAtomic_LoadUInt16((const uint16_t *)obj);
}

inline int32_t
CereggiiAtomic_LoadInt32(const int32_t *obj)
{
    return (int32_t)CereggiiAtomic_LoadUInt32((const uint32_t *)obj);
}

inline int
CereggiiAtomic_LoadInt(const int *obj)
{
    ASSERT_CAN_CAST_OBJ_TO(uint32_t);
    return (int)CereggiiAtomic_LoadUInt32((uint32_t *)obj);
}

inline unsigned int
CereggiiAtomic_LoadUInt(const unsigned int *obj)
{
    ASSERT_CAN_CAST_OBJ_TO(uint32_t);
    return (unsigned int)CereggiiAtomic_LoadUInt32((uint32_t *)obj);
}

inline int64_t
CereggiiAtomic_LoadInt64(const int64_t *obj)
{
    return (int64_t)CereggiiAtomic_LoadUInt64((const uint64_t *)obj);
}

inline void*
CereggiiAtomic_LoadPtr(const void **obj)
{
#if SIZEOF_VOID_P == 8
    return (void*)CereggiiAtomic_LoadUInt64((const uint64_t *)obj);
#else
    return (void*)CereggiiAtomic_LoadUInt32((const uint32_t *)obj);
#endif
}

inline intptr_t
CereggiiAtomic_LoadIntPtr(const intptr_t *obj)
{
    ASSERT_CAN_CAST_OBJ_TO(void*);
    return (intptr_t)CereggiiAtomic_LoadPtr((void*)obj);
}

inline uintptr_t
CereggiiAtomic_LoadUIntPtr(const uintptr_t *obj)
{
    ASSERT_CAN_CAST_OBJ_TO(void*);
    return (uintptr_t)CereggiiAtomic_LoadPtr((void*)obj);
}

inline Py_ssize_t
CereggiiAtomic_LoadSsize(const Py_ssize_t *obj)
{
    ASSERT_CAN_CAST_OBJ_TO(void*);
    return (Py_ssize_t)CereggiiAtomic_LoadPtr((void*)obj);
}


// --- _Py_atomic_load_relaxed -----------------------------------------------

inline int
CereggiiAtomic_LoadIntRelaxed(const int *obj)
{
    return *(volatile int *)obj;
}

inline char
CereggiiAtomic_LoadCharRelaxed(const char *obj)
{
    return *(volatile char *)obj;
}

inline unsigned char
CereggiiAtomic_LoadUCharRelaxed(const unsigned char *obj)
{
    return *(volatile unsigned char *)obj;
}

inline short
CereggiiAtomic_LoadShortRelaxed(const short *obj)
{
    return *(volatile short *)obj;
}

inline unsigned short
CereggiiAtomic_LoadUShortRelaxed(const unsigned short *obj)
{
    return *(volatile unsigned short *)obj;
}

inline long
CereggiiAtomic_LoadLongRelaxed(const long *obj)
{
    return *(volatile long *)obj;
}

inline float
CereggiiAtomic_LoadFloatRelaxed(const float *obj)
{
    return *(volatile float *)obj;
}

inline double
CereggiiAtomic_LoadDoubleRelaxed(const double *obj)
{
    return *(volatile double *)obj;
}

inline int8_t
CereggiiAtomic_LoadInt8Relaxed(const int8_t *obj)
{
    return *(volatile int8_t *)obj;
}

inline int16_t
CereggiiAtomic_LoadInt16Relaxed(const int16_t *obj)
{
    return *(volatile int16_t *)obj;
}

inline int32_t
CereggiiAtomic_LoadInt32Relaxed(const int32_t *obj)
{
    return *(volatile int32_t *)obj;
}

inline int64_t
CereggiiAtomic_LoadInt64Relaxed(const int64_t *obj)
{
    return *(volatile int64_t *)obj;
}

inline intptr_t
CereggiiAtomic_LoadIntPtrRelaxed(const intptr_t *obj)
{
    return *(volatile intptr_t *)obj;
}

inline uint8_t
CereggiiAtomic_LoadUInt8Relaxed(const uint8_t *obj)
{
    return *(volatile uint8_t *)obj;
}

inline uint16_t
CereggiiAtomic_LoadUInt16Relaxed(const uint16_t *obj)
{
    return *(volatile uint16_t *)obj;
}

inline uint32_t
CereggiiAtomic_LoadUInt32Relaxed(const uint32_t *obj)
{
    return *(volatile uint32_t *)obj;
}

inline uint64_t
CereggiiAtomic_LoadUInt64Relaxed(const uint64_t *obj)
{
    return *(volatile uint64_t *)obj;
}

inline uintptr_t
CereggiiAtomic_LoadUIntPtrRelaxed(const uintptr_t *obj)
{
    return *(volatile uintptr_t *)obj;
}

inline unsigned int
CereggiiAtomic_LoadUIntRelaxed(const unsigned int *obj)
{
    return *(volatile unsigned int *)obj;
}

inline Py_ssize_t
CereggiiAtomic_LoadSsizeRelaxed(const Py_ssize_t *obj)
{
    return *(volatile Py_ssize_t *)obj;
}

inline void*
CereggiiAtomic_LoadPtrRelaxed(const void **obj)
{
    return *(void * volatile *)obj;
}

inline unsigned long long
CereggiiAtomic_LoadULLongRelaxed(const unsigned long long *obj)
{
    return *(volatile unsigned long long *)obj;
}

inline long long
CereggiiAtomic_LoadLLongRelaxed(const long long *obj)
{
    return *(volatile long long *)obj;
}


// --- _Py_atomic_store ------------------------------------------------------

inline void
CereggiiAtomic_StoreInt(int *obj, int value)
{
    (void)CereggiiAtomic_ExchangeInt(obj, value);
}

inline void
CereggiiAtomic_StoreInt8(int8_t *obj, int8_t value)
{
    (void)CereggiiAtomic_ExchangeInt8(obj, value);
}

inline void
CereggiiAtomic_StoreInt16(int16_t *obj, int16_t value)
{
    (void)CereggiiAtomic_ExchangeInt16(obj, value);
}

inline void
CereggiiAtomic_StoreInt32(int32_t *obj, int32_t value)
{
    (void)CereggiiAtomic_ExchangeInt32(obj, value);
}

inline void
CereggiiAtomic_StoreInt64(int64_t *obj, int64_t value)
{
    (void)CereggiiAtomic_ExchangeInt64(obj, value);
}

inline void
CereggiiAtomic_StoreIntptr(intptr_t *obj, intptr_t value)
{
    (void)CereggiiAtomic_ExchangeIntPtr(obj, value);
}

inline void
CereggiiAtomic_StoreUInt8(uint8_t *obj, uint8_t value)
{
    (void)CereggiiAtomic_ExchangeUInt8(obj, value);
}

inline void
CereggiiAtomic_StoreUInt16(uint16_t *obj, uint16_t value)
{
    (void)CereggiiAtomic_ExchangeUInt16(obj, value);
}

inline void
CereggiiAtomic_StoreUInt32(uint32_t *obj, uint32_t value)
{
    (void)CereggiiAtomic_ExchangeUInt32(obj, value);
}

inline void
CereggiiAtomic_StoreUInt64(uint64_t *obj, uint64_t value)
{
    (void)CereggiiAtomic_ExchangeUInt64(obj, value);
}

inline void
CereggiiAtomic_StoreUIntptr(uintptr_t *obj, uintptr_t value)
{
    (void)CereggiiAtomic_ExchangeUIntPtr(obj, value);
}

inline void
CereggiiAtomic_StoreUInt(unsigned int *obj, unsigned int value)
{
    (void)CereggiiAtomic_ExchangeUInt(obj, value);
}

inline void
CereggiiAtomic_StorePtr(void **obj, void *value)
{
    (void)CereggiiAtomic_ExchangePtr(obj, value);
}

inline void
CereggiiAtomic_StoreSsize(Py_ssize_t *obj, Py_ssize_t value)
{
    (void)CereggiiAtomic_ExchangeSsize(obj, value);
}


// --- _Py_atomic_store_relaxed ----------------------------------------------

inline void
CereggiiAtomic_StoreIntRelaxed(int *obj, int value)
{
    *(volatile int *)obj = value;
}

inline void
CereggiiAtomic_StoreInt8Relaxed(int8_t *obj, int8_t value)
{
    *(volatile int8_t *)obj = value;
}

inline void
CereggiiAtomic_StoreInt16Relaxed(int16_t *obj, int16_t value)
{
    *(volatile int16_t *)obj = value;
}

inline void
CereggiiAtomic_StoreInt32Relaxed(int32_t *obj, int32_t value)
{
    *(volatile int32_t *)obj = value;
}

inline void
CereggiiAtomic_StoreInt64Relaxed(int64_t *obj, int64_t value)
{
    *(volatile int64_t *)obj = value;
}

inline void
CereggiiAtomic_StoreIntptrRelaxed(intptr_t *obj, intptr_t value)
{
    *(volatile intptr_t *)obj = value;
}

inline void
CereggiiAtomic_StoreUInt8Relaxed(uint8_t *obj, uint8_t value)
{
    *(volatile uint8_t *)obj = value;
}

inline void
CereggiiAtomic_StoreUInt16Relaxed(uint16_t *obj, uint16_t value)
{
    *(volatile uint16_t *)obj = value;
}

inline void
CereggiiAtomic_StoreUInt32Relaxed(uint32_t *obj, uint32_t value)
{
    *(volatile uint32_t *)obj = value;
}

inline void
CereggiiAtomic_StoreUInt64Relaxed(uint64_t *obj, uint64_t value)
{
    *(volatile uint64_t *)obj = value;
}

inline void
CereggiiAtomic_StoreUIntptrRelaxed(uintptr_t *obj, uintptr_t value)
{
    *(volatile uintptr_t *)obj = value;
}

inline void
CereggiiAtomic_StoreUIntRelaxed(unsigned int *obj, unsigned int value)
{
    *(volatile unsigned int *)obj = value;
}

inline void
CereggiiAtomic_StorePtrRelaxed(void **obj, void* value)
{
    *(void * volatile *)obj = value;
}

inline void
CereggiiAtomic_StoreSsizeRelaxed(Py_ssize_t *obj, Py_ssize_t value)
{
    *(volatile Py_ssize_t *)obj = value;
}

inline void
CereggiiAtomic_StoreUllongRelaxed(unsigned long long *obj, unsigned long long value)
{
    *(volatile unsigned long long *)obj = value;
}

inline void
CereggiiAtomic_StoreCharRelaxed(char *obj, char value)
{
    *(volatile char *)obj = value;
}

inline void
CereggiiAtomic_StoreUcharRelaxed(unsigned char *obj, unsigned char value)
{
    *(volatile unsigned char *)obj = value;
}

inline void
CereggiiAtomic_StoreShortRelaxed(short *obj, short value)
{
    *(volatile short *)obj = value;
}

inline void
CereggiiAtomic_StoreUshortRelaxed(unsigned short *obj, unsigned short value)
{
    *(volatile unsigned short *)obj = value;
}

inline void
CereggiiAtomic_StoreUIntRelease(unsigned int *obj, unsigned int value)
{
    *(volatile unsigned int *)obj = value;
}

inline void
CereggiiAtomic_StoreLongRelaxed(long *obj, long value)
{
    *(volatile long *)obj = value;
}

inline void
CereggiiAtomic_StoreFloatRelaxed(float *obj, float value)
{
    *(volatile float *)obj = value;
}

inline void
CereggiiAtomic_StoreDoubleRelaxed(double *obj, double value)
{
    *(volatile double *)obj = value;
}

inline void
CereggiiAtomic_StoreLlongRelaxed(long long *obj, long long value)
{
    *(volatile long long *)obj = value;
}


// --- _Py_atomic_load_ptr_acquire / _Py_atomic_store_ptr_release ------------

inline void *
CereggiiAtomic_LoadPtrAcquire(const void **obj)
{
#if defined(_M_X64) || defined(_M_IX86)
    return *(void * volatile *)obj;
#elif defined(_M_ARM64)
    return (void *)__ldar64((unsigned __int64 volatile *)obj);
#else
#  error "no implementation of CereggiiAtomic_LoadPtrAcquire"
#endif
}

inline uintptr_t
CereggiiAtomic_LoadUIntPtrAcquire(const uintptr_t *obj)
{
#if defined(_M_X64) || defined(_M_IX86)
    return *(uintptr_t volatile *)obj;
#elif defined(_M_ARM64)
    return (uintptr_t)__ldar64((unsigned __int64 volatile *)obj);
#else
#  error "no implementation of CereggiiAtomic_LoadUIntPtrAcquire"
#endif
}

inline void
CereggiiAtomic_StorePtrRelease(void **obj, void *value)
{
#if defined(_M_X64) || defined(_M_IX86)
    *(void * volatile *)obj = value;
#elif defined(_M_ARM64)
    __stlr64((unsigned __int64 volatile *)obj, (uintptr_t)value);
#else
#  error "no implementation of CereggiiAtomic_StorePtrRelease"
#endif
}

inline void
CereggiiAtomic_StoreUIntPtrRelease(uintptr_t *obj, uintptr_t value)
{
#if defined(_M_X64) || defined(_M_IX86)
    *(uintptr_t volatile *)obj = value;
#elif defined(_M_ARM64)
    ASSERT_CAN_CAST_OBJ_TO(unsigned __int64);
    __stlr64((unsigned __int64 volatile *)obj, (unsigned __int64)value);
#else
#  error "no implementation of CereggiiAtomic_StoreUIntPtrRelease"
#endif
}

inline void
CereggiiAtomic_StoreIntRelease(int *obj, int value)
{
#if defined(_M_X64) || defined(_M_IX86)
    *(int volatile *)obj = value;
#elif defined(_M_ARM64)
    ASSERT_CAN_CAST_OBJ_TO(unsigned __int32);
    __stlr32((unsigned __int32 volatile *)obj, (unsigned __int32)value);
#else
#  error "no implementation of CereggiiAtomic_StoreIntRelease"
#endif
}

inline void
CereggiiAtomic_StoreSsizeRelease(Py_ssize_t *obj, Py_ssize_t value)
{
#if defined(_M_X64) || defined(_M_IX86)
    *(Py_ssize_t volatile *)obj = value;
#elif defined(_M_ARM64)
    __stlr64((unsigned __int64 volatile *)obj, (unsigned __int64)value);
#else
#  error "no implementation of CereggiiAtomic_StoreSsizeRelease"
#endif
}

inline int
CereggiiAtomic_LoadIntAcquire(const int *obj)
{
#if defined(_M_X64) || defined(_M_IX86)
    return *(int volatile *)obj;
#elif defined(_M_ARM64)
    ASSERT_CAN_CAST_OBJ_TO(unsigned __int32);
    return (int)__ldar32((unsigned __int32 volatile *)obj);
#else
#  error "no implementation of CereggiiAtomic_LoadIntAcquire"
#endif
}

inline void
CereggiiAtomic_StoreUInt32Release(uint32_t *obj, uint32_t value)
{
#if defined(_M_X64) || defined(_M_IX86)
    *(uint32_t volatile *)obj = value;
#elif defined(_M_ARM64)
    ASSERT_CAN_CAST_OBJ_TO(unsigned __int32);
    __stlr32((unsigned __int32 volatile *)obj, (unsigned __int32)value);
#else
#  error "no implementation of CereggiiAtomic_StoreUInt32Release"
#endif
}

inline void
CereggiiAtomic_StoreUInt64Release(uint64_t *obj, uint64_t value)
{
#if defined(_M_X64) || defined(_M_IX86)
    *(uint64_t volatile *)obj = value;
#elif defined(_M_ARM64)
    ASSERT_CAN_CAST_OBJ_TO(unsigned __int64);
    __stlr64((unsigned __int64 volatile *)obj, (unsigned __int64)value);
#else
#  error "no implementation of CereggiiAtomic_StoreUInt64Release"
#endif
}

inline uint64_t
CereggiiAtomic_LoadUInt64Acquire(const uint64_t *obj)
{
#if defined(_M_X64) || defined(_M_IX86)
    return *(uint64_t volatile *)obj;
#elif defined(_M_ARM64)
    ASSERT_CAN_CAST_OBJ_TO(__int64);
    return (uint64_t)__ldar64((unsigned __int64 volatile *)obj);
#else
#  error "no implementation of CereggiiAtomic_LoadUInt64Acquire"
#endif
}

inline uint32_t
CereggiiAtomic_LoadUInt32Acquire(const uint32_t *obj)
{
#if defined(_M_X64) || defined(_M_IX86)
    return *(uint32_t volatile *)obj;
#elif defined(_M_ARM64)
    return (uint32_t)__ldar32((uint32_t volatile *)obj);
#else
#  error "no implementation of CereggiiAtomic_LoadUInt32Acquire"
#endif
}

inline Py_ssize_t
CereggiiAtomic_LoadSsizeAcquire(const Py_ssize_t *obj)
{
#if defined(_M_X64) || defined(_M_IX86)
    return *(Py_ssize_t volatile *)obj;
#elif defined(_M_ARM64)
    return (Py_ssize_t)__ldar64((unsigned __int64 volatile *)obj);
#else
#  error "no implementation of CereggiiAtomic_LoadSsizeAcquire"
#endif
}

// --- _Py_atomic_fence ------------------------------------------------------

inline void
CereggiiAtomic_FenceSeqCst(void)
{
#if defined(_M_ARM64)
    __dmb(_ARM64_BARRIER_ISH);
#elif defined(_M_X64)
    __faststorefence();
#elif defined(_M_IX86)
    _mm_mfence();
#else
#  error "no implementation of CereggiiAtomic_FenceSeqCst"
#endif
}

inline void
CereggiiAtomic_FenceAcquire(void)
{
#if defined(_M_ARM64)
    __dmb(_ARM64_BARRIER_ISHLD);
#elif defined(_M_X64) || defined(_M_IX86)
    _ReadBarrier();
#else
#  error "no implementation of CereggiiAtomic_FenceAcquire"
#endif
}

inline void
CereggiiAtomic_FenceRelease(void)
{
#if defined(_M_ARM64)
    __dmb(_ARM64_BARRIER_ISH);
#elif defined(_M_X64) || defined(_M_IX86)
    _ReadWriteBarrier();
#else
#  error "no implementation of CereggiiAtomic_FenceRelease"
#endif
}

#undef ASSERT_CAN_CAST_OBJ_TO
