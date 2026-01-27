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
typedef struct AtomicDictEntry {
    uint8_t flags;
    Py_hash_t hash;
    PyObject *key;
    PyObject *value;
} AtomicDictEntry;

#define ENTRY_FLAGS_RESERVED    128
// #define ENTRY_FLAGS_?         64
// #define ENTRY_FLAGS_?         32
// #define ENTRY_FLAGS_?         16
// #define ENTRY_FLAGS_?          8
// #define ENTRY_FLAGS_?          4
// #define ENTRY_FLAGS_?          2
// #define ENTRY_FLAGS_?          1

typedef struct AtomicDictEntryLoc {
    AtomicDictEntry *entry;
    uint64_t location;
} AtomicDictEntryLoc;

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

typedef struct AtomicDictNode {
    uint64_t node;
    uint64_t index;
    uint64_t tag;
} AtomicDictNode;


/// pages
#define ATOMIC_DICT_LOG_ENTRIES_IN_PAGE 6
#define ATOMIC_DICT_ENTRIES_IN_PAGE (1 << ATOMIC_DICT_LOG_ENTRIES_IN_PAGE)

CEREGGII_ALIGNED(LEVEL1_DCACHE_LINESIZE)
typedef struct AtomicDict_PaddedEntry {
    AtomicDictEntry entry;
    int8_t _padding[LEVEL1_DCACHE_LINESIZE - sizeof(AtomicDictEntry)];
} AtomicDict_PaddedEntry;

