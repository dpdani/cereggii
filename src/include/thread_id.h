// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_THREAD_ID_H
#define CEREGGII_THREAD_ID_H

#include "Python.h"

#ifndef Py_GIL_DISABLED
#define _Py_ThreadId (uintptr_t) PyThreadState_GET
#endif

#endif //CEREGGII_THREAD_ID_H
