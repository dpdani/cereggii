// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include "atomic_partitioned_queue.h"


static PyMethodDef AtomicPartitionedQueue_methods[] = {
    {"_debug", (PyCFunction) AtomicPartitionedQueue_Debug, METH_NOARGS, NULL},
    {"producer", (PyCFunction) AtomicPartitionedQueue_Producer, METH_NOARGS, NULL},
    {"consumer", (PyCFunction) AtomicPartitionedQueue_Consumer, METH_NOARGS, NULL},
    {NULL}
};

static PyGetSetDef AtomicPartitionedQueue_getset[] = {
    {"num_partitions", (getter)AtomicPartitionedQueue_NumPartitions, NULL, NULL, NULL},
    {NULL}
};

PyTypeObject AtomicPartitionedQueue_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicPartitionedQueue",
    .tp_doc = PyDoc_STR("A thread-safe FIFO queue, partitioned to enhance parallelism."),
    .tp_basicsize = sizeof(AtomicPartitionedQueue),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = AtomicPartitionedQueue_new,
    .tp_traverse = (traverseproc) AtomicPartitionedQueue_traverse,
    .tp_clear = (inquiry) AtomicPartitionedQueue_clear,
    .tp_dealloc = (destructor) AtomicPartitionedQueue_dealloc,
    .tp_init = (initproc) AtomicPartitionedQueue_init,
    .tp_methods = AtomicPartitionedQueue_methods,
    .tp_getset = AtomicPartitionedQueue_getset,
};


static PyMethodDef AtomicPartitionedQueueProducer_methods[] = {
    {"__enter__", (PyCFunction)AtomicPartitionedQueueProducer_enter, METH_NOARGS, NULL},
    {"__exit__", (PyCFunction)AtomicPartitionedQueueProducer_exit, METH_VARARGS, NULL},
    {"put", (PyCFunction) AtomicPartitionedQueueProducer_Put, METH_O, NULL},
    {"put_many", (PyCFunction) AtomicPartitionedQueueProducer_PutMany, METH_O, NULL},
    {NULL}
};

PyTypeObject AtomicPartitionedQueueProducer_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicPartitionedQueueProducer",
    .tp_doc = PyDoc_STR("A producer instance for cereggii.AtomicPartitionedQueue."),
    .tp_basicsize = sizeof(AtomicPartitionedQueueProducer),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = AtomicPartitionedQueueProducer_new,
    .tp_traverse = (traverseproc) AtomicPartitionedQueueProducer_traverse,
    .tp_clear = (inquiry) AtomicPartitionedQueueProducer_clear,
    .tp_dealloc = (destructor) AtomicPartitionedQueueProducer_dealloc,
    // .tp_init = (initproc) AtomicPartitionedQueueProducer_init,
    .tp_methods = AtomicPartitionedQueueProducer_methods,
};


static PyMethodDef AtomicPartitionedQueueConsumer_methods[] = {
    {"__enter__", (PyCFunction)AtomicPartitionedQueueConsumer_enter, METH_NOARGS, NULL},
    {"__exit__", (PyCFunction)AtomicPartitionedQueueConsumer_exit, METH_VARARGS, NULL},
    {"get", (PyCFunction) AtomicPartitionedQueueConsumer_Get, METH_NOARGS, NULL},
    {"get_many", (PyCFunction) AtomicPartitionedQueueConsumer_GetMany, METH_O, NULL},
    {NULL}
};

PyTypeObject AtomicPartitionedQueueConsumer_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii.AtomicPartitionedQueueConsumer",
    .tp_doc = PyDoc_STR("A consumer instance for cereggii.AtomicPartitionedQueue."),
    .tp_basicsize = sizeof(AtomicPartitionedQueueConsumer),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_new = AtomicPartitionedQueueConsumer_new,
    .tp_traverse = (traverseproc) AtomicPartitionedQueueConsumer_traverse,
    .tp_clear = (inquiry) AtomicPartitionedQueueConsumer_clear,
    .tp_dealloc = (destructor) AtomicPartitionedQueueConsumer_dealloc,
    // .tp_init = (initproc) AtomicPartitionedQueueConsumer_init,
    .tp_methods = AtomicPartitionedQueueConsumer_methods,
};


