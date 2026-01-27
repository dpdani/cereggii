// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H
#define CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H

#include "atomic_dict.h"
#include "atomic_event.h"
#include "internal/cereggiiconfig.h"
#include "internal/misc.h"


/// basic structs
typedef struct AtomicDict_Entry {
    uint8_t flags;
    Py_hash_t hash;
    PyObject *key;
    PyObject *value;
} AtomicDict_Entry;

#define ENTRY_FLAGS_RESERVED    128
// #define ENTRY_FLAGS_?         64
// #define ENTRY_FLAGS_?         32
// #define ENTRY_FLAGS_?         16
// #define ENTRY_FLAGS_?          8
// #define ENTRY_FLAGS_?          4
// #define ENTRY_FLAGS_?          2
// #define ENTRY_FLAGS_?          1

typedef struct AtomicDict_EntryLoc {
    AtomicDict_Entry *entry;
    uint64_t location;
} AtomicDict_EntryLoc;

/*
 * Node layout in memory
 *
 * +--------+--------+
 * | index  |  tag   |
 * +--------+--------+
 * */
#define NODE_SIZE 64
#define TAG_MASK(meta) ((1ULL << (NODE_SIZE - (meta)->log_size)) - 1)
#define TOMBSTONE(meta) (TAG_MASK(meta))

typedef struct AtomicDict_Node {
    uint64_t node;
    uint64_t index;
    uint64_t tag;
} AtomicDict_Node;


/// pages
#define ATOMIC_DICT_LOG_ENTRIES_IN_PAGE 6
#define ATOMIC_DICT_ENTRIES_IN_PAGE (1 << ATOMIC_DICT_LOG_ENTRIES_IN_PAGE)

CEREGGII_ALIGNED(LEVEL1_DCACHE_LINESIZE)
typedef struct AtomicDict_PaddedEntry {
    AtomicDict_Entry entry;
    int8_t _padding[LEVEL1_DCACHE_LINESIZE - sizeof(AtomicDict_Entry)];
} AtomicDict_PaddedEntry;

typedef struct AtomicDict_Page {
    PyObject_HEAD

    void *generation;
    // PyObject *iteration;

    AtomicDict_PaddedEntry entries[ATOMIC_DICT_ENTRIES_IN_PAGE];
} AtomicDict_Page;

extern PyTypeObject AtomicDictPage_Type;

int AtomicDictPage_traverse(AtomicDict_Page *self, visitproc visit, void *arg);

int AtomicDictPage_clear(AtomicDict_Page *self);

void AtomicDictPage_dealloc(AtomicDict_Page *self);


/// meta
struct AtomicDict_Meta;
typedef struct AtomicDict_Meta AtomicDict_Meta;

struct AtomicDict_Meta {
    PyObject_HEAD

    uint8_t log_size;  // = node index_size

    void *generation;

    uint64_t *index;

    AtomicDict_Page **pages;
    int64_t inserting_page;
    int64_t greatest_allocated_page;
    int64_t greatest_deleted_page;
    int64_t greatest_refilled_page;

    // migration
    AtomicDict_Meta *new_gen_metadata;
    uintptr_t migration_leader;
    int64_t node_to_migrate;
    Py_tss_t *accessor_key;
    int64_t *participants;
    int32_t participants_count;
    AtomicEvent *new_metadata_ready;
    AtomicEvent *node_migration_done;
    AtomicEvent *migration_done;
};

#define SIZE_OF(meta) (1ll << (meta)->log_size)

int AtomicDictMeta_traverse(AtomicDict_Meta *self, visitproc visit, void *arg);

int AtomicDictMeta_clear(AtomicDict_Meta *self);

void AtomicDictMeta_dealloc(AtomicDict_Meta *self);

extern PyTypeObject AtomicDictMeta_Type;

AtomicDict_Meta *AtomicDictMeta_New(uint8_t log_size);

void AtomicDictMeta_ClearIndex(AtomicDict_Meta *meta);

int AtomicDictMeta_InitPages(AtomicDict_Meta *meta);

int AtomicDictMeta_CopyPages(AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta);

void AtomicDictMeta_ShrinkPages(AtomicDict *self, AtomicDict_Meta *from_meta, AtomicDict_Meta *to_meta);

AtomicDict_Page *AtomicDictPage_New(AtomicDict_Meta *meta);

uint64_t AtomicDict_PageOf(uint64_t entry_ix);

uint64_t AtomicDict_PositionInPageOf(uint64_t entry_ix);

AtomicDict_Entry *AtomicDict_GetEntryAt(uint64_t ix, AtomicDict_Meta *meta);

void AtomicDict_ReadEntry(AtomicDict_Entry *entry_p, AtomicDict_Entry *entry);

int AtomicDict_CountKeysInPage(int64_t page_ix, AtomicDict_Meta *meta);


/// operations on nodes (see ./node_ops.c)
void AtomicDict_ComputeRawNode(AtomicDict_Node *node, AtomicDict_Meta *meta);

void AtomicDict_ParseNodeFromRaw(uint64_t node_raw, AtomicDict_Node *node,
                                 AtomicDict_Meta *meta);

uint64_t AtomicDict_Distance0Of(Py_hash_t hash, AtomicDict_Meta *meta);

uint64_t AtomicDict_ReadRawNodeAt(uint64_t ix, AtomicDict_Meta *meta);

int AtomicDict_IsEmpty(AtomicDict_Node *node);

int AtomicDict_IsTombstone(AtomicDict_Node *node);

void AtomicDict_ReadNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta);

void AtomicDict_WriteNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta);

void AtomicDict_WriteRawNodeAt(uint64_t ix, uint64_t raw_node, AtomicDict_Meta *meta);

