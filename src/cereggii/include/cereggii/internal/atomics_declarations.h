// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMICS_DECL_H
#define CEREGGII_ATOMICS_DECL_H

#include <Python.h>


inline int
CereggiiAtomic_AddInt(int *obj, int value);

inline int8_t
CereggiiAtomic_AddInt8(int8_t *obj, int8_t value);

inline int16_t
CereggiiAtomic_AddInt16(int16_t *obj, int16_t value);

inline int32_t
CereggiiAtomic_AddInt32(int32_t *obj, int32_t value);

inline int64_t
CereggiiAtomic_AddInt64(int64_t *obj, int64_t value);

inline intptr_t
CereggiiAtomic_AddIntPtr(intptr_t *obj, intptr_t value);

inline unsigned int
CereggiiAtomic_AddUInt(unsigned int *obj, unsigned int value);

inline uint8_t
CereggiiAtomic_AddUInt8(uint8_t *obj, uint8_t value);

inline uint16_t
CereggiiAtomic_AddUInt16(uint16_t *obj, uint16_t value);

inline uint32_t
CereggiiAtomic_AddUInt32(uint32_t *obj, uint32_t value);

inline uint64_t
CereggiiAtomic_AddUInt64(uint64_t *obj, uint64_t value);

inline uintptr_t
CereggiiAtomic_AddUIntPtr(uintptr_t *obj, uintptr_t value);

inline Py_ssize_t
CereggiiAtomic_AddSsize(Py_ssize_t *obj, Py_ssize_t value);


// ----------------------------------------------------------------------------
// compare exchange:
//    1. atomically read the value of obj
//    2. if it is expected, then update it with desired and return 1;
//    3. otherwise, don't change it and return 0

inline int
CereggiiAtomic_CompareExchangeInt(int *obj, int expected, int desired);

inline int
CereggiiAtomic_CompareExchangeInt8(int8_t *obj, int8_t expected, int8_t desired);

inline int
CereggiiAtomic_CompareExchangeInt16(int16_t *obj, int16_t expected, int16_t desired);

inline int
CereggiiAtomic_CompareExchangeInt32(int32_t *obj, int32_t expected, int32_t desired);

inline int
CereggiiAtomic_CompareExchangeInt64(int64_t *obj, int64_t expected, int64_t desired);

inline int
CereggiiAtomic_CompareExchangeIntPtr(intptr_t *obj, intptr_t expected, intptr_t desired);

inline int
CereggiiAtomic_CompareExchangeUInt(unsigned int *obj, unsigned int expected, unsigned int desired);

inline int
CereggiiAtomic_CompareExchangeUInt8(uint8_t *obj, uint8_t expected, uint8_t desired);

inline int
CereggiiAtomic_CompareExchangeUInt16(uint16_t *obj, uint16_t expected, uint16_t desired);

inline int
CereggiiAtomic_CompareExchangeUInt32(uint32_t *obj, uint32_t expected, uint32_t desired);

inline int
CereggiiAtomic_CompareExchangeUInt64(uint64_t *obj, uint64_t expected, uint64_t desired);

inline int
CereggiiAtomic_CompareExchangeUIntPtr(uintptr_t *obj, uintptr_t expected, uintptr_t desired);

inline int
CereggiiAtomic_CompareExchangeSsize(Py_ssize_t *obj, Py_ssize_t expected, Py_ssize_t desired);

inline int
CereggiiAtomic_CompareExchangePtr(void **obj, void *expected, void *desired);


// ----------------------------------------------------------------------------
// exchange:

inline int
CereggiiAtomic_ExchangeInt(int *obj, int value);

inline int8_t
CereggiiAtomic_ExchangeInt8(int8_t *obj, int8_t value);

inline int16_t
CereggiiAtomic_ExchangeInt16(int16_t *obj, int16_t value);

inline int32_t
CereggiiAtomic_ExchangeInt32(int32_t *obj, int32_t value);

inline int64_t
CereggiiAtomic_ExchangeInt64(int64_t *obj, int64_t value);

inline intptr_t
CereggiiAtomic_ExchangeIntPtr(intptr_t *obj, intptr_t value);

inline unsigned int
CereggiiAtomic_ExchangeUInt(unsigned int *obj, unsigned int value);

inline uint8_t
CereggiiAtomic_ExchangeUInt8(uint8_t *obj, uint8_t value);

inline uint16_t
CereggiiAtomic_ExchangeUInt16(uint16_t *obj, uint16_t value);

inline uint32_t
CereggiiAtomic_ExchangeUInt32(uint32_t *obj, uint32_t value);

inline uint64_t
CereggiiAtomic_ExchangeUInt64(uint64_t *obj, uint64_t value);

inline uintptr_t
CereggiiAtomic_ExchangeUIntPtr(uintptr_t *obj, uintptr_t value);

inline Py_ssize_t
CereggiiAtomic_ExchangeSsize(Py_ssize_t *obj, Py_ssize_t value);

