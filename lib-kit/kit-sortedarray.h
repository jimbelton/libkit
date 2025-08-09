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

#ifndef KIT_SORTEDARRAY_H
#define KIT_SORTEDARRAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KIT_SORTEDARRAY_DEFAULT         0       // No special behaviours
#define KIT_SORTEDARRAY_ALLOW_INSERTS   0x01    // Elements can be added to a sorted array out of order (expensive!)
#define KIT_SORTEDARRAY_ALLOW_GROWTH    0x02    // Sorted array is allowed to grow dynamically
#define KIT_SORTEDARRAY_ZERO_COPY       0x04    // Don't copy the key into the array, just return a pointer to the element
#define KIT_SORTEDARRAY_CMP_CAN_FAIL    0x08    // Comparing two values can fail, returning INT_MAX

#define kit_sortedelement_class kit_sortedarray_class    // For backward compatibility

struct kit_sortedarray_class {
    size_t        size;                                  // Sizeof the element (including padding if not packed)
    size_t        keyoffset;                             // Offset of the key within the element
    int         (*cmp)(const void *, const void *);      // Comparitor for element keys
    const char *(*fmt)(const void *);                    // Formatter for element keys that returns the LRU of 4 static buffers
    bool        (*visit)(void *, const void *);          // Visitor for matching elements in intersections, false on error
    void         *value;                                 // Arbitrary value passed as first parameter to visit
    unsigned      flags;                                 // Flags as defined above
};

#include "kit-sortedarray-proto.h"

static inline const char *
kit_sortedarray_element_to_str(const struct kit_sortedarray_class *type, const void *array, unsigned pos)
{
    return type->fmt((const uint8_t *)array + type->size * pos + type->keyoffset);
}

#endif
