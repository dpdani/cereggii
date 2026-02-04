// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_REDUCE_TABLE_H
#define CEREGGII_REDUCE_TABLE_H

#include <Python.h>


// Hash table for local aggregation during AtomicDict.reduce operations.
// Maps keys to (expected, desired) pairs using linear probing.
// Similar to CPython's dict with split entries and index.

#define REDUCE_TABLE_MIN_LOG_SIZE 7
#define REDUCE_TABLE_INITIAL_LOG_SIZE 7

typedef struct ReduceTableEntry {
    Py_hash_t hash;
    PyObject *key;
    PyObject *expected;
    PyObject *desired;
} ReduceTableEntry;

typedef struct ReduceTable {
    uint8_t log_size;               // log2 of index size
    uint64_t used;                  // number of active entries
    uint64_t *index;                // indices into entries array
    ReduceTableEntry *entries;      // contiguous entries array
} ReduceTable;

#define REDUCE_TABLE_SIZE(table) (1ULL << (table)->log_size)
#define REDUCE_TABLE_MASK(table) (REDUCE_TABLE_SIZE(table) - 1)
#define REDUCE_TABLE_EMPTY_ENTRY UINT64_MAX

static inline uint64_t
reduce_table_hash_to_index(Py_hash_t hash, ReduceTable *table)
{
    return hash & REDUCE_TABLE_MASK(table);
}

