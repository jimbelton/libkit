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

#ifndef SXE_SORTEDARRAY_H
#define SXE_SORTEDARRAY_H

#include <stdbool.h>

#define SXE_SORTEDARRAY_DEFAULT         0       // No special behaviours
#define SXE_SORTEDARRAY_ALLOW_INSERTS   0x01    // Elements can be added to a sorted array out of order (expensive!)
#define SXE_SORTEDARRAY_ALLOW_GROWTH    0x02    // Sorted array is allowed to grow dynamically
#define SXE_SORTEDARRAY_ZERO_COPY       0x04    // Don't copy the key into the array, just return a pointer to the element
#define SXE_SORTEDARRAY_CMP_CAN_FAIL    0x08    // Comparing two values can fail, returning INT_MAX

struct sxe_sortedarray_class {
    size_t        size;                                  // Sizeof the element (including padding if not packed)
    size_t        keyoffset;                             // Offset of the key within the element
    int         (*cmp)(const void *, const void *);      // Comparitor for element keys
    const char *(*fmt)(const void *);                    // Formatter for element keys that returns the LRU of 4 static buffers
    bool        (*visit)(void *, const void *);          // Visitor for matching elements in intersections, false on error
    void         *value;                                 // Arbitrary value passed as first parameter to visit
    unsigned      flags;                                 // Flags as defined above
};

#include "sxe-sortedarray-proto.h"

#endif
