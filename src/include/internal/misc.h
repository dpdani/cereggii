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

#define cereggii_unused_in_release_build(x) do { (void)(x); } while (0)

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
#    define cereggii_crc32_u64(crc, v) __crc32cd((uint32_t)(crc), (v))
#  else
#    error "unsupported platform"
#  endif

#else // _MSC_VER

#  define cereggii_prefetch(p) __builtin_prefetch(p)

#  ifdef __aarch64__
#    if !defined(__ARM_FEATURE_CRC32)
#      error "CRC32 hardware support not available"
#    endif
#    if defined(__GNUC__)
#      include <arm_acle.h>
#      define cereggii_crc32_u64(crc, v) __crc32d((crc), (v))
#    elif defined(__clang__)
#      define cereggii_crc32_u64(crc, v) __builtin_arm_crc32d((crc), (v))
#    else
#      error "Unsupported compiler for __aarch64__"
#    endif // __crc32d
#  else // __aarch64__
#    define cereggii_crc32_u64(crc, v) __builtin_ia32_crc32di((crc), (v))
#  endif // __aarch64__

#endif // _MSC_VER


#endif // CEREGGII_MISC_H
