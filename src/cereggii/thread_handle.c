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

Py_hash_t
ThreadHandle_Hash(ThreadHandle *self)
{
    return PyObject_Hash(self->obj);
}

PyObject *
ThreadHandle_RichCompare(ThreadHandle *self, PyObject *other, int op)
{
    return PyObject_RichCompare(self->obj, other, op);
}

/// tp_as_number

#define THREADHANDLE_BIN_OP(op) \
    PyObject * \
    ThreadHandle_##op(ThreadHandle *self, PyObject *other) { \
        PyObject *obj = NULL; \
        if (PyObject_IsInstance(other, (PyObject *) &ThreadHandle_Type)) { \
            /* this is a reflected binary operation => ThreadHandle is in the second argument */ \
            other = ((ThreadHandle *) other)->obj; \
            obj = (PyObject *) self; \
        } \
        else { \
            obj = self->obj; \
        } \
\
        return PyNumber_##op(obj, other); \
    }

THREADHANDLE_BIN_OP(MatrixMultiply);
THREADHANDLE_BIN_OP(Add);
THREADHANDLE_BIN_OP(Subtract);
THREADHANDLE_BIN_OP(Multiply);
THREADHANDLE_BIN_OP(Remainder);
THREADHANDLE_BIN_OP(Divmod);
THREADHANDLE_BIN_OP(Lshift);
THREADHANDLE_BIN_OP(Rshift);
THREADHANDLE_BIN_OP(And);
THREADHANDLE_BIN_OP(Xor);
THREADHANDLE_BIN_OP(Or);
THREADHANDLE_BIN_OP(FloorDivide);
THREADHANDLE_BIN_OP(TrueDivide);

PyObject *
ThreadHandle_Power(ThreadHandle *self, PyObject *other, PyObject *mod)
{
    PyObject *obj = NULL;
    if (PyObject_IsInstance(other, (PyObject *) &ThreadHandle_Type)) {
        /* this is a reflected binary operation => ThreadHandle is in the second argument */
        other = ((ThreadHandle *) other)->obj;
        obj = (PyObject *) self;
    }
    else {
        obj = self->obj;
    }
    return PyNumber_Power(obj, other, mod);
}

PyObject *
ThreadHandle_Negative(ThreadHandle *self)
{
    return PyNumber_Negative(self->obj);
}

PyObject *
ThreadHandle_Positive(ThreadHandle *self)
{
    return PyNumber_Positive(self->obj);
}

PyObject *
ThreadHandle_Absolute(ThreadHandle *self)
{
    return PyNumber_Absolute(self->obj);
}

int
ThreadHandle_Bool(ThreadHandle *self)
{
    return PyObject_IsTrue(self->obj);
}

PyObject *
ThreadHandle_Invert(ThreadHandle *self)
{
    return PyNumber_Invert(self->obj);
}

PyObject *
ThreadHandle_Int(ThreadHandle *self)
{
    return PyNumber_Long(self->obj);
}

PyObject *
ThreadHandle_Float(ThreadHandle *self)
{
    return PyNumber_Float(self->obj);
}

PyObject *
ThreadHandle_Index(ThreadHandle *self)
{
    return PyNumber_Index(self->obj);
}


PyObject *
ThreadHandle_InPlaceAdd(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceAdd(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceSubtract(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceSubtract(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceMultiply(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceMultiply(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceRemainder(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceRemainder(self->obj, other);
}

PyObject *
ThreadHandle_InPlacePower(ThreadHandle *self, PyObject *other, PyObject *mod)
{
    return PyNumber_InPlacePower(self->obj, other, mod);
}

PyObject *
ThreadHandle_InPlaceLshift(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceLshift(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceRshift(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceRshift(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceAnd(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceAnd(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceXor(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceXor(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceOr(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceOr(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceFloorDivide(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceFloorDivide(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceTrueDivide(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceTrueDivide(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceMatrixMultiply(ThreadHandle *self, PyObject *other)
{
    return PyNumber_InPlaceMatrixMultiply(self->obj, other);
}


/// tp_as_sequence

Py_ssize_t
ThreadHandle_Length(ThreadHandle *self)
{
    return PySequence_Length(self->obj);
}

PyObject *
ThreadHandle_Concat(ThreadHandle *self, PyObject *other)
{
    return PySequence_Concat(self->obj, other);
}

PyObject *
ThreadHandle_Repeat(ThreadHandle *self, Py_ssize_t count)
{
    return PySequence_Repeat(self->obj, count);
}

PyObject *
ThreadHandle_GetItem(ThreadHandle *self, Py_ssize_t i)
{
    return PySequence_GetItem(self->obj, i);
}

int
ThreadHandle_SetItem(ThreadHandle *self, Py_ssize_t i, PyObject *v)
{
    if (v == NULL) {
        return PySequence_DelItem(self->obj, i);
    }
    return PySequence_SetItem(self->obj, i, v);
}

int
ThreadHandle_Contains(ThreadHandle *self, PyObject *other)
{
    return PySequence_Contains(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceConcat(ThreadHandle *self, PyObject *other)
{
    return PySequence_InPlaceConcat(self->obj, other);
}

PyObject *
ThreadHandle_InPlaceRepeat(ThreadHandle *self, Py_ssize_t count)
{
    return PySequence_InPlaceRepeat(self->obj, count);
}


/// tp_as_mapping

Py_ssize_t
ThreadHandle_MappingLength(ThreadHandle *self)
{
    return PyMapping_Length(self->obj);
}

PyObject *
ThreadHandle_MappingGetItem(ThreadHandle *self, PyObject *other)
{
    return PyObject_GetItem(self->obj, other);
}

int
ThreadHandle_MappingSetItem(ThreadHandle *self, PyObject *key, PyObject *v)
{
    if (v == NULL) {
        return PyObject_DelItem(self->obj, key);
    }
    return PyObject_SetItem(self->obj, key, v);
}


/// tp_as_async

// PyObject *
// ThreadHandle_Await(ThreadHandle *self) {
//     return PyObject_SelfIter(self->obj);
// }
//
// PyObject *
// ThreadHandle_GetAIter(ThreadHandle *self) {
//     return PyObject_GetAIter(self->obj);
// }
//
// PyObject *
// ThreadHandle_AsyncNext(ThreadHandle *self) {
//     return PyObject_(self->obj);
// }
//
// PySendResult
// ThreadHandle_AsyncSend(ThreadHandle *self, PyObject *arg, PyObject **result) {
//     return PyIter_Send(self->obj, arg, result);
// }


/// misc

PyObject *
ThreadHandle_Repr(ThreadHandle *self)
{
#ifdef Py_GIL_DISABLED
    void *owner = (void *) self->ob_base.ob_tid;
#else
    void *owner = NULL;
#endif
    return PyUnicode_FromFormat("<ThreadHandle(%R) at %p belongs to %p>", self->obj, self, owner);
}

PyObject *
ThreadHandle_Call(ThreadHandle *self, PyObject *args, PyObject *kwargs)
{
    return PyObject_Call(self->obj, args, kwargs);
}

PyObject *
ThreadHandle_GetIter(ThreadHandle *self)
{
    return PyObject_GetIter(self->obj);
}

PyObject *
ThreadHandle_GetAttr(ThreadHandle *self, PyObject *attr_name)
{
    return PyObject_GetAttr(self->obj, attr_name);
}

int
ThreadHandle_SetAttr(ThreadHandle *self, PyObject *attr_name, PyObject *v)
{
    return PyObject_SetAttr(self->obj, attr_name, v);
}
