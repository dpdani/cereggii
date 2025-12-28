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

#if defined(_MSC_VER) && !(defined(_M_ARM64) || defined(_M_ARM64EC))
#  include <immintrin.h>
#  include <nmmintrin.h>
#  define cereggii_prefetch(p) _mm_prefetch((const char*)(p), _MM_HINT_T0)
#  define cereggii_crc32_u64(crc, v) _mm_crc32_u64((crc), (v))
#else
#  if defined(_MSC_VER) && !(defined(_M_ARM64) || defined(_M_ARM64EC))
#    include <arm_neon.h>
#  endif
#  define cereggii_prefetch(p) __builtin_prefetch(p)
#  define cereggii_crc32_u64(crc, v) __builtin_ia32_crc32di((crc), (v))
#endif


#endif // CEREGGII_MISC_H
