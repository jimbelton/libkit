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

#include "kit-bits.h"
#include <string.h>

/*
 * Copy the first NUM_BITS bits of the memory at S to the memory at D.
 * In the last byte written, set any uncopied bits to zero.  Return
 * the number of bytes written.
 */
size_t
kit_bits_copy(void *d, const void *s, size_t num_bits)
{
    size_t num_bytes = (num_bits + 7) / 8;

    memcpy(d, s, num_bytes);

    if (num_bits % 8 > 0)
        ((uint8_t *)d)[num_bytes - 1] &= 0xFF << (8 - num_bits % 8);

    return num_bytes;
}

/*
 * Compare the first NUM_BITS bits of the memory at S1 and S2.  Return
 * a nonzero integer if they are equal, otherwise return zero.
 */
bool
kit_bits_equal(const void *s1, const void *s2, size_t num_bits)
{
    size_t num_bytes = num_bits / 8;

    return memcmp(s1, s2, num_bytes) == 0
        && (num_bits % 8 == 0
         || ((((const uint8_t *)s1)[num_bytes] ^ ((const uint8_t *)s2)[num_bytes]) >> (8 - num_bits % 8)) == 0);
}

bool
kit_bits_isset_any(const void *bits, size_t num_bits)
{
    size_t byte, whole_bytes = num_bits / 8;

    for (byte = 0; byte < whole_bytes; byte++)    // For each whole byte
        if (((const uint8_t *)bits)[byte])
            return true;

    if (num_bits % 8 > 0 && ((const uint8_t *)bits)[whole_bytes] & (0xFF << (8 - num_bits % 8)))
        return true;

    return false;
}
