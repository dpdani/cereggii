// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <cereggii/atomic_partitioned_queue.h>
#include <cereggii/internal/py_core.h>
#include <cereggii/vendor/moodycamel_concurrentqueue/blockingconcurrentqueue.h>

struct AtomicPartitionedQueueImpl {
    moodycamel::BlockingConcurrentQueue<PyObject*> queue;
    std::atomic<bool> closed;

    AtomicPartitionedQueueImpl() : queue(), closed(false) {}
};

PyObject *
AtomicPartitionedQueue_new(PyTypeObject *Py_UNUSED(type), PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds))
{
    AtomicPartitionedQueue *self = PyObject_GC_New(AtomicPartitionedQueue, &AtomicPartitionedQueue_Type);

    if (self == nullptr)
        return nullptr;

    try {
        self->impl = new AtomicPartitionedQueueImpl();
    } catch (const std::bad_alloc&) {
        Py_DECREF(self);
        PyErr_NoMemory();
        return nullptr;
    }

    PyObject_GC_Track(self);

    return reinterpret_cast<PyObject*>(self);
}

int
AtomicPartitionedQueue_init(AtomicPartitionedQueue *Py_UNUSED(self), PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs))
{
    // nothing to be done
    return 0;
}

static int
traverse_block(moodycamel::ConcurrentQueue<PyObject *>::Block *block, visitproc visit, void *arg)
{
    for (size_t i = 0; i < moodycamel::ConcurrentQueueDefaultTraits::BLOCK_SIZE; ++i) {
        if (block->emptyFlags[i])
            continue;

        PyObject *item = *(*block)[i];
        Py_VISIT(item);
    }
    return 0;
}

int
AtomicPartitionedQueue_traverse(AtomicPartitionedQueue *self, visitproc visit, void *arg)
{
    for (size_t block_i = 0; block_i < self->impl->queue.inner.initialBlockPoolSize; ++block_i) {
        moodycamel::ConcurrentQueue<PyObject *>::Block *block = &self->impl->queue.inner.initialBlockPool[block_i];
        while (block != nullptr) {
            traverse_block(block, visit, arg);
            block = block->next;
        }
    }
    return 0;
}

int
AtomicPartitionedQueue_clear(AtomicPartitionedQueue *self)
{
    if (self->impl == nullptr) {
        return 0;
    }
    // Drain the queue and decref all items
    PyObject *item = nullptr;
    while (self->impl->queue.try_dequeue(item)) {
        Py_XDECREF(item);
    }
    return 0;
}

void
AtomicPartitionedQueue_dealloc(AtomicPartitionedQueue *self)
{
    PyObject_GC_UnTrack(self);
    AtomicPartitionedQueue_clear(self);

    auto impl = self->impl;
    if (impl != nullptr) {
        self->impl = nullptr;
        delete impl;
    }

    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}


PyObject *
AtomicPartitionedQueue_Put(AtomicPartitionedQueue *self, PyObject *obj)
{
    if (self->impl->closed.load(std::memory_order_acquire)) {
        PyErr_SetString(PyExc_RuntimeError, "queue is closed");
        return nullptr;
    }

    _Py_SetWeakrefAndIncref(obj);

    if (!self->impl->queue.enqueue(obj)) {
        Py_DECREF(obj);
        PyErr_SetString(PyExc_RuntimeError, "failed to enqueue item");
        return nullptr;
    }

    Py_RETURN_NONE;
}

static void
dequeue_with_timeout(AtomicPartitionedQueue* self, int64_t timeout_usecs, PyObject *&item, bool& success)
{
    Py_BEGIN_ALLOW_THREADS
        success = self->impl->queue.wait_dequeue_timed(item, timeout_usecs);
    Py_END_ALLOW_THREADS
}

static bool
dequeue_wait_indefinitely(AtomicPartitionedQueue* self, PyObject *&item, bool& success)
{
    while (true) {
        dequeue_with_timeout(self, 100000, item, success);
        if (success)
            break;
        if (PyErr_CheckSignals())
            break;
        if (self->impl->closed.load(std::memory_order_acquire))
            return true;
    }
    return false;
}

static PyObject *
get(AtomicPartitionedQueue* self, int block, double timeout)
{
    PyObject *item = nullptr;
    bool success = false;

    if (self->impl->closed.load(std::memory_order_acquire)) {
        goto closed;
    }

    if (block) {
        if (timeout < 0.0) {
            if (dequeue_wait_indefinitely(self, item, success))
                goto closed;
        } else {
            auto timeout_usecs = static_cast<int64_t>(timeout * 1000000.0);  // convert seconds to microseconds
            dequeue_with_timeout(self, timeout_usecs, item, success);
        }
    } else {
        // non-blocking
        success = self->impl->queue.try_dequeue(item);
    }

    if (!success) {
        // timeout or empty queue
        Py_RETURN_NONE;
    }

    // don't INCREF: we're transferring ownership from queue to Python
    return item;
closed:
    PyErr_SetString(PyExc_RuntimeError, "queue is closed");
    return nullptr;
}

PyObject *
AtomicPartitionedQueue_Get(AtomicPartitionedQueue *self, PyObject *args, PyObject *kwargs)
{
    int block = 1;
    double timeout = -1.0;

    const char *kw_list[] = {static_cast<const char*>("block"), static_cast<const char*>("timeout"), nullptr};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|pd", kw_list, &block, &timeout)) {
        return nullptr;
    }

    return get(self, block, timeout);
}

PyObject *
AtomicPartitionedQueue_TryGet(AtomicPartitionedQueue *self)
{
    return get(self, 0, 0.0);
}

PyObject *
AtomicPartitionedQueue_Close(AtomicPartitionedQueue *self)
{
    self->impl->closed.store(true, std::memory_order_release);
    Py_RETURN_NONE;
}

PyObject *
AtomicPartitionedQueue_Closed_Get(AtomicPartitionedQueue *self, void *Py_UNUSED(closure))
{
    bool closed = self->impl->closed.load(std::memory_order_acquire);
    return PyBool_FromLong(closed);
}

PyObject *
AtomicPartitionedQueue_ApproxLen(AtomicPartitionedQueue *self)
{
    size_t len = self->impl->queue.size_approx();
    return PyLong_FromSize_t(len);
}
