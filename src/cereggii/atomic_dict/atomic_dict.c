// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"

#include <stdatomic.h>

#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "pythread.h"
#include "thread_id.h"
#include "thread_handle.h"
#include "_internal_py_core.h"
#include <vendor/pythoncapi_compat/pythoncapi_compat.h>
#include "constants.h"
#include <stdint.h>


PyObject *
AtomicDict_new(PyTypeObject *type, PyObject *Py_UNUSED(args), PyObject *Py_UNUSED(kwds))
{
    AtomicDict *self = NULL;
    self = PyObject_GC_New(AtomicDict, type);
    if (self != NULL) {
        self->metadata = NULL;
        self->min_log_size = 0;
        self->reservation_buffer_size = 0;
        self->sync_op = (PyMutex) {0};
        self->len = 0;
        self->len_dirty = 0;
        self->accessor_key = NULL;
        self->accessors_lock = (PyMutex) {0};
        self->accessors_len = 0;
        self->accessors = NULL;

        self->metadata = (AtomicRef *) AtomicRef_new(&AtomicRef_Type, NULL, NULL);
        if (self->metadata == NULL)
            goto fail;
        AtomicRef_init(self->metadata, NULL, NULL);
        // Py_None is the default value for AtomicRef

        self->reservation_buffer_size = 0;
        Py_tss_t *tss_key = NULL;
        tss_key = PyThread_tss_alloc();
        if (tss_key == NULL)
            goto fail;
        if (PyThread_tss_create(tss_key)) {
            PyThread_tss_free(tss_key);
            PyErr_SetString(PyExc_RuntimeError, "could not create TSS key");
            goto fail;
        }
        assert(PyThread_tss_is_created(tss_key) != 0);
        self->accessor_key = tss_key;

        PyObject_GC_Track(self);
    }
    return (PyObject *) self;

    fail:
    // the VM will call AtomicDict_clear later, only need to decref self here
    Py_DECREF(self);
    return NULL;
}

int
AtomicDict_init(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    int reserved = AtomicRef_CompareAndSet(self->metadata, Py_None, Py_True);
    // if we can succeed in turning None -> True, then this thread reserved
    // the right to run the init function for this AtomicDict()
    if (!reserved) {
        // either another thread concurrently succeeded in making the reservation,
        // or this AtomicDict was already initialized.
        PyErr_SetString(PyExc_RuntimeError, "cannot initialize an AtomicDict more than once.");
        // returning immediately might let the non-reserving __init__() callers
        // see a half-initialized AtomicDict, so we do a little spin-lock
        PyObject *current = AtomicRef_Get(self->metadata);
        while (current == Py_None || current == Py_True) {
            Py_CLEAR(current);
            current = AtomicRef_Get(self->metadata);
        }
        Py_DECREF(current);
        return -1;
    }

    int64_t init_dict_size = 0;
    int64_t min_size = 0;
    int64_t buffer_size = 4;
    PyObject *initial = NULL;
    PyObject *min_size_arg = NULL;
    PyObject *buffer_size_arg = NULL;
    AtomicDictMeta *meta = NULL;

    char *kw_list[] = {"initial", "min_size", "buffer_size", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OOO", kw_list, &initial, &min_size_arg, &buffer_size_arg)) {
        goto fail;
    }
    if (initial != NULL) {
        if (!PyDict_Check(initial)) {
            PyErr_SetString(PyExc_TypeError, "type(initial) is not dict");
            goto fail;
        }
    }

    if (min_size_arg != NULL) {
        int error = PyLong_AsInt64(min_size_arg, &min_size);
        if (error)
            return -1;
        if (min_size > (1LL << ATOMIC_DICT_MAX_LOG_SIZE)) {
            PyErr_SetString(PyExc_ValueError, "min_size > 2 ** 56");
            return -1;
        }
    }
    if (buffer_size_arg != NULL) {
        int error = PyLong_AsInt64(buffer_size_arg, &buffer_size);
        if (error)
            return -1;

        if (buffer_size != 1 && buffer_size != 2 && buffer_size != 4 &&
            buffer_size != 8 && buffer_size != 16 && buffer_size != 32 &&
            buffer_size != 64) {
            PyErr_SetString(PyExc_ValueError, "buffer_size not in (1, 2, 4, 8, 16, 32, 64)");
            return -1;
        }
    }

    if (initial != NULL) {
        init_dict_size = PyDict_Size(initial) * 2;
    }
    if (init_dict_size % ATOMIC_DICT_ENTRIES_IN_PAGE == 0) { // allocate one more entry: cannot write to entry 0
        init_dict_size++;
    }
    if (min_size < ATOMIC_DICT_ENTRIES_IN_PAGE) {
        min_size = ATOMIC_DICT_ENTRIES_IN_PAGE;
    }

    assert(buffer_size >= 0 && buffer_size <= 64);
    self->reservation_buffer_size = (uint8_t) buffer_size;

    uint8_t log_size = 0, init_dict_log_size = 0;
    uint64_t min_size_tmp = (uint64_t) min_size;
    uint64_t init_dict_size_tmp = (uint64_t) init_dict_size;
    while (min_size_tmp >>= 1) {
        log_size++;
    }
    if (min_size > 1ll << log_size) {
        log_size++;
    }
    // 64 = 0b1000000
    // 0 -> 0b1000000
    // 1 -> 0b100000
    // 2 -> 0b10000
    // 3 -> 0b1000
    // 4 -> 0b100
    // 5 -> 0b10
    // 6 -> 0b1
    // 7 -> 0b0
    if (log_size > ATOMIC_DICT_MAX_LOG_SIZE) {
        PyErr_SetString(PyExc_ValueError, "can hold at most 2^56 items.");
        return -1;
    }
    self->min_log_size = log_size;

    while (init_dict_size_tmp >>= 1) {
        init_dict_log_size++;
    }
    if (init_dict_size > 1ll << init_dict_log_size) {
        init_dict_log_size++;
    }
    if (init_dict_log_size > log_size) {
        log_size = init_dict_log_size;
    }

    create:
    meta = NULL;
    meta = AtomicDictMeta_New(log_size);
    if (meta == NULL)
        goto fail;
    meta_clear_index(meta);
    if (meta_init_pages(meta) < 0)
        goto fail;

    AtomicDict_Page *page;
    int64_t i;
    for (i = 0; i < init_dict_size / ATOMIC_DICT_ENTRIES_IN_PAGE; i++) {
        // allocate pages
        page = AtomicDictPage_New(meta);
        if (page == NULL)
            goto fail;
        meta->pages[i] = page;
        if (i + 1 < (1 << log_size) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE) {
            meta->pages[i + 1] = NULL;
        }
        meta->greatest_allocated_page++;
    }
    if (init_dict_size % ATOMIC_DICT_ENTRIES_IN_PAGE > 0) {
        // allocate additional page
        page = AtomicDictPage_New(meta);
        if (page == NULL)
            goto fail;
        meta->pages[i] = page;
        if (i + 1 < (1 << log_size) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE) {
            meta->pages[i + 1] = NULL;
        }
        meta->greatest_allocated_page++;
    }

    meta->inserting_page = 0;
    AtomicDictAccessorStorage *storage;
    AtomicDictEntryLoc entry_loc;
    self->sync_op = (PyMutex) {0};
    self->accessors_lock = (PyMutex) {0};
    self->accessors_len = 0;
    self->len = 0;
    self->len_dirty = 0;

    if (initial != NULL && PyDict_Size(initial) > 0) {
        PyObject *key, *value;
        Py_hash_t hash;
        Py_ssize_t pos = 0;

        Py_BEGIN_CRITICAL_SECTION(initial);

        while (PyDict_Next(initial, &pos, &key, &value)) {
            hash = PyObject_Hash(key);
            if (hash == -1)
                goto fail;

            self->len++; // we want to avoid pos = 0
            AtomicDictEntry *entry = get_entry_at(self->len, meta);
            _Py_SetWeakrefAndIncref(key);
            _Py_SetWeakrefAndIncref(value);
            entry->flags = ENTRY_FLAGS_RESERVED;
            entry->hash = hash;
            assert(key != NULL);
            entry->key = key;
            entry->value = value;
            int inserted = unsafe_insert(meta, hash, self->len);
            if (inserted == -1) {
                Py_DECREF(meta);
                log_size++;
                goto create;
            }
        }

        Py_END_CRITICAL_SECTION();

        meta->inserting_page = self->len >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE;

        if (self->len > 0) {
            storage = get_or_create_accessor_storage(self);
            if (storage == NULL)
                goto fail;

            // handle possibly misaligned reservations on last page
            // => put them into this thread's reservation buffer
            assert(meta->greatest_allocated_page >= 0);
            if (page_of(self->len + 1) <= (uint64_t) meta->greatest_allocated_page) {
                entry_loc.entry = get_entry_at(self->len + 1, meta);
                entry_loc.location = self->len + 1;

                uint8_t n =
                    self->reservation_buffer_size - (uint8_t) (entry_loc.location % self->reservation_buffer_size);
                assert(n <= ATOMIC_DICT_ENTRIES_IN_PAGE);
                while (page_of(self->len + n) > (uint64_t) meta->greatest_allocated_page) {
                    n--;

                    if (n == 0)
                        break;
                }

                if (n > 0) {
                    reservation_buffer_put(&storage->reservation_buffer, &entry_loc, n, meta);
                }
            }
        }
    }

    if (!(get_entry_at(0, meta)->flags & ENTRY_FLAGS_RESERVED)) {
        storage = get_or_create_accessor_storage(self);
        if (storage == NULL)
            goto fail;

        // mark entry 0 as reserved and put the remaining entries
        // into this thread's reservation buffer
        get_entry_at(0, meta)->flags |= ENTRY_FLAGS_RESERVED;
        for (i = 1; i < self->reservation_buffer_size; ++i) {
            entry_loc.entry = get_entry_at(i, meta);
            entry_loc.location = i;
            if (entry_loc.entry->key == NULL) {
                int found = 0;
                for (int j = 0; j < RESERVATION_BUFFER_SIZE; ++j) {
                    if (storage->reservation_buffer.reservations[j].location == entry_loc.location) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    reservation_buffer_put(&storage->reservation_buffer, &entry_loc, 1, meta);
                }
            }
        }
    }

    int success = AtomicRef_CompareAndSet(self->metadata, Py_True, (PyObject *) meta);
    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "error during initialization of AtomicDict.");
        goto fail;
    }

    Py_DECREF(meta); // so that the only meta's refcount depends only on AtomicRef
    return 0;
    fail:
    Py_XDECREF(meta);
    if (!PyErr_Occurred()) {
        PyErr_SetString(PyExc_RuntimeError, "error during initialization.");
    }
    return -1;
}

int
AtomicDict_traverse(AtomicDict *self, visitproc visit, void *arg)
{
    Py_VISIT(self->metadata);
    AtomicDictAccessorStorage *storage;
    FOR_EACH_ACCESSOR(self, storage) {
        Py_VISIT(storage->meta);
    }
    return 0;
}

int
AtomicDict_clear(AtomicDict *self)
{
    Py_CLEAR(self->metadata);
    if (self->accessors != NULL) {
        AtomicDictAccessorStorage *storage = self->accessors;
        self->accessors = NULL;
        free_accessor_storage_list(storage);
    }
    // this should be enough to deallocate the reservation buffers themselves as well:
    // the list should be the only reference to them

    if (self->accessor_key != NULL) {
        Py_tss_t *key = self->accessor_key;
        self->accessor_key = NULL;
        PyThread_tss_delete(key);
        PyThread_tss_free(key);
    }

    return 0;
}

