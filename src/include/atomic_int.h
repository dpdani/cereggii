// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_INT_H
#define CEREGGII_ATOMIC_INT_H

#define PY_SSIZE_T_CLEAN

#include <Python.h>


typedef struct atomic_int {
    PyObject_HEAD

    int64_t integer;
} AtomicInt;


/// basic methods

int64_t AtomicInt_Get(AtomicInt *self);

PyObject *AtomicInt_Get_callable(AtomicInt *self);

void AtomicInt_Set(AtomicInt *self, int64_t updated);

PyObject *AtomicInt_Set_callable(AtomicInt *self, PyObject *updated);

int AtomicInt_CompareAndSet(AtomicInt *self, int64_t expected, int64_t updated);

PyObject *AtomicInt_CompareAndSet_callable(AtomicInt *self, PyObject *args, PyObject *kwargs);

int64_t AtomicInt_GetAndSet(AtomicInt *self, int64_t updated);

PyObject *AtomicInt_GetAndSet_callable(AtomicInt *self, PyObject *args, PyObject *kwargs);


/// java-esque methods

int64_t AtomicInt_IncrementAndGet(AtomicInt *self, int64_t amount);

int64_t AtomicInt_GetAndIncrement(AtomicInt *self, int64_t amount);


int64_t AtomicInt_DecrementAndGet(AtomicInt *self, int64_t amount);

int64_t AtomicInt_GetAndDecrement(AtomicInt *self, int64_t amount);


int64_t AtomicInt_GetAndUpdate(AtomicInt *self, PyObject *callable);

int64_t AtomicInt_UpdateAndGet(AtomicInt *self, PyObject *callable);


/// number methods

PyObject *AtomicInt_Add(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Subtract(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Multiply(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Remainder(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Divmod(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Power(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Negative(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Positive(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Absolute(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Bool(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Invert(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Lshift(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Rshift(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_And(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Xor(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Or(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Int(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_Float(AtomicInt *self, PyObject *amount);


PyObject *AtomicInt_InplaceAdd(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceSubtract(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceMultiply(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceRemainder(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplacePower(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceLshift(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceRshift(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceAnd(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceXor(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceOr(AtomicInt *self, PyObject *amount);


PyObject *AtomicInt_FloorDivide(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_TrueDivide(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceFloorDivide(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceTrueDivide(AtomicInt *self, PyObject *amount);


PyObject *AtomicInt_Index(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_MatrixMultiply(AtomicInt *self, PyObject *amount);

PyObject *AtomicInt_InplaceMatrixMultiply(AtomicInt *self, PyObject *amount);


/// type methods

PyObject *AtomicInt_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);

int AtomicInt_init(AtomicInt *self, PyObject *args, PyObject *kwargs);

void AtomicInt_dealloc(AtomicInt *self);

extern PyTypeObject AtomicInt_Type;


#endif // CEREGGII_ATOMIC_INT_H
