// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"


int AtomicDict_Grow(AtomicDict *self)
{
    PyErr_SetString(PyExc_NotImplementedError, "AtomicDict_Grow. see https://github.com/dpdani/cereggii/issues/5");
    return -1;
}

int AtomicDict_Shrink(AtomicDict *self)
{
    PyErr_SetString(PyExc_NotImplementedError, "AtomicDict_Shrink. see https://github.com/dpdani/cereggii/issues/5");
    return -1;
}
