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

#include <string.h>
#include <tap.h>

#include "kit-bits.h"

int
main(void)
{
    uint8_t bits[2], copy[2];    // 16 bits

    plan_tests(10);
    memset(bits, 0, 2);

    ok(!kit_bits_isset_any(bits, 9),    "No bits set in clear mask");
    kit_bits_set(bits, 9);            // Set the tenth bit
    ok(!kit_bits_isset_any(bits, 9),    "Still no bits set in mask");
    kit_bits_set(bits, 8);            // Set the ninth bit
    ok(kit_bits_isset_any(bits, 9),     "Now a bit is set in mask");
    ok(kit_bits_isset_any(bits, 16),    "The mask is a multiple of 8 bits and has a bit set");

    is(kit_bits_copy(copy, bits, 9), 2, "Copied two bytes");
    ok(!kit_bits_isset_any(bits, 8),    "First byte is clear");
    ok(kit_bits_isset(bits, 8),         "Ninth bit is set");
    ok(kit_bits_isset(bits, 9),         "Tenth bit is clear");

    ok(kit_bits_equal(bits, copy, 9),   "First nine bits are the same");
    ok(!kit_bits_equal(bits, copy, 10), "Tenth bit differs");

    return exit_status();
}
