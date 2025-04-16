// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_event.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

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
#ifdef _WIN32
    self->event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (self->event == NULL) {
        return -1;
    }
#else
    if (pthread_mutex_init(&self->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&self->cond, NULL) != 0) {
        pthread_mutex_destroy(&self->mutex);
        return -1;
    }
    self->state = 0;
#endif
    return 0;
}

void
AtomicEvent_dealloc(AtomicEvent *self)
{
#ifdef _WIN32
    if (self->event) {
        CloseHandle(self->event);
    }
#else
    pthread_mutex_destroy(&self->mutex);
    pthread_cond_destroy(&self->cond);
#endif
    Py_TYPE(self)->tp_free((PyObject *) self);
}

void
AtomicEvent_Set(AtomicEvent *self)
{
#ifdef _WIN32
    SetEvent(self->event);
#else
    pthread_mutex_lock(&self->mutex);
    self->state = 1;
    pthread_cond_broadcast(&self->cond);
    pthread_mutex_unlock(&self->mutex);
#endif
}

int
AtomicEvent_IsSet(AtomicEvent *self)
{
#ifdef _WIN32
    return WaitForSingleObject(self->event, 0) == WAIT_OBJECT_0;
#else
    return self->state;
#endif
}

void
AtomicEvent_Wait(AtomicEvent *self)
{
#ifdef _WIN32
    Py_BEGIN_ALLOW_THREADS
    WaitForSingleObject(self->event, INFINITE);
    Py_END_ALLOW_THREADS
#else
    pthread_mutex_lock(&self->mutex);
    while (self->state == 0) {
        Py_BEGIN_ALLOW_THREADS
        pthread_cond_wait(&self->cond, &self->mutex);
        Py_END_ALLOW_THREADS
    }
    pthread_mutex_unlock(&self->mutex);
#endif
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
