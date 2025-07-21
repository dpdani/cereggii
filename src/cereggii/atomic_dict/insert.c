// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "constants.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "atomic_ops.h"
#include "pythread.h"
#include "_internal_py_core.h"


int
AtomicDict_ExpectedUpdateEntry(AtomicDict_Meta *meta, uint64_t entry_ix,
                               PyObject *key, Py_hash_t hash,
                               PyObject *expected, PyObject *desired, PyObject **current,
                               int *done, int *expectation)
{
    AtomicDict_Entry *entry_p, entry;
    entry_p = AtomicDict_GetEntryAt(entry_ix, meta);
    AtomicDict_ReadEntry(entry_p, &entry);

    if (hash != entry.hash)
        return 0;

    int eq = 0;
    if (entry.key != key) {
        eq = PyObject_RichCompareBool(entry.key, key, Py_EQ);

        if (eq < 0)  // exception raised during compare
            goto fail;
    }

    if (entry.key == key || eq) {
        if (expected == NOT_FOUND) {
            if (entry.value != NULL) {
                *done = 1;
                *expectation = 0;
                return 1;
            }
            // expected == NOT_FOUND && value == NULL:
            //   it means there's another thread T concurrently
            //   deleting this key.
            //   T's linearization point (setting value = NULL) has
            //   already been reached, thus we can proceed visiting
            //   the probe.
        } else {
            // expected != NOT_FOUND
            do {
                if (entry.value != expected && expected != ANY) {
                    *done = 1;
                    *expectation = 0;
                    return 1;
                }

                *current = entry.value;
                *done = CereggiiAtomic_CompareExchangePtr((void **) &entry_p->value, *current, desired);

                if (!*done) {
                    AtomicDict_ReadEntry(entry_p, &entry);

                    if (*current == NULL)
                        return 0;
                }
            } while (!*done);

            if (!*done) {
                *current = NULL;
                return 0;
            }

            return 1;
        }
    }

    return 0;
    fail:
    return -1;
}


PyObject *
AtomicDict_ExpectedInsertOrUpdate(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash,
                                  PyObject *expected, PyObject *desired,
                                  AtomicDict_EntryLoc *entry_loc, int *must_grow,
                                  int skip_entry_check)
{
    assert(meta != NULL);
    assert(key != NULL);
    assert(key != NOT_FOUND);
    assert(key != ANY);
    assert(key != EXPECTATION_FAILED);
    assert(hash != -1);
    assert(expected != NULL);
    assert(expected != EXPECTATION_FAILED);
    assert(desired != NULL);
    assert(desired != NOT_FOUND);
    assert(desired != ANY);
    assert(desired != EXPECTATION_FAILED);
    // assert(entry_loc == NULL => (expected != NOT_FOUND && expected != ANY));
    assert(entry_loc != NULL || (expected != NOT_FOUND && expected != ANY));

    int done, expectation;
    *must_grow = 0;

    uint64_t distance_0 = AtomicDict_Distance0Of(hash, meta);
    AtomicDict_Node node;

    done = 0;
    expectation = 1;
    uint64_t distance = 0;
    PyObject *current = NULL;
    AtomicDict_Node to_insert;

    while (!done) {
        uint64_t ix = (distance_0 + distance) & ((1 << meta->log_size) - 1);
        AtomicDict_ReadNodeAt(ix, &node, meta);

        if (node.node == 0) {
            if (expected != NOT_FOUND && expected != ANY) {
                expectation = 0;
                break;
            }
            assert(entry_loc != NULL);

            to_insert.index = entry_loc->location;
            to_insert.tag = hash;

            done = AtomicDict_AtomicWriteNodeAt(ix, &node, &to_insert, meta);

            if (!done)
                continue;  // don't increase distance
        } else if (node.node == TOMBSTONE(meta)) {
            // pass
        } else if (node.tag != (hash & TAG_MASK(meta))) {
            // pass
        } else if (!skip_entry_check) {
            int updated = AtomicDict_ExpectedUpdateEntry(meta, node.index, key, hash, expected, desired,
                                                         &current, &done, &expectation);
            if (updated < 0)
                goto fail;

            if (updated)
                break;
        }

        distance++;

        if (distance >= (1 << meta->log_size)) {
            // traversed the entire dictionary without finding an empty slot
            *must_grow = 1;
            goto fail;
        }
    }

    // assert(expected == ANY => expectation == 1);
    assert(expected != ANY || expectation == 1);

    if (expectation && expected == NOT_FOUND) {
        Py_INCREF(NOT_FOUND);
        return NOT_FOUND;
    }
    if (expectation && expected == ANY) {
        if (current == NULL) {
            Py_INCREF(NOT_FOUND);
            return NOT_FOUND;
        }
        return current;
    }
    if (expectation) {
        assert(current != NULL);
        // no need to incref:
        //   - should incref because it's being returned
        //   - should decref because it has just been removed from the dict
        return current;
    }
    Py_INCREF(EXPECTATION_FAILED);
    return EXPECTATION_FAILED;

fail:
    return NULL;
}

