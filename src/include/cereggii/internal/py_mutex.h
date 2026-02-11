// SPDX-FileCopyrightText: 2026-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0


// This header contains a compatibility shim for PyMutex to make its usages
// become no-ops in versions of Python that don't support it.
// This is fine because usages of PyMutex in this library protect shared state
// that would otherwise be protected by the GIL.


#ifndef CEREGGII_PY_MUTEX_H
#define CEREGGII_PY_MUTEX_H

#include <stdint.h>
#include <Python.h>

#if !defined(PyMutex_Lock)

#define PyMutex uint8_t
#define PyMutex_Lock(mutex) do { (void)(mutex); } while (0)
#define PyMutex_Unlock(mutex) do { (void)(mutex); } while (0)

#endif

#endif //CEREGGII_PY_MUTEX_H
