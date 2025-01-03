// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_PARTITIONED_QUEUE_H
#define CEREGGII_ATOMIC_PARTITIONED_QUEUE_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "internal/cereggiiconfig.h"


#define ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE (512 - 1)

typedef struct _AtomicPartitionedQueuePage {
  PyObject *items[ATOMIC_PARTITIONED_QUEUE_PAGE_SIZE];
  struct _AtomicPartitionedQueuePage *next;
} _AtomicPartitionedQueuePage;

typedef struct {
  _AtomicPartitionedQueuePage *head_page;
  int head_offset;

  /* The padding makes it so that while a producer modifies the tail, it doesn't
     disturb a consumer modifying the head. */
  int8_t _padding[LEVEL1_DCACHE_LINESIZE - sizeof(_AtomicPartitionedQueuePage *) - sizeof(int)];

  _AtomicPartitionedQueuePage *tail_page;
  int tail_offset;
} _AtomicPartitionedQueuePartition;

typedef struct {
  int num_partitions;
  _AtomicPartitionedQueuePartition **partitions;

  // PyMutex *producers_mx;
  // PyMutex *consumers_mx;
} _AtomicPartitionedQueue;

typedef struct {
  PyObject_HEAD

  _AtomicPartitionedQueue *queue;
} AtomicPartitionedQueue;

typedef struct {
  PyObject_HEAD

  _AtomicPartitionedQueue *queue;
  AtomicPartitionedQueue *q_ob;
  int in_ctx;
} AtomicPartitionedQueueProducer;

typedef struct {
  PyObject_HEAD

  _AtomicPartitionedQueue *queue;
  AtomicPartitionedQueue *q_ob;
  int in_ctx;
} AtomicPartitionedQueueConsumer;

extern PyTypeObject AtomicPartitionedQueue_Type;
extern PyTypeObject AtomicPartitionedQueueProducer_Type;
extern PyTypeObject AtomicPartitionedQueueConsumer_Type;

PyObject *AtomicPartitionedQueue_Producer(AtomicPartitionedQueue *self);

PyObject *AtomicPartitionedQueue_Consumer(AtomicPartitionedQueue *self);

PyObject *AtomicPartitionedQueue_Debug(AtomicPartitionedQueue *self);

PyObject *AtomicPartitionedQueue_NumPartitions(AtomicPartitionedQueue *self, void *closure);

PyObject *AtomicPartitionedQueue_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

int AtomicPartitionedQueue_init(AtomicPartitionedQueue *self, PyObject *args, PyObject *kwargs);

int AtomicPartitionedQueue_traverse(AtomicPartitionedQueue *self, visitproc visit, void *arg);

int AtomicPartitionedQueue_clear(AtomicPartitionedQueue *self);

void AtomicPartitionedQueue_dealloc(AtomicPartitionedQueue *self);


/// producer
PyObject *AtomicPartitionedQueueProducer_enter(AtomicPartitionedQueueProducer *self, PyObject *args);

PyObject *AtomicPartitionedQueueProducer_exit(AtomicPartitionedQueueProducer *self, PyObject *args);

PyObject *AtomicPartitionedQueueProducer_Put(AtomicPartitionedQueueProducer *self, PyObject *item);

PyObject *AtomicPartitionedQueueProducer_PutMany(AtomicPartitionedQueueProducer *self, PyObject *item);

PyObject *AtomicPartitionedQueueProducer_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

int AtomicPartitionedQueueProducer_init(AtomicPartitionedQueueProducer *self, PyObject *args, PyObject *kwargs);

int AtomicPartitionedQueueProducer_traverse(AtomicPartitionedQueueProducer *self, visitproc visit, void *arg);

int AtomicPartitionedQueueProducer_clear(AtomicPartitionedQueueProducer *self);

void AtomicPartitionedQueueProducer_dealloc(AtomicPartitionedQueueProducer *self);


/// consumer
PyObject *AtomicPartitionedQueueConsumer_enter(AtomicPartitionedQueueConsumer *self, PyObject *args);

PyObject *AtomicPartitionedQueueConsumer_exit(AtomicPartitionedQueueConsumer *self, PyObject *args);

PyObject *AtomicPartitionedQueueConsumer_Get(AtomicPartitionedQueueConsumer *self);

PyObject *AtomicPartitionedQueueConsumer_GetMany(AtomicPartitionedQueueConsumer *self, PyObject *size);

PyObject *AtomicPartitionedQueueConsumer_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

int AtomicPartitionedQueueConsumer_init(AtomicPartitionedQueueConsumer *self, PyObject *args, PyObject *kwargs);

int AtomicPartitionedQueueConsumer_traverse(AtomicPartitionedQueueConsumer *self, visitproc visit, void *arg);

int AtomicPartitionedQueueConsumer_clear(AtomicPartitionedQueueConsumer *self);

void AtomicPartitionedQueueConsumer_dealloc(AtomicPartitionedQueueConsumer *self);


/// module

int _AtomicPartitionedQueue_mod_init(PyObject *m);

#endif //CEREGGII_ATOMIC_PARTITIONED_QUEUE_H