PyObject *
AtomicDict_CompareAndSet(AtomicDict *self, PyObject *key, PyObject *expected, PyObject *desired)
{
    if (key == NULL) {
        PyErr_SetString(PyExc_ValueError, "key == NULL");
        return NULL;
    }

    if (expected == NULL) {
        PyErr_SetString(PyExc_ValueError, "expected == NULL");
        return NULL;
    }

    if (desired == NULL) {
        PyErr_SetString(PyExc_ValueError, "desired == NULL");
        return NULL;
    }

    if (key == NOT_FOUND || key == ANY || key == EXPECTATION_FAILED) {
        PyErr_SetString(PyExc_ValueError, "key in (NOT_FOUND, ANY, EXPECTATION_FAILED)");
        return NULL;
    }

    if (expected == EXPECTATION_FAILED) {
        PyErr_SetString(PyExc_ValueError, "expected == EXPECTATION_FAILED");
        return NULL;
    }

    if (desired == NOT_FOUND || desired == ANY || desired == EXPECTATION_FAILED) {
        PyErr_SetString(PyExc_ValueError, "desired in (NOT_FOUND, ANY, EXPECTATION_FAILED)");
        return NULL;
    }

    _Py_SetWeakrefAndIncref(key);
    _Py_SetWeakrefAndIncref(desired);

    AtomicDict_Meta *meta = NULL;

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    AtomicDict_AccessorStorage *storage = NULL;
    storage = AtomicDict_GetOrCreateAccessorStorage(self);
    if (storage == NULL)
        goto fail;

    beginning:
    meta = AtomicDict_GetMeta(self, storage);
    if (meta == NULL)
        goto fail;

    PyMutex_Lock(&storage->self_mutex);
    int migrated = AtomicDict_MaybeHelpMigrate(meta, &storage->self_mutex, self->accessors);
    if (migrated) {
        // self_mutex was unlocked during the operation
        goto beginning;
    }

    AtomicDict_EntryLoc entry_loc = {
        .entry = NULL,
        .location = 0,
    };
    if (expected == NOT_FOUND || expected == ANY) {
        int got_entry = AtomicDict_GetEmptyEntry(self, meta, &storage->reservation_buffer, &entry_loc, hash);
        if (entry_loc.entry == NULL || got_entry == -1) {
            PyMutex_Unlock(&storage->self_mutex);
            goto fail;
        }

        if (got_entry == 0) {  // => must grow
            PyMutex_Unlock(&storage->self_mutex);
            migrated = AtomicDict_Grow(self);

            if (migrated < 0)
                goto fail;

            goto beginning;
        }

        entry_loc.entry->key = key;
        entry_loc.entry->hash = hash;
        entry_loc.entry->value = desired;
    }

    int must_grow;
    PyObject *result = AtomicDict_ExpectedInsertOrUpdate(meta, key, hash, expected, desired, &entry_loc, &must_grow, 0);

    if (result != NOT_FOUND && entry_loc.location != 0) {
        entry_loc.entry->flags &= ENTRY_FLAGS_RESERVED; // keep reserved, or set to 0
        entry_loc.entry->key = 0;
        entry_loc.entry->value = 0;
        entry_loc.entry->hash = 0;
        AtomicDict_ReservationBufferPut(&storage->reservation_buffer, &entry_loc, 1, meta);
    }

    if (result == NOT_FOUND && entry_loc.location != 0) {
        storage->local_len++; // TODO: overflow
        self->len_dirty = 1;
    }
    PyMutex_Unlock(&storage->self_mutex);

    if (result == NULL && !must_grow)
        goto fail;

    if (must_grow || (meta->greatest_allocated_block - meta->greatest_deleted_block + meta->greatest_refilled_block) *
                     ATOMIC_DICT_ENTRIES_IN_BLOCK >= SIZE_OF(meta) * 2 / 3) {
        migrated = AtomicDict_Grow(self);

        if (migrated < 0)
            goto fail;

        if (must_grow) {  // insertion didn't happen
            goto beginning;
        }
    }

    return result;
    fail:
    Py_DECREF(key);
    Py_DECREF(desired);
    return NULL;
}

