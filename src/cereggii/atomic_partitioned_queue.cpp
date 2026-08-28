// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <new>
#include <vector>
#include <cereggii/atomic_partitioned_queue.h>
#include <cereggii/internal/py_core.h>
#include <cereggii/vendor/moodycamel_concurrentqueue/blockingconcurrentqueue.h>

struct AtomicPartitionedQueueImpl {
    moodycamel::BlockingConcurrentQueue<PyObject*> queue;
    std::atomic<bool> closed;

    AtomicPartitionedQueueImpl() : queue(), closed(false) {}
};

struct AtomicPartitionedQueueProducerImpl {
    moodycamel::ProducerToken token;

    explicit AtomicPartitionedQueueProducerImpl(AtomicPartitionedQueueImpl *q) : token(q->queue) {}
};

struct AtomicPartitionedQueueConsumerImpl {
    moodycamel::ConsumerToken token;

    explicit AtomicPartitionedQueueConsumerImpl(AtomicPartitionedQueueImpl *q) : token(q->queue) {}
};

PyObject *
AtomicPartitionedQueue_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds))
{
    AtomicPartitionedQueue *self = reinterpret_cast<AtomicPartitionedQueue*>(type->tp_alloc(type, 0));

    if (self == nullptr) {
        PyErr_NoMemory();
        return nullptr;
    }

    self->impl = nullptr;

    return reinterpret_cast<PyObject*>(self);
}

int
AtomicPartitionedQueue_init(AtomicPartitionedQueue *self, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwargs))
{
    if (self->impl != nullptr) {
        PyErr_SetString(PyExc_RuntimeError, "queue already initialized");
        return -1;
    }

    try {
        self->impl = new AtomicPartitionedQueueImpl();
    } catch (const std::bad_alloc&) {
        PyErr_NoMemory();
        return -1;
    }

    return 0;
}

