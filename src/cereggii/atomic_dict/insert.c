// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"
#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "pythread.h"


typedef enum atomic_dict_robin_hood_result {
    ok,
    failed,
    grow,
} atomic_dict_robin_hood_result;

atomic_dict_robin_hood_result
atomic_dict_robin_hood(atomic_dict_meta *meta, atomic_dict_node *nodes, atomic_dict_node *to_insert, int distance_0_ix)
{
    /*
     * Lythe and listin, gentilmen,
     * That be of frebore blode;
     * I shall you tel of a gode yeman,
     * His name was Robyn Hode.
     * */

    /*
     * assumptions:
     *   1. there are meta->nodes_in_two_regions valid nodes in `nodes`
     *   2. there is at least 1 empty node
     * */

    atomic_dict_node current = *to_insert;
    atomic_dict_node temp;
    int probe = 0;
    int cursor;

    beginning:
    for (; probe < meta->nodes_in_two_regions; probe++) {
        if (probe >= meta->max_distance) {
            return grow;
        }
        cursor = distance_0_ix + probe;
        if (nodes[cursor].node == 0) {
            if (!atomic_dict_node_is_reservation(&current, meta)) {
                current.distance = probe;
            }
            nodes[cursor] = current;
            return ok;
        }

        if (nodes[cursor].distance < probe) {
            if (!atomic_dict_node_is_reservation(&current, meta)) {
                current.distance = probe;
                atomic_dict_compute_raw_node(&current, meta);
            }
            temp = nodes[cursor];
            nodes[cursor] = current;
            current = temp;
            distance_0_ix = cursor - temp.distance;
            probe = temp.distance;
            goto beginning;
        }
    }
    return failed;
}

atomic_dict_robin_hood_result
atomic_dict_robin_hood_reservation(atomic_dict_meta *meta, atomic_dict_node *nodes, long start_ix,
                                   unsigned long distance_0_ix, long *reservation_ix)
{
    /*
     * assumptions:
     *   1. there are meta->nodes_in_two_regions valid nodes in `nodes`
     *   2. nodes[*reservation_ix] is a reservation node
     *   3. distance_0_ix is the distance-0 index (i.e. the ideal index)
     *      for the reservation node
     *   4. start_ix is the index position of nodes[0]
     * */

    assert(*reservation_ix < meta->nodes_in_two_regions);

    atomic_dict_node current = nodes[*reservation_ix];

    assert(atomic_dict_node_is_reservation(&current, meta));

    long aligned_ix = 0;

    if (!index_address_is_aligned(start_ix, 16, meta)) {
        aligned_ix = *reservation_ix - (long) index_address_of(start_ix, meta) % 16;
    }

    if (distance_0_ix < start_ix) {
        assert(*reservation_ix <= 16);
        for (unsigned long i = *reservation_ix; i > aligned_ix; --i) {
            nodes[i] = nodes[i - 1];
            nodes[i].distance++;
            if (nodes[i].distance >= meta->max_distance) {
                return grow;
            }
        }
        nodes[aligned_ix] = current;
        *reservation_ix = start_ix + aligned_ix;
        return ok;
    }

    atomic_dict_parse_node_from_raw(0, &nodes[*reservation_ix], meta);
    assert(start_ix - distance_0_ix <= 16);
    atomic_dict_robin_hood_result result;
    current.distance = 0;
    result = atomic_dict_robin_hood(meta, nodes, &current, (int) (start_ix - distance_0_ix));
    for (int i = 0; i < meta->nodes_in_two_regions; ++i) {
        if (nodes[i].index == current.index) {
            *reservation_ix = start_ix + i;
        }
    }
    return result;
}

void
atomic_dict_nodes_copy_buffers(atomic_dict_node *from_buffer, atomic_dict_node *to_buffer)
{
    for (int i = 0; i < 16; ++i) {
        to_buffer[i] = from_buffer[i];
    }
}

typedef enum atomic_dict_inserted_or_updated {
    error,
    inserted,
    updated,
    nop,
    retry,
} atomic_dict_inserted_or_updated;