PyObject *
AtomicDict_CompareAndSet_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *key = NULL;
    PyObject *expected = NULL;
    PyObject *desired = NULL;

    char *kw_list[] = {"key", "expected", "desired", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOO", kw_list, &key, &expected, &desired))
        goto fail;

    PyObject *ret = AtomicDict_CompareAndSet(self, key, expected, desired);

    if (ret == NULL)
        goto fail;

    if (ret == EXPECTATION_FAILED) {
        PyObject *error = PyUnicode_FromFormat("self[%R] != %R", key, expected);
        PyErr_SetObject(Cereggii_ExpectationFailed, error);
        goto fail;
    }

    Py_RETURN_NONE;

    fail:
    return NULL;
}

int
AtomicDict_SetItem(AtomicDict *self, PyObject *key, PyObject *value)
{
    if (value == NULL) {
        return AtomicDict_DelItem(self, key);
    }

    if (key == NOT_FOUND || key == ANY || key == EXPECTATION_FAILED) {
        PyErr_SetString(PyExc_ValueError, "key in (NOT_FOUND, ANY, EXPECTATION_FAILED)");
        goto fail;
    }

    if (value == NOT_FOUND || value == ANY || value == EXPECTATION_FAILED) {
        PyErr_SetString(PyExc_ValueError, "value in (NOT_FOUND, ANY, EXPECTATION_FAILED)");
        goto fail;
    }

    PyObject *result = AtomicDict_CompareAndSet(self, key, ANY, value);

    if (result == NULL)
        goto fail;

    assert(result != EXPECTATION_FAILED);

    if (result != NOT_FOUND && result != ANY && result != EXPECTATION_FAILED) {
        Py_DECREF(result);
    }

    return 0;
    fail:
    return -1;
}


static inline int
flush_one(AtomicDict *self, PyObject *key, PyObject *expected, PyObject *new, PyObject *aggregate, PyObject *(*specialized)(PyObject *, PyObject *, PyObject *), int is_specialized)
{
    assert(key != NULL);
    assert(expected != NULL);
    assert(new != NULL);
    assert(expected == NOT_FOUND);

    Py_INCREF(key);
    Py_INCREF(expected);
    Py_INCREF(new);

    PyObject *previous = NULL;
    PyObject *current = NULL;
    PyObject *desired = new;
    PyObject *new_desired = NULL;

    while (1) {
        previous = AtomicDict_CompareAndSet(self, key, expected, desired);

        if (previous == NULL)
            goto fail;

        if (previous != EXPECTATION_FAILED) {
            break;
        }

        current = AtomicDict_GetItemOrDefault(self, key, NOT_FOUND);

        // the aggregate function must always be called with `new`, not `desired`
        new_desired = NULL;
        if (is_specialized) {
            new_desired = specialized(key, current, new);
        } else {
            new_desired = PyObject_CallFunctionObjArgs(aggregate, key, current, new, NULL);
        }
        if (new_desired == NULL)
            goto fail;

        Py_DECREF(expected);
        expected = current;
        current = NULL;
        Py_DECREF(desired);
        desired = new_desired;
        new_desired = NULL;
    }
    Py_XDECREF(previous);
    Py_XDECREF(key);
    if (current != expected) {
        Py_XDECREF(current);
    }
    Py_XDECREF(expected);
    if (desired != new) {
        Py_DECREF(desired);
    }
    Py_XDECREF(new);
    return 0;
    fail:
    return -1;
}


