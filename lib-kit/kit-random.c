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

/*
 * Simple and efficient API wrapping arc4random.
 */

#include <limits.h>
#include <sxe-log.h>

#include "kit-random.h"

void
kit_random_init(int seed_fd)
{
    SXEE7("(fd=%d)", seed_fd);
    kit_arc4random_init(seed_fd);
    SXER7("return");
}

uint32_t
kit_random32(void)
{
    return kit_arc4random();
}

uint16_t
kit_random16(void)
{
    static __thread unsigned outleft;
    static __thread uint32_t rnd;

    if (!outleft) {
        rnd = kit_random32();
        outleft = sizeof(rnd) / sizeof(uint16_t);
    }
    outleft--;

    return (uint16_t)(rnd >> (outleft * CHAR_BIT * sizeof(uint16_t)));
}

uint8_t
kit_random8(void)
{
    static __thread unsigned outleft;
    static __thread uint32_t rnd;

    if (!outleft) {
        rnd = kit_random32();
        outleft = sizeof(rnd) / sizeof(uint8_t);
    }
    outleft--;

    return (uint8_t)(rnd >> (outleft * CHAR_BIT * sizeof(uint8_t)));
}