int AtomicDict_AtomicWriteNodeAt(uint64_t ix, AtomicDict_Node *expected, AtomicDict_Node *desired, AtomicDict_Meta *meta);

void AtomicDict_PrintNodeAt(uint64_t ix, AtomicDict_Meta *meta);


/// reservation buffer (see ./reservation_buffer.c)
#define RESERVATION_BUFFER_SIZE 64

typedef struct AtomicDict_ReservationBuffer {
    int head, tail, count;
    AtomicDict_EntryLoc reservations[RESERVATION_BUFFER_SIZE];
} AtomicDict_ReservationBuffer;

void AtomicDict_ReservationBufferPut(AtomicDict_ReservationBuffer *rb, AtomicDict_EntryLoc *entry_loc, int n, AtomicDict_Meta *meta);

void AtomicDict_ReservationBufferPop(AtomicDict_ReservationBuffer *rb, AtomicDict_EntryLoc *entry_loc);

void
AtomicDict_UpdatePagesInReservationBuffer(AtomicDict_ReservationBuffer *rb, uint64_t from_page, uint64_t to_page);

int AtomicDict_GetEmptyEntry(AtomicDict *self, AtomicDict_Meta *meta, AtomicDict_ReservationBuffer *rb,
                             AtomicDict_EntryLoc *entry_loc, Py_hash_t hash);

int atomic_dict_entry_ix_sanity_check(uint64_t entry_ix, AtomicDict_Meta *meta);


/// accessor storage
typedef struct AtomicDict_AccessorStorage {
    struct AtomicDict_AccessorStorage *next_accessor;
    PyMutex self_mutex;
    int64_t local_len;
    int64_t local_inserted;
    int64_t local_tombstones;
    int32_t accessor_ix;
    AtomicDict_ReservationBuffer reservation_buffer;
    AtomicDict_Meta *meta;
} AtomicDict_AccessorStorage;

#define FOR_EACH_ACCESSOR(atomic_dict, a) \
    for ( \
        (a) = atomic_load_explicit((_Atomic (AtomicDict_AccessorStorage *) *) &(atomic_dict)->accessors, memory_order_acquire); \
        (a) != NULL; \
        (a) = atomic_load_explicit((_Atomic (AtomicDict_AccessorStorage *) *) &(a)->next_accessor, memory_order_acquire) \
    )

AtomicDict_AccessorStorage *AtomicDict_GetOrCreateAccessorStorage(AtomicDict *self);

AtomicDict_AccessorStorage *AtomicDict_GetAccessorStorage(Py_tss_t *accessor_key);

void AtomicDict_FreeAccessorStorage(AtomicDict_AccessorStorage *self);

void AtomicDict_FreeAccessorStorageList(AtomicDict_AccessorStorage *head);

AtomicDict_Meta *AtomicDict_GetMeta(AtomicDict *self, AtomicDict_AccessorStorage *storage);

void AtomicDict_BeginSynchronousOperation(AtomicDict *self);

void AtomicDict_EndSynchronousOperation(AtomicDict *self);

int64_t atomic_dict_approx_len(AtomicDict *self);

int64_t atomic_dict_approx_inserted(AtomicDict *self);

void atomic_dict_accessor_len_inc(AtomicDict *self, AtomicDict_AccessorStorage *storage, int32_t inc);

void atomic_dict_accessor_inserted_inc(AtomicDict *self, AtomicDict_AccessorStorage *storage, int32_t inc);

void atomic_dict_accessor_tombstones_inc(AtomicDict *self, AtomicDict_AccessorStorage *storage, int32_t inc);

/// migrations
int AtomicDict_Grow(AtomicDict *self);

int AtomicDict_MaybeHelpMigrate(AtomicDict_Meta *meta, PyMutex *self_mutex);

int AtomicDict_Migrate(AtomicDict *self, AtomicDict_Meta *current_meta, uint8_t from_log_size, uint8_t to_log_size);

int AtomicDict_LeaderMigrate(AtomicDict *self, AtomicDict_Meta *current_meta,
                             uint8_t from_log_size, uint8_t to_log_size);

void AtomicDict_FollowerMigrate(AtomicDict_Meta *current_meta);

void AtomicDict_CommonMigrate(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

int AtomicDict_MigrateReInsertAll(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

void AtomicDict_MigrateNode(AtomicDict_Node *node, AtomicDict_Meta *new_meta, uint64_t trailing_cluster_start, uint64_t trailing_cluster_size);

int64_t AtomicDict_MigrateNodes(AtomicDict_Meta *current_meta, AtomicDict_Meta *new_meta);

int AtomicDict_NodesMigrationDone(AtomicDict_Meta *accessors);


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
} AtomicDict_SearchResult;

void AtomicDict_Lookup(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash,
                       AtomicDict_SearchResult *result);

void AtomicDict_LookupEntry(AtomicDict_Meta *meta, uint64_t entry_ix, Py_hash_t hash,
                            AtomicDict_SearchResult *result);

void AtomicDict_Delete(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash, AtomicDict_SearchResult *result);

int AtomicDict_UnsafeInsert(AtomicDict_Meta *meta, Py_hash_t hash, uint64_t pos);

PyObject *AtomicDict_ExpectedInsertOrUpdate(AtomicDict_Meta *meta, PyObject *key, Py_hash_t hash,
                                            PyObject *expected, PyObject *desired,
                                            AtomicDict_EntryLoc *entry_loc, int *must_grow, int skip_entry_check);


#define ATOMIC_DICT_MIN_LOG_SIZE 6
#define ATOMIC_DICT_MAX_LOG_SIZE 56

#endif //CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H