void
AtomicDict_dealloc(AtomicDict *self)
{
    PyObject_GC_UnTrack(self);
    PyObject_ClearWeakRefs((PyObject *) self);
    AtomicDict_clear(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

/**
 * This is not thread-safe!
 *
 * Used at initialization time, when there can be no concurrent access.
 * Doesn't allocate pages, nor check for migrations, nor update dk->metadata refcount.
 * Doesn't do updates: repeated keys will be repeated, so make sure successive
 * calls to this function don't try to insert the same key into the same AtomicDict.
 **/
int
unsafe_insert(AtomicDictMeta *meta, Py_hash_t hash, uint64_t pos)
{
    // pos === node_index
    AtomicDictNode temp;
    AtomicDictNode node = {
        .index = pos,
        .tag = hash,
    };
    const uint64_t d0 = distance0_of(hash, meta);

    for (uint64_t distance = 0; distance < (uint64_t) SIZE_OF(meta); distance++) {
        read_node_at((d0 + distance) & (SIZE_OF(meta) - 1), &temp, meta);

        if (temp.node == 0) {
            write_node_at((d0 + distance) & (SIZE_OF(meta) - 1), &node, meta);
            goto done;
        }
    }

    // full
    return -1;
    done:
    return 0;
}

PyObject *
AtomicDict_LenBounds(AtomicDict *self)
{
    // deprecated in favor of AtomicDict_ApproxLen
    PyObject *approx_len = AtomicDict_ApproxLen(self);
    if (approx_len == NULL) {
        goto fail;
    }

    return Py_BuildValue("(OO)", approx_len, approx_len);

    fail:
    return NULL;
}


static inline int64_t
sum_of_accessors_len(AtomicDict *self)
{
    int64_t len = 0;
    AtomicDictAccessorStorage *storage;
    FOR_EACH_ACCESSOR(self, storage) {
        len += atomic_load_explicit((_Atomic (int64_t) *) &storage->local_len, memory_order_acquire);
    }
    return len;
}

int64_t
approx_len(AtomicDict *self)
{
    int64_t len = self->len;
    return len + sum_of_accessors_len(self);
}

int64_t
approx_inserted(AtomicDict *self)
{
    int64_t inserted = 0;
    AtomicDictAccessorStorage *storage;
    FOR_EACH_ACCESSOR(self, storage) {
        inserted += atomic_load_explicit((_Atomic (int64_t) *) &storage->local_inserted, memory_order_acquire);
    }
    return inserted;
}

PyObject *
AtomicDict_ApproxLen(AtomicDict *self)
{
    PyObject *added_since_clean = NULL;
    PyObject *latest_len = NULL;
    PyObject *len = NULL;

    latest_len = PyLong_FromSsize_t(self->len);
    if (latest_len == NULL) {
        goto fail;
    }
    added_since_clean = PyLong_FromInt64(sum_of_accessors_len(self));
    if (added_since_clean == NULL) {
        goto fail;
    }
    len = PyNumber_Add(latest_len, added_since_clean);
    if (len == NULL) {
        goto fail;
    }
    Py_DECREF(added_since_clean);
    Py_DECREF(latest_len);
    return len;

    fail:
    Py_XDECREF(added_since_clean);
    Py_XDECREF(latest_len);
    Py_XDECREF(len);
    return NULL;
}

Py_ssize_t
AtomicDict_Len_impl(AtomicDict *self)
{
    PyObject *len = NULL;
    PyObject *added_since_clean = NULL;
    Py_ssize_t len_ssize_t;

    if (!self->len_dirty) {
        len_ssize_t = self->len;
        return len_ssize_t;
    }
    len = PyLong_FromSsize_t(self->len);
    if (len == NULL) {
        goto fail;
    }
    added_since_clean = PyLong_FromInt64(sum_of_accessors_len(self));
    if (added_since_clean == NULL) {
        goto fail;
    }
    len = PyNumber_InPlaceAdd(len, added_since_clean);
    len_ssize_t = PyLong_AsSsize_t(len);
    if (PyErr_Occurred()) {
        goto fail;
    }
    self->len = len_ssize_t;
    self->len_dirty = 0;
    AtomicDictAccessorStorage *storage;
    FOR_EACH_ACCESSOR(self, storage) {
        atomic_store_explicit((_Atomic (int64_t) *) &storage->local_len, 0, memory_order_release);
    }
    Py_DECREF(len);
    Py_DECREF(added_since_clean);
    return len_ssize_t;

    fail:
    Py_XDECREF(len);
    Py_XDECREF(added_since_clean);
    return -1;
}

Py_ssize_t
AtomicDict_Len(AtomicDict *self)
{
    Py_ssize_t len;
    begin_synchronous_operation(self);
    len = AtomicDict_Len_impl(self);
    end_synchronous_operation(self);

    return len;
}

PyObject *
AtomicDict_Debug(AtomicDict *self)
{
    AtomicDictMeta *meta = NULL;
    PyObject *metadata = NULL;
    PyObject *index_nodes = NULL;
    PyObject *pages = NULL;
    PyObject *entries = NULL;
    PyObject *entry_tuple = NULL;
    PyObject *page_info = NULL;

    meta = (AtomicDictMeta *) AtomicRef_Get(self->metadata);
    metadata = Py_BuildValue("{sOsOsOsO}",
                             "log_size\0", Py_BuildValue("B", meta->log_size),
                             "generation\0", Py_BuildValue("n", (Py_ssize_t) meta->generation),
                             "inserting_page\0", Py_BuildValue("L", meta->inserting_page),
                             "greatest_allocated_page\0", Py_BuildValue("L", meta->greatest_allocated_page));
    if (metadata == NULL)
        goto fail;

    index_nodes = Py_BuildValue("[]");
    if (index_nodes == NULL)
        goto fail;

    AtomicDictNode node;
    for (uint64_t i = 0; i < (uint64_t) SIZE_OF(meta); i++) {
        read_node_at(i, &node, meta);
        PyObject *n = Py_BuildValue("K", node.node);
        if (n == NULL)
            goto fail;
        PyList_Append(index_nodes, n);
        Py_DECREF(n);
    }

    pages = Py_BuildValue("[]");
    if (pages == NULL)
        goto fail;

    AtomicDict_Page *page;
    entries = NULL;
    entry_tuple = NULL;
    page_info = NULL;
    for (int64_t i = 0; i <= meta->greatest_allocated_page; i++) {
        page = meta->pages[i];
        entries = Py_BuildValue("[]");
        if (entries == NULL)
            goto fail;

        for (int j = 0; j < 64; j++) {
            PyObject *key = page->entries[j].entry.key;
            PyObject *value = page->entries[j].entry.value;
            if (key != NULL) {
                if (value == NULL) {
                    value = PyExc_KeyError;
                }
                uint64_t entry_ix = (i << ATOMIC_DICT_LOG_ENTRIES_IN_PAGE) + j;
                entry_tuple = Py_BuildValue("(KBnOO)",
                                            entry_ix,
                                            page->entries[j].entry.flags,
                                            page->entries[j].entry.hash,
                                            key,
                                            value);
                if (entry_tuple == NULL)
                    goto fail;
                Py_INCREF(key);
                Py_INCREF(value);
                PyList_Append(entries, entry_tuple);
                Py_DECREF(entry_tuple);
            }
        }

        page_info = Py_BuildValue("{snsO}",
                                   "gen\0", page->generation,
                                   "entries\0", entries);
        Py_DECREF(entries);
        if (page_info == NULL)
            goto fail;
        PyList_Append(pages, page_info);
        Py_DECREF(page_info);
    }

    PyObject *out = Py_BuildValue("{sOsOsO}", "meta\0", metadata, "pages\0", pages, "index\0", index_nodes);
    if (out == NULL)
        goto fail;
    Py_DECREF(meta);
    Py_DECREF(metadata);
    Py_DECREF(pages);
    Py_DECREF(index_nodes);
    return out;

    fail:
    PyErr_SetString(PyExc_RuntimeError, "unable to get debug info");
    Py_XDECREF(meta);
    Py_XDECREF(metadata);
    Py_XDECREF(index_nodes);
    Py_XDECREF(pages);
    Py_XDECREF(entries);
    Py_XDECREF(entry_tuple);
    Py_XDECREF(page_info);
    return NULL;
}

PyObject *
AtomicDict_GetHandle(AtomicDict *self)
{
    ThreadHandle *handle = NULL;
    PyObject *args = NULL;

    handle = (ThreadHandle *) ThreadHandle_new(&ThreadHandle_Type, NULL, NULL);
    if (handle == NULL)
        goto fail;

    args = Py_BuildValue("(O)", self);
    if (ThreadHandle_init(handle, args, NULL) < 0)
        goto fail;
    Py_DECREF(args);

    return (PyObject *) handle;

    fail:
    Py_XDECREF(args);
    return NULL;
}

/// accessor storage

AtomicDictAccessorStorage *
get_or_create_accessor_storage(AtomicDict *self)
{
    assert(self->accessor_key != NULL);
    AtomicDictAccessorStorage *storage = PyThread_tss_get(self->accessor_key);

    if (storage == NULL) {
        storage = PyMem_RawMalloc(sizeof(AtomicDictAccessorStorage));
        if (storage == NULL)
            return NULL;

        storage->next_accessor = NULL;
        storage->self_mutex = (PyMutex) {0};
        atomic_store_explicit((_Atomic(int64_t) *) &storage->local_len, 0, memory_order_release);
        atomic_store_explicit((_Atomic(int64_t) *) &storage->local_inserted, 0, memory_order_release);
        atomic_store_explicit((_Atomic(int64_t) *) &storage->local_tombstones, 0, memory_order_release);
        storage->reservation_buffer.head = 0;
        storage->reservation_buffer.tail = 0;
        storage->reservation_buffer.count = 0;
        memset(storage->reservation_buffer.reservations, 0, sizeof(AtomicDictEntryLoc) * RESERVATION_BUFFER_SIZE);
        storage->meta = (AtomicDictMeta *) AtomicRef_Get(self->metadata);

        int set = PyThread_tss_set(self->accessor_key, storage);
        if (set != 0)
            goto fail;

        PyMutex_Lock(&self->accessors_lock);
        storage->accessor_ix = self->accessors_len;
        if (self->accessors == NULL) {
            self->accessors = storage;
            self->accessors_len = 1;
        } else {
            AtomicDictAccessorStorage *s = NULL;
            for (s = self->accessors; s->next_accessor != NULL; s = s->next_accessor) {}
            assert(s != NULL);
            atomic_store_explicit((_Atomic (AtomicDictAccessorStorage *) *) &s->next_accessor, storage, memory_order_release);
            self->accessors_len++;
        }
        PyMutex_Unlock(&self->accessors_lock);
    }

    return storage;
    fail:
    assert(storage != NULL);
    Py_XDECREF(storage->meta);
    PyMem_RawFree(storage);
    return NULL;
}

AtomicDictAccessorStorage *
get_accessor_storage(Py_tss_t *accessor_key)
{
    assert(accessor_key != NULL);
    AtomicDictAccessorStorage *storage = PyThread_tss_get(accessor_key);
    assert(storage != NULL);
    return storage;
}

void
free_accessor_storage(AtomicDictAccessorStorage *self)
{
    Py_CLEAR(self->meta);
    PyMem_RawFree(self);
}

void
free_accessor_storage_list(AtomicDictAccessorStorage *head)
{
    if (head == NULL)
        return;

    AtomicDictAccessorStorage *prev = head;
    AtomicDictAccessorStorage *next = head->next_accessor;

    while (next) {
        free_accessor_storage(prev);
        prev = next;
        next = next->next_accessor;
    }

    free_accessor_storage(prev);
}

AtomicDictMeta *
get_meta(AtomicDict *self, AtomicDictAccessorStorage *storage)
{
    assert(storage != NULL);
    PyObject *shared = self->metadata->reference;
    PyObject *mine = (PyObject *) storage->meta;
    if (shared == mine)
        return storage->meta;

    Py_CLEAR(storage->meta);
    storage->meta = (AtomicDictMeta *) AtomicRef_Get(self->metadata);
    assert(storage->meta != NULL);
    return storage->meta;
}

void
begin_synchronous_operation(AtomicDict *self)
{
    PyMutex_Lock(&self->sync_op);
    PyMutex_Lock(&self->accessors_lock);
    AtomicDictAccessorStorage *storage;
    FOR_EACH_ACCESSOR(self, storage) {
        PyMutex_Lock(&storage->self_mutex);
    }
}

void
end_synchronous_operation(AtomicDict *self)
{
    PyMutex_Unlock(&self->sync_op);
    AtomicDictAccessorStorage *storage;
    FOR_EACH_ACCESSOR(self, storage) {
        PyMutex_Unlock(&storage->self_mutex);
    }
    PyMutex_Unlock(&self->accessors_lock);
}

/**
 * put [entry; entry + n] into the buffer (inclusive). it may be that n = 1.
 * caller must ensure no segfaults, et similia.
 * */
void
reservation_buffer_put(AtomicDictReservationBuffer *rb, AtomicDictEntryLoc *entry_loc, int n, AtomicDictMeta *meta)
{
    // use asserts to check for circular buffer correctness (don't return and check for error)

    assert(n > 0);
    assert(n <= 64);

    for (int i = 0; i < n; ++i) {
        if (entry_loc->location + i == 0) {
            // entry 0 is reserved for correctness of tombstones
            continue;
        }
        assert(rb->count < 64);
        AtomicDictEntryLoc *head = &rb->reservations[rb->head];
        head->entry = get_entry_at(entry_loc->location + i, meta);
        head->location = entry_loc->location + i;
        assert(atomic_dict_entry_ix_sanity_check(head->location, meta));
        rb->head++;
        if (rb->head == 64) {
            rb->head = 0;
        }
        rb->count++;
    }
    assert(rb->count <= 64);
}

void
reservation_buffer_pop(AtomicDictReservationBuffer *rb, AtomicDictEntryLoc *entry_loc)
{
    if (rb->count == 0) {
        entry_loc->entry = NULL;
        return;
    }

    AtomicDictEntryLoc *tail = &rb->reservations[rb->tail];
    entry_loc->entry = tail->entry;
    entry_loc->location = tail->location;
    memset(&rb->reservations[rb->tail], 0, sizeof(AtomicDictEntryLoc));
    rb->tail++;
    if (rb->tail == 64) {
        rb->tail = 0;
    }
    rb->count--;

    assert(rb->count >= 0);
}

void
update_pages_in_reservation_buffer(AtomicDictReservationBuffer *rb, uint64_t from_page, uint64_t to_page)
{
    for (int i = 0; i < rb->count; ++i) {
        AtomicDictEntryLoc *entry = &rb->reservations[(rb->tail + i) % RESERVATION_BUFFER_SIZE];

        if (entry == NULL)
            continue;

        if (page_of(entry->location) == from_page) {
            entry->location =
                position_in_page_of(entry->location) +
                (to_page << ATOMIC_DICT_LOG_ENTRIES_IN_PAGE);
        }
    }
}

void
accessor_len_inc(AtomicDict *self, AtomicDictAccessorStorage *storage, const int32_t inc)
{
    const int64_t current = atomic_load_explicit((_Atomic (int64_t) *) &storage->local_len, memory_order_acquire);
    const int64_t new = current + inc; // TODO: overflow
    atomic_store_explicit((_Atomic (int64_t) *) &storage->local_len, new, memory_order_release);
    atomic_store_explicit((_Atomic (uint8_t) *) &self->len_dirty, 1, memory_order_release);
}

void
accessor_inserted_inc(AtomicDict *Py_UNUSED(self), AtomicDictAccessorStorage *storage, const int32_t inc)
{
    const int64_t current = atomic_load_explicit((_Atomic (int64_t) *) &storage->local_inserted, memory_order_acquire);
    const int64_t new = current + inc; // TODO: overflow
    atomic_store_explicit((_Atomic (int64_t) *) &storage->local_inserted, new, memory_order_release);
}

void
accessor_tombstones_inc(AtomicDict *Py_UNUSED(self), AtomicDictAccessorStorage *storage, const int32_t inc)
{
    const int64_t current = atomic_load_explicit((_Atomic (int64_t) *) &storage->local_tombstones, memory_order_acquire);
    const int64_t new = current + inc; // TODO: overflow
    atomic_store_explicit((_Atomic (int64_t) *) &storage->local_tombstones, new, memory_order_release);
}

/// delete

void
delete_(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash, AtomicDictSearchResult *result)
{
    lookup(meta, key, hash, result);

    if (result->error) {
        return;
    }

    if (result->entry_p == NULL) {
        return;
    }

    while (!atomic_compare_exchange_strong_explicit(
        (_Atomic(PyObject *) *) &result->entry_p->value,
        &result->entry.value, NULL,
        memory_order_acq_rel, memory_order_acquire
    )) {
        read_entry(result->entry_p, &result->entry);

        if (result->entry.value == NULL) {
            result->found = 0;
            return;
        }
    }

    AtomicDictNode tombstone = {
        .index = 0,
        .tag = TOMBSTONE(meta),
    };

    int ok = atomic_write_node_at(result->position, &result->node, &tombstone, meta);
    assert(ok);
    cereggii_unused_in_release_build(ok);
}

int
AtomicDict_DelItem(AtomicDict *self, PyObject *key)
{
    assert(key != NULL);

    AtomicDictMeta *meta = NULL;
    AtomicDictAccessorStorage *storage = NULL;
    storage = get_or_create_accessor_storage(self);
    if (storage == NULL)
        goto fail;

    beginning:
    meta = get_meta(self, storage);
    if (meta == NULL)
        goto fail;

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    PyMutex_Lock(&storage->self_mutex);  // todo: maybe help migrate
    int migrated = maybe_help_migrate(meta, &storage->self_mutex);
    if (migrated) {
        // self_mutex was unlocked during the operation
        meta = NULL;
        goto beginning;
    }

    AtomicDictSearchResult result;
    delete_(meta, key, hash, &result);

    if (result.error) {
        PyMutex_Unlock(&storage->self_mutex);
        goto fail;
    }

    if (!result.found) {
        PyMutex_Unlock(&storage->self_mutex);
        PyErr_SetObject(PyExc_KeyError, key);
        goto fail;
    }

    accessor_len_inc(self, storage, -1);
    accessor_tombstones_inc(self, storage, 1);

    PyMutex_Unlock(&storage->self_mutex);

    Py_DECREF(result.entry.key);
    Py_DECREF(result.entry.value);

    return 0;

    fail:
    return -1;
}

/// insert

int
AtomicDict_ExpectedUpdateEntry(AtomicDictMeta *meta, uint64_t entry_ix,
                               PyObject *key, Py_hash_t hash,
                               PyObject *expected, PyObject *desired, PyObject **current,
                               int *done, int *expectation)
{
    AtomicDictEntry *entry_p, entry;
    entry_p = get_entry_at(entry_ix, meta);
    read_entry(entry_p, &entry);

    if (entry.value == NULL || hash != entry.hash)
        return 0;

    if (entry.key != key) {
        const int eq = PyObject_RichCompareBool(entry.key, key, Py_EQ);

        if (eq < 0)  // exception raised during compare
            goto fail;

        if (!eq)
            return 0;
    }

    // key found

    if (expected == NOT_FOUND) {
        assert(entry.value != NULL);
        *done = 1;
        *expectation = 0;
        return 1;
    }

    // expected != NOT_FOUND
    do {
        if (entry.value != expected && expected != ANY) {
            *done = 1;
            *expectation = 0;
            return 1;
        }

        if (entry.value == NULL) {
            *current = NULL;
            return 0;
        }

        *current = entry.value;
        PyObject *exp = *current;
        assert(exp != NULL);
        *done = atomic_compare_exchange_strong_explicit((_Atomic(PyObject *) *) &entry_p->value, &exp, desired,
                                                        memory_order_acq_rel, memory_order_acquire);

        if (!*done) {
            read_entry(entry_p, &entry);
        }
    } while (!*done);

    return 1;
    fail:
    return -1;
}


PyObject *
expected_insert_or_update(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash,
                                  PyObject *expected, PyObject *desired,
                                  AtomicDictEntryLoc *entry_loc, int *must_grow,
                                  int skip_entry_check)
{
    assert(meta != NULL);
    assert(key != NULL);
    assert(key != NOT_FOUND);
    assert(key != ANY);
    assert(key != EXPECTATION_FAILED);
    assert(hash != -1);
    assert(expected != NULL);
    assert(expected != EXPECTATION_FAILED);
    assert(desired != NULL);
    assert(desired != NOT_FOUND);
    assert(desired != ANY);
    assert(desired != EXPECTATION_FAILED);
    // assert(entry_loc == NULL => (expected != NOT_FOUND && expected != ANY));
    assert(entry_loc != NULL || (expected != NOT_FOUND && expected != ANY));

    int done, expectation;
    *must_grow = 0;

    uint64_t distance_0 = distance0_of(hash, meta);
    AtomicDictNode node;

    done = 0;
    expectation = 1;
    uint64_t distance = 0;
    PyObject *current = NULL;
    AtomicDictNode to_insert;

    while (!done) {
        uint64_t ix = (distance_0 + distance) & ((1 << meta->log_size) - 1);
        read_node_at(ix, &node, meta);

        if (is_empty(&node)) {
            if (expected != NOT_FOUND && expected != ANY) {
                expectation = 0;
                break;
            }
            assert(entry_loc != NULL);

            to_insert.index = entry_loc->location;
            to_insert.tag = hash;
            assert(atomic_dict_entry_ix_sanity_check(to_insert.index, meta));

            done = atomic_write_node_at(ix, &node, &to_insert, meta);

            if (!done)
                continue;  // don't increase distance
        } else if (is_tombstone(&node)) {
            // pass
        } else if (node.tag != (hash & TAG_MASK(meta))) {
            // pass
        } else if (!skip_entry_check) {
            int updated = AtomicDict_ExpectedUpdateEntry(meta, node.index, key, hash, expected, desired,
                                                         &current, &done, &expectation);
            if (updated < 0)
                goto fail;

            if (updated)
                break;
        }

        distance++;

        if (distance >= (1ull << meta->log_size)) {
            // traversed the entire dictionary without finding an empty slot
            *must_grow = 1;
            goto fail;
        }
    }

    // assert(expected == ANY => expectation == 1);
    assert(expected != ANY || expectation == 1);

    if (expectation && expected == NOT_FOUND) {
        Py_INCREF(NOT_FOUND);
        return NOT_FOUND;
    }
    if (expectation && expected == ANY) {
        if (current == NULL) {
            Py_INCREF(NOT_FOUND);
            return NOT_FOUND;
        }
        return current;
    }
    if (expectation) {
        assert(current != NULL);
        // no need to incref:
        //   - should incref because it's being returned
        //   - should decref because it has just been removed from the dict
        return current;
    }
    Py_INCREF(EXPECTATION_FAILED);
    return EXPECTATION_FAILED;

fail:
    return NULL;
}

PyObject *
AtomicDict_CompareAndSet(AtomicDict *self, PyObject *key, PyObject *expected, PyObject *desired)
{
    if (key == NULL) {
        PyErr_SetString(PyExc_ValueError, "key == NULL");
        return NULL;
    }

    if (expected == NULL) {
        PyErr_SetString(PyExc_ValueError, "expected == NULL");
        return NULL;
    }

    if (desired == NULL) {
        PyErr_SetString(PyExc_ValueError, "desired == NULL");
        return NULL;
    }

    if (key == NOT_FOUND || key == ANY || key == EXPECTATION_FAILED) {
        PyErr_SetString(PyExc_ValueError, "key in (NOT_FOUND, ANY, EXPECTATION_FAILED)");
        return NULL;
    }

    if (expected == EXPECTATION_FAILED) {
        PyErr_SetString(PyExc_ValueError, "expected == EXPECTATION_FAILED");
        return NULL;
    }

    if (desired == NOT_FOUND || desired == ANY || desired == EXPECTATION_FAILED) {
        PyErr_SetString(PyExc_ValueError, "desired in (NOT_FOUND, ANY, EXPECTATION_FAILED)");
        return NULL;
    }

    _Py_SetWeakrefAndIncref(key);
    _Py_SetWeakrefAndIncref(desired);

    AtomicDictMeta *meta = NULL;

    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    AtomicDictAccessorStorage *storage = NULL;
    storage = get_or_create_accessor_storage(self);
    if (storage == NULL)
        goto fail;

    beginning:
    meta = get_meta(self, storage);
    if (meta == NULL)
        goto fail;

    PyMutex_Lock(&storage->self_mutex);  // todo: maybe help migrate
    int migrated = maybe_help_migrate(meta, &storage->self_mutex);
    if (migrated) {
        // self_mutex was unlocked during the operation
        goto beginning;
    }

    AtomicDictEntryLoc entry_loc = {
        .entry = NULL,
        .location = 0,
    };
    if (expected == NOT_FOUND || expected == ANY) {
        int got_entry = get_empty_entry(self, meta, &storage->reservation_buffer, &entry_loc, hash);
        if (entry_loc.entry == NULL || got_entry == -1) {
            PyMutex_Unlock(&storage->self_mutex);
            goto fail;
        }

        if (got_entry == 0) {  // => must grow
            PyMutex_Unlock(&storage->self_mutex);
            migrated = grow(self);

            if (migrated < 0)
                goto fail;

            goto beginning;
        }

        assert(entry_loc.location > 0);
        assert(entry_loc.entry != NULL);
        assert(entry_loc.location < (uint64_t) SIZE_OF(meta));
        assert(atomic_dict_entry_ix_sanity_check(entry_loc.location, meta));
        assert(key != NULL);
        assert(hash != -1);
        assert(desired != NULL);
        atomic_store_explicit((_Atomic(PyObject *) *) &entry_loc.entry->key, key, memory_order_release);
        atomic_store_explicit((_Atomic(Py_hash_t) *) &entry_loc.entry->hash, hash, memory_order_release);
        atomic_store_explicit((_Atomic(PyObject *) *) &entry_loc.entry->value, desired, memory_order_release);
    }

    int must_grow;
    PyObject *result = expected_insert_or_update(meta, key, hash, expected, desired, &entry_loc, &must_grow, 0);

    if (result != NOT_FOUND && entry_loc.location != 0) {  // it was an update
        // keep entry_loc.entry->flags reserved, or set to 0
        uint8_t flags = atomic_load_explicit((_Atomic (uint8_t) *) &entry_loc.entry->flags, memory_order_acquire);
        atomic_store_explicit((_Atomic (uint8_t) *) &entry_loc.entry->flags, flags & ENTRY_FLAGS_RESERVED, memory_order_release);
        atomic_store_explicit((_Atomic (PyObject *) *) &entry_loc.entry->key, NULL, memory_order_release);
        atomic_store_explicit((_Atomic (PyObject *) *) &entry_loc.entry->value, NULL, memory_order_release);
        atomic_store_explicit((_Atomic (Py_hash_t) *) &entry_loc.entry->hash, 0, memory_order_release);
        reservation_buffer_put(&storage->reservation_buffer, &entry_loc, 1, meta);
        Py_DECREF(key);  // for the previous _Py_SetWeakrefAndIncref
    }

    if (result == NOT_FOUND && entry_loc.location != 0) {  // it was an insert
        accessor_len_inc(self, storage, 1);
        accessor_inserted_inc(self, storage, 1);
    }
    PyMutex_Unlock(&storage->self_mutex);

    if (result == NULL && !must_grow)
        goto fail;

    if (must_grow || approx_inserted(self) >= SIZE_OF(meta) * 2 / 3) {
        migrated = grow(self);

        if (migrated < 0)
            goto fail;

        if (must_grow) {  // insertion didn't happen
            goto beginning;
        }
    }

    return result;
    fail:
    Py_DECREF(key);
    Py_DECREF(desired);
    return NULL;
}

PyObject *
AtomicDict_CompareAndSet_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *key = NULL;
    PyObject *expected = NULL;
    PyObject *desired = NULL;

    char *kw_list[] = {"key", "expected", "desired", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOO", kw_list, &key, &expected, &desired))
        goto fail;

    PyObject *ret = AtomicDict_CompareAndSet(self, key, expected, desired);

    if (ret == NULL)
        goto fail;

    if (ret == EXPECTATION_FAILED) {
        PyObject *error = PyUnicode_FromFormat("self[%R] != %R", key, expected);
        PyErr_SetObject(Cereggii_ExpectationFailed, error);
        goto fail;
    }

    Py_RETURN_NONE;

    fail:
    return NULL;
}

int
AtomicDict_SetItem(AtomicDict *self, PyObject *key, PyObject *value)
{
    if (value == NULL) {
        return AtomicDict_DelItem(self, key);
    }

    if (key == NOT_FOUND || key == ANY || key == EXPECTATION_FAILED) {
        PyErr_SetString(PyExc_ValueError, "key in (NOT_FOUND, ANY, EXPECTATION_FAILED)");
        goto fail;
    }

    if (value == NOT_FOUND || value == ANY || value == EXPECTATION_FAILED) {
        PyErr_SetString(PyExc_ValueError, "value in (NOT_FOUND, ANY, EXPECTATION_FAILED)");
        goto fail;
    }

    PyObject *result = AtomicDict_CompareAndSet(self, key, ANY, value);

    if (result == NULL)
        goto fail;

    assert(result != EXPECTATION_FAILED);

    if (result != NOT_FOUND && result != ANY && result != EXPECTATION_FAILED) {
        Py_DECREF(result);
    }

    return 0;
    fail:
    return -1;
}


static inline int
flush_one(AtomicDict *self, PyObject *key, PyObject *expected, PyObject *new, PyObject *aggregate, PyObject *(*specialized)(PyObject *, PyObject *, PyObject *), int is_specialized)
{
    assert(key != NULL);
    assert(expected != NULL);
    assert(new != NULL);
    assert(expected == NOT_FOUND);

    Py_INCREF(key);
    Py_INCREF(expected);
    Py_INCREF(new);

    PyObject *previous = NULL;
    PyObject *current = NULL;
    PyObject *desired = new;
    PyObject *new_desired = NULL;

    while (1) {
        previous = AtomicDict_CompareAndSet(self, key, expected, desired);

        if (previous == NULL)
            goto fail;

        if (previous != EXPECTATION_FAILED) {
            break;
        }

        current = AtomicDict_GetItemOrDefault(self, key, NOT_FOUND);

        // the aggregate function must always be called with `new`, not `desired`
        new_desired = NULL;
        if (is_specialized) {
            new_desired = specialized(key, current, new);
        } else {
            new_desired = PyObject_CallFunctionObjArgs(aggregate, key, current, new, NULL);
        }
        if (new_desired == NULL)
            goto fail;

        Py_DECREF(expected);
        expected = current;
        current = NULL;
        Py_DECREF(desired);
        desired = new_desired;
        new_desired = NULL;
    }
    Py_XDECREF(previous);
    Py_XDECREF(key);
    if (current != expected) {
        Py_XDECREF(current);
    }
    Py_XDECREF(expected);
    if (desired != new) {
        Py_DECREF(desired);
    }
    Py_XDECREF(new);
    return 0;
    fail:
    return -1;
}


static inline int
reduce_flush(AtomicDict *self, PyObject *local_buffer, PyObject *aggregate, PyObject *(*specialized)(PyObject *, PyObject *, PyObject *), int is_specialized)
{
    Py_ssize_t pos = 0;
    PyObject *key = NULL;
    PyObject *tuple = NULL;
    PyObject *expected = NULL;
    PyObject *current = NULL;
    PyObject *desired = NULL;
    PyObject *new = NULL;
    PyObject *previous = NULL;
    // local_buffer is thread-local: it's created in the reduce function,
    // and its reference never leaves the scope of Reduce() or reduce_flush().
    // so there's no need for Py_BEGIN_CRITICAL_SECTION here.
    while (PyDict_Next(local_buffer, &pos, &key, &tuple)) {
        assert(key != NULL);
        assert(tuple != NULL);
        assert(PyTuple_Size(tuple) == 2);
        expected = PyTuple_GetItem(tuple, 0);
        new = PyTuple_GetItem(tuple, 1);
        // don't Py_DECREF(tuple) because PyDict_Next returns borrowed references

        int error = flush_one(self, key, expected, new, aggregate, specialized, is_specialized);
        if (error) {
            goto fail;
        }
    }
    return 0;
    fail:
    Py_XDECREF(key);
    Py_XDECREF(expected);
    Py_XDECREF(current);
    Py_XDECREF(desired);
    Py_XDECREF(new);
    Py_XDECREF(previous);
    return -1;
}

static int
AtomicDict_Reduce_impl(AtomicDict *self, PyObject *iterable, PyObject *aggregate,
    PyObject *(*specialized)(PyObject *, PyObject *, PyObject *), const int is_specialized)
{
    // is_specialized => specialized != NULL
    assert(!is_specialized || specialized != NULL);
    // !is_specialized => aggregate != NULL
    assert(is_specialized || aggregate != NULL);

    PyObject *item = NULL;
    PyObject *key = NULL;
    PyObject *value = NULL;
    PyObject *current = NULL;
    PyObject *expected = NULL;
    PyObject *desired = NULL;
    PyObject *tuple_get = NULL;
    PyObject *tuple_set = NULL;
    PyObject *local_buffer = NULL;
    PyObject *iterator = NULL;

    if (!is_specialized) {
        if (!PyCallable_Check(aggregate)) {
            PyErr_Format(PyExc_TypeError, "%R is not callable.", aggregate);
            goto fail;
        }
    }

    local_buffer = PyDict_New();
    if (local_buffer == NULL)
        goto fail;

    iterator = PyObject_GetIter(iterable);
    if (iterator == NULL) {
        PyErr_Format(PyExc_TypeError, "%R is not iterable.", iterable);
        goto fail;
    }
    if (iterable == iterator) {
        // if iterable is already an iterator, then PyObject_GetIter returns
        // its argument, without creating a new object.
        // since there are calls to Py_DECREF(iterator) next, we need to ensure
        // that we don't destroy the iterable, whose reference is borrowed.
        Py_INCREF(iterable);
    }

    while ((item = PyIter_Next(iterator))) {
        if (!PyTuple_CheckExact(item)) {
            PyErr_Format(PyExc_TypeError, "type(%R) != tuple", item);
            goto fail;
        }

        if (PyTuple_Size(item) != 2) {
            PyErr_Format(PyExc_TypeError, "len(%R) != 2", item);
            goto fail;
        }

        key = PyTuple_GetItem(item, 0);
        Py_INCREF(key);
        value = PyTuple_GetItem(item, 1);
        Py_INCREF(value);
        if (PyDict_Contains(local_buffer, key)) {
            if (PyDict_GetItemRef(local_buffer, key, &tuple_get) < 0)
                goto fail;

            expected = PyTuple_GetItem(tuple_get, 0);
            Py_INCREF(expected);
            current = PyTuple_GetItem(tuple_get, 1);
            Py_INCREF(current);
            Py_CLEAR(tuple_get);
        } else {
            expected = NOT_FOUND;
            Py_INCREF(expected);
            current = NOT_FOUND;
            Py_INCREF(current);
        }

        if (is_specialized) {
            desired = specialized(key, current, value);
        } else {
            desired = PyObject_CallFunctionObjArgs(aggregate, key, current, value, NULL);
        }
        if (desired == NULL)
            goto fail;
        Py_INCREF(desired);

        tuple_set = PyTuple_Pack(2, expected, desired);
        if (tuple_set == NULL)
            goto fail;

        if (PyDict_SetItem(local_buffer, key, tuple_set) < 0)
            goto fail;

        Py_CLEAR(item);
        Py_CLEAR(key);
        Py_CLEAR(value);
        Py_CLEAR(current);
        Py_CLEAR(expected);
        Py_CLEAR(desired);
        Py_CLEAR(tuple_set);
    }

    if (PyErr_Occurred())
        goto fail;

    int error = reduce_flush(self, local_buffer, aggregate, specialized, is_specialized);
    if (error) {
        goto fail;
    }
    Py_DECREF(iterator);
    Py_DECREF(local_buffer);
    return 0;

    fail:
    Py_XDECREF(local_buffer);
    Py_XDECREF(iterator);
    Py_XDECREF(item);
    Py_XDECREF(key);
    Py_XDECREF(value);
    Py_XDECREF(current);
    Py_XDECREF(expected);
    Py_XDECREF(desired);
    Py_XDECREF(tuple_get);
    Py_XDECREF(tuple_set);
    return -1;
}

int
AtomicDict_Reduce(AtomicDict *self, PyObject *iterable, PyObject *aggregate)
{
    return AtomicDict_Reduce_impl(self, iterable, aggregate, NULL, 0);
}

PyObject *
AtomicDict_Reduce_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;
    PyObject *aggregate = NULL;

    char *kw_list[] = {"iterable", "aggregate", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", kw_list, &iterable, &aggregate))
        goto fail;

    int res = AtomicDict_Reduce(self, iterable, aggregate);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_specialized_sum(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return new;
    }
    return PyNumber_Add(current, new);
}

int
AtomicDict_ReduceSum(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_sum, 1);
}


PyObject *
AtomicDict_ReduceSum_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceSum(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_specialized_and(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return new;
    }
    if (PyObject_IsTrue(current) && PyObject_IsTrue(new)) {
        return Py_True;  // Py_INCREF() called externally
    }
    return Py_False;  // Py_INCREF() called externally
}

int
AtomicDict_ReduceAnd(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_and, 1);
}

