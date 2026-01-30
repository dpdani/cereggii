// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#define PY_SSIZE_T_CLEAN

#include "atomic_dict.h"

#include <stdatomic.h>

#include "atomic_dict_internal.h"
#include "atomic_ref.h"
#include "pythread.h"
#include "thread_handle.h"
#include "_internal_py_core.h"
#include <vendor/pythoncapi_compat/pythoncapi_compat.h>


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
    int64_t buffer_size = 16;
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

    AtomicDictPage *page;
    int64_t i;
    for (i = 0; i < init_dict_size / ATOMIC_DICT_ENTRIES_IN_PAGE; i++) {
        // allocate pages
        page = AtomicDictPage_New();
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
        page = AtomicDictPage_New();
        if (page == NULL)
            goto fail;
        meta->pages[i] = page;
        if (i + 1 < (1 << log_size) >> ATOMIC_DICT_LOG_ENTRIES_IN_PAGE) {
            meta->pages[i + 1] = NULL;
        }
        meta->greatest_allocated_page++;
    }
    if (meta->greatest_allocated_page == -1) {
        page = AtomicDictPage_New();
        if (page == NULL)
            goto fail;
        meta->pages[0] = page;
        meta->greatest_allocated_page = 0;
    }

    uint64_t location;
    self->sync_op = (PyMutex) {0};
    self->accessors_lock = (PyMutex) {0};
    self->accessors_len = 0;
    self->len = 0;
    self->len_dirty = 0;
    AtomicDictAccessorStorage *storage = get_or_create_accessor_storage(self);
    if (storage == NULL)
        goto fail;

    if (initial != NULL && PyDict_Size(initial) > 0) {
        get_entry_at(0, meta)->flags |= ENTRY_FLAGS_RESERVED;

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

        if (self->len > 0) {
            // handle possibly misaligned reservations on last page
            // => put them into this thread's reservation buffer
            assert(meta->greatest_allocated_page >= 0);
            if (page_of(self->len + 1) <= (uint64_t) meta->greatest_allocated_page) {
                location = self->len + 1;

                uint8_t n = self->reservation_buffer_size - (uint8_t) (location % self->reservation_buffer_size);
                assert(n <= ATOMIC_DICT_ENTRIES_IN_PAGE);
                while (page_of(self->len + n) > (uint64_t) meta->greatest_allocated_page) {
                    n--;
                    if (n == 0)
                        break;
                }

                if (n > 0) {
                    reservation_buffer_put(&storage->reservation_buffer, location, n, meta);
                }
            }
        }
    }

    if (self->len == 0) {
        get_entry_at(0, meta)->flags |= ENTRY_FLAGS_RESERVED;
        reservation_buffer_put(&storage->reservation_buffer, 1, self->reservation_buffer_size - 1, meta);
    }
    assert(get_entry_at(0, meta)->flags & ENTRY_FLAGS_RESERVED);
    // entry 0 must always be reserved: tombstones are pointers
    // to entry 0, so it must always be empty.

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
 * Doesn't allocate pages, nor check for resizes, nor update dk->metadata refcount.
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
        .tag = REHASH(hash),
    };
    const uint64_t d0 = distance0_of(hash, meta);

    for (uint64_t distance = 0; distance < (uint64_t) SIZE_OF(meta); distance++) {
        read_node_at(d0 + distance, &temp, meta);

        if (temp.node == 0) {
            node.distance = distance;
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
    metadata = Py_BuildValue("{sOsO}",
                             "log_size\0", Py_BuildValue("B", meta->log_size),
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

    AtomicDictPage *page;
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

        page_info = Py_BuildValue("{sO}", "entries\0", entries);
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
