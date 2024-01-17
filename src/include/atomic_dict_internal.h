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
} AtomicDict_Entry;

#define ENTRY_FLAGS_RESERVED    128
#define ENTRY_FLAGS_TOMBSTONE   64
#define ENTRY_FLAGS_INSERTED    32
#define ENTRY_FLAGS_SWAPPING    16
// #define ENTRY_FLAGS_?    8
// #define ENTRY_FLAGS_?    4
// #define ENTRY_FLAGS_?    2
// #define ENTRY_FLAGS_?    1

typedef struct {
    AtomicDict_Entry *entry;
    uint64_t location;
} AtomicDict_EntryLoc;


typedef struct AtomicDict_Node {
    uint64_t node;
    uint64_t index;
    uint8_t distance;
    uint64_t tag;
} AtomicDict_Node;


typedef struct {
    PyObject_HEAD
} AtomicDict_Gen;

static PyTypeObject AtomicDictGen = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_basicsize = sizeof(AtomicDict_Gen),
    .tp_itemsize = 0,
    .tp_new = PyType_GenericNew,
};


typedef struct {
    PyObject *generation;
//    PyObject *iteration;

    AtomicDict_Entry entries[64];
} AtomicDict_Block;


/// meta
struct AtomicDict_Meta;
typedef struct AtomicDict_Meta AtomicDict_Meta;

struct AtomicDict_Meta {
    PyObject_HEAD

    uint8_t log_size;  // = node index_size
    uint64_t size;

    PyObject *generation;

    uint64_t *index;

    AtomicDict_Block **blocks;
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

    AtomicDict_Node tombstone;

    void (*read_nodes_in_region)(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

    void (*read_nodes_in_zone)(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);
};

void AtomicDictMeta_dealloc(AtomicDict_Meta *self);

static PyTypeObject AtomicDictMeta = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "cereggii._atomic_dict_meta",
    .tp_basicsize = sizeof(AtomicDict_Meta),
    .tp_itemsize = 0,
    .tp_new = PyType_GenericNew,
    .tp_dealloc = (destructor) AtomicDictMeta_dealloc,
};

AtomicDict_Meta *AtomicDict_NewMeta(uint8_t log_size, AtomicDict_Meta *previous_meta);

AtomicDict_Block *AtomicDict_NewBlock(AtomicDict_Meta *meta);

AtomicDict_Entry *AtomicDict_GetEntryAt(uint64_t ix, AtomicDict_Meta *meta);


/// operations on nodes (see ./node_ops.c)
void AtomicDict_ComputeRawNode(AtomicDict_Node *node, AtomicDict_Meta *meta);

void AtomicDict_ParseNodeFromRaw(uint64_t node_raw, AtomicDict_Node *node,
                                 AtomicDict_Meta *meta);

void AtomicDict_ParseNodeFromRegion(uint64_t ix, uint64_t region, AtomicDict_Node *node,
                                    AtomicDict_Meta *meta);

uint64_t AtomicDict_RegionOf(uint64_t ix, AtomicDict_Meta *meta);

uint64_t AtomicDict_ZoneOf(uint64_t ix, AtomicDict_Meta *meta);

uint64_t AtomicDict_Distance0Of(Py_hash_t hash, AtomicDict_Meta *meta);

uint64_t AtomicDict_ShiftInRegionOf(uint64_t ix, AtomicDict_Meta *meta);

uint8_t *AtomicDict_IndexAddressOf(uint64_t ix, AtomicDict_Meta *meta);

int AtomicDict_IndexAddressIsAligned(uint64_t ix, int alignment, AtomicDict_Meta *meta);

void AtomicDict_ReadNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta);

void AtomicDict_Read1NodeAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

void AtomicDict_Read2NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

void AtomicDict_Read4NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

void AtomicDict_Read8NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

void AtomicDict_Read16NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

void AtomicDict_CopyNodeBuffers(AtomicDict_Node *from_buffer, AtomicDict_Node *to_buffer);

void AtomicDict_ComputeBeginEndWrite(AtomicDict_Meta *meta, AtomicDict_Node *read_buffer, AtomicDict_Node *temp,
                                     int *begin_write, int *end_write, int64_t *start_ix);