atomic_dict_inserted_or_updated
AtomicDict_CheckNodeEntryAndMaybeUpdate(unsigned long distance_0, unsigned long i, atomic_dict_node *node,
                                        atomic_dict_meta *meta, Py_hash_t hash, PyObject *key, PyObject *value)
{
    if (atomic_dict_node_is_reservation(node, meta))
        goto check_entry;
    if ((distance_0 + i - node->distance) % (1 << meta->log_size) > distance_0)
        return nop;
    if (node->tag != (hash & meta->tag_mask))
        return nop;

    atomic_dict_entry *entry_p;
    atomic_dict_entry entry;
    check_entry:
    entry_p = AtomicDict_GetEntryAt(node->index, meta);
    entry = *entry_p;

    if (entry.hash != hash)
        return nop;
    if (entry.key == key)
        goto same_key_reservation;

    int cmp = PyObject_RichCompareBool(entry.key, key, Py_EQ);
    if (cmp < 0) {
        // exception thrown during compare
        return error;
    }
    if (cmp == 0)
        return nop;

    same_key_reservation:
    if (entry.flags & ENTRY_FLAGS_TOMBSTONE) {
        // help delete
        return retry;
    }
    if (atomic_dict_node_is_reservation(node, meta)) {
        // help insert
        return retry;
    }

    if (_Py_atomic_compare_exchange_ptr(&entry_p->value, entry.value, value)) {
        Py_DECREF(entry.value);
        return updated;
    }
    goto check_entry;
}

void compute_begin_end_write(atomic_dict_meta *meta, atomic_dict_node *read_buffer, atomic_dict_node *temp,
                             int *begin_write, int *end_write)
{
    int j;
    *begin_write = -1;
    for (j = 0; j < meta->nodes_in_two_regions; ++j) {
        atomic_dict_compute_raw_node(&temp[j], meta);
        if (temp[j].node != read_buffer[j].node) {
            *begin_write = j;
            break;
        }
    }
    assert(*begin_write != -1);
    *end_write = -1;
    for (j = *begin_write + 1; j < meta->nodes_in_two_regions; ++j) {
        atomic_dict_compute_raw_node(&temp[j], meta);
        if (temp[j].node == read_buffer[j].node) {
            *end_write = j;
            break;
        }
    }
    if (*end_write == -1) {
        *end_write = meta->nodes_in_two_regions;
    }
    assert(*end_write > *begin_write);
}

atomic_dict_inserted_or_updated
AtomicDict_InsertOrUpdateCloseToDistance0(AtomicDict *self, atomic_dict_meta *meta, atomic_dict_entry_loc *entry_loc,
                                          Py_hash_t hash, PyObject *key, PyObject *value,
                                          atomic_dict_node *read_buffer, atomic_dict_node *temp, long *region)
{
    atomic_dict_node node = {
        .index = entry_loc->location,
        .tag = hash,
    };

    unsigned long ix = hash & ((1 << meta->log_size) - 1);

    beginning:
    meta->read_double_region_nodes_at(ix, read_buffer, meta);
    *region = (long) region_of(ix, meta) | 1;
    atomic_dict_inserted_or_updated check_result = 0;

    for (int i = 0; i < meta->nodes_in_two_regions; ++i) {
        if (read_buffer[i].node == 0) {
            atomic_dict_nodes_copy_buffers(read_buffer, temp);
            atomic_dict_robin_hood_result rh = atomic_dict_robin_hood(meta, temp, &node, 0);
            if (rh == grow) {
                AtomicDict_Grow(self);
                return inserted;
            }
            assert(rh == ok);
            int begin_write, end_write;
            compute_begin_end_write(meta, read_buffer, temp, &begin_write, &end_write);
            if (atomic_dict_atomic_write_nodes_at(ix + begin_write, end_write - begin_write,
                                                  &read_buffer[begin_write], &temp[begin_write], meta)) {
                goto done;
            }
            goto beginning;
        }

        check_result = AtomicDict_CheckNodeEntryAndMaybeUpdate(ix, i, &read_buffer[i], meta, hash, key, value);
        if (check_result == retry) {
            goto beginning;
        }
        if (check_result == error) {
            goto error;
        }
        if (check_result == updated) {
            return updated;
        }
    }

    return nop;

    done:
    return inserted;
    error:
    return error;
}

