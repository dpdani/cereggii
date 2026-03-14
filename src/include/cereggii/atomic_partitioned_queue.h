// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_PARTITIONED_QUEUE_H
#define CEREGGII_ATOMIC_PARTITIONED_QUEUE_H

#define PY_SSIZE_T_CLEAN

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration of C++ implementation
struct AtomicPartitionedQueueImpl;

typedef struct atomic_partitioned_queue {
    PyObject_HEAD

    struct AtomicPartitionedQueueImpl *impl;
} AtomicPartitionedQueue;

PyObject *AtomicPartitionedQueue_Put(AtomicPartitionedQueue *self, PyObject *obj);

PyObject *AtomicPartitionedQueue_Get(AtomicPartitionedQueue *self, PyObject *args, PyObject *kwargs);

PyObject *AtomicPartitionedQueue_TryGet(AtomicPartitionedQueue *self);

PyObject *AtomicPartitionedQueue_Close(AtomicPartitionedQueue *self);

PyObject *AtomicPartitionedQueue_Closed_Get(AtomicPartitionedQueue *self, void *closure);

PyObject *AtomicPartitionedQueue_ApproxLen(AtomicPartitionedQueue *self);


PyObject *AtomicPartitionedQueue_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

int AtomicPartitionedQueue_init(AtomicPartitionedQueue *self, PyObject *args, PyObject *kwargs);

int AtomicPartitionedQueue_traverse(AtomicPartitionedQueue *self, visitproc visit, void *arg);

int AtomicPartitionedQueue_clear(AtomicPartitionedQueue *self);

void AtomicPartitionedQueue_dealloc(AtomicPartitionedQueue *self);

extern PyTypeObject AtomicPartitionedQueue_Type;

#ifdef __cplusplus
}
#endif

#endif // CEREGGII_ATOMIC_PARTITIONED_QUEUE_H
