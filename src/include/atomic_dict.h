// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include <Python.h>
#include "structmember.h"
#include "node_sizes_table.h"


typedef struct {
    unsigned char flags;
    Py_hash_t hash;
    PyObject *key;
    PyObject *value;
} atomic_dict_entry;

#define ENTRY_FLAGS_RESERVED    128
#define ENTRY_FLAGS_TOMBSTONE   64

typedef struct {
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
    long greatest_allocated_block;
    long greatest_deleted_block;
    long greatest_refilled_block;

    unsigned char node_size;
    unsigned char distance_size;
    unsigned char tag_size;

    unsigned long node_mask;
    unsigned long index_mask;
    unsigned long distance_mask;
    unsigned long tag_mask;
    unsigned long shift_mask;

    unsigned long reserved_node;
};

static PyTypeObject AtomicDictMeta = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_basicsize = sizeof(atomic_dict_meta),
    .tp_itemsize = 0,
    .tp_new = PyType_GenericNew,
    // add destructor (need to free index & blocks)
};

atomic_dict_meta *atomic_dict_new_meta(unsigned char log_size, atomic_dict_meta *previous_meta);

atomic_dict_block *atomic_dict_block_new(atomic_dict_meta *meta);

void atomic_dict_compute_raw_node(atomic_dict_node *node, atomic_dict_meta *meta);

void atomic_dict_read_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta);

int atomic_dict_write_node_at(unsigned long ix, atomic_dict_node *node, atomic_dict_meta *meta);


typedef struct {
    PyObject_HEAD

    atomic_dict_meta *metadata;
    atomic_dict_meta *new_gen_metadata;

    unsigned char reservation_buffer_size;
} AtomicDict;

typedef struct {
    int error;
    unsigned long index;
    atomic_dict_node node;
    const atomic_dict_entry *entry_p;
    atomic_dict_entry entry;
} atomic_dict_search_result;

void atomic_dict_search(AtomicDict *dk, atomic_dict_meta *meta, PyObject *key, Py_hash_t hash,
                        atomic_dict_search_result *result);

int atomic_dict_insert_or_update(AtomicDict *dk, PyObject *key, PyObject *value);

int atomic_dict_unsafe_insert(AtomicDict *self, PyObject *key, Py_hash_t hash, PyObject *value, Py_ssize_t pos);
