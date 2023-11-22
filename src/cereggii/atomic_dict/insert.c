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
atomic_dict_robin_hood(atomic_dict_meta *meta, atomic_dict_node *nodes, atomic_dict_node to_insert, int distance_0_ix)
{
    /*
     * Lythe and listin, gentilmen,
     * That be of frebore blode;
     * I shall you tel of a gode yeman,
     * His name was Robyn Hode.
     * */

    /*
     * assumptions:
     *   1. there are 16 / (meta->node_size / 8) valid nodes in `nodes`
     *   2. there is at least 1 empty node
     * */

    atomic_dict_node current = to_insert;
    atomic_dict_node temp;
    int probe = 0;
    int reservations = 0;
    int cursor;

    beginning:
    for (; probe < meta->nodes_in_two_regions; probe++) {
        if (probe >= (1 << meta->distance_size) - 1) {
            return grow;
        }
        cursor = distance_0_ix + probe + reservations;
        if (nodes[cursor].node == 0) {
            current.distance = probe;
            nodes[cursor] = current;
            return ok;
        }

        if (atomic_dict_node_is_reservation(&nodes[cursor], meta)) {
            probe--;
            reservations++;
//            if (look_into_reservations) {
//                result->is_reservation = 1;
//                goto check_entry;
//            } else {
//                continue;
//            }
        }

        if (nodes[cursor].distance < probe) {
            current.distance = probe;
            atomic_dict_compute_raw_node(&current, meta);
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

void
atomic_dict_robin_hood_reservation(atomic_dict_meta *meta, atomic_dict_node *nodes, int distance_0_ix)
{
    /*
     * assumptions:
     *   1. there are 16 / (meta->node_size / 8) valid nodes in `nodes`
     *   2. the right-most node is a reservation node
     *   3. distance_0_ix is the distance-0 index (i.e. the ideal index)
     *      for the right-most reservation node
     * */

    int nodes_in_two_words = 16 / (meta->node_size / 8);
    atomic_dict_node current = nodes[nodes_in_two_words - 1];
    atomic_dict_node temp;
    int probe = 0;
    int reservations = 0;

    beginning:
    for (; probe < nodes_in_two_words; probe++) {
        if (nodes[probe + reservations].node == 0) {
            current.distance = probe;
            nodes[probe + reservations] = current;
            return;
        }

        if (nodes[probe + reservations].distance < probe) {
            current.distance = probe;
            temp = nodes[probe + reservations];
            nodes[probe + reservations] = current;
            current = temp;
            goto beginning;
        }
    }
}

void
atomic_dict_nodes_copy_buffers(atomic_dict_node *from_buffer, atomic_dict_node *to_buffer)
{
    for (int i = 0; i < 16; ++i) {
        to_buffer[i] = from_buffer[i];
    }
}

int
atomic_dict_insert_entry(atomic_dict_meta *meta, atomic_dict_entry_loc *entry_loc, Py_hash_t hash)
{
    atomic_dict_node read_buffer[16]; // can read at most 16 nodes at a time
    atomic_dict_node temp[16];
    atomic_dict_node node = {
        .index = entry_loc->location,
        .tag = hash,
    };
    atomic_dict_node reservation = {
        .index = entry_loc->location,
        .distance = (1 << meta->distance_size) - 1,
        .tag = 0,
    };

    unsigned long ix = hash & ((1 << meta->log_size) - 1);

    beginning:
    meta->read_double_word_nodes_at(ix, read_buffer, meta);

    for (int i = 0; i < meta->nodes_in_two_regions; ++i) {
        if (read_buffer[i].node == 0) {
            atomic_dict_nodes_copy_buffers(read_buffer, temp);
            atomic_dict_robin_hood_result rh = atomic_dict_robin_hood(meta, temp, node, 0);
            // if (rh == grow) {
            //     atomic_dict_grow();
            // }
            assert(rh == ok);
            int begin_write = -1;
            for (int j = 0; j < meta->nodes_in_two_regions; ++j) {
                atomic_dict_compute_raw_node(&temp[j], meta);
                if (temp[j].node != read_buffer[j].node) {
                    begin_write = j;
                    break;
                }
            }
            assert(begin_write != -1);
            int end_write;
            for (int j = begin_write + 1; j < meta->nodes_in_two_regions; ++j) {
                atomic_dict_compute_raw_node(&temp[j], meta);
                if (temp[j].node == read_buffer[j].node) {
                    end_write = j;
                    break;
                }
            }
            assert(end_write > begin_write);
            if (atomic_dict_atomic_write_nodes_at(ix + begin_write, end_write - begin_write,
                                                  &read_buffer[begin_write], &temp[begin_write], meta)) {
                goto done;
            }
            goto beginning;
        }
        // check reservations
    }

    assert(0);

    int cursor = meta->nodes_in_two_regions - 1;

    int reservation_offset;
    for (reservation_offset = meta->nodes_in_two_regions;
         reservation_offset < 1 << meta->log_size; ++reservation_offset) {
        cursor++;
        cursor = cursor % meta->nodes_in_two_regions;
        if (cursor == 0) {
            meta->read_double_word_nodes_at(ix + reservation_offset, read_buffer, meta);
        }

        if (read_buffer[cursor].node == 0) {
            if (atomic_dict_atomic_write_node_at(ix + reservation_offset, &read_buffer[cursor], &reservation, meta)) {
                goto reserved;
            }
            goto beginning;
        }
        // check reservations
    }

    reserved:
    // 1. help reservations on the left
    // 2. proceed with my reservation

    done:
    return 0;
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
    meta = (atomic_dict_meta *) atomic_ref_get_ref(self->metadata);

    atomic_dict_search_result sr;
    search_for_update:
    AtomicDict_Lookup(meta, key, hash, 0, &sr);
    if (sr.error)
        goto fail;

    Py_INCREF(value);
    if (sr.entry_p != NULL) {
        if (_Py_atomic_compare_exchange_ptr(&sr.entry_p->value, sr.entry.value, value)) {
            Py_DECREF(sr.entry.value);
            goto done;
        }
        goto search_for_update;
    }

    atomic_dict_entry_loc entry_loc;
    atomic_dict_get_empty_entry(self, meta, rb, &entry_loc, hash);
    if (entry_loc.entry == NULL)
        goto fail;

    entry_loc.entry->key = key;
    entry_loc.entry->hash = hash;
    entry_loc.entry->value = value;
    atomic_dict_insert_entry(meta, &entry_loc, hash);

    done:
    Py_DECREF(meta);
    return 0;
    fail:
    Py_XDECREF(meta);
    Py_DECREF(value);
    return -1;
}
