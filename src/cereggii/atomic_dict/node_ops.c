// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include "atomic_dict_internal.h"
#include "atomic_ops.h"

// these functions take a pointer to meta, but to avoid multiple reads
// you should dereference dk->meta (i.e. make a thread-local copy) and
// then pass a reference to the copy to these functions instead.


inline void
AtomicDict_ComputeRawNode(AtomicDict_Node *node, AtomicDict_Meta *meta)
{
    assert(node->index < (1 << meta->log_size));
    node->node =
        (node->index << (NODE_SIZE - meta->log_size))
        | (node->tag & TAG_MASK(meta));
}


#ifdef __aarch64__
#  if !defined(__ARM_FEATURE_CRC32)
#    error "CRC32 hardware support not available"
#  endif
#  if defined(__GNUC__)
#    include <arm_acle.h>
#    define CRC32(x, y) __crc32d((x), (y))
#  elif defined(__clang__)
#    define CRC32(x, y) __builtin_arm_crc32d((x), (y))
#  else
#    error "Unsupported compiler for __aarch64__"
#  endif // __crc32d
#else
#  define CRC32(x, y) __builtin_ia32_crc32di((x), (y))
#endif // __aarch64__

#define UPPER_SEED 12923598712359872066ull
#define LOWER_SEED 7467732452331123588ull
#define REHASH(x) (uint64_t) (CRC32((x), LOWER_SEED) | (((uint64_t) CRC32((x), UPPER_SEED)) << 32))

PyObject *
AtomicDict_ReHash(AtomicDict *Py_UNUSED(self), PyObject *ob)
{
    Py_hash_t hash = PyObject_Hash(ob);
    return PyLong_FromUnsignedLongLong(REHASH(hash));
}

inline uint64_t
AtomicDict_Distance0Of(Py_hash_t hash, AtomicDict_Meta *meta)
{
    return REHASH(hash) >> (SIZEOF_PY_HASH_T * CHAR_BIT - meta->log_size);
}

inline void
AtomicDict_ParseNodeFromRaw(uint64_t node_raw, AtomicDict_Node *node,
                            AtomicDict_Meta *meta)
{
    node->node = node_raw;
    node->index = node_raw >> (NODE_SIZE - meta->log_size);
    node->tag = node_raw & TAG_MASK(meta);
}

inline void
AtomicDict_ReadNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta)
{
    const uint64_t raw = meta->index[ix & ((1 << meta->log_size) - 1)];
    AtomicDict_ParseNodeFromRaw(raw, node, meta);
}

inline void
AtomicDict_WriteNodeAt(uint64_t ix, AtomicDict_Node *node, AtomicDict_Meta *meta)
{
    AtomicDict_ComputeRawNode(node, meta);
    AtomicDict_WriteRawNodeAt(ix, node->node, meta);
}

inline void
AtomicDict_WriteRawNodeAt(uint64_t ix, uint64_t raw_node, AtomicDict_Meta *meta)
{
    assert(ix >= 0);
    assert(ix < (1 << meta->log_size));

    meta->index[ix] = raw_node;
}

inline int
AtomicDict_AtomicWriteNodeAt(uint64_t ix, AtomicDict_Node *expected, AtomicDict_Node *desired, AtomicDict_Meta *meta)
{
    AtomicDict_ComputeRawNode(expected, meta);
    AtomicDict_ComputeRawNode(desired, meta);

    return CereggiiAtomic_CompareExchangeUInt64(&meta->index[ix], expected->node, desired->node);
}
