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

// Kit bit masks are weird in that bit 0 is stored as 0x80 in the first byte of the bit mask. This is done so that they can
// store network byte ordered subnet masks. Subnet mask /25 is stored as 0xFF, 0xFF, 0FF, 0x80.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kit-bits-proto.h"

static inline void
kit_bits_set(void *me, size_t i)
{

    ((uint8_t *)me)[i / 8] |= 1 << (7 - i % 8);
}

static inline void
kit_bits_clear(void *me, size_t i)
{
    ((uint8_t *)me)[i / 8] &= ~(1 << (7 - i % 8));
}

static inline bool
kit_bits_isset(const void *me, size_t i)
{
    return (((const uint8_t *)me)[i / 8] & (1 << (7 - i % 8))) != 0;
}
