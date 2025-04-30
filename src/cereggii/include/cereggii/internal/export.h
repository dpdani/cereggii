// SPDX-FileCopyrightText: 2023-present dpdani <git@danieleparmeggiani.me>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef CEREGGII_PUBLIC_H
#define CEREGGII_PUBLIC_H

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef BUILDING_CEREGGII
    #define CEREGGII_PUBLIC __declspec(dllexport)
  #else
    #define CEREGGII_PUBLIC __declspec(dllimport)
  #endif
#else
  #define CEREGGII_PUBLIC __attribute__((visibility("default")))
#endif

#endif // CEREGGII_PUBLIC_H