int
_AtomicPartitionedQueue_mod_init(PyObject *m)
{
    if (PyType_Ready(&AtomicPartitionedQueue_Type) < 0)
        goto fail;
    if (PyType_Ready(&AtomicPartitionedQueueProducer_Type) < 0)
        goto fail;
    if (PyType_Ready(&AtomicPartitionedQueueConsumer_Type) < 0)
        goto fail;

    if (PyModule_AddObjectRef(m, "AtomicPartitionedQueue", (PyObject *) &AtomicPartitionedQueue_Type) < 0)
        goto fail;
    Py_DECREF(&AtomicPartitionedQueue_Type);

    if (PyModule_AddObjectRef(m, "AtomicPartitionedQueueProducer",
                              (PyObject *) &AtomicPartitionedQueueProducer_Type) < 0)
        goto fail;
    Py_DECREF(&AtomicPartitionedQueue_Type);

    if (PyModule_AddObjectRef(m, "AtomicPartitionedQueueConsumer",
                              (PyObject *) &AtomicPartitionedQueueConsumer_Type) < 0)
        goto fail;
    Py_DECREF(&AtomicPartitionedQueue_Type);

    return 0;
fail:
    return -1;
}


PyObject *
AtomicPartitionedQueue_Debug(AtomicPartitionedQueue *self)
{
    Py_RETURN_NONE;
}

PyObject *
AtomicPartitionedQueue_NumPartitions(AtomicPartitionedQueue *self, void *Py_UNUSED(closure))
{
    return PyLong_FromLong(self->queue->num_partitions);
}


PyObject *
AtomicPartitionedQueue_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    AtomicPartitionedQueue *self = NULL;
    self = PyObject_GC_New(AtomicPartitionedQueue, &AtomicPartitionedQueue_Type);
    if (self == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    _AtomicPartitionedQueue *queue = NULL;
    queue = PyMem_RawMalloc(sizeof(_AtomicPartitionedQueue));
    if (queue == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    self->queue = queue;
    self->queue->partitions = NULL;

    PyObject_GC_Track(self);

    return (PyObject *) self;
}

int
AtomicPartitionedQueue_init(AtomicPartitionedQueue *self, PyObject *args, PyObject *kwargs)
{
    char *kwlist[] = {"num_partitions", NULL};
    int num_partitions = 1; // todo: choose a better default
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|i", kwlist, &num_partitions)) {
        goto fail;
    }

    if (num_partitions < 1) {
        PyErr_SetString(PyExc_ValueError, "num_partitions < 1");
        goto fail;
    }

    assert(num_partitions == 1); // fixme

    self->queue->consumers_mx = 0;
    self->queue->producers_mx = 0;

    self->queue->num_partitions = num_partitions;
    self->queue->partitions = PyMem_RawMalloc(sizeof(_AtomicPartitionedQueuePartition *) * num_partitions);
    if (self->queue->partitions == NULL) {
        PyErr_NoMemory();
        goto fail;
    }

    for (int i = 0; i < num_partitions; i++) {
        _AtomicPartitionedQueuePartition *partition = NULL;
        partition = PyMem_RawMalloc(sizeof(_AtomicPartitionedQueuePartition));

        if (partition == NULL) {
            PyErr_NoMemory();
            goto fail;
        }

        _AtomicPartitionedQueuePage *page = NULL;
        page = PyMem_RawMalloc(sizeof(_AtomicPartitionedQueuePage));

        if (page == NULL) {
            PyErr_NoMemory();
            goto fail;
        }

        page->next = NULL;
#ifndef NDEBUG
        memset(page->items, 0, sizeof(PyObject *) * ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE);
#endif

        partition->consumer.head_page = page;
        partition->producer.tail_page = page;
        partition->consumer.head_offset = -1;
        partition->producer.tail_offset = -1;
        partition->consumer.consumed = 0;
        partition->producer.produced = 0;
        partition->consumer.mx = 0;
        partition->producer.mx = 0;

        self->queue->partitions[i] = partition;
    }

    return 0;
fail:
    return -1;
}

int
AtomicPartitionedQueue_traverse(AtomicPartitionedQueue *self, visitproc visit, void *arg)
{
    if (self->queue == NULL || self->queue->partitions == NULL)
        return 0;

    for (int p = 0; p < self->queue->num_partitions; p++) {
        _AtomicPartitionedQueuePartition *partition = self->queue->partitions[p];

        for (int i = partition->consumer.head_offset; i >= 0 && i < ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE; i++) {
            Py_VISIT(partition->consumer.head_page->items[i]);
        }

        for (_AtomicPartitionedQueuePage *page = partition->consumer.head_page; page != partition->producer.tail_page; page = page->next) {
            for (int i = 0; i < ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE; i++) {
                Py_VISIT(page->items[i]);
            }
        }

        if (partition->consumer.head_page != partition->producer.tail_page) {
            for (int i = 0; i < partition->producer.tail_offset; i++) {
                Py_VISIT(partition->producer.tail_page->items[i]);
            }
        }
    }

    return 0;
}