PyObject *
AtomicDict_ReduceAnd_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceAnd(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_specialized_or(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return new;
    }
    if (PyObject_IsTrue(current) || PyObject_IsTrue(new)) {
        return Py_True;  // Py_INCREF() called externally
    }
    return Py_False;  // Py_INCREF() called externally
}

int
AtomicDict_ReduceOr(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_or, 1);
}

PyObject *
AtomicDict_ReduceOr_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceOr(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_specialized_max(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return new;
    }
    Py_INCREF(current);
    Py_INCREF(new);
    const int cmp = PyObject_RichCompareBool(current, new, Py_GE);
    if (cmp < 0)
        return NULL;
    if (cmp) {
        return current;
    }
    return new;
}

int
AtomicDict_ReduceMax(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_max, 1);
}

PyObject *
AtomicDict_ReduceMax_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceMax(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_specialized_min(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return new;
    }
    Py_INCREF(current);
    Py_INCREF(new);
    const int cmp = PyObject_RichCompareBool(current, new, Py_LE);
    if (cmp < 0)
        return NULL;
    if (cmp) {
        return current;
    }
    return new;
}

int
AtomicDict_ReduceMin(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_min, 1);
}

PyObject *
AtomicDict_ReduceMin_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceMin(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
to_list(PyObject *maybe_list)
{
    assert(maybe_list != NULL);
    if (PyList_CheckExact(maybe_list))
        return maybe_list;

    PyObject *list = NULL;
    list = PyList_New(1);
    if (list == NULL)
        return NULL;
    Py_INCREF(maybe_list); // not a list
    if (PyList_SetItem(list, 0, maybe_list) < 0) {
        Py_DECREF(list);
        return NULL;
    }
    return list;
}

static inline PyObject *
reduce_specialized_list(PyObject *Py_UNUSED(key), PyObject *current, PyObject *new)
{
    assert(current != NULL);
    assert(new != NULL);
    if (current == NOT_FOUND) {
        return to_list(new);
    }
    PyObject *current_list = to_list(current);
    if (current_list == NULL)
        return NULL;
    PyObject *new_list = to_list(new);
    if (new_list == NULL) {
        if (current != current_list) {
            Py_DECREF(current_list);
        }
        return NULL;
    }
    PyObject *concat = PyNumber_Add(current_list, new_list);
    if (concat == NULL) {
        if (current_list != current) {
            Py_DECREF(current_list);
        }
        if (new_list != new) {
            Py_DECREF(new_list);
        }
        return NULL;
    }
    return concat;
}

int
AtomicDict_ReduceList(AtomicDict *self, PyObject *iterable)
{
    return AtomicDict_Reduce_impl(self, iterable, NULL, reduce_specialized_list, 1);
}

PyObject *
AtomicDict_ReduceList_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceList(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

static inline PyObject *
reduce_count_zip_iter_with_ones(AtomicDict *Py_UNUSED(self), PyObject *iterable)
{
    // we want to return `zip(iterable, itertools.repeat(1))`
    PyObject *iterator = NULL;
    PyObject *builtin_zip = NULL;
    PyObject *itertools = NULL;
    PyObject *itertools_namespace = NULL;
    PyObject *itertools_repeat = NULL;
    PyObject *one = NULL;
    PyObject *repeat_one = NULL;
    PyObject *zipped = NULL;

    iterator = PyObject_GetIter(iterable);
    if (iterator == NULL) {
        PyErr_Format(PyExc_TypeError, "%R is not iterable.", iterable);
        goto fail;
    }

    if (PyDict_GetItemStringRef(PyEval_GetFrameBuiltins(), "zip", &builtin_zip) < 0)
        goto fail;

    // the following lines implement this Python code:
    //     import itertools
    //     itertools.repeat(1)
    itertools = PyImport_ImportModule("itertools");
    if (itertools == NULL)
        goto fail;
    itertools_namespace = PyModule_GetDict(itertools);
    if (itertools_namespace == NULL)
        goto fail;
    Py_INCREF(itertools_namespace);
    if (PyDict_GetItemStringRef(itertools_namespace, "repeat", &itertools_repeat) < 0)
        goto fail;
    Py_CLEAR(itertools_namespace);
    one = PyLong_FromLong(1);
    if (one == NULL)
        goto fail;
    repeat_one = PyObject_CallFunctionObjArgs(itertools_repeat, one, NULL);
    if (repeat_one == NULL)
        goto fail;

    zipped = PyObject_CallFunctionObjArgs(builtin_zip, iterator, repeat_one, NULL);
    if (zipped == NULL)
        goto fail;

    Py_DECREF(iterator);
    Py_DECREF(builtin_zip);
    Py_DECREF(itertools);
    Py_DECREF(itertools_repeat);
    Py_DECREF(one);
    Py_DECREF(repeat_one);
    return zipped;
    fail:
    Py_XDECREF(iterator);
    Py_XDECREF(builtin_zip);
    Py_XDECREF(itertools);
    Py_XDECREF(itertools_namespace);
    Py_XDECREF(itertools_repeat);
    Py_XDECREF(one);
    Py_XDECREF(repeat_one);
    return NULL;
}

int
AtomicDict_ReduceCount(AtomicDict *self, PyObject *iterable)
{
    PyObject *it = NULL;

    // todo: extend to all mappings
    //   the problem is that PyMapping_Check(iterable) returns 1 also when iterable is a list or tuple
    if (PyDict_Check(iterable)) {  // cannot fail: no error check
        it = PyDict_Items(iterable);
    } else {
        it = reduce_count_zip_iter_with_ones(self, iterable);
    }
    if (it == NULL)
        goto fail;

    const int res = AtomicDict_Reduce_impl(self, it, NULL, reduce_specialized_sum, 1);
    if (res < 0)
        goto fail;

    Py_DECREF(it);
    return 0;
    fail:
    Py_XDECREF(it);
    return -1;
}

PyObject *
AtomicDict_ReduceCount_callable(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *iterable = NULL;

    char *kw_list[] = {"iterable", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", kw_list, &iterable))
        goto fail;

    int res = AtomicDict_ReduceCount(self, iterable);
    if (res < 0)
        goto fail;

    Py_RETURN_NONE;

    fail:
    return NULL;
}

/// iter

PyObject *
AtomicDict_FastIter(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    int partitions = 1, this_partition = 0;

    char *kw_list[] = {"partitions", "this_partition", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ii", kw_list, &partitions, &this_partition))
        goto fail_parse;

    if (partitions <= 0) {
        PyErr_SetString(PyExc_ValueError, "partitions <= 0");
        goto fail_parse;
    }

    if (this_partition > partitions) {
        PyErr_SetString(PyExc_ValueError, "this_partition > partitions");
        goto fail_parse;
    }

    AtomicDict_FastIterator *iter = NULL;
    Py_INCREF(self);

    iter = PyObject_New(AtomicDict_FastIterator, &AtomicDictFastIterator_Type);
    if (iter == NULL)
        goto fail;

    iter->meta = NULL;
    iter->meta = (AtomicDictMeta *) AtomicRef_Get(self->metadata);
    if (iter->meta == NULL)
        goto fail;

    iter->dict = self;
    iter->position = this_partition * ATOMIC_DICT_ENTRIES_IN_PAGE;
    iter->partitions = partitions;

    return (PyObject *) iter;

    fail:
    Py_XDECREF(iter);
    Py_DECREF(self);
    fail_parse:
    return NULL;
}

void
AtomicDictFastIterator_dealloc(AtomicDict_FastIterator *self)
{
    Py_CLEAR(self->dict);
    Py_CLEAR(self->meta);

    Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject *AtomicDictFastIterator_GetIter(AtomicDict_FastIterator *self)
{
    Py_INCREF(self);
    return (PyObject *) self;
}

PyObject *
AtomicDictFastIterator_Next(AtomicDict_FastIterator *self)
{
    AtomicDictEntry *entry_p, entry = {
        .value = NULL,
    };

    AtomicDictMeta *meta = self->meta;
    int64_t gap = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_page, memory_order_acquire);
    if (gap < 0) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    while (entry.value == NULL) {
        if (page_of(self->position) > (uint64_t) gap) {
            PyErr_SetNone(PyExc_StopIteration);
            return NULL;
        }

        entry_p = get_entry_at(self->position, self->meta);
        read_entry(entry_p, &entry);

        // it doesn't seem to be worth it
//        if ((self->position & (ATOMIC_DICT_ENTRIES_IN_PAGE - 1)) == 0
//            && AtomicDict_PageOf(self->position + ATOMIC_DICT_ENTRIES_IN_PAGE * 2) <= self->meta->greatest_allocated_block)
//        {
//            for (uint64_t i = self->position; i < self->position + ATOMIC_DICT_ENTRIES_IN_PAGE * 2; ++i) {
//                cereggii_prefetch(AtomicDict_GetEntryAt(i, self->meta), 0, 1);
//                // 0: the prefetch is for a read
//                // 1: the prefetched address is unlikely to be read again soon
//            }
//        }

        if (((self->position + 1) & (ATOMIC_DICT_ENTRIES_IN_PAGE - 1)) == 0) {
            self->position = (self->position & ~(ATOMIC_DICT_ENTRIES_IN_PAGE - 1))
                + self->partitions * ATOMIC_DICT_ENTRIES_IN_PAGE;
        }
        else {
            self->position++;
        }
    }
    if (entry.key == NULL || !_Py_TryIncref(entry.key)) {
        goto concurrent_usage_detected;
    }
    if (entry.value == NULL || !_Py_TryIncref(entry.value)) {
        Py_DECREF(entry.key);
        goto concurrent_usage_detected;
    }
    return Py_BuildValue("(NN)", entry.key, entry.value);
    concurrent_usage_detected:
    PyErr_SetString(Cereggii_ConcurrentUsageDetected, "please see https://dpdani.github.io/cereggii/api/AtomicDict/#cereggii._cereggii.AtomicDict.fast_iter");
    return NULL;
}

/// lookup

void
lookup(AtomicDictMeta *meta, PyObject *key, Py_hash_t hash,
                  AtomicDictSearchResult *result)
{
    // caller must ensure PyObject_Hash(.) didn't raise an error
    const uint64_t d0 = distance0_of(hash, meta);
    uint64_t distance = 0;

    for (; distance < (1ull << meta->log_size); distance++) {
        read_node_at(d0 + distance, &result->node, meta);

        if (is_empty(&result->node)) {
            goto not_found;
        }

        if (is_tombstone(&result->node))
            continue;

        if (result->node.tag == (hash & TAG_MASK(meta))) {
            result->entry_p = get_entry_at(result->node.index, meta);
            read_entry(result->entry_p, &result->entry);

            if (result->entry.value == NULL) {
                continue;
            }
            if (result->entry.key == key) {
                goto found;
            }
            if (result->entry.hash != hash) {
                continue;
            }

            int cmp = PyObject_RichCompareBool(result->entry.key, key, Py_EQ);
            if (cmp < 0) {
                // exception thrown during compare
                goto error;
            }
            if (cmp == 0) {
                continue;
            }
            goto found;
        }
    }

    // have looped over the entire index without finding the key => not found

    not_found:
    result->error = 0;
    result->found = 0;
    result->entry_p = NULL;
    return;
    error:
    result->error = 1;
    return;
    found:
    result->error = 0;
    result->found = 1;
    result->position = (d0 + distance) & ((1 << meta->log_size) - 1);
}

void
lookup_entry(AtomicDictMeta *meta, uint64_t entry_ix, Py_hash_t hash,
                       AtomicDictSearchResult *result)
{
    // index-only search

    const uint64_t d0 = distance0_of(hash, meta);
    uint64_t distance = 0;

    for (; distance < 1ull << meta->log_size; distance++) {
        read_node_at(d0 + distance, &result->node, meta);

        if (is_empty(&result->node)) {
            goto not_found;
        }
        if (result->node.index == entry_ix) {
            goto found;
        }
    }  // probes exhausted

    not_found:
    result->error = 0;
    result->found = 0;
    return;
    found:
    result->error = 0;
    result->found = 1;
    result->position = (d0 + distance) & ((1 << meta->log_size) - 1);
}


PyObject *
AtomicDict_GetItemOrDefault(AtomicDict *self, PyObject *key, PyObject *default_value)
{
    AtomicDictMeta *meta = NULL;
    Py_hash_t hash = PyObject_Hash(key);
    if (hash == -1)
        goto fail;

    AtomicDictSearchResult result;
    AtomicDictAccessorStorage *storage = NULL;
    storage = get_or_create_accessor_storage(self);
    if (storage == NULL)
        goto fail;

    retry:
    meta = get_meta(self, storage);

    result.entry.value = NULL;
    lookup(meta, key, hash, &result);
    if (result.error)
        goto fail;

    if (get_meta(self, storage) != meta)
        goto retry;

    if (result.entry_p == NULL) {
        result.entry.value = default_value;
    }

    if (result.entry.value == NULL) {
        return NULL;
    }

    if (!_Py_TryIncref(result.entry.value))
        goto retry;

    return result.entry.value;
    fail:
    return NULL;
}

PyObject *
AtomicDict_GetItem(AtomicDict *self, PyObject *key)
{
    PyObject *value = NULL;
    value = AtomicDict_GetItemOrDefault(self, key, NULL);

    if (value == NULL) {
        if (!PyErr_Occurred()) {
            PyErr_SetObject(PyExc_KeyError, key);
        }
    }

    return value;
}

PyObject *
AtomicDict_GetItemOrDefaultVarargs(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *key = NULL, *default_value = NULL;
    static char *keywords[] = {"key", "default", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", keywords, &key, &default_value))
        return NULL;

    if (default_value == NULL)
        default_value = Py_None;

    return AtomicDict_GetItemOrDefault(self, key, default_value);
}

PyObject *
AtomicDict_BatchGetItem(AtomicDict *self, PyObject *args, PyObject *kwargs)
{
    PyObject *batch = NULL;
    Py_ssize_t chunk_size = 128;

    char *kw_list[] = {"batch", "chunk_size", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|n", kw_list, &batch, &chunk_size))
        return NULL;

    if (!PyDict_CheckExact(batch)) {
        PyErr_SetString(PyExc_TypeError, "type(batch) != dict");
        return NULL;
    }

    if (chunk_size <= 0) {
        PyErr_SetString(PyExc_ValueError, "chunk_size <= 0");
        return NULL;
    }

    PyObject *key, *value;
    Py_hash_t hash;
    Py_hash_t *hashes = NULL;
    PyObject **keys = NULL;
    AtomicDictMeta *meta = NULL;

    hashes = PyMem_RawMalloc(chunk_size * sizeof(Py_hash_t));
    if (hashes == NULL)
        goto fail;

    keys = PyMem_RawMalloc(chunk_size * sizeof(PyObject *));
    if (keys == NULL)
        goto fail;

    AtomicDictSearchResult result;
    AtomicDictAccessorStorage *storage = NULL;
    storage = get_or_create_accessor_storage(self);
    if (storage == NULL)
        goto fail;

    retry:
    meta = get_meta(self, storage);

    if (meta == NULL)
        goto fail;

    Py_ssize_t chunk_start = 0, chunk_end = 0;
    Py_ssize_t pos = 0;

    Py_BEGIN_CRITICAL_SECTION(batch);

    next_chunk:
    while (PyDict_Next(batch, &pos, &key, &value)) {
        chunk_end++;
        hash = PyObject_Hash(key);
        if (hash == -1)
            goto fail;

        hashes[(chunk_end - 1) % chunk_size] = hash;
        keys[(chunk_end - 1) % chunk_size] = key;

        cereggii_prefetch(&meta->index[distance0_of(hash, meta)]);

        if (chunk_end % chunk_size == 0)
            break;
    }

    for (Py_ssize_t i = chunk_start; i < chunk_end; ++i) {
        hash = hashes[i % chunk_size];
        key = keys[i % chunk_size];

        uint64_t d0 = distance0_of(hash, meta);
        AtomicDictNode node;

        read_node_at(d0, &node, meta);

        if (is_empty(&node))
            continue;

        if (is_tombstone(&node))
            continue;

        if (node.tag == (hash & TAG_MASK(meta))) {
            cereggii_prefetch(get_entry_at(node.index, meta));
        }
    }

    for (Py_ssize_t i = chunk_start; i < chunk_end; ++i) {
        hash = hashes[i % chunk_size];
        key = keys[i % chunk_size];

        result.found = 0;
        lookup(meta, key, hash, &result);
        if (result.error)
            goto fail;

        assert(_PyDict_GetItem_KnownHash(batch, key, hash) != NULL); // returns a borrowed reference
        if (result.found) {
            if (PyDict_SetItem(batch, key, result.entry.value) < 0)
                goto fail;
        } else {
            if (PyDict_SetItem(batch, key, NOT_FOUND) < 0)
                goto fail;
        }
    }

    if (PyDict_Size(batch) != chunk_end) {
        chunk_start = chunk_end;
        goto next_chunk;
    }

    Py_END_CRITICAL_SECTION();

    if (get_meta(self, storage) != meta)
        goto retry;

    PyMem_RawFree(hashes);
    PyMem_RawFree(keys);
    Py_INCREF(batch);
    return batch;
    fail:
    if (hashes != NULL) {
        PyMem_RawFree(hashes);
    }
    if (keys != NULL) {
        PyMem_RawFree(keys);
    }
    return NULL;
}

/// meta

AtomicDictMeta *
AtomicDictMeta_New(uint8_t log_size)
{
    void *generation = NULL;
    uint64_t *index = NULL;
    AtomicDictMeta *meta = NULL;

    generation = PyMem_RawMalloc(1);
    if (generation == NULL)
        goto fail;

    index = PyMem_RawMalloc(sizeof(uint64_t) * (1ull << log_size));
    if (index == NULL)
        goto fail;

    meta = PyObject_GC_New(AtomicDictMeta, &AtomicDictMeta_Type);
    if (meta == NULL)
        goto fail;

    meta->pages = NULL;
    meta->greatest_allocated_page = -1;
    meta->inserting_page = -1;

    meta->log_size = log_size;
    meta->generation = generation;
    meta->index = index;

    meta->new_gen_metadata = NULL;
    meta->migration_leader = 0;
    meta->node_to_migrate = 0;
    meta->accessor_key = NULL;
    meta->participants = NULL;

    meta->new_metadata_ready = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->new_metadata_ready == NULL)
        goto fail;
    meta->node_migration_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->node_migration_done == NULL)
        goto fail;
    meta->migration_done = (AtomicEvent *) PyObject_CallObject((PyObject *) &AtomicEvent_Type, NULL);
    if (meta->migration_done == NULL)
        goto fail;

    PyObject_GC_Track(meta);
    return meta;
    fail:
    if (generation != NULL) {
        PyMem_RawFree(generation);
    }
    Py_XDECREF(meta);
    if (index != NULL) {
        PyMem_RawFree(index);
    }
    return NULL;
}

void
meta_clear_index(AtomicDictMeta *meta)
{
    memset(meta->index, 0, sizeof(uint64_t) * SIZE_OF(meta));
}

int
meta_init_pages(AtomicDictMeta *meta)
{
    AtomicDict_Page **pages = NULL;
    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    pages = PyMem_RawMalloc(sizeof(AtomicDict_Page *) * (SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE));
    if (pages == NULL)
        goto fail;

    pages[0] = NULL;
    meta->pages = pages;
    meta->inserting_page = -1;
    meta->greatest_allocated_page = -1;

    return 0;

    fail:
    return -1;
}

int
meta_copy_pages(AtomicDictMeta *from_meta, AtomicDictMeta *to_meta)
{
    assert(from_meta != NULL);
    assert(to_meta != NULL);
    assert(from_meta->log_size <= to_meta->log_size);

    AtomicDict_Page **previous_pages = from_meta->pages;
    int64_t inserting_page = from_meta->inserting_page;
    int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &from_meta->greatest_allocated_page, memory_order_acquire);


    // here we're abusing virtual memory:
    // the entire array will not necessarily be allocated to physical memory.
    AtomicDict_Page **pages = PyMem_RawMalloc(sizeof(AtomicDict_Page *) *
                                                (SIZE_OF(to_meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE));
    if (pages == NULL)
        goto fail;

    if (previous_pages != NULL) {
        for (int64_t page_i = 0; page_i <= greatest_allocated_page; ++page_i) {
            pages[page_i] = previous_pages[page_i];
        }
    }

    if (previous_pages == NULL) {
        pages[0] = NULL;
    } else {
        pages[greatest_allocated_page + 1] = NULL;
    }

    to_meta->pages = pages;

    to_meta->inserting_page = inserting_page;
    atomic_store_explicit((_Atomic (int64_t) *) &to_meta->greatest_allocated_page, greatest_allocated_page, memory_order_release);

    return 1;

    fail:
    return -1;
}

int
AtomicDictMeta_traverse(AtomicDictMeta *self, visitproc visit, void *arg)
{
    Py_VISIT(self->new_gen_metadata);
    Py_VISIT(self->new_metadata_ready);
    Py_VISIT(self->node_migration_done);
    Py_VISIT(self->migration_done);

    if (self->pages == NULL)
        return 0;

    int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &self->greatest_allocated_page, memory_order_acquire);
    for (int64_t page_i = 0; page_i <= greatest_allocated_page; ++page_i) {
        Py_VISIT(self->pages[page_i]);
    }
    return 0;
}

int
AtomicDictMeta_clear(AtomicDictMeta *self)
{
    int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &self->greatest_allocated_page, memory_order_acquire);
    for (int64_t page_i = 0; page_i <= greatest_allocated_page; ++page_i) {
        Py_CLEAR(self->pages[page_i]);
    }

    Py_CLEAR(self->new_gen_metadata);
    Py_CLEAR(self->new_metadata_ready);
    Py_CLEAR(self->node_migration_done);
    Py_CLEAR(self->migration_done);

    return 0;
}

void
AtomicDictMeta_dealloc(AtomicDictMeta *self)
{
    PyObject_GC_UnTrack(self);

    AtomicDictMeta_clear(self);

    uint64_t *index = self->index;
    if (index != NULL) {
        self->index = NULL;
        PyMem_RawFree(index);
    }

    if (self->pages != NULL) {
        PyMem_RawFree(self->pages);
    }

    if (self->participants != NULL) {
        PyMem_RawFree(self->participants);
    }

    PyMem_RawFree(self->generation);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

/// migrate

int
grow(AtomicDict *self)
{
    AtomicDictMeta *meta = NULL;
    AtomicDictAccessorStorage *storage = NULL;
    storage = get_or_create_accessor_storage(self);
    if (storage == NULL)
        goto fail;

    meta = get_meta(self, storage);

    int migrated = migrate(self, meta);
    if (migrated < 0)
        goto fail;

    return migrated;

    fail:
    return -1;
}

int
maybe_help_migrate(AtomicDictMeta *current_meta, PyMutex *self_mutex)
{
    if (atomic_load_explicit((_Atomic(uintptr_t) *) &current_meta->migration_leader, memory_order_acquire) == 0) {
        return 0;
    }

    PyMutex_Unlock(self_mutex);
    follower_migrate(current_meta);
    return 1;
}


int
migrate(AtomicDict *self, AtomicDictMeta *current_meta /* borrowed */)
{
    if (atomic_load_explicit((_Atomic(uintptr_t) *) &current_meta->migration_leader, memory_order_acquire) == 0) {
        uintptr_t expected = 0;
        int i_am_leader = atomic_compare_exchange_strong_explicit((_Atomic(uintptr_t) *)
            &current_meta->migration_leader,
            &expected, _Py_ThreadId(), memory_order_acq_rel, memory_order_acquire);
        if (i_am_leader) {
            return leader_migrate(self, current_meta);
        }
    }

    follower_migrate(current_meta);

    return 1;
}

int
leader_migrate(AtomicDict *self, AtomicDictMeta *current_meta /* borrowed */)
{
    int holding_sync_lock = 0;
    AtomicDictMeta *new_meta;
    uint8_t to_log_size = current_meta->log_size + 1;

    beginning:
    new_meta = NULL;
    new_meta = AtomicDictMeta_New(to_log_size);
    if (new_meta == NULL)
        goto fail;

    if (to_log_size > ATOMIC_DICT_MAX_LOG_SIZE) {
        PyErr_SetString(PyExc_ValueError, "can hold at most 2^56 items.");
        goto fail;
    }

    if (to_log_size < self->min_log_size) {
        to_log_size = self->min_log_size;
        Py_DECREF(new_meta);
        new_meta = NULL;
        goto beginning;
    }

    // pages
    begin_synchronous_operation(self);
    holding_sync_lock = 1;
    int ok = meta_copy_pages(current_meta, new_meta);

    if (ok < 0)
        goto fail;

    for (int64_t page_i = 0; page_i <= new_meta->greatest_allocated_page; ++page_i) {
        Py_INCREF(new_meta->pages[page_i]);
    }


    int32_t accessors_len = atomic_load_explicit((_Atomic (int32_t) *) &self->accessors_len, memory_order_acquire);
    assert(accessors_len > 0);
    int64_t *participants = PyMem_RawMalloc(sizeof(int64_t) * accessors_len);
    if (participants == NULL) {
        PyErr_NoMemory();
        goto fail;
    }
    for (int32_t i = 0; i < accessors_len; ++i) {
        atomic_store_explicit((_Atomic (int64_t) *) &participants[i], 0, memory_order_release);
    }
    atomic_store_explicit((_Atomic (int64_t *) *) &current_meta->participants, participants, memory_order_release);
    atomic_store_explicit((_Atomic (int32_t) *) &current_meta->participants_count, accessors_len, memory_order_release);

#ifdef CEREGGII_DEBUG
    int64_t inserted_before_migration = 0;
    int64_t tombstones_before_migration = 0;
    AtomicDictAccessorStorage *accessor;
    FOR_EACH_ACCESSOR(self, accessor) {
        int64_t local_inserted = atomic_load_explicit((_Atomic (int64_t) *) &accessor->local_inserted, memory_order_acquire);
        int64_t local_tombstones = atomic_load_explicit((_Atomic (int64_t) *) &accessor->local_tombstones, memory_order_acquire);
        inserted_before_migration += local_inserted;
        tombstones_before_migration += local_tombstones;
        atomic_store_explicit((_Atomic (int64_t) *) &accessor->local_inserted, 0, memory_order_release);
        atomic_store_explicit((_Atomic (int64_t) *) &accessor->local_tombstones, 0, memory_order_release);
    }
#endif

    atomic_store_explicit((_Atomic (Py_tss_t *) *) &current_meta->accessor_key, self->accessor_key, memory_order_release);

    for (int64_t page = 0; page <= new_meta->greatest_allocated_page; ++page) {
        new_meta->pages[page]->generation = new_meta->generation;
    }

    // 👀
    Py_INCREF(new_meta);
    atomic_store_explicit((_Atomic (AtomicDictMeta *) *) &current_meta->new_gen_metadata, new_meta, memory_order_release);
    AtomicEvent_Set(current_meta->new_metadata_ready);

    // birds flying
    common_migrate(current_meta, new_meta);

    // 🎉
    int set = AtomicRef_CompareAndSet(self->metadata, (PyObject *) current_meta, (PyObject *) new_meta);
    assert(set);
    cereggii_unused_in_release_build(set);

#ifdef CEREGGII_DEBUG
    assert(holding_sync_lock);
    int64_t inserted_after_migration = 0;
    FOR_EACH_ACCESSOR(self, accessor) {
        inserted_after_migration += atomic_load_explicit((_Atomic (int64_t) *) &accessor->local_inserted, memory_order_acquire);
    }
    assert(inserted_after_migration == inserted_before_migration - tombstones_before_migration);
#endif

    AtomicEvent_Set(current_meta->migration_done);
    Py_DECREF(new_meta);  // this may seem strange: why decref the new meta?
    // the reason is that AtomicRef_CompareAndSet also increases new_meta's refcount,
    // which is exactly what we want. but the reference count was already 1, as it
    // was set during the initialization of new_meta. that's what we're decref'ing
    // for in here.

    if (holding_sync_lock) {
        end_synchronous_operation(self);
    }
    return 1;

    fail:
    if (holding_sync_lock) {
        end_synchronous_operation(self);
    }
    // don't block other threads indefinitely
    AtomicEvent_Set(current_meta->migration_done);
    AtomicEvent_Set(current_meta->node_migration_done);
    AtomicEvent_Set(current_meta->new_metadata_ready);
    return -1;
}

void
follower_migrate(AtomicDictMeta *current_meta)
{
    AtomicEvent_Wait(current_meta->new_metadata_ready);
    AtomicDictMeta *new_meta = atomic_load_explicit((_Atomic (AtomicDictMeta *) *) &current_meta->new_gen_metadata, memory_order_acquire);

    common_migrate(current_meta, new_meta);

    AtomicEvent_Wait(current_meta->migration_done);
}

void
common_migrate(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta)
{
    if (AtomicEvent_IsSet(current_meta->node_migration_done))
        return;

    Py_tss_t *ak = atomic_load_explicit((_Atomic(Py_tss_t *) *) &current_meta->accessor_key, memory_order_acquire);
    AtomicDictAccessorStorage *storage = get_accessor_storage(ak);
    assert(storage != NULL);

    int64_t *participants = atomic_load_explicit((_Atomic(int64_t *) *) &current_meta->participants, memory_order_acquire);
    int64_t *participant = &participants[storage->accessor_ix];
    assert(participant != NULL);

    int64_t expected, ok;

    expected = 0ll;
    ok = atomic_compare_exchange_strong_explicit((_Atomic (int64_t) *) participant, &expected, 1ll, memory_order_acq_rel, memory_order_acquire);
    assert(ok);
    cereggii_unused_in_release_build(ok);

    int64_t migrated_count = migrate_nodes(current_meta, new_meta);

    expected = 1ll;
    ok = atomic_compare_exchange_strong_explicit((_Atomic (int64_t) *) participant, &expected, 2ll, memory_order_acq_rel, memory_order_acquire);
    assert(ok);
    cereggii_unused_in_release_build(ok);

    atomic_store_explicit((_Atomic(int64_t) *) &storage->local_inserted, migrated_count, memory_order_release);

    if (nodes_migration_done(current_meta)) {
        AtomicEvent_Set(current_meta->node_migration_done);
    }
    AtomicEvent_Wait(current_meta->node_migration_done);
}

inline void
migrate_node(AtomicDictNode *node, AtomicDictMeta *new_meta, const uint64_t trailing_cluster_start, const uint64_t trailing_cluster_size)
{
    assert(node->index != 0);
    Py_hash_t hash = get_entry_at(node->index, new_meta)->hash;
    uint64_t d0 = distance0_of(hash, new_meta);
    node->tag = hash;
    uint64_t position;

    for (int64_t distance = 0; distance < SIZE_OF(new_meta); distance++) {
        position = (d0 + distance) & (SIZE_OF(new_meta) - 1);

        if (read_raw_node_at(position, new_meta) == 0) {
            uint64_t range_start = (trailing_cluster_start * 2) & (SIZE_OF(new_meta) - 1);
            uint64_t range_end = (2 * (trailing_cluster_start + trailing_cluster_size + 1)) & (SIZE_OF(new_meta) - 1);
            if (range_start < range_end) {
                assert(position >= range_start && position < range_end);
            } else {
                assert(position >= range_start || position < range_end);
            }
            write_node_at(position, node, new_meta);
            break;
        }
    }
}

void
initialize_in_new_meta(AtomicDictMeta *new_meta, const uint64_t start, const uint64_t end)
{
    // initialize slots in range [start, end)
    for (uint64_t j = 2 * start; j < 2 * (end + 1); ++j) {
        write_raw_node_at(j & (SIZE_OF(new_meta) - 1), 0, new_meta);
    }
}

#define ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE 4096


int64_t
to_migrate(AtomicDictMeta *current_meta, int64_t start_of_block, uint64_t end_of_block)
{
    uint64_t current_size = SIZE_OF(current_meta);
    uint64_t current_size_mask = current_size - 1;
    uint64_t i = start_of_block;
    int64_t to_migrate = 0;
    AtomicDictNode node = {0};

    while (i < end_of_block) {
        if (read_raw_node_at(i, current_meta) == 0)
            break;

        i++;
    }

    if (i >= end_of_block)
        return 0;

    while (i < end_of_block) {
        read_node_at(i, &node, current_meta);
        if (node.node != 0 && node.index != 0) {
            to_migrate++;
        }
        i++;
    }

    assert(i == end_of_block);

    do {
        read_node_at(i & current_size_mask, &node, current_meta);
        if (node.node != 0 && node.index != 0) {
            to_migrate++;
        }
        i++;
    } while (node.node != 0);

    return to_migrate;
}

int64_t
AtomicDict_BlockWiseMigrate(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta, int64_t start_of_block)
{
    int64_t migrated_count = 0;
    uint64_t current_size = SIZE_OF(current_meta);
    uint64_t current_size_mask = current_size - 1;
    uint64_t i = start_of_block;

    uint64_t end_of_block = start_of_block + ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE;
    if (end_of_block > current_size) {
        end_of_block = current_size;
    }
    assert(end_of_block > i);

    AtomicDictNode node = {0};

    // find first empty slot
    while (i < end_of_block) {
        if (read_raw_node_at(i, current_meta) == 0)
            break;

        i++;
    }

    if (i >= end_of_block)
        return 0;

    uint64_t start_of_cluster = i;
    uint64_t cluster_size = 0;

    initialize_in_new_meta(new_meta, i, end_of_block);

    for (; i < end_of_block; i++) {
        read_node_at(i, &node, current_meta);

        if (is_empty(&node)) {
            start_of_cluster = i + 1;
            cluster_size = 0;
            continue;
        }
        cluster_size++;
        if (is_tombstone(&node))
            continue;

        migrate_node(&node, new_meta, start_of_cluster, cluster_size);
        migrated_count++;
    }

    assert(i == end_of_block); // that is, start of next block

    if (cluster_size == 0) {
        start_of_cluster = end_of_block & current_size_mask;
    }

    uint64_t j = end_of_block;
    while (1) {
        read_node_at(j & current_size_mask, &node, current_meta);
        if (is_empty(&node)) {
            break;
        }
        j++;
    }
    if (j > end_of_block) {
        initialize_in_new_meta(new_meta, end_of_block, j - 1);
        while (1) {
            read_node_at(i & current_size_mask, &node, current_meta);
            if (is_empty(&node)) {
                break;
            }
            cluster_size++;
            if (!is_tombstone(&node)) {
                migrate_node(&node, new_meta, start_of_cluster, cluster_size);
                migrated_count++;
            }
            i++;
        }
    }

    assert(to_migrate(current_meta, start_of_block, end_of_block) == migrated_count);
    return migrated_count;
}

int64_t
migrate_nodes(AtomicDictMeta *current_meta, AtomicDictMeta *new_meta)
{
    uint64_t current_size = SIZE_OF(current_meta);
    int64_t node_to_migrate = atomic_fetch_add_explicit((_Atomic(int64_t) *) &current_meta->node_to_migrate,
                                                      ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE, memory_order_acq_rel);
    int64_t migrated_count = 0;

    while ((uint64_t) node_to_migrate < current_size) {
        migrated_count += AtomicDict_BlockWiseMigrate(current_meta, new_meta, node_to_migrate);
        node_to_migrate = atomic_fetch_add_explicit((_Atomic(int64_t) *) &current_meta->node_to_migrate, ATOMIC_DICT_BLOCKWISE_MIGRATE_SIZE, memory_order_acq_rel);
    }

    return migrated_count;
}

int
nodes_migration_done(AtomicDictMeta *current_meta)
{
    int done = 1;

    int64_t *participants = atomic_load_explicit((_Atomic(int64_t *) *) &current_meta->participants, memory_order_acquire);

    for (int32_t accessor = 0; accessor < current_meta->participants_count; accessor++) {
        if (atomic_load_explicit((_Atomic(int64_t) *) &participants[accessor], memory_order_acquire) == 1ll) {
            done = 0;
            break;
        }
    }

    return done;
}

/// node ops

// these functions take a pointer to meta, but to avoid multiple reads
// you should dereference dk->meta (i.e. make a thread-local copy) and
// then pass a reference to the copy to these functions instead.

void
compute_raw_node(AtomicDictNode *node, AtomicDictMeta *meta)
{
#ifdef CEREGGII_DEBUG
    assert(node->index < (1ull << meta->log_size));
    uint64_t index = node->index;
    int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_page, memory_order_acquire);
    if (greatest_allocated_page >= 0) {
        assert(atomic_dict_entry_ix_sanity_check(index, meta));
    }
#endif

    node->node =
        (node->index << (NODE_SIZE - meta->log_size))
        | (node->tag & TAG_MASK(meta));

#ifdef CEREGGII_DEBUG
    AtomicDictNode check_node;
    parse_node_from_raw(node->node, &check_node, meta);
    assert(index == check_node.index);
#endif
}

#define UPPER_SEED 12923598712359872066ull
#define LOWER_SEED 7467732452331123588ull
#define REHASH(x) (uint64_t) ( \
    (uint64_t) cereggii_crc32_u64((uint64_t)(x), LOWER_SEED) \
    | (((uint64_t) cereggii_crc32_u64((uint64_t)(x), UPPER_SEED)) << 32ull))

PyObject *
AtomicDict_ReHash(AtomicDict *Py_UNUSED(self), PyObject *ob)
{
    Py_hash_t hash = PyObject_Hash(ob);
    if (hash == -1) {
        return NULL;
    }
    return PyLong_FromUInt64(REHASH(hash));
}

uint64_t
distance0_of(Py_hash_t hash, AtomicDictMeta *meta)
{
    return REHASH(hash) >> (SIZEOF_PY_HASH_T * CHAR_BIT - meta->log_size);
}

void
parse_node_from_raw(uint64_t node_raw, AtomicDictNode *node,
                            AtomicDictMeta *meta)
{
    node->node = node_raw;
    node->index = node_raw >> (NODE_SIZE - meta->log_size);
    node->tag = node_raw & TAG_MASK(meta);
}

uint64_t
read_raw_node_at(uint64_t ix, AtomicDictMeta *meta)
{
    return atomic_load_explicit((_Atomic (uint64_t) *) &meta->index[ix & ((1 << meta->log_size) - 1)], memory_order_acquire);
}

int
is_empty(AtomicDictNode *node)
{
    return node->node == 0;
}

int
is_tombstone(AtomicDictNode *node)
{
    return node->node != 0 && node->index == 0;
}

void
read_node_at(uint64_t ix, AtomicDictNode *node, AtomicDictMeta *meta)
{
    const uint64_t raw = read_raw_node_at(ix, meta);
    parse_node_from_raw(raw, node, meta);
}

void
write_node_at(uint64_t ix, AtomicDictNode *node, AtomicDictMeta *meta)
{
    assert(ix < (1ull << meta->log_size));
    compute_raw_node(node, meta);
    assert(atomic_dict_entry_ix_sanity_check(node->index, meta));
    write_raw_node_at(ix, node->node, meta);
}

void
write_raw_node_at(uint64_t ix, uint64_t raw_node, AtomicDictMeta *meta)
{
    assert(ix < (1ull << meta->log_size));
#ifdef CEREGGII_DEBUG
    AtomicDictNode node;
    parse_node_from_raw(raw_node, &node, meta);
    assert(atomic_dict_entry_ix_sanity_check(node.index, meta));
#endif

    atomic_store_explicit((_Atomic (uint64_t) *) &meta->index[ix], raw_node, memory_order_release);
}

int
atomic_write_node_at(uint64_t ix, AtomicDictNode *expected, AtomicDictNode *desired, AtomicDictMeta *meta)
{
    compute_raw_node(expected, meta);
    compute_raw_node(desired, meta);
    assert(atomic_dict_entry_ix_sanity_check(expected->index, meta));
    assert(atomic_dict_entry_ix_sanity_check(desired->index, meta));

    uint64_t _expected = expected->node;
    return atomic_compare_exchange_strong_explicit((_Atomic (uint64_t) *) &meta->index[ix], &_expected, desired->node, memory_order_acq_rel, memory_order_acquire);
}

void
print_node_at(const uint64_t ix, AtomicDictMeta *meta)
{
    AtomicDictNode node;
    read_node_at(ix, &node, meta);
    if (is_tombstone(&node)) {
        printf("<node at %" PRIu64 ": %" PRIu64 " (tombstone) seen by thread=%" PRIuPTR ">\n", ix, node.node, _Py_ThreadId());
        return;
    }
    printf("<node at %" PRIu64 ": %" PRIu64 " (index=%" PRIu64 ", tag=%" PRIu64 ") seen by thread=%" PRIuPTR ">\n", ix, node.node, node.index, node.tag, _Py_ThreadId());
}

/// pages

AtomicDict_Page *
AtomicDictPage_New(AtomicDictMeta *meta)
{
    AtomicDict_Page *new = NULL;
    new = PyObject_GC_New(AtomicDict_Page, &AtomicDictPage_Type);

    if (new == NULL)
        return NULL;

    new->generation = meta->generation;
    cereggii_tsan_ignore_writes_begin();
    memset(new->entries, 0, sizeof(AtomicDict_PaddedEntry) * ATOMIC_DICT_ENTRIES_IN_PAGE);
    cereggii_tsan_ignore_writes_end();

    PyObject_GC_Track(new);

    return new;
}

int
AtomicDictPage_traverse(AtomicDict_Page *self, visitproc visit, void *arg)
{
    AtomicDictEntry entry;
    for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_PAGE; ++i) {
        entry = self->entries[i].entry;

        if (entry.value == NULL)
            continue;

        Py_VISIT(entry.key);
        Py_VISIT(entry.value);
    }

    return 0;
}

