// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H
#define CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H

#include "atomic_dict.h"


/// basic structs
typedef struct {
    unsigned char flags;
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
    unsigned long location;
} atomic_dict_entry_loc;


typedef struct atomic_dict_node {
    unsigned long node;
    unsigned long index;
    unsigned char distance;
    unsigned long tag;
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

    unsigned char log_size;  // = node index_size

    PyObject *generation;

    unsigned long *index;

    atomic_dict_block **blocks;
    long inserting_block;
    long greatest_allocated_block;
    long greatest_deleted_block;
    long greatest_refilled_block;

    unsigned char node_size;
    unsigned char distance_size;
    unsigned char max_distance;
    unsigned char tag_size;
    unsigned char nodes_in_region;
    unsigned char nodes_in_two_regions;

    unsigned long node_mask;
    unsigned long index_mask;
    unsigned long distance_mask;
    unsigned long tag_mask;
    unsigned long shift_mask;

    void (*read_single_region_nodes_at)(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

    void (*read_double_region_nodes_at)(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);
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

atomic_dict_meta *atomic_dict_new_meta(unsigned char log_size, atomic_dict_meta *previous_meta);

atomic_dict_block *atomic_dict_block_new(atomic_dict_meta *meta);

atomic_dict_entry *AtomicDict_GetEntryAt(unsigned long ix, atomic_dict_meta *meta);


/// operations on nodes (see ./node_ops.c)
void atomic_dict_compute_raw_node(atomic_dict_node *node, atomic_dict_meta *meta);

void atomic_dict_parse_node_from_raw(unsigned long node_raw, atomic_dict_node *node,
                                     atomic_dict_meta *meta);

void atomic_dict_parse_node_from_region(unsigned long ix, unsigned long region, atomic_dict_node *node,
                                        atomic_dict_meta *meta);

unsigned long region_of(unsigned long ix, atomic_dict_meta *meta);

unsigned long shift_in_region_of(unsigned long ix, atomic_dict_meta *meta);

unsigned char *index_address_of(unsigned long ix, atomic_dict_meta *meta);

int index_address_is_aligned(unsigned long ix, int alignment, atomic_dict_meta *meta);

void atomic_dict_read_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta);

int atomic_dict_write_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta);

int atomic_dict_node_is_reservation(atomic_dict_node *node, atomic_dict_meta *meta);

void atomic_dict_read_1_node_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_2_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_4_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_8_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_16_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

int must_write_bytes(int n, atomic_dict_meta *meta);

int atomic_dict_atomic_write_nodes_at(unsigned long ix, int n, atomic_dict_node *expected, atomic_dict_node *new,
                                      atomic_dict_meta *meta);


/// migrations

int AtomicDict_Grow(AtomicDict *self);

int AtomicDict_Shrink(AtomicDict *self);


/// reservation buffer (see ./reservation_buffer.c)
typedef struct {
    PyObject_HEAD

    int head, tail, count;
    atomic_dict_entry_loc reservations[64];
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
    unsigned long index;
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
    const unsigned char node_size;
    const unsigned char distance_size;
    const unsigned char tag_size;
} node_size_info;

#define ATOMIC_DICT_MIN_LOG_SIZE 6
#define ATOMIC_DICT_MAX_LOG_SIZE 56

extern const node_size_info node_sizes_table[];

#endif //CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H
