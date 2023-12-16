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

typedef struct atomic_int_handle {
    PyObject_HEAD

    AtomicInt *integer;
} AtomicIntHandle;


/// basic methods

int64_t AtomicInt_Get(AtomicInt *self);

PyObject *AtomicInt_Get_callable(AtomicInt *self);

void AtomicInt_Set(AtomicInt *self, int64_t updated);

PyObject *AtomicInt_Set_callable(AtomicInt *self, PyObject *updated);

int AtomicInt_CompareAndSet(AtomicInt *self, int64_t expected, int64_t updated);

PyObject *AtomicInt_CompareAndSet_callable(AtomicInt *self, PyObject *args, PyObject *kwargs);

int64_t AtomicInt_GetAndSet(AtomicInt *self, int64_t updated);

PyObject *AtomicInt_GetAndSet_callable(AtomicInt *self, PyObject *args, PyObject *kwargs);


/// handle

PyObject *AtomicInt_GetHandle(AtomicInt *self);


/// java-esque methods

int64_t AtomicInt_IncrementAndGet(AtomicInt *self, int64_t other);

int64_t AtomicInt_GetAndIncrement(AtomicInt *self, int64_t other);


int64_t AtomicInt_DecrementAndGet(AtomicInt *self, int64_t other);

int64_t AtomicInt_GetAndDecrement(AtomicInt *self, int64_t other);


int64_t AtomicInt_GetAndUpdate(AtomicInt *self, PyObject *callable);

int64_t AtomicInt_UpdateAndGet(AtomicInt *self, PyObject *callable);


/// number methods

PyObject *AtomicInt_Add(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Subtract(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Multiply(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Remainder(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Divmod(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Power(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Negative(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Positive(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Absolute(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Bool(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Invert(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Lshift(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Rshift(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_And(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Xor(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Or(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Int(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_Float(AtomicInt *self, PyObject *other);


PyObject *AtomicInt_InplaceAdd(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceSubtract(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceMultiply(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceRemainder(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplacePower(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceLshift(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceRshift(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceAnd(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceXor(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceOr(AtomicInt *self, PyObject *other);


PyObject *AtomicInt_FloorDivide(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_TrueDivide(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceFloorDivide(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceTrueDivide(AtomicInt *self, PyObject *other);


PyObject *AtomicInt_Index(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_MatrixMultiply(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_InplaceMatrixMultiply(AtomicInt *self, PyObject *other);

PyObject *AtomicInt_RichCompare(AtomicInt *self, PyObject *other, int op);


/// handle

int64_t AtomicIntHandle_Get(AtomicIntHandle *self);

PyObject *AtomicIntHandle_Get_callable(AtomicIntHandle *self);

void AtomicIntHandle_Set(AtomicIntHandle *self, int64_t updated);

PyObject *AtomicIntHandle_Set_callable(AtomicIntHandle *self, PyObject *updated);

int AtomicIntHandle_CompareAndSet(AtomicIntHandle *self, int64_t expected, int64_t updated);

PyObject *AtomicIntHandle_CompareAndSet_callable(AtomicIntHandle *self, PyObject *args, PyObject *kwargs);

int64_t AtomicIntHandle_GetAndSet(AtomicIntHandle *self, int64_t updated);

PyObject *AtomicIntHandle_GetAndSet_callable(AtomicIntHandle *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicIntHandle_GetHandle(AtomicIntHandle *self);

int64_t AtomicIntHandle_IncrementAndGet(AtomicIntHandle *self, int64_t other);

int64_t AtomicIntHandle_GetAndIncrement(AtomicIntHandle *self, int64_t other);

int64_t AtomicIntHandle_DecrementAndGet(AtomicIntHandle *self, int64_t other);

int64_t AtomicIntHandle_GetAndDecrement(AtomicIntHandle *self, int64_t other);

int64_t AtomicIntHandle_GetAndUpdate(AtomicIntHandle *self, PyObject *callable);

int64_t AtomicIntHandle_UpdateAndGet(AtomicIntHandle *self, PyObject *callable);

PyObject *AtomicIntHandle_Add(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Subtract(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Multiply(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Remainder(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Divmod(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Power(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Negative(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Positive(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Absolute(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Bool(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Invert(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Lshift(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Rshift(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_And(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Xor(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Or(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Int(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Float(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceAdd(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceSubtract(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceMultiply(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceRemainder(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplacePower(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceLshift(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceRshift(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceAnd(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceXor(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceOr(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_FloorDivide(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_TrueDivide(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceFloorDivide(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceTrueDivide(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_Index(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_MatrixMultiply(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_InplaceMatrixMultiply(AtomicIntHandle *self, PyObject *other);

PyObject *AtomicIntHandle_RichCompare(AtomicIntHandle *self, PyObject *other, int op);


/// type methods

PyObject *AtomicInt_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);

PyObject *AtomicIntHandle_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);

int AtomicInt_init(AtomicInt *self, PyObject *args, PyObject *kwargs);

int AtomicIntHandle_init(AtomicIntHandle *self, PyObject *args, PyObject *kwargs);

void AtomicInt_dealloc(AtomicInt *self);

void AtomicIntHandle_dealloc(AtomicIntHandle *self);

extern PyTypeObject AtomicInt_Type;
extern PyTypeObject AtomicIntHandle_Type;


#endif // CEREGGII_ATOMIC_INT_H