int
AtomicDictPage_clear(AtomicDict_Page *self)
{
    AtomicDictEntry *entry;
    for (int i = 0; i < ATOMIC_DICT_ENTRIES_IN_PAGE; ++i) {
        entry = &self->entries[i].entry;

        if (entry->value == NULL)
            continue;

        assert(entry->key != NULL);
        Py_CLEAR(entry->key);
        Py_CLEAR(entry->value);
    }

    return 0;
}

void
AtomicDictPage_dealloc(AtomicDict_Page *self)
{
    PyObject_GC_UnTrack(self);
    AtomicDictPage_clear(self);
    Py_TYPE(self)->tp_free((PyObject *) self);
}


int
atomic_dict_entry_ix_sanity_check(uint64_t entry_ix, AtomicDictMeta *meta)
{
    int64_t gap = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_page, memory_order_acquire);
    assert(gap >= 0);
    assert(page_of(entry_ix) <= (uint64_t) gap);
    cereggii_unused_in_release_build(entry_ix);
    cereggii_unused_in_release_build(gap);
    return 1;
}


int
get_empty_entry(AtomicDict *self, AtomicDictMeta *meta, AtomicDictReservationBuffer *rb,
                         AtomicDictEntryLoc *entry_loc, Py_hash_t hash)
{
    reservation_buffer_pop(rb, entry_loc);

    if (entry_loc->entry == NULL) {
        Py_ssize_t insert_position = hash & (ATOMIC_DICT_ENTRIES_IN_PAGE - 1) & ~(self->reservation_buffer_size - 1);
        int64_t inserting_page;

        reserve_in_inserting_page:
        inserting_page = atomic_load_explicit((_Atomic (int64_t) *) &meta->inserting_page, memory_order_acquire);
        for (int offset = 0; offset < ATOMIC_DICT_ENTRIES_IN_PAGE; offset += self->reservation_buffer_size) {
            AtomicDict_Page *page = atomic_load_explicit((_Atomic (AtomicDict_Page *) *) &meta->pages[inserting_page], memory_order_acquire);
            entry_loc->entry = &(page
                ->entries[(insert_position + offset) % ATOMIC_DICT_ENTRIES_IN_PAGE]
                .entry);
            if (atomic_load_explicit((_Atomic (uint8_t) *) &entry_loc->entry->flags, memory_order_acquire) == 0) {
                uint8_t expected = 0;
                if (atomic_compare_exchange_strong_explicit((_Atomic(uint8_t) *) &entry_loc->entry->flags, &expected, ENTRY_FLAGS_RESERVED, memory_order_acq_rel, memory_order_acquire)) {
                    entry_loc->location =
                        (inserting_page << ATOMIC_DICT_LOG_ENTRIES_IN_PAGE) +
                        ((insert_position + offset) % ATOMIC_DICT_ENTRIES_IN_PAGE);
                    assert(atomic_dict_entry_ix_sanity_check(entry_loc->location, meta));
                    reservation_buffer_put(rb, entry_loc, self->reservation_buffer_size, meta);
                    reservation_buffer_pop(rb, entry_loc);
                    goto done;
                }
            }
        }

        if (atomic_load_explicit((_Atomic (int64_t) *) &meta->inserting_page, memory_order_acquire) != inserting_page)
            goto reserve_in_inserting_page;

        int64_t greatest_allocated_page = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_page, memory_order_acquire);
        if (greatest_allocated_page > inserting_page) {
            int64_t expected = inserting_page;
            atomic_compare_exchange_strong_explicit((_Atomic(int64_t) *) &meta->inserting_page, &expected, inserting_page + 1, memory_order_acq_rel, memory_order_acquire);
            goto reserve_in_inserting_page; // even if the above CAS fails
        }
        assert(greatest_allocated_page >= 0);
        if ((uint64_t) greatest_allocated_page + 1u >= (uint64_t) SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE) {
            return 0; // must grow
        }
        assert((uint64_t) greatest_allocated_page + 1u <= (uint64_t) SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE);

        AtomicDict_Page *page = NULL;
        page = AtomicDictPage_New(meta);
        if (page == NULL)
            goto fail;

        page->entries[0].entry.flags = ENTRY_FLAGS_RESERVED;

        AtomicDict_Page *expected = NULL;
        int64_t new_page = greatest_allocated_page + 1;
        if (atomic_compare_exchange_strong_explicit((_Atomic(AtomicDict_Page *) *) &meta->pages[new_page], &expected, page, memory_order_acq_rel, memory_order_acquire)) {
            if ((uint64_t) greatest_allocated_page + 2u < (uint64_t) SIZE_OF(meta) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE) {
                atomic_store_explicit((_Atomic(AtomicDict_Page *) *) &meta->pages[new_page + 1], NULL, memory_order_release);
            }
            int64_t expected2 = greatest_allocated_page;
            int ok = atomic_compare_exchange_strong_explicit((_Atomic(int64_t) *) &meta->greatest_allocated_page,
                                                    &expected2, new_page, memory_order_acq_rel, memory_order_acquire);
            assert(ok);
            cereggii_unused_in_release_build(ok);
            expected2 = greatest_allocated_page;
            atomic_compare_exchange_strong_explicit((_Atomic(int64_t) *) &meta->inserting_page,
                                                    &expected2, new_page, memory_order_acq_rel, memory_order_acquire);
            // this cas may fail because another thread helped increasing this counter
            entry_loc->entry = &(page->entries[0].entry);
            entry_loc->location = new_page << ATOMIC_DICT_LOG_ENTRIES_IN_PAGE;
            assert(atomic_dict_entry_ix_sanity_check(entry_loc->location, meta));
            reservation_buffer_put(rb, entry_loc, self->reservation_buffer_size, meta);
            reservation_buffer_pop(rb, entry_loc);
        } else {
            Py_DECREF(page);
            goto reserve_in_inserting_page;
        }
    }

    done:
    assert(entry_loc->entry != NULL);
    assert(entry_loc->entry->key == NULL);
    assert(entry_loc->entry->value == NULL);
    assert(entry_loc->entry->hash == 0);
    assert(entry_loc->location > 0);
    assert(entry_loc->location < (uint64_t) SIZE_OF(meta));
    assert(atomic_dict_entry_ix_sanity_check(entry_loc->location, meta));
    return 1;
    fail:
    entry_loc->entry = NULL;
    return -1;
}