inline void *
CereggiiAtomic_ExchangePtr(void **obj, void *value);

inline uint8_t
CereggiiAtomic_AndUInt8(uint8_t *obj, uint8_t value);

inline uint16_t
CereggiiAtomic_AndUInt16(uint16_t *obj, uint16_t value);

inline uint32_t
CereggiiAtomic_AndUInt32(uint32_t *obj, uint32_t value);

inline uint64_t
CereggiiAtomic_AndUInt64(uint64_t *obj, uint64_t value);

inline uintptr_t
CereggiiAtomic_AndUIntPtr(uintptr_t *obj, uintptr_t value);

inline uint8_t
CereggiiAtomic_OrUInt8(uint8_t *obj, uint8_t value);

inline uint16_t
CereggiiAtomic_OrUInt16(uint16_t *obj, uint16_t value);

inline uint32_t
CereggiiAtomic_OrUInt32(uint32_t *obj, uint32_t value);

inline uint64_t
CereggiiAtomic_OrUInt64(uint64_t *obj, uint64_t value);

inline uintptr_t
CereggiiAtomic_OrUIntPtr(uintptr_t *obj, uintptr_t value);

inline int
CereggiiAtomic_LoadInt(const int *obj);

inline int8_t
CereggiiAtomic_LoadInt8(const int8_t *obj);

inline int16_t
CereggiiAtomic_LoadInt16(const int16_t *obj);

inline int32_t
CereggiiAtomic_LoadInt32(const int32_t *obj);

inline int64_t
CereggiiAtomic_LoadInt64(const int64_t *obj);

inline intptr_t
CereggiiAtomic_LoadIntPtr(const intptr_t *obj);

inline uint8_t
CereggiiAtomic_LoadUInt8(const uint8_t *obj);

inline uint16_t
CereggiiAtomic_LoadUInt16(const uint16_t *obj);

inline uint32_t
CereggiiAtomic_LoadUInt32(const uint32_t *obj);

inline uint64_t
CereggiiAtomic_LoadUInt64(const uint64_t *obj);

inline uintptr_t
CereggiiAtomic_LoadUIntPtr(const uintptr_t *obj);

inline unsigned int
CereggiiAtomic_LoadUInt(const unsigned int *obj);

inline Py_ssize_t
CereggiiAtomic_LoadSsize(const Py_ssize_t *obj);

inline void *
CereggiiAtomic_LoadPtr(const void **obj);

inline int
CereggiiAtomic_LoadIntRelaxed(const int *obj);

inline char
CereggiiAtomic_LoadCharRelaxed(const char *obj);

inline unsigned char
CereggiiAtomic_LoadUCharRelaxed(const unsigned char *obj);

inline short
CereggiiAtomic_LoadShortRelaxed(const short *obj);

inline unsigned short
CereggiiAtomic_LoadUShortRelaxed(const unsigned short *obj);

inline long
CereggiiAtomic_LoadLongRelaxed(const long *obj);

inline float
CereggiiAtomic_LoadFloatRelaxed(const float *obj);

inline double
CereggiiAtomic_LoadDoubleRelaxed(const double *obj);

inline int8_t
CereggiiAtomic_LoadInt8Relaxed(const int8_t *obj);

inline int16_t
CereggiiAtomic_LoadInt16Relaxed(const int16_t *obj);

inline int32_t
CereggiiAtomic_LoadInt32Relaxed(const int32_t *obj);

inline int64_t
CereggiiAtomic_LoadInt64Relaxed(const int64_t *obj);

inline intptr_t
CereggiiAtomic_LoadIntPtrRelaxed(const intptr_t *obj);

inline uint8_t
CereggiiAtomic_LoadUInt8Relaxed(const uint8_t *obj);

inline uint16_t
CereggiiAtomic_LoadUInt16Relaxed(const uint16_t *obj);

inline uint32_t
CereggiiAtomic_LoadUInt32Relaxed(const uint32_t *obj);

inline uint64_t
CereggiiAtomic_LoadUInt64Relaxed(const uint64_t *obj);

inline uintptr_t
CereggiiAtomic_LoadUIntPtrRelaxed(const uintptr_t *obj);

inline unsigned int
CereggiiAtomic_LoadUIntRelaxed(const unsigned int *obj);

inline Py_ssize_t
CereggiiAtomic_LoadSsizeRelaxed(const Py_ssize_t *obj);

inline void *
CereggiiAtomic_LoadPtrRelaxed(const void **obj);

inline unsigned long long
CereggiiAtomic_LoadULLongRelaxed(const unsigned long long *obj);

inline long long
CereggiiAtomic_LoadLLongRelaxed(const long long *obj);

inline void
CereggiiAtomic_StoreInt(int *obj, int value);

inline void
CereggiiAtomic_StoreInt8(int8_t *obj, int8_t value);

inline void
CereggiiAtomic_StoreInt16(int16_t *obj, int16_t value);

