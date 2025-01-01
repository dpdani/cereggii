// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include "atomic_partitioned_queue.h"


PyObject *
AtomicPartitionedQueue_Producer(AtomicPartitionedQueue *self)
{
    AtomicPartitionedQueueProducer *producer = NULL;
    producer = (AtomicPartitionedQueueProducer *)
        AtomicPartitionedQueueProducer_new(&AtomicPartitionedQueueProducer_Type, NULL, NULL);

    if (producer == NULL)
        return NULL;

    producer->queue = self->queue;
    producer->in_ctx = 0;

    return (PyObject *) producer;
}

PyObject *
AtomicPartitionedQueueProducer_enter(AtomicPartitionedQueueProducer *self, PyObject *args)
{
    self->in_ctx = 1;
    // todo: rebalancing
    return (PyObject *) self;
}

PyObject *
AtomicPartitionedQueueProducer_exit(AtomicPartitionedQueueProducer *self, PyObject *args)
{
    self->in_ctx = 0;
    // todo: rebalancing
    Py_RETURN_NONE;
}

PyObject *
AtomicPartitionedQueueProducer_Put(AtomicPartitionedQueueProducer *self, PyObject *item)
{
    if (!self->in_ctx) {
        PyErr_SetString(PyExc_RuntimeError, "producer without context, please see "); // todo
        return NULL;
    }

    _AtomicPartitionedQueuePartition *partition = self->queue->partitions[0];
    _AtomicPartitionedQueuePage *page = partition->tail_page;

    assert(page != NULL);

    if (partition->tail_offset + 1 >= ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE - 1) {
        // todo: new page
        PyErr_SetNone(PyExc_RuntimeError);
        return NULL;
    }

    Py_INCREF(item);
    page->items[++partition->tail_offset] = item;

    // wakeup consumer
    Py_RETURN_NONE;
}

PyObject *
AtomicPartitionedQueueProducer_PutMany(AtomicPartitionedQueueProducer *self, PyObject *item)
{
    if (!self->in_ctx) {
        PyErr_SetString(PyExc_RuntimeError, "producer without context, please see "); // todo
        return NULL;
    }

    Py_RETURN_NONE;
}
