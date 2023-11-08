// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

/*
 * Node layout in memory
 *
 * +--------+--------+--------+
 * | index  | inv(d) |  tag   |
 * +--------+--------+--------+
 * */

#ifndef CEREGGII_NODE_SIZES_TABLE_H
#define CEREGGII_NODE_SIZES_TABLE_H

typedef struct {
    const unsigned char node_size;
    const unsigned char distance_size;
    const unsigned char tag_size;
} node_size_info;

#define ATOMIC_DICT_MIN_LOG_SIZE 6
#define ATOMIC_DICT_MAX_LOG_SIZE 56

extern const node_size_info node_sizes_table[];

#endif // CEREGGII_NODE_SIZES_TABLE_H
