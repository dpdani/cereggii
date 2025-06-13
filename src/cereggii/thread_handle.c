// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "thread_handle.h"

int
ThreadHandle_init(ThreadHandle *self, PyObject *args, PyObject *Py_UNUSED(kwargs))
{
    PyObject *obj = NULL;

    if (!PyArg_ParseTuple(args, "O", &obj))
        goto fail;

    if (PyObject_IsInstance(obj, (PyObject *) &ThreadHandle_Type)) {
        obj = ((ThreadHandle *) obj)->obj;
    }
    Py_INCREF(obj);
    self->obj = obj;

    if (!PyObject_GC_IsTracked((PyObject *) self))
        PyObject_GC_Track(self);

    return 0;

fail:
    return -1;
}


int
ThreadHandle_traverse(ThreadHandle *self, visitproc visit, void *arg)
{
    Py_VISIT(self->obj);
    return 0;
}

void
ThreadHandle_clear(ThreadHandle *self)
{
    Py_CLEAR(self->obj);
}

void
ThreadHandle_dealloc(ThreadHandle *self)
{
    PyObject_GC_UnTrack(self);
    ThreadHandle_clear(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

inline Py_hash_t
ThreadHandle_Hash(ThreadHandle *self)
{
    return PyObject_Hash(self->obj);
}

inline PyObject *
ThreadHandle_RichCompare(ThreadHandle *self, PyObject *other, int op)
{
    return PyObject_RichCompare(self->obj, other, op);
}

/// tp_as_number

inline PyObject *
ThreadHandle_Add(ThreadHandle *self, PyObject *other)
{
    return PyNumber_Add(self->obj, other);
}

inline PyObject *
ThreadHandle_Subtract(ThreadHandle *self, PyObject *other)
{
    return PyNumber_Subtract(self->obj, other);
}

inline PyObject *
ThreadHandle_Multiply(ThreadHandle *self, PyObject *other)
{
    return PyNumber_Multiply(self->obj, other);
}

inline PyObject *
ThreadHandle_Remainder(ThreadHandle *self, PyObject *other)
{
    return PyNumber_Remainder(self->obj, other);
}

inline PyObject *
ThreadHandle_Divmod(ThreadHandle *self, PyObject *other)
{
    return PyNumber_Divmod(self->obj, other);
}

inline PyObject *
ThreadHandle_Power(ThreadHandle *self, PyObject *other, PyObject *mod)
{
    return PyNumber_Power(self->obj, other, mod);
}

inline PyObject *
ThreadHandle_Negative(ThreadHandle *self)
{
    return PyNumber_Negative(self->obj);
}

inline PyObject *
ThreadHandle_Positive(ThreadHandle *self)
{
    return PyNumber_Positive(self->obj);
}

inline PyObject *
ThreadHandle_Absolute(ThreadHandle *self)
{
    return PyNumber_Absolute(self->obj);
}

inline int
ThreadHandle_Bool(ThreadHandle *self)
{
    return PyObject_IsTrue(self->obj);
}

inline PyObject *
ThreadHandle_Invert(ThreadHandle *self)
{
    return PyNumber_Invert(self->obj);
}

inline PyObject *
ThreadHandle_Lshift(ThreadHandle *self, PyObject *other)
{
    return PyNumber_Lshift(self->obj, other);
}

inline PyObject *
ThreadHandle_Rshift(ThreadHandle *self, PyObject *other)
{
    return PyNumber_Rshift(self->obj, other);
}

inline PyObject *
ThreadHandle_And(ThreadHandle *self, PyObject *other)
{
    return PyNumber_And(self->obj, other);
}

inline PyObject *
ThreadHandle_Xor(ThreadHandle *self, PyObject *other)
{
    return PyNumber_Xor(self->obj, other);
}

inline PyObject *
ThreadHandle_Or(ThreadHandle *self, PyObject *other)
{
    return PyNumber_Or(self->obj, other);
}

inline PyObject *
ThreadHandle_Int(ThreadHandle *self)
{
    return PyNumber_Long(self->obj);
}

inline PyObject *
ThreadHandle_Float(ThreadHandle *self)
{
    return PyNumber_Float(self->obj);
}

inline PyObject *
ThreadHandle_FloorDivide(ThreadHandle *self, PyObject *other)
{
    return PyNumber_FloorDivide(self->obj, other);
}

inline PyObject *
ThreadHandle_TrueDivide(ThreadHandle *self, PyObject *other)
{
    return PyNumber_TrueDivide(self->obj, other);
}

inline PyObject *
ThreadHandle_Index(ThreadHandle *self)
{
    return PyNumber_Index(self->obj);
}


inline PyObject *
ThreadHandle_InPlaceAdd(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceAdd(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceSubtract(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceSubtract(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceMultiply(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceMultiply(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceRemainder(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceRemainder(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlacePower(ThreadHandle *self, PyObject *other, PyObject *mod)
{
    return PyNumber_InPlacePower(self->obj, other, mod);
}

inline PyObject *
ThreadHandle_InPlaceLshift(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceLshift(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceRshift(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceRshift(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceAnd(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceAnd(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceXor(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceXor(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceOr(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceOr(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceFloorDivide(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceFloorDivide(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceTrueDivide(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceTrueDivide(self->obj, other);
}

inline PyObject *
ThreadHandle_MatrixMultiply(ThreadHandle *self, PyObject *other)
{
    return PyNumber_MatrixMultiply(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceMatrixMultiply(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceMatrixMultiply(self->obj, other);
}


/// tp_as_sequence

inline Py_ssize_t
ThreadHandle_Length(ThreadHandle *self)
{
    return PySequence_Length(self->obj);
}

inline PyObject *
ThreadHandle_Concat(ThreadHandle *self, PyObject *other)
{
    return PySequence_Concat(self->obj, other);
}

inline PyObject *
ThreadHandle_Repeat(ThreadHandle *self, Py_ssize_t count)
{
    return PySequence_Repeat(self->obj, count);
}

inline PyObject *
ThreadHandle_GetItem(ThreadHandle *self, Py_ssize_t i)
{
    return PySequence_GetItem(self->obj, i);
}

inline int
ThreadHandle_SetItem(ThreadHandle *self, Py_ssize_t i, PyObject *v)
{
    return PySequence_SetItem(self->obj, i, v);
}

inline int
ThreadHandle_Contains(ThreadHandle *self, PyObject *other)
{
    return PySequence_Contains(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceConcat(ThreadHandle *self, PyObject *other)
{
    return PySequence_InPlaceConcat(self->obj, other);
}

inline PyObject *
ThreadHandle_InPlaceRepeat(ThreadHandle *self, Py_ssize_t count)
{
    return PySequence_InPlaceRepeat(self->obj, count);
}


/// tp_as_mapping

inline Py_ssize_t
ThreadHandle_MappingLength(ThreadHandle *self)
{
    return PyMapping_Length(self->obj);
}

inline PyObject *
ThreadHandle_MappingGetItem(ThreadHandle *self, PyObject *other)
{
    return PyObject_GetItem(self->obj, other);
}

inline int
ThreadHandle_MappingSetItem(ThreadHandle *self, PyObject *key, PyObject *v)
{
    return PyObject_SetItem(self->obj, key, v);
}


/// tp_as_async

// inline PyObject *
// ThreadHandle_Await(ThreadHandle *self) {
//     return PyObject_SelfIter(self->obj);
// }
//
// inline PyObject *
// ThreadHandle_GetAIter(ThreadHandle *self) {
//     return PyObject_GetAIter(self->obj);
// }
//
// inline PyObject *
// ThreadHandle_AsyncNext(ThreadHandle *self) {
//     return PyObject_(self->obj);
// }
//
// inline PySendResult
// ThreadHandle_AsyncSend(ThreadHandle *self, PyObject *arg, PyObject **result) {
//     return PyIter_Send(self->obj, arg, result);
// }


/// misc

inline PyObject *
ThreadHandle_Repr(ThreadHandle *self)
{
#ifdef Py_GIL_DISABLED
    void *owner = (void *) self->ob_base.ob_tid;
#else
    void *owner = NULL;
#endif
    return PyUnicode_FromFormat("<ThreadHandle(%R) at %p belongs to %p>", self->obj, self, owner);
}

inline PyObject *
ThreadHandle_Call(ThreadHandle *self, PyObject *args, PyObject *kwargs)
{
    return PyObject_Call(self->obj, args, kwargs);
}

inline PyObject *
ThreadHandle_GetIter(ThreadHandle *self)
{
    return PyObject_GetIter(self->obj);
}

inline PyObject *
ThreadHandle_GetAttr(ThreadHandle *self, PyObject *attr_name)
{
    return PyObject_GetAttr(self->obj, attr_name);
}

inline int
ThreadHandle_SetAttr(ThreadHandle *self, PyObject *attr_name, PyObject *v)
{
    return PyObject_SetAttr(self->obj, attr_name, v);
}