inline void
CereggiiAtomic_StoreInt32(int32_t *obj, int32_t value);

inline void
CereggiiAtomic_StoreInt64(int64_t *obj, int64_t value);

inline void
CereggiiAtomic_StoreIntPtr(intptr_t *obj, intptr_t value);

inline void
CereggiiAtomic_StoreUInt8(uint8_t *obj, uint8_t value);

inline void
CereggiiAtomic_StoreUInt16(uint16_t *obj, uint16_t value);

inline void
CereggiiAtomic_StoreUInt32(uint32_t *obj, uint32_t value);

inline void
CereggiiAtomic_StoreUInt64(uint64_t *obj, uint64_t value);

inline void
CereggiiAtomic_StoreUIntPtr(uintptr_t *obj, uintptr_t value);

inline void
CereggiiAtomic_StoreUInt(unsigned int *obj, unsigned int value);

inline void
CereggiiAtomic_StorePtr(void **obj, void *value);

inline void
CereggiiAtomic_StoreSsize(Py_ssize_t *obj, Py_ssize_t value);

inline void
CereggiiAtomic_StoreIntRelaxed(int *obj, int value);

inline void
CereggiiAtomic_StoreInt8Relaxed(int8_t *obj, int8_t value);

inline void
CereggiiAtomic_StoreInt16Relaxed(int16_t *obj, int16_t value);

inline void
CereggiiAtomic_StoreInt32Relaxed(int32_t *obj, int32_t value);

inline void
CereggiiAtomic_StoreInt64Relaxed(int64_t *obj, int64_t value);

inline void
CereggiiAtomic_StoreIntPtrRelaxed(intptr_t *obj, intptr_t value);

inline void
CereggiiAtomic_StoreUInt8Relaxed(uint8_t *obj, uint8_t value);

inline void
CereggiiAtomic_StoreUInt16Relaxed(uint16_t *obj, uint16_t value);

inline void
CereggiiAtomic_StoreUInt32Relaxed(uint32_t *obj, uint32_t value);

inline void
CereggiiAtomic_StoreUInt64Relaxed(uint64_t *obj, uint64_t value);

inline void
CereggiiAtomic_StoreUIntPtrRelaxed(uintptr_t *obj, uintptr_t value);

inline void
CereggiiAtomic_StoreUIntRelaxed(unsigned int *obj, unsigned int value);

inline void
CereggiiAtomic_StorePtrRelaxed(void **obj, void *value);

inline void
CereggiiAtomic_StoreSsizeRelaxed(Py_ssize_t *obj, Py_ssize_t value);

inline void
CereggiiAtomic_StoreULLongRelaxed(unsigned long long *obj, unsigned long long value);

inline void
CereggiiAtomic_StoreCharRelaxed(char *obj, char value);

inline void
CereggiiAtomic_StoreUCharRelaxed(unsigned char *obj, unsigned char value);

inline void
CereggiiAtomic_StoreShortRelaxed(short *obj, short value);

inline void
CereggiiAtomic_StoreUShortRelaxed(unsigned short *obj, unsigned short value);

inline void
CereggiiAtomic_StoreLongRelaxed(long *obj, long value);

inline void
CereggiiAtomic_StoreFloatRelaxed(float *obj, float value);

inline void
CereggiiAtomic_StoreDoubleRelaxed(double *obj, double value);

inline void
CereggiiAtomic_StoreLLongRelaxed(long long *obj, long long value);

inline void *
CereggiiAtomic_LoadPtrAcquire(const void **obj);

inline uintptr_t
CereggiiAtomic_LoadUIntPtrAcquire(const uintptr_t *obj);

inline void
CereggiiAtomic_StorePtrRelease(void **obj, void *value);

inline void
CereggiiAtomic_StoreUIntPtrRelease(uintptr_t *obj, uintptr_t value);

inline void
CereggiiAtomic_StoreIntRelease(int *obj, int value);

inline void
CereggiiAtomic_StoreUIntRelease(unsigned int *obj, unsigned int value);

inline void
CereggiiAtomic_StoreSsizeRelease(Py_ssize_t *obj, Py_ssize_t value);

inline int
CereggiiAtomic_LoadIntAcquire(const int *obj);

inline void
CereggiiAtomic_StoreUInt32Release(uint32_t *obj, uint32_t value);

inline void
CereggiiAtomic_StoreUInt64Release(uint64_t *obj, uint64_t value);

inline uint64_t
CereggiiAtomic_LoadUInt64Acquire(const uint64_t *obj);

inline uint32_t
CereggiiAtomic_LoadUInt32Acquire(const uint32_t *obj);

inline Py_ssize_t
CereggiiAtomic_LoadSsizeAcquire(const Py_ssize_t *obj);

inline void
CereggiiAtomic_FenceSeqCst(void);

inline void
CereggiiAtomic_FenceAcquire(void);

inline void
CereggiiAtomic_FenceRelease(void);

#endif // CEREGGII_ATOMICS_DECL_H
