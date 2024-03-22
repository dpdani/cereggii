// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "constants.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "atomic_ops.h"
#include "pythread.h"


PyObject *
AtomicDict_ExpectedInsertOrUpdate(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash,
                                  PyObject *expected, PyObject *desired,
                                  AtomicDict_EntryLoc *entry_loc, int *must_grow)
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

    beginning:
    done = 0;
    expectation = 1;
    uint64_t distance = 0;
    uint64_t distance_0 = AtomicDict_Distance0Of(hash, meta);
    uint64_t d0_zone = AtomicDict_ZoneOf(distance_0, meta);
    AtomicDict_BufferedNodeReader reader;
    reader.zone = -1;
    PyObject *current = NULL;
    uint8_t is_compact = meta->is_compact;
    AtomicDict_Node temp[16];
    AtomicDict_Node to_insert;
    int begin_write, end_write;
    int64_t _ = 0;

    while (!done) {
        AtomicDict_ReadNodesFromZoneStartIntoBuffer(distance_0 + distance, &reader, meta);

        if (reader.node.node == 0) {
            if (expected != NOT_FOUND && expected != ANY) {
                expectation = 0;
                break;
            }
            assert(entry_loc != NULL);

            if (d0_zone == AtomicDict_ZoneOf(distance_0 + distance, meta)) {
                AtomicDict_CopyNodeBuffers(reader.buffer, temp);

                to_insert.index = entry_loc->location;
                to_insert.distance = 0;
                to_insert.tag = hash;

                AtomicDict_RobinHoodResult rhr =
                    AtomicDict_RobinHoodInsert(meta, temp, &to_insert, (int) (distance_0 % meta->nodes_in_zone));

                if (rhr == grow) {
                    *must_grow = 1;
                    goto fail;
                }
                assert(rhr == ok);

                AtomicDict_ComputeBeginEndWrite(meta, reader.buffer, temp, &begin_write, &end_write, &_);
                assert(_ == 0);

                done = AtomicDict_AtomicWriteNodesAt(
                    distance_0 - (distance_0 % meta->nodes_in_zone) + begin_write, end_write - begin_write,
                    &reader.buffer[begin_write], &temp[begin_write], meta
                );
            } else {
                // non-compact insert

                if (is_compact) {
                    CereggiiAtomic_StoreUInt8(&meta->is_compact, 0);
                }

                to_insert.index = entry_loc->location;
                to_insert.distance = meta->max_distance;
                to_insert.tag = hash;

                done = AtomicDict_AtomicWriteNodesAt(distance_0 + distance, 1,
                                                     &reader.buffer[reader.idx_in_buffer], &to_insert,
                                                     meta);
            }

            if (!done)
                continue;  // don't increase distance
        } else if (reader.node.node == meta->tombstone.node) {
            // pass
        } else if (is_compact && !AtomicDict_NodeIsReservation(&reader.node, meta) && (
            (distance_0 + distance - reader.node.distance > distance_0)
        )) {
            if (expected != NOT_FOUND && expected != ANY) {
                expectation = 0;
                break;
            }
        } else if (reader.node.tag != (hash & meta->tag_mask)) {
            // pass
        } else {
            AtomicDict_Entry *entry_p, entry;
            entry_p = AtomicDict_GetEntryAt(reader.node.index, meta);
            AtomicDict_ReadEntry(entry_p, &entry);

            int eq = 0;
            if (entry.key != key) {
                eq = PyObject_RichCompareBool(entry.key, key, Py_EQ);

                if (eq < 0)  // exception raised during compare
                    goto fail;
            }

            if (entry.key == key || eq) {
                if (expected == NOT_FOUND) {
                    if (entry.value != NULL) {
                        done = 1;
                        expectation = 0;
                    }
                    // if expected == NOT_FOUND && value == NULL:
                    //   it means there's another thread T concurrently
                    //   deleting this key.
                    //   T's linearization point (setting value = NULL) has
                    //   already been reached, thus we can proceed visiting
                    //   the probe.
                } else {
                    // expected != NOT_FOUND, value may be NULL
                    if (entry.value != expected && expected != ANY) {
                        done = 1;
                        expectation = 0;
                    } else {
                        current = entry.value;
                        done = CereggiiAtomic_CompareExchangePtr((void **) &entry_p->value, entry.value, desired);

                        if (!done) {
                            current = NULL;
                            continue;
                        }
                    }
                }
            }
        }

        distance++;

        if (distance >= meta->size) {
            *must_grow = 1;
            goto fail;
        }
    }

    // assert(expected == ANY => expectation == 1);
    assert(expected != ANY || expectation == 1);

    if (expected != NOT_FOUND && expectation == 0 && meta->is_compact != is_compact)
        goto beginning;

    if (expectation && expected == NOT_FOUND) {
        Py_INCREF(NOT_FOUND);
        return NOT_FOUND;
    } else if (expectation && expected == ANY) {
        if (current == NULL) {
            Py_INCREF(NOT_FOUND);
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
        Py_INCREF(EXPECTATION_FAILED);
        return EXPECTATION_FAILED;
    }

    fail:
    return NULL;
}

int
AtomicDict_SetItem(AtomicDict *self, PyObject *key, PyObject *value)
{
    if (value == NULL) {
        return AtomicDict_DelItem(self, key);
    }

    assert(key != NULL);
    assert(value != NULL);

    Py_INCREF(key);
    Py_INCREF(value);

    AtomicDict_Meta *meta = NULL;

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    AtomicDict_ReservationBuffer *rb = NULL;
    rb = AtomicDict_GetReservationBuffer(self);
    if (rb == NULL)
        goto fail;

    beginning:
    meta = (AtomicDict_Meta *) AtomicRef_Get(self->metadata);
    if (meta == NULL)
        goto fail;

    int migrated = AtomicDict_MaybeHelpMigrate(meta);
    if (migrated) {
        Py_DECREF(meta);
        meta = NULL;
        goto beginning;
    }

    AtomicDict_EntryLoc entry_loc;
    int got_entry = AtomicDict_GetEmptyEntry(self, meta, rb, &entry_loc, hash);
    if (entry_loc.entry == NULL || got_entry == -1)
        goto fail;

    if (got_entry == 0) {  // => must grow
        migrated = AtomicDict_Grow(self);

        if (migrated < 0)
            goto fail;

        Py_DECREF(meta);
        meta = NULL;
        goto beginning;
    }

    entry_loc.entry->key = key;
    entry_loc.entry->hash = hash;
    entry_loc.entry->value = value;

    int must_grow;
    PyObject *result = AtomicDict_ExpectedInsertOrUpdate(meta, key, hash, ANY, value, &entry_loc, &must_grow);
    assert(result != EXPECTATION_FAILED);

    if (result != NOT_FOUND) {
        entry_loc.entry->flags &= ENTRY_FLAGS_RESERVED; // keep reserved, or set to 0
        entry_loc.entry->key = 0;
        entry_loc.entry->value = 0;
        entry_loc.entry->hash = 0;
        AtomicDict_ReservationBufferPut(rb, &entry_loc, 1);
    }

    if (result == NULL && !must_grow)
        goto fail;

    Py_XDECREF(result);

    if (must_grow) {
        migrated = AtomicDict_Grow(self);

        if (migrated < 0)
            goto fail;

        Py_DECREF(meta);
        goto beginning;
    }

    Py_DECREF(meta);
    return 0;
    fail:
    Py_XDECREF(meta);
    Py_DECREF(key);
    Py_DECREF(value);
    return -1;
}