static inline int
reduce_flush(AtomicDict *self, PyObject *local_buffer, PyObject *aggregate, PyObject *(*specialized)(PyObject *, PyObject *, PyObject *), int is_specialized)
{
    Py_ssize_t pos = 0;
    PyObject *key = NULL;
    PyObject *tuple = NULL;
    PyObject *expected = NULL;
    PyObject *current = NULL;
    PyObject *desired = NULL;
    PyObject *new = NULL;
    PyObject *previous = NULL;
    // local_buffer is thread-local: it's created in the reduce function,
    // and its reference never leaves the scope of Reduce() or reduce_flush().
    // so there's no need for Py_BEGIN_CRITICAL_SECTION here.
    while (PyDict_Next(local_buffer, &pos, &key, &tuple)) {
        assert(key != NULL);
        assert(tuple != NULL);
        assert(PyTuple_Size(tuple) == 2);
        expected = PyTuple_GetItem(tuple, 0);
        new = PyTuple_GetItem(tuple, 1);
        // don't Py_DECREF(tuple) because PyDict_Next returns borrowed references

        int error = flush_one(self, key, expected, new, aggregate, specialized, is_specialized);
        if (error) {
            goto fail;
        }
    }
    return 0;
    fail:
    Py_XDECREF(key);
    Py_XDECREF(expected);
    Py_XDECREF(current);
    Py_XDECREF(desired);
    Py_XDECREF(new);
    Py_XDECREF(previous);
    return -1;
}

static int
AtomicDict_Reduce_impl(AtomicDict *self, PyObject *iterable, PyObject *aggregate,
    PyObject *(*specialized)(PyObject *, PyObject *, PyObject *), const int is_specialized)
{
    // is_specialized => specialized != NULL
    assert(!is_specialized || specialized != NULL);
    // !is_specialized => aggregate != NULL
    assert(is_specialized || aggregate != NULL);

    PyObject *item = NULL;
    PyObject *key = NULL;
    PyObject *value = NULL;
    PyObject *current = NULL;
    PyObject *expected = NULL;
    PyObject *desired = NULL;
    PyObject *tuple_get = NULL;
    PyObject *tuple_set = NULL;
    PyObject *local_buffer = NULL;
    PyObject *iterator = NULL;

    if (!is_specialized) {
        if (!PyCallable_Check(aggregate)) {
            PyErr_Format(PyExc_TypeError, "%R is not callable.", aggregate);
            goto fail;
        }
    }

    local_buffer = PyDict_New();
    if (local_buffer == NULL)
        goto fail;

    iterator = PyObject_GetIter(iterable);
    if (iterator == NULL) {
        PyErr_Format(PyExc_TypeError, "%R is not iterable.", iterable);
        goto fail;
    }

    while ((item = PyIter_Next(iterator))) {
        if (!PyTuple_CheckExact(item)) {
            PyErr_Format(PyExc_TypeError, "type(%R) != tuple", item);
            goto fail;
        }

        if (PyTuple_Size(item) != 2) {
            PyErr_Format(PyExc_TypeError, "len(%R) != 2", item);
            goto fail;
        }

        key = PyTuple_GetItem(item, 0);
        Py_INCREF(key);
        value = PyTuple_GetItem(item, 1);
        Py_INCREF(value);
        if (PyDict_Contains(local_buffer, key)) {
            if (PyDict_GetItemRef(local_buffer, key, &tuple_get) < 0)
                goto fail;

            expected = PyTuple_GetItem(tuple_get, 0);
            Py_INCREF(expected);
            current = PyTuple_GetItem(tuple_get, 1);
            Py_INCREF(current);
            Py_CLEAR(tuple_get);
        } else {
            expected = NOT_FOUND;
            Py_INCREF(expected);
            current = NOT_FOUND;
            Py_INCREF(current);
        }

        if (is_specialized) {
            desired = specialized(key, current, value);
        } else {
            desired = PyObject_CallFunctionObjArgs(aggregate, key, current, value, NULL);
        }
        if (desired == NULL)
            goto fail;
        Py_INCREF(desired);

        tuple_set = PyTuple_Pack(2, expected, desired);
        if (tuple_set == NULL)
            goto fail;

        if (PyDict_SetItem(local_buffer, key, tuple_set) < 0)
            goto fail;

        Py_CLEAR(item);
        Py_CLEAR(key);
        Py_CLEAR(value);
        Py_CLEAR(current);
        Py_CLEAR(expected);
        Py_CLEAR(desired);
        Py_CLEAR(tuple_set);
    }

    Py_DECREF(iterator);
    if (PyErr_Occurred())
        goto fail;

    reduce_flush(self, local_buffer, aggregate, specialized, is_specialized);
    Py_DECREF(local_buffer);
    return 0;

    fail:
    Py_XDECREF(local_buffer);
    Py_XDECREF(iterator);
    Py_XDECREF(item);
    Py_XDECREF(key);
    Py_XDECREF(value);
    Py_XDECREF(current);
    Py_XDECREF(expected);
    Py_XDECREF(desired);
    Py_XDECREF(tuple_get);
    Py_XDECREF(tuple_set);
    return -1;
}

