// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include "atomic_partitioned_queue.h"


PyObject *
AtomicPartitionedQueue_Consumer(AtomicPartitionedQueue *self)
{
    AtomicPartitionedQueueConsumer *consumer = NULL;
    consumer = PyObject_GC_New(AtomicPartitionedQueueConsumer, &AtomicPartitionedQueueConsumer_Type);

    if (consumer == NULL)
        return NULL;

    consumer->queue = self->queue;
    consumer->in_ctx = 0;
    Py_INCREF(self);
    consumer->q_ob = self;

    PyObject_GC_Track(consumer);
    Py_INCREF(consumer); // fixme

    return (PyObject *) consumer;
}

PyObject *
AtomicPartitionedQueueConsumer_enter(AtomicPartitionedQueueConsumer *self, PyObject *args)
{
    self->in_ctx = 1;
    // todo: rebalancing
    return (PyObject *) self;
}

PyObject *
AtomicPartitionedQueueConsumer_exit(AtomicPartitionedQueueConsumer *self, PyObject *args)
{
    self->in_ctx = 0;
    // todo: rebalancing
    Py_RETURN_NONE;
}

PyObject *
AtomicPartitionedQueueConsumer_Get(AtomicPartitionedQueueConsumer *self)
{
    if (!self->in_ctx) {
        PyErr_SetString(PyExc_RuntimeError, "consumer without context, please see "); // todo
        return NULL;
    }

    _AtomicPartitionedQueuePartition *partition = self->queue->partitions[0];
    _AtomicPartitionedQueuePage *page = partition->consumer.head_page;

    assert(page != NULL);

    if (partition->consumer.head_offset + 1 >= ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE) {
        while (page->next == NULL) {}

        _AtomicPartitionedQueuePage *next = page->next;

        partition->consumer.head_page = next;
        partition->consumer.head_offset = -1;

        PyMem_RawFree(page);
        page = next;
    }

    PyObject *item = NULL;
    item = page->items[++partition->consumer.head_offset];

    if (partition->consumer.consumed == INT64_MAX) {
        // todo: handle overflow
    }
    partition->consumer.consumed++;

    // don't decref: passing to python

    assert(item != NULL);

    // todo: wakeup producer
    return item;
}

PyObject *
AtomicPartitionedQueueConsumer_GetMany(AtomicPartitionedQueueConsumer *self, PyObject *size)
{
    if (!self->in_ctx) {
        PyErr_SetString(PyExc_RuntimeError, "consumer without context, please see "); // todo
        return NULL;
    }

    Py_RETURN_NONE;
}
