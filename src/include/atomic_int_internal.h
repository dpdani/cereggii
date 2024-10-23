// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_INT_INTERNAL_H
#define CEREGGII_ATOMIC_INT_INTERNAL_H

#define PY_SSIZE_T_CLEAN

#include "atomic_int.h"


int AtomicInt64_ConvertToCLongOrSetException(PyObject *py_integer, int64_t *integer);

int AtomicInt64_AddOrSetOverflow(int64_t current, int64_t to_add, int64_t *result);

int AtomicInt64_SubOrSetOverflow(int64_t current, int64_t to_add, int64_t *result);

int AtomicInt64_MulOrSetOverflow(int64_t current, int64_t to_add, int64_t *result);

int AtomicInt64_DivOrSetException(int64_t current, int64_t to_div, int64_t *result);

PyObject *AtomicInt64_InplaceAdd_internal(AtomicInt64 *self, PyObject *other, int do_refcount);

PyObject *AtomicInt64_InplaceSubtract_internal(AtomicInt64 *self, PyObject *other, int do_refcount);

PyObject *AtomicInt64_InplaceMultiply_internal(AtomicInt64 *self, PyObject *other, int do_refcount);

PyObject *AtomicInt64_InplaceRemainder_internal(AtomicInt64 *self, PyObject *other, int do_refcount);

PyObject *AtomicInt64_InplacePower_internal(AtomicInt64 *self, PyObject *other, PyObject *mod, int do_refcount);

PyObject *AtomicInt64_InplaceLshift_internal(AtomicInt64 *self, PyObject *other, int do_refcount);

PyObject *AtomicInt64_InplaceRshift_internal(AtomicInt64 *self, PyObject *other, int do_refcount);

PyObject *AtomicInt64_InplaceAnd_internal(AtomicInt64 *self, PyObject *other, int do_refcount);

PyObject *AtomicInt64_InplaceXor_internal(AtomicInt64 *self, PyObject *other, int do_refcount);

PyObject *AtomicInt64_InplaceOr_internal(AtomicInt64 *self, PyObject *other, int do_refcount);

PyObject *AtomicInt64_InplaceFloorDivide_internal(AtomicInt64 *self, PyObject *other, int do_refcount);

#endif //CEREGGII_ATOMIC_INT_INTERNAL_H