int
AtomicPartitionedQueue_clear(AtomicPartitionedQueue *self)
{
    if (self->queue == NULL || self->queue->partitions == NULL)
        return 0;

    _AtomicPartitionedQueuePartition *partition = NULL;
    for (int p = 0; p < self->queue->num_partitions; p++) {
        partition = self->queue->partitions[p];

        int end_offset = ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE;
        if (partition->producer.tail_page == partition->consumer.head_page) {
            end_offset = partition->producer.tail_offset;
        }

        for (int i = partition->consumer.head_offset; i >= 0 && i < end_offset; i++) {
            Py_CLEAR(partition->consumer.head_page->items[i]);
        }

        for (_AtomicPartitionedQueuePage *page = partition->consumer.head_page; page != partition->producer.tail_page; page = page->next) {
            for (int i = 0; i < ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE; i++) {
                Py_CLEAR(page->items[i]);
            }
        }

        if (partition->consumer.head_page != partition->producer.tail_page) {
            for (int i = 0; i < partition->producer.tail_offset; i++) {
                Py_CLEAR(partition->producer.tail_page->items[i]);
            }
        }
    }

    return 0;
}

void
AtomicPartitionedQueue_dealloc(AtomicPartitionedQueue *self)
{
    PyObject_GC_UnTrack(self);
    AtomicPartitionedQueue_clear(self);

    if (self->queue) {
        if (self->queue->partitions) {
            for (int part = 0; part < self->queue->num_partitions; part++) {
                _AtomicPartitionedQueuePartition *partition = self->queue->partitions[part];

                _AtomicPartitionedQueuePage *page = partition->consumer.head_page;
                for (_AtomicPartitionedQueuePage *next = page->next; next; next = page->next) {
                    PyMem_RawFree(page);
                    page = next;
                }
                PyMem_RawFree(page);

                PyMem_RawFree(partition);
            }

            PyMem_RawFree(self->queue->partitions);
        }

        PyMem_RawFree(self->queue);
    }

    Py_TYPE(self)->tp_free((PyObject *) self);
}


PyObject *
AtomicPartitionedQueueProducer_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    AtomicPartitionedQueueProducer *self = NULL;
    self = PyObject_GC_New(AtomicPartitionedQueueProducer, &AtomicPartitionedQueueProducer_Type);

    return (PyObject *) self;
}

int
AtomicPartitionedQueueProducer_init(AtomicPartitionedQueueProducer *self, PyObject *args, PyObject *kwargs)
{
    return 0;
}

int
AtomicPartitionedQueueProducer_traverse(AtomicPartitionedQueueProducer *self, visitproc visit, void *arg)
{
    Py_VISIT(self->q_ob);
    return 0;
}

int
AtomicPartitionedQueueProducer_clear(AtomicPartitionedQueueProducer *self)
{
    Py_CLEAR(self->q_ob);
    return 0;
}

void
AtomicPartitionedQueueProducer_dealloc(AtomicPartitionedQueueProducer *self)
{
    PyObject_GC_UnTrack(self);
    AtomicPartitionedQueueProducer_clear(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}


PyObject *
AtomicPartitionedQueueConsumer_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    AtomicPartitionedQueueConsumer *self = NULL;
    self = PyObject_GC_New(AtomicPartitionedQueueConsumer, &AtomicPartitionedQueueConsumer_Type);

    return (PyObject *) self;
}

int
AtomicPartitionedQueueConsumer_init(AtomicPartitionedQueueConsumer *self, PyObject *args, PyObject *kwargs)
{
    return 0;
}

int
AtomicPartitionedQueueConsumer_traverse(AtomicPartitionedQueueConsumer *self, visitproc visit, void *arg)
{
    Py_VISIT(self->q_ob);
    return 0;
}

int
AtomicPartitionedQueueConsumer_clear(AtomicPartitionedQueueConsumer *self)
{
    Py_CLEAR(self->q_ob);
    return 0;
}

void
AtomicPartitionedQueueConsumer_dealloc(AtomicPartitionedQueueConsumer *self)
{
    PyObject_GC_UnTrack(self);
    AtomicPartitionedQueueConsumer_clear(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}
