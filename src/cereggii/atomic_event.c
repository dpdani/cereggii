// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_event.h"


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
    pthread_mutex_init(&self->mutex, NULL);
    pthread_cond_init(&self->cond, NULL);
    self->state = 0;

    return 0;
}

void
AtomicEvent_dealloc(AtomicEvent *self)
{
    pthread_mutex_destroy(&self->mutex);
    pthread_cond_destroy(&self->cond);

    Py_TYPE(self)->tp_free((PyObject *) self);
}

void
AtomicEvent_Set(AtomicEvent *self)
{
    pthread_mutex_lock(&self->mutex);
    self->state = 1;
    pthread_cond_broadcast(&self->cond);
    pthread_mutex_unlock(&self->mutex);
}

int
AtomicEvent_IsSet(AtomicEvent *self)
{
    return self->state;
}

static void
wait_gil_released(AtomicEvent *self)
{
    pthread_mutex_lock(&self->mutex);
    while (self->state == 0) {
        pthread_cond_wait(&self->cond, &self->mutex);
    }
    pthread_mutex_unlock(&self->mutex);
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
