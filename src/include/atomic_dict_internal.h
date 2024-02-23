// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H
#define CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H

#include "atomic_dict.h"
#include "atomic_event.h"


/// basic structs
typedef struct AtomicDict_Entry {
    uint8_t flags;
    Py_hash_t hash;
    PyObject *key;
    PyObject *value;
} AtomicDict_Entry;

#define ENTRY_FLAGS_RESERVED    128
#define ENTRY_FLAGS_TOMBSTONE   64
#define ENTRY_FLAGS_SWAPPED     32
#define ENTRY_FLAGS_COMPACTED   16
#define ENTRY_FLAGS_LOCKED      8
// #define ENTRY_FLAGS_?    4
// #define ENTRY_FLAGS_?    2
// #define ENTRY_FLAGS_?    1

typedef struct AtomicDict_EntryLoc {
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


/// blocks
#define ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK 6
#define ATOMIC_DICT_ENTRIES_IN_BLOCK (1 << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK)

typedef struct AtomicDict_Block {
    PyObject_HEAD

    PyObject *generation;
    // PyObject *iteration;

    AtomicDict_Entry entries[ATOMIC_DICT_ENTRIES_IN_BLOCK];
} AtomicDict_Block;

extern PyTypeObject AtomicDictBlock_Type;

void AtomicDictBlock_dealloc(AtomicDict_Block *self);


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
    Py_hash_t tag_mask;
    uint64_t shift_mask;

    AtomicDict_Node tombstone;
    AtomicDict_Node zero;

    void (*read_nodes_in_region)(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

    void (*read_nodes_in_zone)(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

    // migration
    AtomicDict_Meta *new_gen_metadata;
    uintptr_t migration_leader;
    uint8_t *copy_nodes_locks;
    AtomicEvent *new_metadata_ready;
    AtomicEvent *copy_nodes_done;
    AtomicEvent *compaction_done;
    AtomicEvent *node_migration_done;
    AtomicEvent *migration_done;
};

void AtomicDictMeta_dealloc(AtomicDict_Meta *self);

extern PyTypeObject AtomicDictMeta_Type;

AtomicDict_Meta *AtomicDictMeta_New(uint8_t log_size);

void AtomicDictMeta_ClearIndex(AtomicDict_Meta *meta);

int AtomicDictMeta_InitBlocks(AtomicDict_Meta *meta);

int AtomicDictMeta_CopyBlocks(AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta);

void AtomicDictMeta_ShrinkBlocks(AtomicDict *self, AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta);

AtomicDict_Block *AtomicDictBlock_New(AtomicDict_Meta *meta);

uint64_t AtomicDict_BlockOf(uint64_t entry_ix);

uint64_t AtomicDict_PositionInBlockOf(uint64_t entry_ix);

AtomicDict_Entry *AtomicDict_GetEntryAt(uint64_t ix, AtomicDict_Meta *meta);

void AtomicDict_ReadEntry(AtomicDict_Entry *entry_p, AtomicDict_Entry *entry);


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

int AtomicDict_WriteRawNodeAt(uint64_t ix, uint64_t raw_node, AtomicDict_Meta *meta);

int AtomicDict_NodeIsReservation(AtomicDict_Node *node, AtomicDict_Meta *meta);

int AtomicDict_NodeIsTombstone(AtomicDict_Node *node, AtomicDict_Meta *meta);

int AtomicDict_MustWriteBytes(int n, AtomicDict_Meta *meta);

int AtomicDict_AtomicWriteNodesAt(uint64_t ix, int n, AtomicDict_Node *expected, AtomicDict_Node *desired,
                                  AtomicDict_Meta *meta);


/// delete
int AtomicDict_IncrementGreatestDeletedBlock(AtomicDict_Meta *meta, int64_t gab, int64_t gdb);


/// insert
typedef enum AtomicDict_InsertedOrUpdated {
    error,
    inserted,
    updated,
    nop,
    retry,
    must_grow,
} AtomicDict_InsertedOrUpdated;

AtomicDict_InsertedOrUpdated
AtomicDict_CheckNodeEntryAndMaybeUpdate(uint64_t distance_0, uint64_t i, AtomicDict_Node *node,
                                        AtomicDict_Meta *meta, Py_hash_t hash, PyObject *key, PyObject *value);


/// migrations
int AtomicDict_Grow(AtomicDict *self);

int AtomicDict_Shrink(AtomicDict *self);

int AtomicDict_MaybeHelpMigrate(AtomicDict_Meta *meta);

int AtomicDict_Migrate(AtomicDict *self, AtomicDict_Meta *current_meta, uint8_t from_log_size, uint8_t to_log_size);

int AtomicDict_LeaderMigrate(AtomicDict *self, AtomicDict_Meta *current_meta,
                             uint8_t from_log_size, uint8_t to_log_size);

void AtomicDict_FollowerMigrate(AtomicDict_Meta *current_meta);

void AtomicDict_CommonMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

int AtomicDict_MigrateReInsertAll(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);


/// reservation buffer (see ./reservation_buffer.c)
#define RESERVATION_BUFFER_SIZE 64

typedef struct AtomicDict_ReservationBuffer {
    PyObject_HEAD

    int head, tail, count;
    AtomicDict_EntryLoc reservations[RESERVATION_BUFFER_SIZE];
} AtomicDict_ReservationBuffer;


extern PyTypeObject AtomicDictReservationBuffer_Type;

AtomicDict_ReservationBuffer *AtomicDict_GetReservationBuffer(AtomicDict *dk);

void AtomicDict_ReservationBuffer_dealloc(AtomicDict_ReservationBuffer *self);

void AtomicDict_ReservationBufferPut(AtomicDict_ReservationBuffer *rb, AtomicDict_EntryLoc *entry_loc, int n);

void AtomicDict_ReservationBufferPop(AtomicDict_ReservationBuffer *rb, AtomicDict_EntryLoc *entry_loc);

void
AtomicDict_UpdateBlocksInReservationBuffer(AtomicDict_ReservationBuffer *rb, uint64_t from_block, uint64_t to_block);

int AtomicDict_GetEmptyEntry(AtomicDict *self, AtomicDict_Meta *meta, AtomicDict_ReservationBuffer *rb,
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

AtomicDict_RobinHoodResult AtomicDict_RobinHoodCompact(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta,
                                                       uint64_t probe_head, uint64_t probe_length);

void AtomicDict_RobinHoodCompact_LeftRightSort(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta,
                                               uint64_t probe_head, uint64_t probe_length,
                                               AtomicDict_BufferedNodeReader *reader_lx,
                                               AtomicDict_BufferedNodeReader *reader_rx,
                                               int (*should_go_left)(AtomicDict_Node *node, AtomicDict_Entry *entry,
                                                                     AtomicDict_Meta *current_meta,
                                                                     AtomicDict_Meta *new_meta));

void AtomicDict_RobinHoodCompact_ReadLeftRight(AtomicDict_Meta *new_meta, uint64_t left, uint64_t right,
                                               AtomicDict_Entry *left_entry, AtomicDict_Entry *right_entry,
                                               AtomicDict_Entry **left_entry_p, AtomicDict_Entry **right_entry_p,
                                               AtomicDict_BufferedNodeReader *reader_lx,
                                               AtomicDict_BufferedNodeReader *reader_rx);

int AtomicDict_RobinHoodCompact_ShouldGoLeft(AtomicDict_Node *node, AtomicDict_Entry *entry,
                                             AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

int AtomicDict_RobinHoodCompact_IsNotTombstone(AtomicDict_Node *node, AtomicDict_Entry *_unused_entry,
                                               AtomicDict_Meta *_unused_current_meta, AtomicDict_Meta *new_meta);

int AtomicDict_RobinHoodCompact_CompactNodes(uint64_t probe_head, uint64_t probe_length, int right,
                                             AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

int AtomicDict_RobinHoodCompact_CompactNodes_Sort(uint64_t probe_head, uint64_t probe_length,
                                                  AtomicDict_BufferedNodeReader *reader, AtomicDict_Meta *new_meta,
                                                  Py_hash_t hash_mask);

uint64_t
AtomicDict_RobinHoodCompact_CompactNodes_Partition(uint64_t probe_head, uint64_t probe_length,
                                                   AtomicDict_BufferedNodeReader *reader_lx,
                                                   AtomicDict_BufferedNodeReader *reader_rx,
                                                   uint64_t *distance_0_cache, Py_hash_t hash_mask,
                                                   AtomicDict_Meta *new_meta);

void
AtomicDict_RobinHoodCompact_CompactNodes_Swap(uint64_t ix_a, uint64_t ix_b, AtomicDict_BufferedNodeReader *reader_a,
                                              AtomicDict_BufferedNodeReader *reader_b, AtomicDict_Meta *new_meta,
                                              uint64_t *distance_0_cache, uint64_t probe_head);

/// semi-internal
typedef struct AtomicDict_SearchResult {
    int error;
    int found;
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

int AtomicDict_Delete(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash);

int AtomicDict_UnsafeInsert(AtomicDict *self, PyObject *key, Py_hash_t hash, PyObject *value, Py_ssize_t pos);

AtomicDict_InsertedOrUpdated AtomicDict_InsertOrUpdate(AtomicDict_Meta *meta, AtomicDict_EntryLoc *entry_loc);

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
