// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_MISC_H
#define CEREGGII_MISC_H

#include "Python.h"

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

#if defined(_WIN32)
#  include <windows.h>
#  define CEREGGII_YIELD SwitchToThread()
#else
#  include <sched.h>
#  define CEREGGII_YIELD sched_yield()
#endif

static inline void
cereggii_yield()
{
    Py_BEGIN_ALLOW_THREADS;
#ifdef Py_GIL_DISABLED
    CEREGGII_YIELD;
#endif
    Py_END_ALLOW_THREADS;
}

#endif // CEREGGII_MISC_H