typedef struct AtomicDictPage {
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
struct AtomicDictMeta;
typedef struct AtomicDictMeta AtomicDictMeta;

struct AtomicDictMeta {
    PyObject_HEAD

    uint8_t log_size;  // = node index_size

    void *generation;

    uint64_t *index;

    AtomicDict_Page **pages;
    int64_t inserting_page;
    int64_t greatest_allocated_page;

    // migration
    AtomicDictMeta *new_gen_metadata;
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

int AtomicDictMeta_traverse(AtomicDictMeta *self, visitproc visit, void *arg);

int AtomicDictMeta_clear(AtomicDictMeta *self);

void AtomicDictMeta_dealloc(AtomicDictMeta *self);

extern PyTypeObject AtomicDictMeta_Type;

AtomicDictMeta *AtomicDictMeta_New(uint8_t log_size);

void AtomicDictMeta_ClearIndex(AtomicDictMeta *meta);

int AtomicDictMeta_InitPages(AtomicDictMeta *meta);

int AtomicDictMeta_CopyPages(AtomicDictMeta *from_meta, AtomicDictMeta *to_meta);

AtomicDict_Page *AtomicDictPage_New(AtomicDictMeta *meta);

uint64_t AtomicDict_PageOf(uint64_t entry_ix);

uint64_t AtomicDict_PositionInPageOf(uint64_t entry_ix);

AtomicDictEntry *AtomicDict_GetEntryAt(uint64_t ix, AtomicDictMeta *meta);

void AtomicDict_ReadEntry(AtomicDictEntry *entry_p, AtomicDictEntry *entry);

int AtomicDict_CountKeysInPage(int64_t page_ix, AtomicDictMeta *meta);


/// operations on nodes (see ./node_ops.c)
void AtomicDict_ComputeRawNode(AtomicDictNode *node, AtomicDictMeta *meta);

void AtomicDict_ParseNodeFromRaw(uint64_t node_raw, AtomicDictNode *node,
                                 AtomicDictMeta *meta);

uint64_t AtomicDict_Distance0Of(Py_hash_t hash, AtomicDictMeta *meta);

uint64_t AtomicDict_ReadRawNodeAt(uint64_t ix, AtomicDictMeta *meta);

int AtomicDict_IsEmpty(AtomicDictNode *node);

int AtomicDict_IsTombstone(AtomicDictNode *node);

void AtomicDict_ReadNodeAt(uint64_t ix, AtomicDictNode *node, AtomicDictMeta *meta);

void AtomicDict_WriteNodeAt(uint64_t ix, AtomicDictNode *node, AtomicDictMeta *meta);

void AtomicDict_WriteRawNodeAt(uint64_t ix, uint64_t raw_node, AtomicDictMeta *meta);

int AtomicDict_AtomicWriteNodeAt(uint64_t ix, AtomicDictNode *expected, AtomicDictNode *desired, AtomicDictMeta *meta);

void AtomicDict_PrintNodeAt(uint64_t ix, AtomicDictMeta *meta);


/// reservation buffer (see ./reservation_buffer.c)
#define RESERVATION_BUFFER_SIZE 64

typedef struct AtomicDictReservationBuffer {
    int head, tail, count;
    AtomicDictEntryLoc reservations[RESERVATION_BUFFER_SIZE];
} AtomicDictReservationBuffer;

void AtomicDict_ReservationBufferPut(AtomicDictReservationBuffer *rb, AtomicDictEntryLoc *entry_loc, int n, AtomicDictMeta *meta);

void AtomicDict_ReservationBufferPop(AtomicDictReservationBuffer *rb, AtomicDictEntryLoc *entry_loc);

void
AtomicDict_UpdatePagesInReservationBuffer(AtomicDictReservationBuffer *rb, uint64_t from_page, uint64_t to_page);

int AtomicDict_GetEmptyEntry(AtomicDict *self, AtomicDictMeta *meta, AtomicDictReservationBuffer *rb,
                             AtomicDictEntryLoc *entry_loc, Py_hash_t hash);

int atomic_dict_entry_ix_sanity_check(uint64_t entry_ix, AtomicDictMeta *meta);


/// accessor storage
typedef struct AtomicDictAccessorStorage {
    struct AtomicDictAccessorStorage *next_accessor;
    PyMutex self_mutex;
    int64_t local_len;
    int64_t local_inserted;
    int64_t local_tombstones;
    int32_t accessor_ix;
    AtomicDictReservationBuffer reservation_buffer;
    AtomicDictMeta *meta;
} AtomicDictAccessorStorage;

#define FOR_EACH_ACCESSOR(atomic_dict, a) \
    for ( \
        (a) = atomic_load_explicit((_Atomic (AtomicDictAccessorStorage *) *) &(atomic_dict)->accessors, memory_order_acquire); \
        (a) != NULL; \
        (a) = atomic_load_explicit((_Atomic (AtomicDictAccessorStorage *) *) &(a)->next_accessor, memory_order_acquire) \
    )

AtomicDictAccessorStorage *AtomicDict_GetOrCreateAccessorStorage(AtomicDict *self);

AtomicDictAccessorStorage *AtomicDict_GetAccessorStorage(Py_tss_t *accessor_key);

void AtomicDict_FreeAccessorStorage(AtomicDictAccessorStorage *self);

void AtomicDict_FreeAccessorStorageList(AtomicDictAccessorStorage *head);

AtomicDictMeta *AtomicDict_GetMeta(AtomicDict *self, AtomicDictAccessorStorage *storage);

void AtomicDict_BeginSynchronousOperation(AtomicDict *self);

void AtomicDict_EndSynchronousOperation(AtomicDict *self);

int64_t atomic_dict_approx_len(AtomicDict *self);

int64_t atomic_dict_approx_inserted(AtomicDict *self);

void atomic_dict_accessor_len_inc(AtomicDict *self, AtomicDictAccessorStorage *storage, int32_t inc);

void atomic_dict_accessor_inserted_inc(AtomicDict *self, AtomicDictAccessorStorage *storage, int32_t inc);

void atomic_dict_accessor_tombstones_inc(AtomicDict *self, AtomicDictAccessorStorage *storage, int32_t inc);

/// migrations
int AtomicDict_Grow(AtomicDict *self);

int AtomicDict_MaybeHelpMigrate(AtomicDictMeta *meta, PyMutex *self_mutex);

int AtomicDict_Migrate(AtomicDict *self, AtomicDictMeta *current_meta);

int AtomicDict_LeaderMigrate(AtomicDict *self, AtomicDictMeta *current_meta);

void AtomicDict_FollowerMigrate(AtomicDictMeta *current_meta);

void AtomicDict_CommonMigrate(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta);

int AtomicDict_MigrateReInsertAll(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta);

void AtomicDict_MigrateNode(AtomicDictNode *node, AtomicDictMeta *new_meta, uint64_t trailing_cluster_start, uint64_t trailing_cluster_size);

int64_t AtomicDict_MigrateNodes(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta);

int AtomicDict_NodesMigrationDone(AtomicDictMeta *accessors);


/// iter
struct AtomicDictFastIterator {
    PyObject_HEAD

    AtomicDict *dict;
    AtomicDictMeta *meta;
    uint64_t position;

    int partitions;
};

extern PyTypeObject AtomicDictFastIterator_Type;

void AtomicDictFastIterator_dealloc(AtomicDict_FastIterator *self);

PyObject *AtomicDictFastIterator_Next(AtomicDict_FastIterator *self);

PyObject *AtomicDictFastIterator_GetIter(AtomicDict_FastIterator *self);


/// semi-internal
typedef struct AtomicDictSearchResult {
    int error;
    int found;
    uint64_t position;
    AtomicDictNode node;
    AtomicDictEntry *entry_p;
    AtomicDictEntry entry;
} AtomicDictSearchResult;

void AtomicDict_Lookup(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash,
                       AtomicDictSearchResult *result);

void AtomicDict_LookupEntry(AtomicDictMeta *meta, uint64_t entry_ix, Py_hash_t hash,
                            AtomicDictSearchResult *result);

void AtomicDict_Delete(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash, AtomicDictSearchResult *result);

int AtomicDict_UnsafeInsert(AtomicDictMeta *meta, Py_hash_t hash, uint64_t pos);

PyObject *AtomicDict_ExpectedInsertOrUpdate(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash,
                                            PyObject *expected, PyObject *desired,
                                            AtomicDictEntryLoc *entry_loc, int *must_grow, int skip_entry_check);


#define ATOMIC_DICT_MIN_LOG_SIZE 6
#define ATOMIC_DICT_MAX_LOG_SIZE 56

#endif //CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H