typedef struct AtomicDict_BufferedNodeReader {
    int64_t zone;
    AtomicDict_Node buffer[16];
    AtomicDict_Node node;
    int idx_in_buffer;
    int nodes_offset;
} AtomicDict_BufferedNodeReader;

void AtomicDict_ReadNodesFromZoneIntoBuffer(uint64_t idx, AtomicDict_BufferedNodeReader *reader,
                                            AtomicDict_Meta *meta);

void AtomicDict_ReadNodesFromZoneStartIntoBuffer(uint64_t idx, AtomicDict_BufferedNodeReader *reader,
                                                 AtomicDict_Meta *meta);

int AtomicDict_WriteNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta);

int AtomicDict_NodeIsReservation(AtomicDict_Node *node, AtomicDict_Meta *meta);

int AtomicDict_NodeIsTombstone(AtomicDict_Node *node, AtomicDict_Meta *meta);

int AtomicDict_MustWriteBytes(int n, AtomicDict_Meta *meta);

int AtomicDict_AtomicWriteNodesAt(uint64_t ix, int n, AtomicDict_Node *expected, AtomicDict_Node *desired,
                                  AtomicDict_Meta *meta);


/// insert
typedef enum AtomicDict_InsertedOrUpdated {
    error,
    inserted,
    updated,
    nop,
    retry,
} AtomicDict_InsertedOrUpdated;

AtomicDict_InsertedOrUpdated
AtomicDict_CheckNodeEntryAndMaybeUpdate(uint64_t distance_0, uint64_t i, AtomicDict_Node *node,
                                        AtomicDict_Meta *meta, Py_hash_t hash, PyObject *key, PyObject *value);


/// migrations
int AtomicDict_Grow(AtomicDict *self);

int AtomicDict_Shrink(AtomicDict *self);


/// reservation buffer (see ./reservation_buffer.c)
#define RESERVATION_BUFFER_SIZE 64

typedef struct {
    PyObject_HEAD

    int head, tail, count;
    AtomicDict_EntryLoc reservations[RESERVATION_BUFFER_SIZE];
} AtomicDict_ReservationBuffer;

static PyTypeObject AtomicDictReservationBuffer = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_basicsize = sizeof(AtomicDict_ReservationBuffer),
    .tp_itemsize = 0,
    .tp_new = PyType_GenericNew,
};

AtomicDict_ReservationBuffer *AtomicDict_GetReservationBuffer(AtomicDict *dk);

void AtomicDict_ReservationBufferPut(AtomicDict_ReservationBuffer *rb, AtomicDict_EntryLoc *entry_loc, int n);

void AtomicDict_ReservationBufferPop(AtomicDict_ReservationBuffer *rb, AtomicDict_EntryLoc *entry_loc);

void AtomicDict_GetEmptyEntry(AtomicDict *dk, AtomicDict_Meta *meta, AtomicDict_ReservationBuffer *rb,
                              AtomicDict_EntryLoc *entry_loc, Py_hash_t hash);


/// robin hood hashing
typedef enum AtomicDict_RobinHoodResult {
    ok,
    failed,
    grow,
} AtomicDict_RobinHoodResult;

AtomicDict_RobinHoodResult AtomicDict_RobinHoodInsert(AtomicDict_Meta *meta, AtomicDict_Node *nodes,
                                                      AtomicDict_Node *to_insert, int distance_0_ix);

AtomicDict_RobinHoodResult AtomicDict_RobinHoodDelete(AtomicDict_Meta *meta, AtomicDict_Node *nodes,
                                                      int to_delete);


/// semi-internal
typedef struct {
    int error;
    uint64_t position;
    AtomicDict_Node node;
    AtomicDict_Entry *entry_p;
    AtomicDict_Entry entry;
    int is_reservation;
} AtomicDict_SearchResult;

void AtomicDict_Lookup(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash,
                       AtomicDict_SearchResult *result);

void AtomicDict_LookupEntry(AtomicDict_Meta *meta, uint64_t entry_ix, Py_hash_t hash,
                            AtomicDict_SearchResult *result);

int AtomicDict_UnsafeInsert(AtomicDict *self, PyObject *key, Py_hash_t hash, PyObject *value, Py_ssize_t pos);

int AtomicDict_InsertOrUpdate(AtomicDict *self, AtomicDict_Meta *meta,
                              AtomicDict_EntryLoc *entry_loc);

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
