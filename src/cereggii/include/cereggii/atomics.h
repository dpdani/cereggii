// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

/*
Cross-platform atomic primitives.

Largely adapted from CPython.
*/

#ifndef CEREGGII_ATOMICS_H
#define CEREGGII_ATOMICS_H


#include <cereggii/internal/atomics_declarations.h>


#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
#  define USE_GCC_ATOMICS 1
#elif defined(__clang__)
#  if __has_builtin(__atomic_load)
#    define USE_GCC_ATOMICS 1
#  endif
#endif

#if USE_GCC_ATOMICS
#  include "cereggii/internal/atomics_gcc.h"
#elif __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#  include "cereggii/internal/atomics_stdatomic.h"
#elif defined(_MSC_VER)
#  include "cereggii/internal/atomics_msc.h"
#else
#  error "no available atomics implementation for this platform/compiler"
#endif

#ifdef USE_GCC_ATOMICS
#undef USE_GCC_ATOMICS
#endif


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

#endif //CEREGGII_ATOMICS_H
