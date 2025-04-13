// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_EVENT_H
#define CEREGGII_ATOMIC_EVENT_H

#include <Python.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct AtomicEvent {
    PyObject_HEAD

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int state;
} AtomicEvent;

PyObject *AtomicEvent_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds));

int AtomicEvent_init(AtomicEvent *self, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs));

void AtomicEvent_dealloc(AtomicEvent *self);

void AtomicEvent_Set(AtomicEvent *self);

PyObject *AtomicEvent_Set_callable(AtomicEvent *self);

int AtomicEvent_IsSet(AtomicEvent *self);

PyObject *AtomicEvent_IsSet_callable(AtomicEvent *self);

void AtomicEvent_Wait(AtomicEvent *self);

PyObject *AtomicEvent_Wait_callable(AtomicEvent *self);

extern PyTypeObject AtomicEvent_Type;

#endif //CEREGGII_ATOMIC_EVENT_H
