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


/// blocks
#define ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK 6
#define ATOMIC_DICT_ENTRIES_IN_BLOCK (1 << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK)

typedef struct AtomicDict_Block {
    PyObject_HEAD

    void *generation;
    // PyObject *iteration;

    AtomicDict_Entry entries[ATOMIC_DICT_ENTRIES_IN_BLOCK];
} AtomicDict_Block;

extern PyTypeObject AtomicDictBlock_Type;

int AtomicDictBlock_traverse(AtomicDict_Block *self, visitproc visit, void *arg);

int AtomicDictBlock_clear(AtomicDict_Block *self);

void AtomicDictBlock_dealloc(AtomicDict_Block *self);


/// meta
struct AtomicDict_Meta;
typedef struct AtomicDict_Meta AtomicDict_Meta;

struct AtomicDict_Meta {
    PyObject_HEAD

    uint8_t log_size;  // = node index_size
    uint64_t size;

    void *generation;

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
    uint64_t d0_shift;

    AtomicDict_Node tombstone;
    AtomicDict_Node zero;

    void (*read_nodes_in_region)(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

    void (*read_nodes_in_zone)(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

    // migration
    AtomicDict_Meta *new_gen_metadata;
    uintptr_t migration_leader;
    int64_t node_to_migrate;
    Py_tss_t *accessor_key;
    PyObject *accessors;
    AtomicEvent *new_metadata_ready;
    AtomicEvent *node_migration_done;
    AtomicEvent *migration_done;
};

int AtomicDictMeta_traverse(AtomicDict_Meta *self, visitproc visit, void *arg);

int AtomicDictMeta_clear(AtomicDict_Meta *self);

void AtomicDictMeta_dealloc(AtomicDict_Meta *self);

extern PyTypeObject AtomicDictMeta_Type;

AtomicDict_Meta *AtomicDictMeta_New(uint8_t log_size);

void AtomicDictMeta_ClearIndex(AtomicDict_Meta *meta);

int AtomicDictMeta_InitBlocks(AtomicDict_Meta *meta);

int AtomicDictMeta_CopyBlocks(AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta);

void AtomicDictMeta_ShrinkBlocks(AtomicDict *self, AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta);

AtomicDict_Block *AtomicDictBlock_New(AtomicDict_Meta *meta);

int64_t AtomicDict_BlockOf(uint64_t entry_ix);

uint64_t AtomicDict_PositionInBlockOf(uint64_t entry_ix);

AtomicDict_Entry *AtomicDict_GetEntryAt(uint64_t ix, AtomicDict_Meta *meta);

void AtomicDict_ReadEntry(AtomicDict_Entry *entry_p, AtomicDict_Entry *entry);

int AtomicDict_CountKeysInBlock(int64_t block_ix, AtomicDict_Meta *meta);


/// operations on nodes (see ./node_ops.c)
void AtomicDict_ComputeRawNode(AtomicDict_Node *node, AtomicDict_Meta *meta);

void AtomicDict_ParseNodeFromRaw(uint64_t node_raw, AtomicDict_Node *node,
                                 AtomicDict_Meta *meta);

void AtomicDict_ParseNodeFromRegion(uint64_t ix, uint64_t region, AtomicDict_Node *node,
                                    AtomicDict_Meta *meta);

uint64_t AtomicDict_ParseRawNodeFromRegion(uint64_t ix, uint64_t region, AtomicDict_Meta *meta);

uint64_t AtomicDict_RegionOf(uint64_t ix, AtomicDict_Meta *meta);

uint64_t AtomicDict_ZoneOf(uint64_t ix, AtomicDict_Meta *meta);

uint64_t AtomicDict_Distance0Of(Py_hash_t hash, AtomicDict_Meta *meta);

uint64_t AtomicDict_ShiftInRegionOf(uint64_t ix, AtomicDict_Meta *meta);

uint8_t *AtomicDict_IndexAddressOf(uint64_t ix, AtomicDict_Meta *meta);

int AtomicDict_IndexAddressIsAligned(uint64_t ix, int alignment, AtomicDict_Meta *meta);

void AtomicDict_ReadNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta);

int64_t AtomicDict_ReadRawNodeAt(uint64_t ix, AtomicDict_Meta *meta);

void AtomicDict_Read1NodeAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

void AtomicDict_Read2NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

void AtomicDict_Read4NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

void AtomicDict_Read8NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

void AtomicDict_Read16NodesAt(uint64_t ix, AtomicDict_Node *nodes, AtomicDict_Meta *meta);

void AtomicDict_CopyNodeBuffers(AtomicDict_Node *from_buffer, AtomicDict_Node *to_buffer);

void AtomicDict_ComputeBeginEndWrite(AtomicDict_Meta *meta, AtomicDict_Node *read_buffer, AtomicDict_Node *temp,
                                     int *begin_write, int *end_write);

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

void AtomicDict_WriteNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta);

void AtomicDict_WriteRawNodeAt(uint64_t ix, uint64_t raw_node, AtomicDict_Meta *meta);

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