int
AtomicDict_InsertOrUpdate(AtomicDict *self, atomic_dict_meta *meta, atomic_dict_entry_loc *entry_loc)
{
    Py_hash_t hash = entry_loc->entry->hash;
    PyObject *key = entry_loc->entry->key;
    PyObject *value = entry_loc->entry->value;
    assert(key != NULL && value != NULL);
    long distance_0_ix = hash & ((1 << meta->log_size) - 1);
    assert(distance_0_ix >= 0);

    atomic_dict_node read_buffer[16];
    atomic_dict_node temp[16];
    atomic_dict_node node, reservation;
    long idx;
    long region = -1;
    int nodes_offset = 0;
    long start_ix, idx_in_buffer;

    atomic_dict_inserted_or_updated close_to_0;
    close_to_0 = AtomicDict_InsertOrUpdateCloseToDistance0(self, meta, entry_loc, hash, key, value,
                                                           read_buffer, temp, &region);
    if (close_to_0 == error) {
        goto error;
    }
    if (close_to_0 == inserted || close_to_0 == updated) {
        return close_to_0;
    }

    beginning:
    for (int i = 0; i < 1 << meta->log_size; ++i) {
        idx = (distance_0_ix + i) % (1 << meta->log_size);

        if (region != (region_of(idx, meta) | 1)) {
            meta->read_double_region_nodes_at(idx, read_buffer, meta);
            region = (long) region_of(idx, meta) | 1;
            nodes_offset = (int) -(idx % meta->nodes_in_two_regions);
        }
        idx_in_buffer = idx % meta->nodes_in_two_regions + nodes_offset;
        node = read_buffer[idx_in_buffer];

        if (node.node == 0)
            goto tail_found;

        atomic_dict_inserted_or_updated check_entry;
        check_entry = AtomicDict_CheckNodeEntryAndMaybeUpdate(distance_0_ix, i, &node, meta, hash, key, value);
        switch (check_entry) {
            case retry:
                goto beginning;
            case error:
                goto error;
            case updated:
                return updated;
            case nop:
                continue;
            case inserted:
                assert(0);
        }
    }

    AtomicDict_Grow(self);
    return inserted; // linearization point is inside grow()

    tail_found:
    reservation.index = entry_loc->location;
    reservation.distance = meta->max_distance;
    reservation.tag = hash;
    assert(node.node == 0);
    atomic_dict_nodes_copy_buffers(read_buffer, temp);
    temp[idx_in_buffer] = reservation;
    atomic_dict_robin_hood_reservation(meta, temp, idx, distance_0_ix, &idx_in_buffer);
    int begin_write, end_write;
    compute_begin_end_write(meta, read_buffer, temp, &begin_write, &end_write);
    if (!atomic_dict_atomic_write_nodes_at(idx + begin_write, end_write - begin_write,
                                           &read_buffer[begin_write], &temp[begin_write], meta)) {
        region = -1;
        goto beginning;
    }
    atomic_dict_nodes_copy_buffers(temp, read_buffer);
    long pi = idx;
    long i = distance_0_ix;

    find_reservation:
    reservation.node = 0;
    for (; i <= pi; ++i) {  // XXX make it circular
        idx = (i) % (1 << meta->log_size);

        if (region != (region_of(idx, meta) | 1)) {
            meta->read_double_region_nodes_at(idx, read_buffer, meta);
            region = (long) region_of(idx, meta) | 1;
            nodes_offset = (int) -(idx % meta->nodes_in_two_regions);
        }
        node = read_buffer[idx % meta->nodes_in_two_regions + nodes_offset];

        if (atomic_dict_node_is_reservation(&node, meta)) {
            reservation.node = node.node;
            atomic_dict_parse_node_from_raw(reservation.node, &reservation, meta);
            atomic_dict_entry *e = AtomicDict_GetEntryAt(node.index, meta);
            distance_0_ix = e->hash & ((1 << meta->log_size) - 1);
            break;
        }
    }

    if (reservation.node) {
        atomic_dict_node real = {
            .index = reservation.index,
            .tag = reservation.tag,
        };

        long previous_start_ix = -1;

        for (long j = i; j > distance_0_ix; --j) { // XXX make circular
            idx = (j) % (1 << meta->log_size);
            start_ix = (idx - meta->nodes_in_two_regions + 1) % (1 << meta->log_size);
            if (idx < 0) {
                idx += (1 << meta->log_size);
            }
            if (start_ix < 0) {
                start_ix += (1 << meta->log_size);
            }

            do_read:
            if (region != (region_of(start_ix, meta) | 1) || start_ix < previous_start_ix) {
                previous_start_ix = start_ix;
                meta->read_double_region_nodes_at(start_ix, read_buffer, meta);
                region = (long) region_of(start_ix, meta) | 1;
                nodes_offset = (int) -(start_ix % meta->nodes_in_two_regions);
            }
            idx_in_buffer = idx % meta->nodes_in_two_regions + nodes_offset;
            if (idx_in_buffer < 0) {
                idx_in_buffer += meta->nodes_in_two_regions;
            }
            node = read_buffer[idx_in_buffer];

            if (node.index == real.index && node.node != reservation.node) {
                // another thread did the work for this insertion
                goto find_reservation;
            }

            if (node.node != reservation.node)
                continue;

            atomic_dict_nodes_copy_buffers(read_buffer, temp);
            atomic_dict_robin_hood_result result;
            idx = idx_in_buffer;
            result = atomic_dict_robin_hood_reservation(meta, temp, start_ix, distance_0_ix, &idx);
            if (result == grow) {
                AtomicDict_Grow(self);
                return inserted;
            }
            assert(result == ok);

            compute_begin_end_write(meta, read_buffer, temp, &begin_write, &end_write);
            if (must_write_bytes(end_write - begin_write, meta) == 16) {
                while (!index_address_is_aligned(start_ix + begin_write, 16, meta)) {
                    --begin_write;
                }
                if (begin_write < 0) {
                    start_ix += begin_write;
                    goto do_read;
                }
            }
            if (atomic_dict_atomic_write_nodes_at(start_ix + begin_write, end_write - begin_write,
                                                  &read_buffer[begin_write], &temp[begin_write], meta)) {
                atomic_dict_nodes_copy_buffers(temp, read_buffer);
            } else {
                region = -1;
            }
            j = idx + 1;
        }

        goto find_reservation;
    }

    return inserted;
    error:
    return error;
}