int
AtomicDict_Reduce(AtomicDict *self, PyObject *iterable, PyObject *aggregate)
{
    return AtomicDict_Reduce_impl(self, iterable, aggregate, NULL, 0);
}

PyObject *
AtomicDict_Reduce_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;
    PyObject *aggregate = NULL;

    char *kw_list[] = {"iterable", "aggregate", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kw_list, &iterable, &aggregate))
        goto fail;

    int res = AtomicDict_Reduce(self, iterable, aggregate);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_specialized_sum(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return new;
    }
    return PyNumber_Add(current, new);
}

int
AtomicDict_ReduceSum(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_sum, 1);
}


PyObject *
AtomicDict_ReduceSum_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceSum(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_specialized_and(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return new;
    }
    if (PyObject_IsTrue(current) && PyObject_IsTrue(new)) {
        return Py_True;  // Py_INCREF() called externally
    }
    return Py_False;  // Py_INCREF() called externally
}

int
AtomicDict_ReduceAnd(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_and, 1);
}

PyObject *
AtomicDict_ReduceAnd_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceAnd(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_specialized_or(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return new;
    }
    if (PyObject_IsTrue(current) || PyObject_IsTrue(new)) {
        return Py_True;  // Py_INCREF() called externally
    }
    return Py_False;  // Py_INCREF() called externally
}

int
AtomicDict_ReduceOr(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_or, 1);
}

PyObject *
AtomicDict_ReduceOr_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceOr(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_specialized_max(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return new;
    }
    Py_INCREF(current);
    Py_INCREF(new);
    const int cmp = PyObject_RichCompareBool(current, new, Py_GE);
    if (cmp < 0)
        return NULL;
    if (cmp) {
        return current;
    }
    return new;
}

int
AtomicDict_ReduceMax(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_max, 1);
}

PyObject *
AtomicDict_ReduceMax_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceMax(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_specialized_min(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return new;
    }
    Py_INCREF(current);
    Py_INCREF(new);
    const int cmp = PyObject_RichCompareBool(current, new, Py_LE);
    if (cmp < 0)
        return NULL;
    if (cmp) {
        return current;
    }
    return new;
}

int
AtomicDict_ReduceMin(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_min, 1);
}