static inline ReduceTable *
reduce_table_new(uint8_t log_size)
{
    if (log_size < REDUCE_TABLE_MIN_LOG_SIZE) {
        log_size = REDUCE_TABLE_MIN_LOG_SIZE;
    }

    ReduceTable *table = PyMem_Malloc(sizeof(ReduceTable));
    if (table == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    table->log_size = log_size;
    table->used = 0;

    uint64_t index_size = REDUCE_TABLE_SIZE(table);
    table->index = PyMem_Malloc(index_size * sizeof(uint64_t));
    if (table->index == NULL) {
        PyMem_Free(table);
        PyErr_NoMemory();
        return NULL;
    }

    table->entries = PyMem_Malloc(index_size * sizeof(ReduceTableEntry));
    if (table->entries == NULL) {
        PyMem_Free(table->index);
        PyMem_Free(table);
        PyErr_NoMemory();
        return NULL;
    }

    // Initialize index with empty markers
    for (uint64_t i = 0; i < index_size; i++) {
        table->index[i] = REDUCE_TABLE_EMPTY_ENTRY;
    }

    return table;
}

static inline void
reduce_table_free(ReduceTable *table)
{
    if (table == NULL)
        return;

    PyMem_Free(table->entries);
    PyMem_Free(table->index);
    PyMem_Free(table);
}

static inline int
reduce_table_resize(ReduceTable *table)
{
    uint8_t old_log_size = table->log_size;
    uint8_t new_log_size = old_log_size + 1;
    uint64_t new_index_size = 1ULL << new_log_size;

    // Allocate new index
    uint64_t *new_index = PyMem_Malloc(new_index_size * sizeof(uint64_t));
    if (new_index == NULL) {
        PyErr_NoMemory();
        return -1;
    }

    // Allocate new entries array
    ReduceTableEntry *new_entries = PyMem_Malloc(new_index_size * sizeof(ReduceTableEntry));
    if (new_entries == NULL) {
        PyMem_Free(new_index);
        PyErr_NoMemory();
        return -1;
    }

    // Initialize new index
    for (uint64_t i = 0; i < new_index_size; i++) {
        new_index[i] = REDUCE_TABLE_EMPTY_ENTRY;
    }

    // Rehash all entries
    ReduceTable new_table = {
        .log_size = new_log_size,
        .used = 0,
        .index = new_index,
        .entries = new_entries,
    };

    for (uint64_t i = 0; i < table->used; i++) {
        ReduceTableEntry *old_entry = &table->entries[i];
        Py_hash_t hash = old_entry->hash;
        uint64_t ix = reduce_table_hash_to_index(hash, &new_table);

        // Linear probing to find empty slot
        while (new_index[ix] != REDUCE_TABLE_EMPTY_ENTRY) {
            ix = (ix + 1) & (new_index_size - 1);
        }

        // Insert into new table
        uint64_t entry_ix = new_table.used++;
        new_index[ix] = entry_ix;
        new_entries[entry_ix] = *old_entry;
    }

    // Free old allocations and update table
    PyMem_Free(table->index);
    PyMem_Free(table->entries);
    table->log_size = new_log_size;
    table->index = new_index;
    table->entries = new_entries;

    return 0;
}

// Lookup key in table. Returns pointer to entry if found, NULL otherwise.
// Sets *entry_ix to the entry index if found.
static inline ReduceTableEntry *
reduce_table_lookup(ReduceTable *table, PyObject *key, Py_hash_t hash, uint64_t *entry_ix)
{
    uint64_t ix = reduce_table_hash_to_index(hash, table);
    uint64_t start_ix = ix;
    uint64_t mask = REDUCE_TABLE_MASK(table);

    while (1) {
        uint64_t entry_index = table->index[ix];

        if (entry_index == REDUCE_TABLE_EMPTY_ENTRY) {
            return NULL;
        }

        ReduceTableEntry *entry = &table->entries[entry_index];

        if (entry->hash == hash) {
            if (entry->key == key) {
                if (entry_ix != NULL) {
                    *entry_ix = entry_index;
                }
                return entry;
            }

            int eq = PyObject_RichCompareBool(entry->key, key, Py_EQ);
            if (eq < 0) {
                return NULL;  // exception during compare
            }
            if (eq) {
                if (entry_ix != NULL) {
                    *entry_ix = entry_index;
                }
                return entry;
            }
        }

        // Linear probe
        ix = (ix + 1) & mask;
        if (ix == start_ix) {
            // Wrapped around, table is full (shouldn't happen with proper resizing)
            return NULL;
        }
    }
}

// Insert or update entry in table.
// Steals references to key, expected, and desired.
// Returns 0 on success, -1 on error.
static inline int
reduce_table_set(ReduceTable *table, PyObject *key, Py_hash_t hash,
                 PyObject *expected, PyObject *desired)
{
    assert(key != NULL);
    assert(expected != NULL);
    assert(desired != NULL);

    // Check if resize is needed
    if (table->used > REDUCE_TABLE_SIZE(table) * 2 / 3) {
        if (reduce_table_resize(table) < 0) {
            return -1;
        }
    }

    // Check if key already exists
    uint64_t existing_entry_ix;
    ReduceTableEntry *existing = reduce_table_lookup(table, key, hash, &existing_entry_ix);

    if (existing != NULL) {
        // Update existing entry - steal new refs, decref old ones
        PyObject *old_expected = existing->expected;
        PyObject *old_desired = existing->desired;

        existing->expected = expected;  // steal reference
        existing->desired = desired;    // steal reference

        Py_DECREF(old_expected);
        Py_DECREF(old_desired);
        Py_DECREF(key);  // we don't need the key since entry already exists

        return 0;
    }

    // Insert new entry
    uint64_t ix = reduce_table_hash_to_index(hash, table);
    uint64_t mask = REDUCE_TABLE_MASK(table);

    // Linear probe for empty slot
    while (table->index[ix] != REDUCE_TABLE_EMPTY_ENTRY) {
        ix = (ix + 1) & mask;
    }

    uint64_t entry_ix = table->used++;
    table->index[ix] = entry_ix;

    ReduceTableEntry *entry = &table->entries[entry_ix];
    entry->hash = hash;
    entry->key = key;            // steal reference
    entry->expected = expected;  // steal reference
    entry->desired = desired;    // steal reference

    return 0;
}

// Get entry by key. Returns 1 if found (sets *expected and *desired), 0 if not found, -1 on error.
static inline int
reduce_table_get(ReduceTable *table, PyObject *key, Py_hash_t hash,
                 PyObject **expected, PyObject **desired)
{
    ReduceTableEntry *entry = reduce_table_lookup(table, key, hash, NULL);

    if (entry == NULL) {
        if (PyErr_Occurred()) {
            return -1;
        }
        return 0;
    }

    *expected = entry->expected;
    *desired = entry->desired;
    return 1;
}

#endif  // CEREGGII_REDUCE_TABLE_H
