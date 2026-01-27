// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include "atomic_dict_internal.h"
#include <thread_id.h>
#include <stdatomic.h>
#include <vendor/pythoncapi_compat/pythoncapi_compat.h>

// these functions take a pointer to meta, but to avoid multiple reads
// you should dereference dk->meta (i.e. make a thread-local copy) and
// then pass a reference to the copy to these functions instead.


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

int64_t
distance0_of(Py_hash_t hash, AtomicDictMeta *meta)
{
    uint64_t ix = REHASH(hash) >> (SIZEOF_PY_HASH_T * CHAR_BIT - meta->log_size);
    assert(ix < (1ull << meta->log_size));
    assert(ix <= (1ull << ATOMIC_DICT_MAX_LOG_SIZE));
    assert(ix <= INT64_MAX);
    return (int64_t) ix;
}

AtomicDictNode
read_node_at(int64_t ix, AtomicDictMeta* meta)
{
    assert(ix >= 0);
    return (AtomicDictNode) {
        .index = atomic_load_explicit((_Atomic (int64_t) *) &meta->index[ix & ((1 << meta->log_size) - 1)], memory_order_acquire),
    };
}

int
is_empty(AtomicDictNode node)
{
    return node.index == NODE_EMPTY.index;
}

int
is_tombstone(AtomicDictNode node)
{
    return node.index == NODE_TOMBSTONE.index;
}

void
write_node_at(int64_t ix, AtomicDictNode node, AtomicDictMeta *meta)
{
    assert(ix < (1ll << meta->log_size));
    assert(atomic_dict_entry_ix_sanity_check(node.index, meta));
    atomic_store_explicit((_Atomic (AtomicDictNode) *) &meta->index[ix], node, memory_order_release);
}

int
atomic_write_node_at(int64_t ix, AtomicDictNode expected, AtomicDictNode desired, AtomicDictMeta *meta)
{
    assert(atomic_dict_entry_ix_sanity_check(expected.index, meta));
    assert(atomic_dict_entry_ix_sanity_check(desired.index, meta));

    AtomicDictNode _expected = expected;
    return atomic_compare_exchange_strong_explicit((_Atomic (AtomicDictNode) *) &meta->index[ix], &_expected, desired, memory_order_acq_rel, memory_order_acquire);
}

void
print_node_at(const int64_t ix, AtomicDictMeta *meta)
{
    AtomicDictNode node = read_node_at(ix, meta);
    if (is_tombstone(node)) {
        printf("<node at %" PRIu64 ": %" PRIu64 " (tombstone) seen by thread=%" PRIuPTR ">\n", ix, node.index, _Py_ThreadId());
        return;
    }
    printf("<node at %" PRIu64 ": %" PRIu64 " seen by thread=%" PRIuPTR ">\n", ix, node.index, _Py_ThreadId());
}