int AtomicDict_MaybeHelpMigrate(AtomicDict_Meta *meta, PyMutex *self_mutex);

int AtomicDict_Migrate(AtomicDict *self, AtomicDict_Meta *current_meta, uint8_t from_log_size, uint8_t to_log_size);

int AtomicDict_LeaderMigrate(AtomicDict *self, AtomicDict_Meta *current_meta,
                             uint8_t from_log_size, uint8_t to_log_size);

void AtomicDict_FollowerMigrate(AtomicDict_Meta *current_meta);

void AtomicDict_CommonMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

void AtomicDict_QuickMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

void AtomicDict_SlowMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

int AtomicDict_MigrateReInsertAll(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

int AtomicDict_PrepareHashArray(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

void
AtomicDict_MigrateNode(AtomicDict_Node *node, uint64_t *distance_0, uint64_t *distance, AtomicDict_Meta *new_meta,
                       uint64_t size_mask);

int AtomicDict_MigrateNodes(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

void
AtomicDict_SeekToProbeStart(AtomicDict_Meta *meta, uint64_t *pos, uint64_t displacement, uint64_t current_size_mask);

void AtomicDict_SeekToProbeEnd(AtomicDict_Meta *meta, uint64_t *pos, uint64_t displacement, uint64_t current_size_mask);

int AtomicDict_NodesMigrationDone(const AtomicDict_Meta *current_meta);


/// reservation buffer (see ./reservation_buffer.c)
#define RESERVATION_BUFFER_SIZE 64

typedef struct AtomicDict_ReservationBuffer {
    int head, tail, count;
    AtomicDict_EntryLoc reservations[RESERVATION_BUFFER_SIZE];
} AtomicDict_ReservationBuffer;

void AtomicDict_ReservationBufferPut(AtomicDict_ReservationBuffer *rb, AtomicDict_EntryLoc *entry_loc, int n);

void AtomicDict_ReservationBufferPop(AtomicDict_ReservationBuffer *rb, AtomicDict_EntryLoc *entry_loc);

void
AtomicDict_UpdateBlocksInReservationBuffer(AtomicDict_ReservationBuffer *rb, uint64_t from_block, uint64_t to_block);

int AtomicDict_GetEmptyEntry(AtomicDict *self, AtomicDict_Meta *meta, AtomicDict_ReservationBuffer *rb,
                             AtomicDict_EntryLoc *entry_loc, Py_hash_t hash);


/// accessor storage
typedef struct AtomicDict_AccessorStorage {
    PyObject_HEAD

    PyMutex self_mutex;

    int32_t local_len;

    int participant_in_migration;

    AtomicDict_ReservationBuffer reservation_buffer;
} AtomicDict_AccessorStorage;

extern PyTypeObject AtomicDictAccessorStorage_Type;

AtomicDict_AccessorStorage *AtomicDict_GetOrCreateAccessorStorage(AtomicDict *self);

AtomicDict_AccessorStorage *AtomicDict_GetAccessorStorage(Py_tss_t *accessor_key);

void AtomicDict_BeginSynchronousOperation(AtomicDict *self);

void AtomicDict_EndSynchronousOperation(AtomicDict *self);

void AtomicDict_AccessorStorage_dealloc(AtomicDict_AccessorStorage *self);


/// robin hood hashing
typedef enum AtomicDict_RobinHoodResult {
    ok,
    failed,
    grow,
} AtomicDict_RobinHoodResult;

AtomicDict_RobinHoodResult AtomicDict_RobinHoodInsert(AtomicDict_Meta *meta, AtomicDict_Node *nodes,
                                                      AtomicDict_Node *to_insert, int distance_0_ix);

AtomicDict_RobinHoodResult AtomicDict_RobinHoodInsertRaw(AtomicDict_Meta *meta, AtomicDict_Node *to_insert,
                                                         int64_t distance_0_ix);

AtomicDict_RobinHoodResult AtomicDict_RobinHoodDelete(AtomicDict_Meta *meta, AtomicDict_Node *nodes,
                                                      int to_delete);


/// iter
struct AtomicDict_FastIterator {
    PyObject_HEAD

    AtomicDict *dict;
    AtomicDict_Meta *meta;
    uint64_t position;

    int partitions;
};

extern PyTypeObject AtomicDictFastIterator_Type;

void AtomicDictFastIterator_dealloc(AtomicDict_FastIterator *self);

PyObject *AtomicDictFastIterator_Next(AtomicDict_FastIterator *self);

PyObject *AtomicDictFastIterator_GetIter(AtomicDict_FastIterator *self);


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

int AtomicDict_UnsafeInsert(AtomicDict_Meta *meta, Py_hash_t hash, uint64_t pos);

PyObject *AtomicDict_ExpectedInsertOrUpdate(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash,
                                            PyObject *expected, PyObject *desired,
                                            AtomicDict_EntryLoc *entry_loc, int *must_grow, int skip_entry_check);


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