uint64_t
page_of(uint64_t entry_ix)
{
    return entry_ix >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE;
}

uint64_t
position_in_page_of(uint64_t entry_ix)
{
    return entry_ix & (ATOMIC_DICT_ENTRIES_IN_PAGE - 1);
}

AtomicDictEntry *
get_entry_at(uint64_t ix, AtomicDictMeta *meta)
{
    assert(atomic_dict_entry_ix_sanity_check(ix, meta));
    AtomicDict_Page *page = atomic_load_explicit((_Atomic (AtomicDict_Page *) *) &meta->pages[page_of(ix)], memory_order_acquire);
    assert(page != NULL);
    return &(
        page
            ->entries[position_in_page_of(ix)]
            .entry
    );
}

void
read_entry(AtomicDictEntry *entry_p, AtomicDictEntry *entry)
{
    entry->flags = atomic_load_explicit((_Atomic(uint8_t) *) &entry_p->flags, memory_order_acquire);
    entry->value = atomic_load_explicit((_Atomic(PyObject *) *) &entry_p->value, memory_order_acquire);
    if (entry->value == NULL) {
        entry->key = NULL;
        entry->value = NULL;
        entry->hash = -1;
        return;
    }
    entry->key = atomic_load_explicit((_Atomic(PyObject *) *) &entry_p->key, memory_order_acquire);
    entry->hash = atomic_load_explicit((_Atomic(Py_hash_t) *) &entry_p->hash, memory_order_acquire);
}
