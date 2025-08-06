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

// This interface is deprecated; please use the updated kit-sortedarray interface instead

#ifndef SXE_SORTEDARRAY_H
#define SXE_SORTEDARRAY_H

#include "kit-sortedarray.h"

#define SXE_SORTEDARRAY_DEFAULT       KIT_SORTEDARRAY_DEFAULT
#define SXE_SORTEDARRAY_ALLOW_INSERTS KIT_SORTEDARRAY_ALLOW_INSERTS
#define SXE_SORTEDARRAY_ALLOW_GROWTH  KIT_SORTEDARRAY_ALLOW_GROWTH
#define SXE_SORTEDARRAY_ZERO_COPY     KIT_SORTEDARRAY_ZERO_COPY
#define SXE_SORTEDARRAY_CMP_CAN_FAIL  KIT_SORTEDARRAY_CMP_CAN_FAIL

#define sxe_sortedarray_class     kit_sortedarray_class
#define sxe_sortedarray_add       kit_sortedarray_add_element
#define sxe_sortedarray_delete    kit_sortedarray_delete
#define sxe_sortedarray_find      kit_sortedarray_find
#define sxe_sortedarray_get       kit_sortedarray_get
#define sxe_sortedarray_intersect kit_sortedarray_intersect

#endif
