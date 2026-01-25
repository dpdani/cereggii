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
AtomicDict_ComputeRawNode(AtomicDict_Node *node, AtomicDict_Meta *meta)
{
    assert(node->index < (1ull << meta->log_size));
    uint64_t index = node->index;
    int64_t greatest_allocated_block = atomic_load_explicit((_Atomic (int64_t) *) &meta->greatest_allocated_block, memory_order_acquire);
    if (greatest_allocated_block >= 0) {
        assert(node->index < (((uint64_t) greatest_allocated_block + 1) << ATOMIC_DICT_LOG_ENTRIES_IN_BLOCK));
    }
    node->node =
        (node->index << (NODE_SIZE - meta->log_size))
        | (node->tag & TAG_MASK(meta));
    AtomicDict_Node check_node;
    AtomicDict_ParseNodeFromRaw(node->node, &check_node, meta);
    assert(index == check_node.index);
    cereggii_unused_in_release_build(index);
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
AtomicDict_Distance0Of(Py_hash_t hash, AtomicDict_Meta *meta)
{
    return REHASH(hash) >> (SIZEOF_PY_HASH_T * CHAR_BIT - meta->log_size);
}

void
AtomicDict_ParseNodeFromRaw(uint64_t node_raw, AtomicDict_Node *node,
                            AtomicDict_Meta *meta)
{
    node->node = node_raw;
    node->index = node_raw >> (NODE_SIZE - meta->log_size);
    node->tag = node_raw & TAG_MASK(meta);
}

uint64_t
AtomicDict_ReadRawNodeAt(uint64_t ix, AtomicDict_Meta *meta)
{
    return atomic_load_explicit((_Atomic (uint64_t) *) &meta->index[ix & ((1 << meta->log_size) - 1)], memory_order_acquire);
}

int
AtomicDict_IsEmpty(AtomicDict_Node *node)
{
    return node->node == 0;
}

int
AtomicDict_IsTombstone(AtomicDict_Node *node)
{
    return node->node != 0 && node->index == 0;
}

void
AtomicDict_ReadNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta)
{
    const uint64_t raw = AtomicDict_ReadRawNodeAt(ix, meta);
    AtomicDict_ParseNodeFromRaw(raw, node, meta);
}

void
AtomicDict_WriteNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta)
{
    assert(ix < (1ull << meta->log_size));
    AtomicDict_ComputeRawNode(node, meta);
    assert(atomic_dict_entry_ix_sanity_check(node->index, meta));
    AtomicDict_WriteRawNodeAt(ix, node->node, meta);
}

void
AtomicDict_WriteRawNodeAt(uint64_t ix, uint64_t raw_node, AtomicDict_Meta *meta)
{
    assert(ix < (1ull << meta->log_size));
    AtomicDict_Node node;
    AtomicDict_ParseNodeFromRaw(raw_node, &node, meta);
    assert(atomic_dict_entry_ix_sanity_check(node.index, meta));

    atomic_store_explicit((_Atomic (uint64_t) *) &meta->index[ix], raw_node, memory_order_release);
}

int
AtomicDict_AtomicWriteNodeAt(uint64_t ix, AtomicDict_Node *expected, AtomicDict_Node *desired, AtomicDict_Meta *meta)
{
    AtomicDict_ComputeRawNode(expected, meta);
    AtomicDict_ComputeRawNode(desired, meta);
    assert(atomic_dict_entry_ix_sanity_check(expected->index, meta));
    assert(atomic_dict_entry_ix_sanity_check(desired->index, meta));

    uint64_t _expected = expected->node;
    return atomic_compare_exchange_strong_explicit((_Atomic (uint64_t) *) &meta->index[ix], &_expected, desired->node, memory_order_acq_rel, memory_order_acquire);
}

void
AtomicDict_PrintNodeAt(const uint64_t ix, AtomicDict_Meta *meta)
{
    AtomicDict_Node node;
    AtomicDict_ReadNodeAt(ix, &node, meta);
    if (AtomicDict_IsTombstone(&node)) {
        printf("<node at %" PRIu64 ": %" PRIu64 " (tombstone) seen by thread=%" PRIuPTR ">\n", ix, node.node, _Py_ThreadId());
        return;
    }
    printf("<node at %" PRIu64 ": %" PRIu64 " (index=%" PRIu64 ", tag=%" PRIu64 ") seen by thread=%" PRIuPTR ">\n", ix, node.node, node.index, node.tag, _Py_ThreadId());
}