int
AtomicDict_SetItem(AtomicDict *self, PyObject *key, PyObject *value)
{
    assert(key != NULL);
    assert(value != NULL);

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1) {
        return -1;
    }

    atomic_dict_reservation_buffer *rb = atomic_dict_get_reservation_buffer(self);
    if (rb == NULL)
        goto fail;

    atomic_dict_meta *meta;
    meta = (atomic_dict_meta *) AtomicRef_Get(self->metadata);

    atomic_dict_entry_loc entry_loc;
    atomic_dict_get_empty_entry(self, meta, rb, &entry_loc, hash);
    if (entry_loc.entry == NULL)
        goto fail;

    Py_INCREF(key);
    Py_INCREF(value);
    entry_loc.entry->key = key;
    entry_loc.entry->hash = hash;
    entry_loc.entry->value = value;

    atomic_dict_inserted_or_updated result = AtomicDict_InsertOrUpdate(self, meta, &entry_loc);

    if (result != inserted) {
        entry_loc.entry->flags &= ENTRY_FLAGS_RESERVED; // keep reserved, or set to 0
        entry_loc.entry->key = 0;
        entry_loc.entry->value = 0;
        entry_loc.entry->hash = 0;
        atomic_dict_reservation_buffer_put(rb, &entry_loc, 1);
    }
    if (result == error) {
        goto fail;
    }

    Py_DECREF(meta);
    return 0;
    fail:
    Py_XDECREF(meta);
    Py_DECREF(key);
    Py_DECREF(value);
    return -1;
}
