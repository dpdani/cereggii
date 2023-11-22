// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_ATOMIC_DICT_H
#define CEREGGII_ATOMIC_DICT_H

#define PY_SSIZE_T_CLEAN


#include <Python.h>
#include <structmember.h>
#include "node_sizes_table.h"
#include "atomic_ref.h"


typedef struct {
    unsigned char flags;
    Py_hash_t hash;
    PyObject *key;
    PyObject *value;
} atomic_dict_entry;

#define ENTRY_FLAGS_RESERVED    128
#define ENTRY_FLAGS_TOMBSTONE   64

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
    unsigned char tag_size;
    unsigned char nodes_in_region;
    unsigned char nodes_in_two_regions;

    unsigned long node_mask;
    unsigned long index_mask;
    unsigned long distance_mask;
    unsigned long tag_mask;
    unsigned long shift_mask;

    void (*read_single_word_nodes_at)(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

    void (*read_double_word_nodes_at)(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);
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

void atomic_dict_compute_raw_node(atomic_dict_node *node, atomic_dict_meta *meta);

void atomic_dict_read_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta);

int atomic_dict_write_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta);

void atomic_dict_read_1_node_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_2_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_4_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_8_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void atomic_dict_read_16_nodes_at(unsigned long ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

int atomic_dict_atomic_write_node_at(unsigned long ix, atomic_dict_node *expected, atomic_dict_node *new,
                                     atomic_dict_meta *meta);

int atomic_dict_atomic_write_nodes_at(unsigned long ix, int n, atomic_dict_node *expected, atomic_dict_node *new,
                                      atomic_dict_meta *meta);


typedef struct {
    PyObject_HEAD

    AtomicRef *metadata;
    AtomicRef *new_gen_metadata;

    unsigned char reservation_buffer_size;
    Py_tss_t *tss_key;
    PyObject *reservation_buffers; // PyListObject
} AtomicDict;

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

typedef struct {
    int error;
    unsigned long index;
    atomic_dict_node node;
    atomic_dict_entry *entry_p;
    atomic_dict_entry entry;
    int is_reservation;
} atomic_dict_search_result;

void AtomicDict_Search(atomic_dict_meta *meta, PyObject *key, Py_hash_t hash,
                       int look_into_reservations, atomic_dict_search_result *result);

PyObject *AtomicDict_GetItem(AtomicDict *self, PyObject *key);

int AtomicDict_SetItem(AtomicDict *dk, PyObject *key, PyObject *value);

int AtomicDict_UnsafeInsert(AtomicDict *self, PyObject *key, Py_hash_t hash, PyObject *value, Py_ssize_t pos);

PyObject *AtomicDict_Debug(AtomicDict *self);

PyObject *AtomicDict_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

int AtomicDict_init(AtomicDict *self, PyObject *args, PyObject *kwargs);

int AtomicDict_traverse(AtomicDict *self, visitproc visit, void *arg);

void AtomicDict_dealloc(AtomicDict *self);


// operations on nodes

void atomic_dict_compute_raw_node(atomic_dict_node *node, atomic_dict_meta *meta);

void atomic_dict_parse_node_from_region(unsigned long ix, unsigned long region, atomic_dict_node *node,
                                        atomic_dict_meta *meta);

unsigned long region_of(unsigned long ix, atomic_dict_meta *meta);

unsigned long shift_in_region_of(unsigned long ix, atomic_dict_meta *meta);

void atomic_dict_read_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta);

int atomic_dict_write_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta);

int atomic_dict_node_is_reservation(atomic_dict_node *node, atomic_dict_meta *meta);

#endif //CEREGGII_ATOMIC_DICT_H