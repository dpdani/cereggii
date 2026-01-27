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
    int8_t flags;
    Py_hash_t hash;
    PyObject *key;
    PyObject *value;
} AtomicDictEntry;

#define ENTRY_FLAGS_EMPTY         0
#define ENTRY_FLAGS_RESERVED     64
// #define ENTRY_FLAGS_?         32
// #define ENTRY_FLAGS_?         16
// #define ENTRY_FLAGS_?          8
// #define ENTRY_FLAGS_?          4
// #define ENTRY_FLAGS_?          2
// #define ENTRY_FLAGS_?          1

typedef struct AtomicDictEntryLoc {
    AtomicDictEntry *entry;
    int64_t location;
} AtomicDictEntryLoc;


typedef struct AtomicDictNode {
    int64_t index;
} AtomicDictNode;

#define NODE_EMPTY ((AtomicDictNode){ .index = -1 })
#define NODE_TOMBSTONE ((AtomicDictNode){ .index = -2 })


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

    AtomicDictNode *index;

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

AtomicDictMeta *AtomicDictMeta_New(int8_t log_size);

void meta_clear_index(AtomicDictMeta *meta);

int meta_init_pages(AtomicDictMeta *meta);

int meta_copy_pages(AtomicDictMeta *from_meta, AtomicDictMeta *to_meta);

AtomicDict_Page *AtomicDictPage_New(AtomicDictMeta *meta);

int64_t page_of(int64_t entry_ix);

int64_t position_in_page_of(int64_t entry_ix);

AtomicDictEntry *get_entry_at(int64_t ix, AtomicDictMeta *meta);

void read_entry(AtomicDictEntry *entry_p, AtomicDictEntry *entry);


/// operations on nodes (see ./node_ops.c)

int64_t distance0_of(Py_hash_t hash, AtomicDictMeta* meta);

AtomicDictNode read_node_at(int64_t ix, AtomicDictMeta* meta);

int is_empty(AtomicDictNode node);

int is_tombstone(AtomicDictNode node);

void write_node_at(int64_t ix, AtomicDictNode node, AtomicDictMeta *meta);

int atomic_write_node_at(int64_t ix, AtomicDictNode expected, AtomicDictNode desired, AtomicDictMeta *meta);

void print_node_at(int64_t ix, AtomicDictMeta *meta);


/// reservation buffer (see ./reservation_buffer.c)
#define RESERVATION_BUFFER_SIZE 64

typedef struct AtomicDictReservationBuffer {
    int head, tail, count;
    AtomicDictEntryLoc reservations[RESERVATION_BUFFER_SIZE];
} AtomicDictReservationBuffer;

void reservation_buffer_put(AtomicDictReservationBuffer *rb, AtomicDictEntryLoc *entry_loc, int n, AtomicDictMeta *meta);

void reservation_buffer_pop(AtomicDictReservationBuffer *rb, AtomicDictEntryLoc *entry_loc);

void update_pages_in_reservation_buffer(AtomicDictReservationBuffer *rb, int64_t from_page, int64_t to_page);

int get_empty_entry(AtomicDict *self, AtomicDictMeta *meta, AtomicDictReservationBuffer *rb,
                             AtomicDictEntryLoc *entry_loc, Py_hash_t hash);

int atomic_dict_entry_ix_sanity_check(int64_t entry_ix, AtomicDictMeta *meta);


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

AtomicDictAccessorStorage *get_or_create_accessor_storage(AtomicDict *self);

AtomicDictAccessorStorage *get_accessor_storage(Py_tss_t *accessor_key);

void free_accessor_storage(AtomicDictAccessorStorage *self);

void free_accessor_storage_list(AtomicDictAccessorStorage *head);

AtomicDictMeta *get_meta(AtomicDict *self, AtomicDictAccessorStorage *storage);

void begin_synchronous_operation(AtomicDict *self);

void end_synchronous_operation(AtomicDict *self);

int64_t approx_len(AtomicDict *self);

int64_t approx_inserted(AtomicDict *self);

void accessor_len_inc(AtomicDict *self, AtomicDictAccessorStorage *storage, int32_t inc);

void accessor_inserted_inc(AtomicDict *self, AtomicDictAccessorStorage *storage, int32_t inc);

void accessor_tombstones_inc(AtomicDict *self, AtomicDictAccessorStorage *storage, int32_t inc);

/// migrations
int grow(AtomicDict *self);

int maybe_help_migrate(AtomicDictMeta *meta, PyMutex *self_mutex);

int migrate(AtomicDict *self, AtomicDictMeta *current_meta);

int leader_migrate(AtomicDict *self, AtomicDictMeta *current_meta);

void follower_migrate(AtomicDictMeta *current_meta);

void common_migrate(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta);

void migrate_node(AtomicDictNode *node, AtomicDictMeta *new_meta, int64_t trailing_cluster_start, int64_t trailing_cluster_size);

int64_t migrate_nodes(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta);

int nodes_migration_done(AtomicDictMeta *accessors);


/// iter
struct AtomicDictFastIterator {
    PyObject_HEAD

    AtomicDict *dict;
    AtomicDictMeta *meta;
    int64_t position;

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

void lookup(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash,
                       AtomicDictSearchResult *result);

void lookup_entry(AtomicDictMeta *meta, int64_t entry_ix, Py_hash_t hash,
                  AtomicDictSearchResult *result);

void delete_(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash, AtomicDictSearchResult *result);

int unsafe_insert(AtomicDictMeta *meta, Py_hash_t hash, int64_t pos);

PyObject* expected_insert_or_update(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash,
                                    PyObject *expected, PyObject *desired,
                                    AtomicDictEntryLoc *entry_loc, int *must_grow, int skip_entry_check);


#define ATOMIC_DICT_MIN_LOG_SIZE 6
#define ATOMIC_DICT_MAX_LOG_SIZE 56

#endif //CEREGGII_DEV_ATOMIC_DICT_INTERNAL_H
