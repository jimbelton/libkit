/*
 * Copyright (c) 2022 Cisco Systems, Inc. and its affiliates
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef KIT_COUNTERS_H
#define KIT_COUNTERS_H

#include <sxe-log.h>

#define KIT_COUNTERS_MAX            600
#define KIT_COUNTERS_INVALID        0       // Uninitialized counters hopefully have this value
#define KIT_COUNTERS_FLAG_NONE      0x00
#define KIT_COUNTERS_FLAG_SUMMARIZE 0x01

/* Special values that can be passed to kit_counter_get_data as threadnum
 */
#define KIT_THREAD_TOTAL  -1    // Get the total for all threads
#define KIT_THREAD_SHARED -2    // Get the shared count (for threads that haven't initialized per thread counters)

#include <inttypes.h>
#include <stdbool.h>

struct kit_counters {
    unsigned long long val[KIT_COUNTERS_MAX];
};

typedef unsigned kit_counter_t;
typedef void (*kit_counters_mib_callback_t)(void *, const char *, const char *);
typedef void (*kit_mibfn_t)(kit_counter_t, const char *subtree, const char *mib, void *v, kit_counters_mib_callback_t cb, int threadnum, unsigned cflags);

#include "kit-counters-proto.h"

#endif
