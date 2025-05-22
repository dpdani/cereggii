// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef THREAD_HANDLE_H
#define THREAD_HANDLE_H

#include "Python.h"


typedef struct {
    PyObject_HEAD

    PyObject *obj;
} ThreadHandle;


extern PyTypeObject ThreadHandle_Type;

int ThreadHandle_init(ThreadHandle *self, PyObject *args, PyObject *kwargs);

int ThreadHandle_traverse(ThreadHandle *self, visitproc visit, void *arg);

void ThreadHandle_clear(ThreadHandle *self);

void ThreadHandle_dealloc(ThreadHandle *self);

Py_hash_t ThreadHandle_Hash(ThreadHandle *self);

PyObject *ThreadHandle_Add(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Subtract(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Multiply(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Remainder(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Divmod(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Power(ThreadHandle *self, PyObject *other, PyObject *mod);

PyObject *ThreadHandle_Negative(ThreadHandle *self);

PyObject *ThreadHandle_Positive(ThreadHandle *self);

PyObject *ThreadHandle_Absolute(ThreadHandle *self);

int ThreadHandle_Bool(ThreadHandle *self);

PyObject *ThreadHandle_Invert(ThreadHandle *self);

PyObject *ThreadHandle_Lshift(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Rshift(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_And(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Xor(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Or(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Int(ThreadHandle *self);

PyObject *ThreadHandle_Float(ThreadHandle *self);

PyObject *ThreadHandle_InPlaceAdd(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceSubtract(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceMultiply(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceRemainder(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlacePower(ThreadHandle *self, PyObject *other, PyObject *mod);

PyObject *ThreadHandle_InPlaceLshift(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceRshift(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceAnd(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceXor(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceOr(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_FloorDivide(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_TrueDivide(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceFloorDivide(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceTrueDivide(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Index(ThreadHandle *self);

PyObject *ThreadHandle_MatrixMultiply(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceMatrixMultiply(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_RichCompare(ThreadHandle *self, PyObject *other, int op);

Py_ssize_t ThreadHandle_Length(ThreadHandle *self);

PyObject *ThreadHandle_Concat(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_Repeat(ThreadHandle *self, Py_ssize_t count);

PyObject *ThreadHandle_GetItem(ThreadHandle *self, Py_ssize_t i);

int ThreadHandle_SetItem(ThreadHandle *self, Py_ssize_t i, PyObject *v);

int ThreadHandle_Contains(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceConcat(ThreadHandle *self, PyObject *other);

PyObject *ThreadHandle_InPlaceRepeat(ThreadHandle *self, Py_ssize_t count);

Py_ssize_t ThreadHandle_MappingLength(ThreadHandle *self);

PyObject *ThreadHandle_MappingGetItem(ThreadHandle *self, PyObject *other);

int ThreadHandle_MappingSetItem(ThreadHandle *self, PyObject *key, PyObject *v);

PyObject *ThreadHandle_Repr(ThreadHandle *self);

PyObject *ThreadHandle_Call(ThreadHandle *self, PyObject *args, PyObject *kwargs);

PyObject *ThreadHandle_GetIter(ThreadHandle *self);

PyObject *ThreadHandle_GetAttr(ThreadHandle *self, PyObject *attr_name);

int ThreadHandle_SetAttr(ThreadHandle *self, PyObject *attr_name, PyObject *v);

#endif //THREAD_HANDLE_H