int
AtomicPartitionedQueue_traverse(AtomicPartitionedQueue *self, visitproc visit, void *arg)
{
    // Traverse all items in the queue using PyDict_Next-like interface
    // Keep iteration state on the stack
    moodycamel::ConcurrentQueue<PyObject*>::TraverseState state;
    PyObject* item;

    while (self->impl->queue.traverse(&state, &item)) {
        Py_VISIT(item);
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


static PyObject *
put(AtomicPartitionedQueue *self, PyObject *obj, moodycamel::ProducerToken *token)
{
    if (self->impl->closed.load(std::memory_order_acquire)) {
        PyErr_SetString(PyExc_RuntimeError, "queue is closed");
        return nullptr;
    }

    _Py_SetWeakrefAndIncref(obj);

    bool ok;
    if (token != nullptr) {
        ok = self->impl->queue.enqueue(*token, obj);
    }
    else {
        ok = self->impl->queue.enqueue(obj);
    }

    if (!ok) {
        Py_DECREF(obj);
        PyErr_SetString(PyExc_RuntimeError, "failed to enqueue item");
        return nullptr;
    }

    Py_RETURN_NONE;
}

PyObject *
AtomicPartitionedQueue_Put(AtomicPartitionedQueue *self, PyObject *obj)
{
    return put(self, obj, nullptr);
}

static void
dequeue_with_timeout(AtomicPartitionedQueue* self, moodycamel::ConsumerToken *token, int64_t timeout_usecs, PyObject *&item, bool& success)
{
    Py_BEGIN_ALLOW_THREADS;
    if (token != nullptr) {
        success = self->impl->queue.wait_dequeue_timed(*token, item, timeout_usecs);
    } else {
        success = self->impl->queue.wait_dequeue_timed(item, timeout_usecs);
    }
    Py_END_ALLOW_THREADS;
}

static bool
dequeue_wait_indefinitely(AtomicPartitionedQueue* self, moodycamel::ConsumerToken *token, PyObject *&item, bool& success)
{
    while (true) {
        dequeue_with_timeout(self, token, 100000, item, success);
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
get(AtomicPartitionedQueue* self, moodycamel::ConsumerToken *token, int block, double timeout)
{
    PyObject *item = nullptr;
    bool success = false;

    if (self->impl->closed.load(std::memory_order_acquire)) {
        goto closed;
    }

    if (block) {
        if (timeout < 0.0) {
            if (dequeue_wait_indefinitely(self, token, item, success))
                goto closed;
        } else {
            auto timeout_usecs = static_cast<int64_t>(timeout * 1000000.0);  // convert seconds to microseconds
            dequeue_with_timeout(self, token, timeout_usecs, item, success);
        }
    } else {
        // non-blocking
        if (token != nullptr) {
            success = self->impl->queue.try_dequeue(*token, item);
        } else {
            success = self->impl->queue.try_dequeue(item);
        }
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

static bool
parse_get_args(PyObject *args, PyObject *kwargs, int &block, double &timeout)
{
    block = 1;
    timeout = -1.0;

    char *kw_list[] = {"block", "timeout", nullptr};

    return (bool)PyArg_ParseTupleAndKeywords(args, kwargs, "|pd", kw_list, &block, &timeout);
}

static PyObject *
put_many(AtomicPartitionedQueue *self, PyObject *iterable, moodycamel::ProducerToken *token)
{
    if (self->impl->closed.load(std::memory_order_acquire)) {
        PyErr_SetString(PyExc_RuntimeError, "queue is closed");
        return nullptr;
    }

    PyObject *seq = PySequence_Fast(iterable, "put_many() requires an iterable");
    if (seq == nullptr) {
        return nullptr;
    }

    Py_ssize_t count = PySequence_Fast_GET_SIZE(seq);
    if (count == 0) {
        Py_DECREF(seq);
        Py_RETURN_NONE;
    }

    PyObject **items_ptr = PySequence_Fast_ITEMS(seq);

    for (Py_ssize_t i = 0; i < count; i++) {
        _Py_SetWeakrefAndIncref(items_ptr[i]);
    }

    bool ok = false;
    try {
        if (token != nullptr) {
            ok = self->impl->queue.enqueue_bulk(*token, items_ptr, static_cast<size_t>(count));
        } else {
            ok = self->impl->queue.enqueue_bulk(items_ptr, static_cast<size_t>(count));
        }
    } catch (const std::bad_alloc&) {
        ok = false;
    }

    if (!ok) {
        for (Py_ssize_t i = 0; i < count; i++) {
            Py_DECREF(items_ptr[i]);
        }
        Py_DECREF(seq);
        PyErr_SetString(PyExc_RuntimeError, "failed to enqueue items");
        return nullptr;
    }

    Py_DECREF(seq);
    Py_RETURN_NONE;
}

static void
dequeue_bulk_with_timeout(AtomicPartitionedQueue *self, moodycamel::ConsumerToken *token, PyObject **out, size_t max_items, int64_t timeout_usecs, size_t &count)
{
    Py_BEGIN_ALLOW_THREADS;
    if (token != nullptr) {
        count = self->impl->queue.wait_dequeue_bulk_timed(*token, out, max_items, timeout_usecs);
    } else {
        count = self->impl->queue.wait_dequeue_bulk_timed(out, max_items, timeout_usecs);
    }
    Py_END_ALLOW_THREADS;
}

static bool
dequeue_bulk_wait_indefinitely(AtomicPartitionedQueue *self, moodycamel::ConsumerToken *token, PyObject **out, size_t max_items, size_t &count)
{
    while (true) {
        dequeue_bulk_with_timeout(self, token, out, max_items, 100000, count);
        if (count > 0)
            break;
        if (PyErr_CheckSignals())
            break;
        if (self->impl->closed.load(std::memory_order_acquire))
            return true;
    }
    return false;
}

static PyObject *
get_many(AtomicPartitionedQueue *self, moodycamel::ConsumerToken *token, Py_ssize_t max_items, int block, double timeout)
{
    assert(max_items >= 1);
    if (self->impl->closed.load(std::memory_order_acquire)) {
        PyErr_SetString(PyExc_RuntimeError, "queue is closed");
        return nullptr;
    }

    std::vector<PyObject*> buffer;
    try {
        buffer.assign(static_cast<size_t>(max_items), nullptr);
    } catch (const std::bad_alloc&) {
        PyErr_NoMemory();
        return nullptr;
    }

    size_t count = 0;
    bool closed = false;

    if (block) {
        if (timeout < 0.0) {
            if (dequeue_bulk_wait_indefinitely(self, token, buffer.data(), static_cast<size_t>(max_items), count)) {
                closed = true;
            }
        } else {
            auto timeout_usecs = static_cast<int64_t>(timeout * 1000000.0);
            dequeue_bulk_with_timeout(self, token, buffer.data(), static_cast<size_t>(max_items), timeout_usecs, count);
        }
    } else {
        if (token != nullptr) {
            count = self->impl->queue.try_dequeue_bulk(*token, buffer.data(), static_cast<size_t>(max_items));
        } else {
            count = self->impl->queue.try_dequeue_bulk(buffer.data(), static_cast<size_t>(max_items));
        }
    }

    if (closed && count == 0) {
        PyErr_SetString(PyExc_RuntimeError, "queue is closed");
        return nullptr;
    }

    PyObject *result = PyList_New(static_cast<Py_ssize_t>(count));
    if (result == nullptr) {
        for (size_t i = 0; i < count; i++) {
            Py_DECREF(buffer[i]);
        }
        return nullptr;
    }

    for (size_t i = 0; i < count; i++) {
        // transfer ownership from queue to list
        PyList_SET_ITEM(result, static_cast<Py_ssize_t>(i), buffer[i]);
    }

    return result;
}

static bool
parse_get_many_args(PyObject *args, PyObject *kwargs, Py_ssize_t &max_items, int &block, double &timeout)
{
    block = 1;
    timeout = -1.0;
    max_items = 0;

    char *kw_list[] = {"max_items", "block", "timeout", nullptr};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "n|pd", kw_list, &max_items, &block, &timeout)) {
        return false;
    }
    if (max_items < 1) {
        PyErr_SetString(PyExc_ValueError, "max_items must be >= 1");
        return false;
    }
    return true;
}

static bool
parse_try_get_many_args(PyObject *args, PyObject *kwargs, Py_ssize_t &max_items)
{
    char *kw_list[] = {"max_items", nullptr};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "n", kw_list, &max_items)) {
        return false;
    }
    if (max_items < 1) {
        PyErr_SetString(PyExc_ValueError, "max_items must be >= 1");
        return false;
    }
    return true;
}

PyObject *
AtomicPartitionedQueue_Get(AtomicPartitionedQueue *self, PyObject *args, PyObject *kwargs)
{
    int block;
    double timeout;
    if (!parse_get_args(args, kwargs, block, timeout)) {
        return nullptr;
    }

    return get(self, nullptr, block, timeout);
}

PyObject *
AtomicPartitionedQueue_TryGet(AtomicPartitionedQueue *self)
{
    return get(self, nullptr, 0, 0.0);
}

PyObject *
AtomicPartitionedQueue_PutMany(AtomicPartitionedQueue *self, PyObject *iterable)
{
    return put_many(self, iterable, nullptr);
}

PyObject *
AtomicPartitionedQueue_GetMany(AtomicPartitionedQueue *self, PyObject *args, PyObject *kwargs)
{
    Py_ssize_t max_items;
    int block;
    double timeout;
    if (!parse_get_many_args(args, kwargs, max_items, block, timeout)) {
        return nullptr;
    }

    return get_many(self, nullptr, max_items, block, timeout);
}

PyObject *
AtomicPartitionedQueue_TryGetMany(AtomicPartitionedQueue *self, PyObject *args, PyObject *kwargs)
{
    Py_ssize_t max_items;
    if (!parse_try_get_many_args(args, kwargs, max_items)) {
        return nullptr;
    }

    return get_many(self, nullptr, max_items, 0, 0.0);
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

/// Producer

PyObject *
AtomicPartitionedQueue_Producer(AtomicPartitionedQueue *self, PyObject *Py_UNUSED(args))
{
    AtomicPartitionedQueueProducer *producer = reinterpret_cast<AtomicPartitionedQueueProducer*>(
        AtomicPartitionedQueueProducer_Type.tp_alloc(&AtomicPartitionedQueueProducer_Type, 0)
    );
    if (producer == nullptr) {
        PyErr_NoMemory();
        return nullptr;
    }

    producer->impl = nullptr;
    producer->queue = nullptr;

    try {
        producer->impl = new AtomicPartitionedQueueProducerImpl(self->impl);
    } catch (const std::bad_alloc&) {
        Py_DECREF(producer);
        PyErr_NoMemory();
        return nullptr;
    }

    Py_INCREF(self);
    producer->queue = self;

    return reinterpret_cast<PyObject*>(producer);
}

int
AtomicPartitionedQueueProducer_traverse(AtomicPartitionedQueueProducer *self, visitproc visit, void *arg)
{
    Py_VISIT(self->queue);
    return 0;
}

int
AtomicPartitionedQueueProducer_clear(AtomicPartitionedQueueProducer *self)
{
    // The token's destructor accesses the queue's internal state,
    // so destroy the token before releasing the queue reference.
    auto impl = self->impl;
    if (impl != nullptr) {
        self->impl = nullptr;
        delete impl;
    }
    Py_CLEAR(self->queue);
    return 0;
}

void
AtomicPartitionedQueueProducer_dealloc(AtomicPartitionedQueueProducer *self)
{
    PyObject_GC_UnTrack(self);
    AtomicPartitionedQueueProducer_clear(self);
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject *
AtomicPartitionedQueueProducer_Put(AtomicPartitionedQueueProducer *self, PyObject *obj)
{
    return put(self->queue, obj, &self->impl->token);
}

PyObject *
AtomicPartitionedQueueProducer_PutMany(AtomicPartitionedQueueProducer *self, PyObject *iterable)
{
    return put_many(self->queue, iterable, &self->impl->token);
}

PyObject *
AtomicPartitionedQueueProducer_Enter(AtomicPartitionedQueueProducer *self, PyObject *Py_UNUSED(args))
{
    Py_INCREF(self);
    return reinterpret_cast<PyObject*>(self);
}

PyObject *
AtomicPartitionedQueueProducer_Exit(AtomicPartitionedQueueProducer *Py_UNUSED(self), PyObject *Py_UNUSED(args))
{
    Py_RETURN_NONE;
}

/// Consumer

PyObject *
AtomicPartitionedQueue_Consumer(AtomicPartitionedQueue *self, PyObject *Py_UNUSED(args))
{
    AtomicPartitionedQueueConsumer *consumer = reinterpret_cast<AtomicPartitionedQueueConsumer*>(
        AtomicPartitionedQueueConsumer_Type.tp_alloc(&AtomicPartitionedQueueConsumer_Type, 0)
    );
    if (consumer == nullptr) {
        PyErr_NoMemory();
        return nullptr;
    }

    consumer->impl = nullptr;
    consumer->queue = nullptr;

    try {
        consumer->impl = new AtomicPartitionedQueueConsumerImpl(self->impl);
    } catch (const std::bad_alloc&) {
        Py_DECREF(consumer);
        PyErr_NoMemory();
        return nullptr;
    }

    Py_INCREF(self);
    consumer->queue = self;

    return reinterpret_cast<PyObject*>(consumer);
}

int
AtomicPartitionedQueueConsumer_traverse(AtomicPartitionedQueueConsumer *self, visitproc visit, void *arg)
{
    Py_VISIT(self->queue);
    return 0;
}

int
AtomicPartitionedQueueConsumer_clear(AtomicPartitionedQueueConsumer *self)
{
    auto impl = self->impl;
    if (impl != nullptr) {
        self->impl = nullptr;
        delete impl;
    }
    Py_CLEAR(self->queue);
    return 0;
}

void
AtomicPartitionedQueueConsumer_dealloc(AtomicPartitionedQueueConsumer *self)
{
    PyObject_GC_UnTrack(self);
    AtomicPartitionedQueueConsumer_clear(self);
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject *
AtomicPartitionedQueueConsumer_Get(AtomicPartitionedQueueConsumer *self, PyObject *args, PyObject *kwargs)
{
    int block;
    double timeout;
    if (!parse_get_args(args, kwargs, block, timeout)) {
        return nullptr;
    }

    return get(self->queue, &self->impl->token, block, timeout);
}

PyObject *
AtomicPartitionedQueueConsumer_TryGet(AtomicPartitionedQueueConsumer *self)
{
    return get(self->queue, &self->impl->token, 0, 0.0);
}

PyObject *
AtomicPartitionedQueueConsumer_GetMany(AtomicPartitionedQueueConsumer *self, PyObject *args, PyObject *kwargs)
{
    Py_ssize_t max_items;
    int block;
    double timeout;
    if (!parse_get_many_args(args, kwargs, max_items, block, timeout)) {
        return nullptr;
    }

    return get_many(self->queue, &self->impl->token, max_items, block, timeout);
}

PyObject *
AtomicPartitionedQueueConsumer_TryGetMany(AtomicPartitionedQueueConsumer *self, PyObject *args, PyObject *kwargs)
{
    Py_ssize_t max_items;
    if (!parse_try_get_many_args(args, kwargs, max_items)) {
        return nullptr;
    }

    return get_many(self->queue, &self->impl->token, max_items, 0, 0.0);
}

PyObject *
AtomicPartitionedQueueConsumer_Enter(AtomicPartitionedQueueConsumer *self, PyObject *Py_UNUSED(args))
{
    Py_INCREF(self);
    return reinterpret_cast<PyObject*>(self);
}

PyObject *
AtomicPartitionedQueueConsumer_Exit(AtomicPartitionedQueueConsumer *Py_UNUSED(self), PyObject *Py_UNUSED(args))
{
    Py_RETURN_NONE;
}
