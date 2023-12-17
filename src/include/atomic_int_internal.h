// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_INT_INTERNAL_H
#define CEREGGII_ATOMIC_INT_INTERNAL_H

#define PY_SSIZE_T_CLEAN

#include "atomic_int.h"


int AtomicInt_ConvertToCLongOrSetException(PyObject *py_integer, int64_t *integer);

int AtomicInt_AddOrSetOverflow(int64_t current, int64_t to_add, int64_t *result);

int AtomicInt_SubOrSetOverflow(int64_t current, int64_t to_add, int64_t *result);

int AtomicInt_MulOrSetOverflow(int64_t current, int64_t to_add, int64_t *result);

int AtomicInt_DivOrSetException(int64_t current, int64_t to_div, int64_t *result);

PyObject *AtomicInt_InplaceAdd_internal(AtomicInt *self, PyObject *other, int do_refcount);

PyObject *AtomicInt_InplaceSubtract_internal(AtomicInt *self, PyObject *other, int do_refcount);

PyObject *AtomicInt_InplaceMultiply_internal(AtomicInt *self, PyObject *other, int do_refcount);

PyObject *AtomicInt_InplaceRemainder_internal(AtomicInt *self, PyObject *other, int do_refcount);

PyObject *AtomicInt_InplacePower_internal(AtomicInt *self, PyObject *other, PyObject *mod, int do_refcount);

PyObject *AtomicInt_InplaceLshift_internal(AtomicInt *self, PyObject *other, int do_refcount);

PyObject *AtomicInt_InplaceRshift_internal(AtomicInt *self, PyObject *other, int do_refcount);

PyObject *AtomicInt_InplaceAnd_internal(AtomicInt *self, PyObject *other, int do_refcount);

PyObject *AtomicInt_InplaceXor_internal(AtomicInt *self, PyObject *other, int do_refcount);

PyObject *AtomicInt_InplaceOr_internal(AtomicInt *self, PyObject *other, int do_refcount);

PyObject *AtomicInt_InplaceFloorDivide_internal(AtomicInt *self, PyObject *other, int do_refcount);

#endif //CEREGGII_ATOMIC_INT_INTERNAL_H
