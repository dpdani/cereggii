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

    uint8_t distance;
    if (node->distance > UINT8_MAX) {
        distance = UINT8_MAX;
    } else {
        distance = (uint8_t) node->distance;
    }

    node->node =
        (node->index << (NODE_SIZE - meta->log_size))
        | (node->tag & TAG_MASK(meta))
        | distance;

#ifdef CEREGGII_DEBUG
    AtomicDictNode check_node;
    parse_node_from_raw(node->node, &check_node, meta);
    assert(index == check_node.index);
    assert(node->distance == check_node.distance || check_node.distance == UINT8_MAX);
#endif
}

int
check_tag(Py_hash_t hash, AtomicDictNode node, AtomicDictMeta *meta)
{
    return (node.tag & TAG_MASK(meta)) == (REHASH(hash) & TAG_MASK(meta));
}

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
    node->distance = node_raw & DISTANCE_MASK;
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
