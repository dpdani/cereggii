// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#include "atomic_dict_internal.h"


const node_size_info node_sizes_table[] = {
    {0,  0, 0},
    {0,  0, 0},
    {0,  0, 0},
    {0,  0, 0},
    {0,  0, 0},
    {0,  0, 0},
    {8,  2, 0},  // this is the minimum log_size
    {16, 3, 6},
    {16, 3, 5},
    {16, 4, 3},
    {16, 4, 2},
    {32, 4, 17},
    {32, 4, 16},
    {32, 4, 15},
    {32, 4, 14},
    {32, 4, 13},
    {32, 4, 12},
    {32, 5, 10},
    {32, 5, 9},
    {32, 5, 8},
    {32, 5, 7},
    {32, 5, 6},
    {32, 5, 5},
    {32, 5, 4},
    {32, 5, 3},
    {32, 5, 2},
    {64, 5, 33},
    {64, 5, 32},
    {64, 5, 31},
    {64, 5, 30},
    {64, 5, 29},
    {64, 5, 28},
    {64, 5, 27},
    {64, 6, 25},
    {64, 6, 24},
    {64, 6, 23},
    {64, 6, 22},
    {64, 6, 21},
    {64, 6, 20},
    {64, 6, 19},
    {64, 6, 18},
    {64, 6, 17},
    {64, 6, 16},
    {64, 6, 15},
    {64, 6, 14},
    {64, 6, 13},
    {64, 6, 12},
    {64, 6, 11},
    {64, 6, 10},
    {64, 6, 9},
    {64, 6, 8},
    {64, 6, 7},
    {64, 6, 6},
    {64, 6, 5},
    {64, 6, 4},
    {64, 6, 3},
    {64, 6, 2},
};
