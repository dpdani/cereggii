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
AtomicEvent_Set(AtomicEvent *lock)
{
    pthread_mutex_lock(&lock->mutex);
    lock->state = 1;
    pthread_cond_broadcast(&lock->cond);
    pthread_mutex_unlock(&lock->mutex);
}

int
AtomicEvent_IsSet(AtomicEvent *self)
{
    return self->state;
}

void
AtomicEvent_Wait(AtomicEvent *lock)
{
    pthread_mutex_lock(&lock->mutex);
    while (lock->state == 0) {
        Py_BEGIN_ALLOW_THREADS
        pthread_cond_wait(&lock->cond, &lock->mutex);
        Py_END_ALLOW_THREADS
    }
    pthread_mutex_unlock(&lock->mutex);
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
