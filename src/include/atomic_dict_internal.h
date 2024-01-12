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
    uint64_t size;

    PyObject *generation;

    uint64_t *index;

    atomic_dict_block **blocks;
    int64_t inserting_block;
    int64_t greatest_allocated_block;
    int64_t greatest_deleted_block;
    int64_t greatest_refilled_block;

    uint8_t is_compact;

    uint8_t node_size;
    uint8_t distance_size;
    uint8_t max_distance;
    uint8_t tag_size;
    uint8_t nodes_in_region;
    uint8_t nodes_in_zone;

    uint64_t node_mask;
    uint64_t index_mask;
    uint64_t distance_mask;
    uint64_t tag_mask;
    uint64_t shift_mask;

    atomic_dict_node tombstone;

    void (*read_nodes_in_region)(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

    void (*read_nodes_in_zone)(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);
};

void AtomicDictMeta_dealloc(atomic_dict_meta *self);

static PyTypeObject AtomicDictMeta = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii._atomic_dict_meta",
    .tp_basicsize = sizeof(atomic_dict_meta),
    .tp_itemsize = 0,
    .tp_new = PyType_GenericNew,
    .tp_dealloc = (destructor) AtomicDictMeta_dealloc,
};

atomic_dict_meta *AtomicDict_NewMeta(uint8_t log_size, atomic_dict_meta *previous_meta);

atomic_dict_block *AtomicDict_NewBlock(atomic_dict_meta *meta);

atomic_dict_entry *AtomicDict_GetEntryAt(uint64_t ix, atomic_dict_meta *meta);


/// operations on nodes (see ./node_ops.c)
void AtomicDict_ComputeRawNode(atomic_dict_node *node, atomic_dict_meta *meta);

void AtomicDict_ParseNodeFromRaw(uint64_t node_raw, atomic_dict_node *node,
                                 atomic_dict_meta *meta);

void AtomicDict_ParseNodeFromRegion(uint64_t ix, uint64_t region, atomic_dict_node *node,
                                    atomic_dict_meta *meta);

uint64_t AtomicDict_RegionOf(uint64_t ix, atomic_dict_meta *meta);

uint64_t AtomicDict_ZoneOf(uint64_t ix, atomic_dict_meta *meta);

uint64_t AtomicDict_Distance0Of(Py_hash_t hash, atomic_dict_meta *meta);

uint64_t AtomicDict_ShiftInRegionOf(uint64_t ix, atomic_dict_meta *meta);

uint8_t *AtomicDict_IndexAddressOf(uint64_t ix, atomic_dict_meta *meta);

int AtomicDict_IndexAddressIsAligned(uint64_t ix, int alignment, atomic_dict_meta *meta);

void AtomicDict_ReadNodeAt(uint64_t ix, atomic_dict_node *node, atomic_dict_meta *meta);

void AtomicDict_Read1NodeAt(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void AtomicDict_Read2NodesAt(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void AtomicDict_Read4NodesAt(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void AtomicDict_Read8NodesAt(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void AtomicDict_Read16NodesAt(uint64_t ix, atomic_dict_node *nodes, atomic_dict_meta *meta);

void AtomicDict_CopyNodeBuffers(atomic_dict_node *from_buffer, atomic_dict_node *to_buffer);

void AtomicDict_ComputeBeginEndWrite(atomic_dict_meta *meta, atomic_dict_node *read_buffer, atomic_dict_node *temp,
                                     int *begin_write, int *end_write, int64_t *start_ix);

void AtomicDict_ReadNodesFromZoneIntoBuffer(uint64_t idx, int64_t *zone, atomic_dict_node *buffer,
                                            atomic_dict_node *node, int *idx_in_buffer, int *nodes_offset,
                                            atomic_dict_meta *meta);

void AtomicDict_ReadNodesFromZoneStartIntoBuffer(uint64_t idx, int64_t *zone, atomic_dict_node *buffer,
                                                 atomic_dict_node *node, int *idx_in_buffer, int *nodes_offset,
                                                 atomic_dict_meta *meta);

int AtomicDict_WriteNodeAt(uint64_t ix, atomic_dict_node *node, atomic_dict_meta *meta);

int AtomicDict_NodeIsReservation(atomic_dict_node *node, atomic_dict_meta *meta);

int AtomicDict_NodeIsTombstone(atomic_dict_node *node, atomic_dict_meta *meta);

int AtomicDict_MustWriteBytes(int n, atomic_dict_meta *meta);

int AtomicDict_AtomicWriteNodesAt(uint64_t ix, int n, atomic_dict_node *expected, atomic_dict_node *desired,
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

atomic_dict_reservation_buffer *AtomicDict_GetReservationBuffer(AtomicDict *dk);

void AtomicDict_ReservationBufferPut(atomic_dict_reservation_buffer *rb, atomic_dict_entry_loc *entry_loc, int n);

void AtomicDict_ReservationBufferPop(atomic_dict_reservation_buffer *rb, atomic_dict_entry_loc *entry_loc);

void AtomicDict_GetEmptyEntry(AtomicDict *dk, atomic_dict_meta *meta, atomic_dict_reservation_buffer *rb,
                              atomic_dict_entry_loc *entry_loc, Py_hash_t hash);


/// robin hood hashing
typedef enum AtomicDict_RobinHoodResult {
    ok,
    failed,
    grow,
} AtomicDict_RobinHoodResult;

AtomicDict_RobinHoodResult AtomicDict_RobinHoodInsert(atomic_dict_meta *meta, atomic_dict_node *nodes,
                                                      atomic_dict_node *to_insert, int distance_0_ix);

AtomicDict_RobinHoodResult AtomicDict_RobinHoodDelete(atomic_dict_meta *meta, atomic_dict_node *nodes,
                                                      int to_delete);


/// semi-internal
typedef struct {
    int error;
    uint64_t position;
    atomic_dict_node node;
    atomic_dict_entry *entry_p;
    atomic_dict_entry entry;
    int is_reservation;
} atomic_dict_search_result;

void AtomicDict_Lookup(atomic_dict_meta *meta, PyObject *key, Py_hash_t hash,
                       atomic_dict_search_result *result);

void AtomicDict_LookupEntry(atomic_dict_meta *meta, uint64_t entry_ix, Py_hash_t hash,
                            atomic_dict_search_result *result);

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
} AtomicDict_NodeSizeInfo;

#define ATOMIC_DICT_MIN_LOG_SIZE 6
#define ATOMIC_DICT_MAX_LOG_SIZE 56

extern const AtomicDict_NodeSizeInfo AtomicDict_NodeSizesTable[];

#endif //CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H