PyObject *
AtomicDict_ReduceMin_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceMin(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
to_list(PyObject *maybe_list)
{
    assert(maybe_list != NULL);
    if (PyList_CheckExact(maybe_list))
        return maybe_list;

    PyObject *list = NULL;
    list = PyList_New(1);
    if (list == NULL)
        return NULL;
    Py_INCREF(maybe_list); // not a list
    if (PyList_SetItem(list, 0, maybe_list) < 0) {
        Py_DECREF(list);
        return NULL;
    }
    return list;
}

static inline PyObject *
reduce_specialized_list(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return to_list(new);
    }
    PyObject *current_list = to_list(current);
    if (current_list == NULL)
        return NULL;
    PyObject *new_list = to_list(new);
    if (new_list == NULL) {
        if (current != current_list) {
            Py_DECREF(current_list);
        }
        return NULL;
    }
    PyObject *concat = PyNumber_Add(current_list, new_list);
    if (concat == NULL) {
        if (current_list != current) {
            Py_DECREF(current_list);
        }
        if (new_list != new) {
            Py_DECREF(new_list);
        }
        return NULL;
    }
    return concat;
}

int
AtomicDict_ReduceList(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_list, 1);
}

PyObject *
AtomicDict_ReduceList_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceList(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_count_zip_iter_with_ones(AtomicDict *self, PyObject *iterable)
{
    // we want to return `zip(iterable, itertools.repeat(1))`
    PyObject *iterator = NULL;
    PyObject *builtin_zip = NULL;
    PyObject *itertools = NULL;
    PyObject *itertools_namespace = NULL;
    PyObject *itertools_repeat = NULL;
    PyObject *one = NULL;
    PyObject *repeat_one = NULL;
    PyObject *zipped = NULL;

    iterator = PyObject_GetIter(iterable);
    if (iterator == NULL) {
        PyErr_Format(PyExc_TypeError, "%R is not iterable.", iterable);
        goto fail;
    }

    if (PyDict_GetItemStringRef(PyEval_GetFrameBuiltins(), "zip", &builtin_zip) < 0)
        goto fail;

    // the following lines implement this Python code:
    //     import itertools
    //     itertools.repeat(1)
    itertools = PyImport_ImportModule("itertools");
    if (itertools == NULL)
        goto fail;
    itertools_namespace = PyModule_GetDict(itertools);
    if (itertools_namespace == NULL)
        goto fail;
    Py_INCREF(itertools_namespace);
    if (PyDict_GetItemStringRef(itertools_namespace, "repeat", &itertools_repeat) < 0)
        goto fail;
    Py_CLEAR(itertools_namespace);
    one = PyLong_FromLong(1);
    if (one == NULL)
        goto fail;
    repeat_one = PyObject_CallFunctionObjArgs(itertools_repeat, one, NULL);
    if (repeat_one == NULL)
        goto fail;

    zipped = PyObject_CallFunctionObjArgs(builtin_zip, iterator, repeat_one, NULL);
    if (zipped == NULL)
        goto fail;

    Py_DECREF(iterator);
    Py_DECREF(builtin_zip);
    Py_DECREF(itertools);
    Py_DECREF(itertools_repeat);
    Py_DECREF(one);
    Py_DECREF(repeat_one);
    return zipped;
    fail:
    Py_XDECREF(iterator);
    Py_XDECREF(builtin_zip);
    Py_XDECREF(itertools);
    Py_XDECREF(itertools_namespace);
    Py_XDECREF(itertools_repeat);
    Py_XDECREF(one);
    Py_XDECREF(repeat_one);
    return NULL;
}

int
AtomicDict_ReduceCount(AtomicDict *self, PyObject *iterable)
{
    PyObject *it = NULL;

    // todo: extend to all mappings
    //   the problem is that PyMapping_Check(iterable) returns 1 also when iterable is a list or tuple
    if (PyDict_Check(iterable)) {  // cannot fail: no error check
        it = PyDict_Items(iterable);
    } else {
        it = reduce_count_zip_iter_with_ones(self, iterable);
    }
    if (it == NULL)
        goto fail;

    const int res = AtomicDict_Reduce_impl(self, it, NULL, reduce_specialized_sum, 1);
    if (res < 0)
        goto fail;

    Py_DECREF(it);
    return 0;
    fail:
    Py_XDECREF(it);
    return -1;
}

PyObject *
AtomicDict_ReduceCount_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceCount(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

