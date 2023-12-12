// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H
#define CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H

#include "atomic_dict.h"


/// basic structs
typedef struct {
    uint8_t flags;
    Py_hash_t hash;
    PyObject *key;
    PyObject *value;
} atomic_dict_entry;

#define ENTRY_FLAGS_RESERVED    128
#define ENTRY_FLAGS_TOMBSTONE   64
#define ENTRY_FLAGS_INSERTED    32
#define ENTRY_FLAGS_SWAPPING    16
// #define ENTRY_FLAGS_?    8
// #define ENTRY_FLAGS_?    4
// #define ENTRY_FLAGS_?    2
// #define ENTRY_FLAGS_?    1

typedef struct {
    atomic_dict_entry *entry;
    uint64_t location;
} atomic_dict_entry_loc;


typedef struct atomic_dict_node {
    uint64_t node;
    uint64_t index;
    uint8_t distance;
    uint64_t tag;
} atomic_dict_node;


typedef struct {
    PyObject_HEAD
} atomic_dict_gen;

static PyTypeObject AtomicDictGen = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_basicsize = sizeof(atomic_dict_gen),
    .tp_itemsize = 0,
    .tp_new = PyType_GenericNew,
};


typedef struct {
    PyObject *generation;
//    PyObject *iteration;

    atomic_dict_entry entries[64];
} atomic_dict_block;


/// meta
struct atomic_dict_meta;
typedef struct atomic_dict_meta atomic_dict_meta;

struct atomic_dict_meta {
    PyObject_HEAD

    uint8_t log_size;  // = node index_size

    PyObject *generation;

    uint64_t *index;

    atomic_dict_block **blocks;
    int64_t inserting_block;
    int64_t greatest_allocated_block;
    int64_t greatest_deleted_block;
    int64_t greatest_refilled_block;

    uint8_t node_size;
    uint8_t distance_size;
    uint8_t max_distance;
    uint8_t tag_size;
    uint8_t nodes_in_region;
    uint8_t nodes_in_two_regions;

    uint64_t node_mask;
    uint64_t index_mask;
    uint64_t distance_mask;
    uint64_t tag_mask;
    uint64_t shift_mask;

    void (*read_single_region_nodes_at)(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

    void (*read_double_region_nodes_at)(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);
};

void atomic_dict_meta_dealloc(atomic_dict_meta *self);

static PyTypeObject AtomicDictMeta = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii._atomic_dict_meta",
    .tp_basicsize = sizeof(atomic_dict_meta),
    .tp_itemsize = 0,
    .tp_new = PyType_GenericNew,
    .tp_dealloc = (destructor) atomic_dict_meta_dealloc,
};

atomic_dict_meta *atomic_dict_new_meta(uint8_t log_size, atomic_dict_meta *previous_meta);

atomic_dict_block *atomic_dict_block_new(atomic_dict_meta *meta);

atomic_dict_entry *AtomicDict_GetEntryAt(uint64_t ix, atomic_dict_meta *meta);


/// operations on nodes (see ./node_ops.c)
void atomic_dict_compute_raw_node(atomic_dict_node *node, atomic_dict_meta *meta);

void atomic_dict_parse_node_from_raw(uint64_t node_raw, atomic_dict_node *node,
                                     atomic_dict_meta *meta);

void atomic_dict_parse_node_from_region(uint64_t ix, uint64_t region, atomic_dict_node *node,
                                        atomic_dict_meta *meta);

uint64_t region_of(uint64_t ix, atomic_dict_meta *meta);

uint64_t shift_in_region_of(uint64_t ix, atomic_dict_meta *meta);

uint8_t *index_address_of(uint64_t ix, atomic_dict_meta *meta);

int index_address_is_aligned(uint64_t ix, int alignment, atomic_dict_meta *meta);

void atomic_dict_read_node_at(uint64_t ix, atomic_dict_node *node, atomic_dict_meta *meta);

int atomic_dict_write_node_at(uint64_t ix, atomic_dict_node *node, atomic_dict_meta *meta);

int atomic_dict_node_is_reservation(atomic_dict_node *node, atomic_dict_meta *meta);

void atomic_dict_read_1_node_at(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_2_nodes_at(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_4_nodes_at(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_8_nodes_at(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_16_nodes_at(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

int must_write_bytes(int n, atomic_dict_meta *meta);

int atomic_dict_atomic_write_nodes_at(uint64_t ix, int n, atomic_dict_node *expected, atomic_dict_node *new,
                                      atomic_dict_meta *meta);


/// migrations

int AtomicDict_Grow(AtomicDict *self);

int AtomicDict_Shrink(AtomicDict *self);


/// reservation buffer (see ./reservation_buffer.c)
#define RESERVATION_BUFFER_SIZE 64

typedef struct {
    PyObject_HEAD

    int head, tail, count;
    atomic_dict_entry_loc reservations[RESERVATION_BUFFER_SIZE];
} atomic_dict_reservation_buffer;

static PyTypeObject AtomicDictReservationBuffer = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_basicsize = sizeof(atomic_dict_reservation_buffer),
    .tp_itemsize = 0,
    .tp_new = PyType_GenericNew,
};

atomic_dict_reservation_buffer *atomic_dict_get_reservation_buffer(AtomicDict *dk);

void atomic_dict_reservation_buffer_put(atomic_dict_reservation_buffer *rb, atomic_dict_entry_loc *entry_loc, int n);

void atomic_dict_reservation_buffer_pop(atomic_dict_reservation_buffer *rb, atomic_dict_entry_loc *entry_loc);

void atomic_dict_get_empty_entry(AtomicDict *dk, atomic_dict_meta *meta, atomic_dict_reservation_buffer *rb,
                                 atomic_dict_entry_loc *entry_loc, Py_hash_t hash);

/// semi-internal
typedef struct {
    int error;
    uint64_t index;
    atomic_dict_node node;
    atomic_dict_entry *entry_p;
    atomic_dict_entry entry;
    int is_reservation;
} atomic_dict_search_result;

void AtomicDict_Lookup(atomic_dict_meta *meta, PyObject *key, Py_hash_t hash,
                       int look_into_reservations, atomic_dict_search_result *result);

int AtomicDict_UnsafeInsert(AtomicDict *self, PyObject *key, Py_hash_t hash, PyObject *value, Py_ssize_t pos);

int AtomicDict_InsertOrUpdate(AtomicDict *self, atomic_dict_meta *meta,
                              atomic_dict_entry_loc *entry_loc);

/// node sizes table
/*
 * Node layout in memory
 *
 * +--------+--------+--------+
 * | index  | inv(d) |  tag   |
 * +--------+--------+--------+
 *
 * inv(d) := max_distance - distance
 * max_distance := (1 << distance_size)
 * */

typedef struct {
    const uint8_t node_size;
    const uint8_t distance_size;
    const uint8_t tag_size;
} node_size_info;

#define ATOMIC_DICT_MIN_LOG_SIZE 6
#define ATOMIC_DICT_MAX_LOG_SIZE 56

extern const node_size_info node_sizes_table[];

#endif //CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H
