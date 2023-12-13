// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_DICT_H
#define CEREGGII_ATOMIC_DICT_H

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <structmember.h>
#include "atomic_ref.h"


typedef struct {
    PyObject_HEAD

    AtomicRef *metadata;
    AtomicRef *new_gen_metadata;

    uint8_t min_log_size;

    uint8_t reservation_buffer_size;
    Py_tss_t *tss_key;
    PyObject *reservation_buffers; // PyListObject
} AtomicDict;


PyObject *AtomicDict_GetItemOrDefault(AtomicDict *self, PyObject *key, PyObject *default_value);

PyObject *AtomicDict_GetItemOrDefaultVarargs(AtomicDict *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicDict_GetItem(AtomicDict *self, PyObject *key);

int AtomicDict_SetItem(AtomicDict *self, PyObject *key, PyObject *value);

int AtomicDict_DelItem(AtomicDict *self, PyObject *key);

PyObject *AtomicDict_Debug(AtomicDict *self);

PyObject *AtomicDict_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

int AtomicDict_init(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_traverse(AtomicDict *self, visitproc visit, void *arg);

void AtomicDict_dealloc(AtomicDict *self);


#endif //CEREGGII_ATOMIC_DICT_H
