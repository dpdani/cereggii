// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "constants.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "atomic_ops.h"
#include "pythread.h"


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
            // expected != NOT_FOUND, value may be NULL
            if (entry.value != expected && expected != ANY) {
                *done = 1;
                *expectation = 0;
                return 1;
            }

            do {
                *current = entry.value;
                *done = CereggiiAtomic_CompareExchangePtr((void **) &entry_p->value, entry.value, desired);

                if (!*done) {
                    AtomicDict_ReadEntry(entry_p, &entry);

                    if (entry.value == NULL || entry.flags & ENTRY_FLAGS_TOMBSTONE || entry.flags & ENTRY_FLAGS_SWAPPED)
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

int
AtomicDict_ExpectedInsertOrUpdateCloseToDistance0(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash,
                                                  PyObject *expected, PyObject *desired, PyObject **current,
                                                  AtomicDict_EntryLoc *entry_loc, int *must_grow, int *done,
                                                  int *expectation, AtomicDict_Node *to_insert, uint64_t distance_0,
                                                  int skip_entry_check)
{
    assert(entry_loc != NULL);
    AtomicDict_BufferedNodeReader reader;
    AtomicDict_Node temp[16];
    int begin_write, end_write;

    beginning:
    reader.zone = -1;
    AtomicDict_ReadNodesFromZoneStartIntoBuffer(distance_0, &reader, meta);

    for (int i = (int) (distance_0 % meta->nodes_in_zone); i < meta->nodes_in_zone; i++) {
        if (reader.buffer[i].node == 0)
            goto empty_slot;

        if (!skip_entry_check) {
            int updated = AtomicDict_ExpectedUpdateEntry(meta, reader.buffer[i].index, key, hash, expected, desired,
                                                         current, done, expectation);
            if (updated < 0)
                goto fail;

            if (updated)
                return 1;
        }
    }
    return 0;

    empty_slot:
    if (expected != NOT_FOUND && expected != ANY) {
        *done = 1;
        *expectation = 0;
        return 0;
    }

    AtomicDict_CopyNodeBuffers(reader.buffer, temp);

    to_insert->index = entry_loc->location;
    to_insert->distance = 0;
    to_insert->tag = hash;

    AtomicDict_RobinHoodResult rhr =
        AtomicDict_RobinHoodInsert(meta, temp, to_insert, (int) (distance_0 % meta->nodes_in_zone));

    if (rhr == grow) {
        if (skip_entry_check) {
            return 0;
        }

        *must_grow = 1;
        goto fail;
    }
    assert(rhr == ok);

    AtomicDict_ComputeBeginEndWrite(meta, reader.buffer, temp, &begin_write, &end_write);

    *done = AtomicDict_AtomicWriteNodesAt(
        distance_0 - (distance_0 % meta->nodes_in_zone) + begin_write, end_write - begin_write,
        &reader.buffer[begin_write], &temp[begin_write], meta
    );

    if (!*done) {
        reader.zone = -1;
        goto beginning;
    }

    return 1;
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

    if (expected == NOT_FOUND || expected == ANY) {
        // insert fast-path
        if (AtomicDict_ReadRawNodeAt(distance_0, meta) == 0) {
            assert(entry_loc != NULL);

            node.index = entry_loc->location;
            node.distance = 0;
            node.tag = hash;

            if (AtomicDict_AtomicWriteNodesAt(distance_0, 1, &meta->zero, &node, meta)) {
                return NOT_FOUND;
            }
        }
    }

    beginning:
    done = 0;
    expectation = 1;
    uint64_t distance = 0;
    PyObject *current = NULL;
    uint8_t is_compact = meta->is_compact;
    AtomicDict_Node to_insert;

    if (AtomicDict_ExpectedInsertOrUpdateCloseToDistance0(meta, key, hash, expected, desired, &current, entry_loc,
                                                          must_grow, &done, &expectation, &to_insert,
                                                          distance_0, skip_entry_check) < 0)
        goto fail;

    while (!done) {
        uint64_t ix = (distance_0 + distance) & (meta->size - 1);
        AtomicDict_ReadNodeAt(ix, &node, meta);

        if (node.node == 0) {
            if (expected != NOT_FOUND && expected != ANY) {
                expectation = 0;
                break;
            }
            assert(entry_loc != NULL);

            // non-compact insert
            if (is_compact) {
                CereggiiAtomic_StoreUInt8(&meta->is_compact, 0);
            }

            to_insert.index = entry_loc->location;
            to_insert.distance = meta->max_distance;
            to_insert.tag = hash;

            done = AtomicDict_AtomicWriteNodesAt(ix, 1, &node, &to_insert, meta);

            if (!done)
                continue;  // don't increase distance
        } else if (node.node == meta->tombstone.node) {
            // pass
        } else if (is_compact && !AtomicDict_NodeIsReservation(&node, meta) && (
            (ix - node.distance > distance_0)
        )) {
            if (expected != NOT_FOUND && expected != ANY) {
                expectation = 0;
                break;
            }
        } else if (node.tag != (hash & meta->tag_mask)) {
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

        if (distance >= meta->size) {
            // traversed the entire dictionary without finding an empty slot
            *must_grow = 1;
            goto fail;
        }
    }

    // assert(expected == ANY => expectation == 1);
    assert(expected != ANY || expectation == 1);

    if (expected != NOT_FOUND && expectation == 0 && meta->is_compact != is_compact)
        goto beginning;

    if (expectation && expected == NOT_FOUND) {
        return NOT_FOUND;
    } else if (expectation && expected == ANY) {
        if (current == NULL) {
            return NOT_FOUND;
        } else {
            return current;
        }
    } else if (expectation) {
        assert(current != NULL);
        // no need to incref:
        //   - should incref because it's being returned
        //   - should decref because it has just been removed from the dict
        return current;
    } else {
        return EXPECTATION_FAILED;
    }

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

    Py_INCREF(key);
    Py_INCREF(desired);

    AtomicDict_Meta *meta = NULL;

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    AtomicDict_AccessorStorage *storage = NULL;
    storage = AtomicDict_GetOrCreateAccessorStorage(self);
    if (storage == NULL)
        goto fail;

    beginning:
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
    if (meta == NULL)
        goto fail;

    _PyMutex_lock(&storage->self_mutex);
    int migrated = AtomicDict_MaybeHelpMigrate(meta, &storage->self_mutex);
    if (migrated) {
        // self_mutex was unlocked during the operation
        Py_DECREF(meta);
        meta = NULL;
        goto beginning;
    }

    AtomicDict_EntryLoc entry_loc = {
        .entry = NULL,
        .location = 0,
    };
    if (expected == NOT_FOUND || expected == ANY) {
        int got_entry = AtomicDict_GetEmptyEntry(self, meta, &storage->reservation_buffer, &entry_loc, hash);
        if (entry_loc.entry == NULL || got_entry == -1)
            goto fail;

        if (got_entry == 0) {  // => must grow
            _PyMutex_unlock(&storage->self_mutex);
            migrated = AtomicDict_Grow(self);

            if (migrated < 0)
                goto fail;

            Py_DECREF(meta);
            meta = NULL;
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
        AtomicDict_ReservationBufferPut(&storage->reservation_buffer, &entry_loc, 1);
    }

    if (result == NOT_FOUND && entry_loc.location != 0) {
        storage->local_len++;
        self->len_dirty = 1;
    }
    _PyMutex_unlock(&storage->self_mutex);

    if (result == NULL && !must_grow)
        goto fail;

    if (must_grow || (meta->greatest_allocated_block - meta->greatest_deleted_block + meta->greatest_refilled_block) *
                     ATOMIC_DICT_ENTRIES_IN_BLOCK >= meta->size * 2 / 3) {
        migrated = AtomicDict_Grow(self);

        if (migrated < 0)
            goto fail;

        Py_DECREF(meta);
        goto beginning;
    }

    Py_DECREF(meta);
    return result;
    fail:
    Py_XDECREF(meta);
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
