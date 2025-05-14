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
        return NOT_FOUND;
    }
    if (expectation && expected == ANY) {
        if (current == NULL) {
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


#define REDUCE_FLUSH_CHUNK_SIZE 64


static inline int
reduce_flush(AtomicDict *self, PyObject *local_buffer, PyObject *aggregate)
{
    AtomicDict_Meta *meta = NULL;
    PyObject **keys = NULL;
    PyObject **expected = NULL;
    PyObject **desired = NULL;

    keys = PyMem_RawMalloc(REDUCE_FLUSH_CHUNK_SIZE * sizeof(PyObject *));
    if (keys == NULL)
        goto fail;

    expected = PyMem_RawMalloc(REDUCE_FLUSH_CHUNK_SIZE * sizeof(PyObject *));
    if (expected == NULL)
        goto fail;

    desired = PyMem_RawMalloc(REDUCE_FLUSH_CHUNK_SIZE * sizeof(PyObject *));
    if (desired == NULL)
        goto fail;

    Py_ssize_t pos = 0;

    AtomicDict_AccessorStorage *storage = NULL;
    storage = AtomicDict_GetOrCreateAccessorStorage(self);
    if (storage == NULL)
        goto fail;

    get_meta:
    meta = AtomicDict_GetMeta(self, storage);
    if (meta == NULL)
        goto fail;

    for (;;) {
        int num_items = 0;
        int done = 0;

        for (int i = 0; i < REDUCE_FLUSH_CHUNK_SIZE; ++i) {
            PyObject *values;
            if (PyDict_Next(local_buffer, &pos, &keys[num_items], &values)) {
                expected[num_items] = PyTuple_GetItem(values, 0);
                desired[num_items] = PyTuple_GetItem(values, 1);

                if (expected[num_items] == NULL || desired[num_items] == NULL)
                    goto fail;

                num_items++;
            } else {
                done = 1;
                break;
            }
        }

        for (int i = 0; i < num_items; ++i) {
            // prefetch index
            Py_hash_t hash = PyObject_Hash(keys[i]);
            if (hash == -1)
                goto fail;

            __builtin_prefetch(&meta->index[AtomicDict_Distance0Of(hash, meta)]);
        }

        for (int i = 0; i < num_items; ++i) {
            PyObject *result = NULL;
            PyObject *new = desired[i];

            retry:
            result = AtomicDict_CompareAndSet(self, keys[i], expected[i], new);

            if (result == NULL)
                goto fail;

            if (result == EXPECTATION_FAILED) {
                PyObject *current = AtomicDict_GetItemOrDefault(self, keys[i], NOT_FOUND);
                if (current == NULL)
                    goto fail;

                expected[i] = current;
                new = PyObject_CallFunctionObjArgs(aggregate, keys[i], expected[i], desired[i], NULL);
                if (new == NULL)
                    goto fail;

                goto retry;
            }
        }

        if (done)
            break;


        if (meta->new_gen_metadata != NULL)
            goto get_meta;
    }

    PyMem_RawFree(keys);
    PyMem_RawFree(expected);
    PyMem_RawFree(desired);
    return 0;

    fail:
    if (keys != NULL) {
        PyMem_RawFree(keys);
    }
    if (expected != NULL) {
        PyMem_RawFree(expected);
    }
    if (desired != NULL) {
        PyMem_RawFree(desired);
    }
    return -1;
}


int
AtomicDict_Reduce(AtomicDict *self, PyObject *iterable, PyObject *aggregate, int chunk_size)
{
    PyObject *local_buffer = NULL;
    PyObject *item = NULL;

    if (!PyCallable_Check(aggregate)) {
        PyErr_Format(PyExc_TypeError, "%R is not callable.", aggregate);
        goto fail;
    }

    if (chunk_size != 0) {
        PyErr_SetString(PyExc_ValueError, "only supporting chunk_size=0 at the moment.");
        goto fail;
    }

    local_buffer = PyDict_New();
    if (local_buffer == NULL)
        goto fail;

    PyObject *iterator = PyObject_GetIter(iterable);

    if (iterator == NULL) {
        PyErr_Format(PyExc_TypeError, "%R is not iterable.", iterable);
        goto fail;
    }

    while ((item = PyIter_Next(iterator))) {
        PyObject *key = NULL;
        PyObject *value = NULL;
        PyObject *current = NULL;
        PyObject *expected = NULL;
        PyObject *desired = NULL;

        if (!PyTuple_CheckExact(item)) {
            PyErr_Format(PyExc_TypeError, "type(%R) != tuple", item);
            goto fail;
        }

        if (PyTuple_Size(item) != 2) {
            PyErr_Format(PyExc_TypeError, "len(%R) != 2", item);
            goto fail;
        }

        key = PyTuple_GetItem(item, 0);
        value = PyTuple_GetItem(item, 1);
        if (PyDict_Contains(local_buffer, key)) {
            if (PyDict_GetItemRef(local_buffer, key, &current) < 0)
                goto fail;

            expected = PyTuple_GetItem(current, 0);
            current = PyTuple_GetItem(current, 1);
        } else {
            expected = NOT_FOUND;
            current = expected;
        }

        desired = PyObject_CallFunctionObjArgs(aggregate, key, current, value, NULL);
        if (desired == NULL)
            goto fail;

        PyObject *tuple = PyTuple_Pack(2, expected, desired);
        if (tuple == NULL)
            goto fail;

        if (PyDict_SetItem(local_buffer, key, tuple) < 0)
            goto fail;

        Py_DECREF(item);
    }

    Py_DECREF(iterator);

    if (PyErr_Occurred()) {
        goto fail;
    }

    reduce_flush(self, local_buffer, aggregate);

    Py_DECREF(local_buffer);
    return 0;

    fail:
    Py_XDECREF(local_buffer);
    Py_XDECREF(item);
    return -1;
}

PyObject *
AtomicDict_Reduce_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;
    PyObject *aggregate = NULL;
    int chunk_size = 0;

    char *kw_list[] = {"iterable", "aggregate", "chunk_size", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|i", kw_list, &iterable, &aggregate, &chunk_size))
        goto fail;

    int res = AtomicDict_Reduce(self, iterable, aggregate, chunk_size);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}
