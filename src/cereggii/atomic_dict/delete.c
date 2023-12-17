// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_dict.h"

int
AtomicDict_DelItem(AtomicDict *self, PyObject *key)
{
    PyErr_SetString(PyExc_NotImplementedError, "AtomicDict_DelItem. see https://github.com/dpdani/cereggii/issues/4");
    return -1;
}
