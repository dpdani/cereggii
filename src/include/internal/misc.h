// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_MISC_H
#define CEREGGII_MISC_H

#if defined(_MSC_VER)
#  define CEREGGII_ALIGNED(x) __declspec(align(x))
#elif defined(__GNUC__) || defined(__clang__)
#  define CEREGGII_ALIGNED(x) __attribute__((aligned(x)))
#else
#  define CEREGGII_ALIGNED(x) alignas(x)  // C++11 standard
#endif

#if defined(_MSC_VER)
#  define CEREGGII_UNUSED __pragma(warning(suppress: 4100))
#elif defined(__GNUC__) || defined(__clang__)
#  define CEREGGII_UNUSED __attribute__((unused))
#else
#  define CEREGGII_UNUSED
#endif

#if defined(_MSC_VER)
#  include <intrin.h>

#  if defined(_M_IX86) || defined(_M_X64)
#    include <xmmintrin.h>
#    define cereggii_prefetch(p) _mm_prefetch((const char*)(p), _MM_HINT_T0)
#  elif defined(_M_ARM64) || defined(_M_ARM64EC)
#    define cereggii_prefetch(p) __prefetch((const void*)(p))
#  else
#    error "unsupported platform"
#  endif

#  if defined(_M_IX86) || defined(_M_X64)
#    define cereggii_crc32_u64(crc, v) _mm_crc32_u64((crc), (v))
#  elif defined(_M_ARM64) || defined(_M_ARM64EC)
#    define cereggii_crc32_u64(crc, v) __crc32cd((crc), (v))
#  else
#    error "unsupported platform"
#  endif

#else
#  define cereggii_prefetch(p) __builtin_prefetch(p)
#  define cereggii_crc32_u64(crc, v) __builtin_ia32_crc32di((crc), (v))
#endif


#endif // CEREGGII_MISC_H
