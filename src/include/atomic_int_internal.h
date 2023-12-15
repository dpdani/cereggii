// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_INT_INTERNAL_H
#define CEREGGII_ATOMIC_INT_INTERNAL_H

#define PY_SSIZE_T_CLEAN

#include "atomic_int.h"


int AtomicInt_ConvertToCLongOrSetException(PyObject *py_integer, int64_t *integer);


#endif //CEREGGII_ATOMIC_INT_INTERNAL_H
