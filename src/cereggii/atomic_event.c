// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_event.h"
#include <stdatomic.h>


PyObject *
AtomicEvent_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds))
{
    AtomicEvent *self = NULL;
    self = PyObject_New(AtomicEvent, &AtomicEvent_Type);
    return (PyObject *) self;
}

int
AtomicEvent_init(AtomicEvent *self, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs))
{
    mtx_init(&self->mutex, mtx_plain);
    cnd_init(&self->cond);
    self->state = 0;

    return 0;
}

void
AtomicEvent_dealloc(AtomicEvent *self)
{
    mtx_destroy(&self->mutex);
    cnd_destroy(&self->cond);

    Py_TYPE(self)->tp_free((PyObject *) self);
}

void
AtomicEvent_Set(AtomicEvent *self)
{
    mtx_lock(&self->mutex);
    atomic_store_explicit((_Atomic(int) *) &self->state, 1, memory_order_relaxed);
    cnd_broadcast(&self->cond);
    mtx_unlock(&self->mutex);
}

int
AtomicEvent_IsSet(AtomicEvent *self)
{
    return atomic_load_explicit((_Atomic(int) *) &self->state, memory_order_relaxed);
}

static void
wait_gil_released(AtomicEvent *self)
{
    mtx_lock(&self->mutex);
    while (atomic_load_explicit((_Atomic(int) *) &self->state, memory_order_relaxed) == 0) {
        cnd_wait(&self->cond, &self->mutex);
    }
    mtx_unlock(&self->mutex);
}

void
AtomicEvent_Wait(AtomicEvent *self)
{
    Py_BEGIN_ALLOW_THREADS
    wait_gil_released(self);
    Py_END_ALLOW_THREADS
}

PyObject *AtomicEvent_Set_callable(AtomicEvent *self)
{
    AtomicEvent_Set(self);
    Py_RETURN_NONE;
}

PyObject *AtomicEvent_IsSet_callable(AtomicEvent *self)
{
    int state = AtomicEvent_IsSet(self);
    return PyLong_FromLong(state);
}

PyObject *AtomicEvent_Wait_callable(AtomicEvent *self)
{
    AtomicEvent_Wait(self);
    Py_RETURN_NONE;
}
