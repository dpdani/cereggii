// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include "atomic_partitioned_queue.h"
#include "atomic_ops.h"
#include "internal/misc.h"


PyObject *
AtomicPartitionedQueue_Producer(AtomicPartitionedQueue *self)
{
    AtomicPartitionedQueueProducer *producer = NULL;
    producer = PyObject_GC_New(AtomicPartitionedQueueProducer, &AtomicPartitionedQueueProducer_Type);

    if (producer == NULL)
        return NULL;

    producer->queue = self->queue;
    producer->in_ctx = 0;
    Py_INCREF(self);
    producer->q_ob = self;

    PyObject_GC_Track(producer);
    Py_INCREF(producer); // fixme

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

static inline void
wait_for_rebalance(_AtomicPartitionedQueue *self)
{
    while (self->producers_mx) {
        cereggii_yield();
    }
}

static inline int
try_lock(_AtomicPartitionedQueuePartition *partition)
{
    return CereggiiAtomic_CompareExchangeInt8(&partition->producer.mx, 0, 1);
}

static inline void
unlock(_AtomicPartitionedQueuePartition *partition)
{
    CereggiiAtomic_StoreInt8(&partition->producer.mx, 0);
}

PyObject *
AtomicPartitionedQueueProducer_Put(AtomicPartitionedQueueProducer *self, PyObject *item)
{
beginning:
    if (!self->in_ctx) {
        PyErr_SetString(PyExc_RuntimeError, "producer without context, please see "); // todo
        return NULL;
    }

    _AtomicPartitionedQueuePartition *partition = self->queue->partitions[0];

    if (!try_lock(partition)) {
        wait_for_rebalance(self->queue);
        goto beginning;
    }

    _AtomicPartitionedQueuePage *page = partition->producer.tail_page;

    assert(page != NULL);

    if (partition->producer.tail_offset + 1 >= ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE) {
        assert(page->next == NULL);

        _AtomicPartitionedQueuePage *new_page = NULL;
        new_page = PyMem_RawMalloc(sizeof(_AtomicPartitionedQueuePage));
        if (new_page == NULL) {
            PyErr_NoMemory();
            unlock(partition);
            return NULL;
        }
#ifndef NDEBUG
        memset(new_page->items, 0, sizeof(PyObject *) * ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE);
#endif
        new_page->next = NULL;

        partition->producer.tail_page = new_page;
        partition->producer.tail_offset = -1;

        CereggiiAtomic_StorePtr((void **) &page->next, new_page);
        // "atomic" store is necessary because we don't want
        // the compiler to reorder things after this

        page = new_page;
    }

    Py_INCREF(item);
    int tail_offset = partition->producer.tail_offset + 1;
    page->items[tail_offset] = item;

    CereggiiAtomic_StoreInt(&partition->producer.tail_offset, tail_offset);
    // again, compiler mustn't feel funky

    if (partition->producer.produced == INT64_MAX) {
        // todo: handle overflow
    }
    partition->producer.produced++;

    unlock(partition);

    // todo: wakeup consumer
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
