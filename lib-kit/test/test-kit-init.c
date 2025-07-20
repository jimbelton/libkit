/*
 * Copyright (c) 2023 Cisco Systems, Inc. and its affiliates
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

#include <tap.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"

int
main(void)
{
    void *mem;

    tap_plan(6, TAP_FLAG_LINE_ON_OK, NULL);

    is(kit_memory_allocations(), 0, "Expected no allocations before initializing memory");
    kit_memory_initialize(KIT_MEMORY_CHECK_OVERFLOWS);
    kit_memory_set_assert_on_enomem(false);    /* This can be called after initalization */
    is(kit_memory_allocations(), 0, "Expected no allocations after initializing memory either");

    ok(mem = kit_malloc(0), "Got a non-NULL value from kit_malloc(0)");

    MOCKFAIL_START_TESTS(2, kit_malloc_diag);
    ok(kit_malloc(100) == NULL, "When kit_malloc() fails, we get NULL (no abort())");
    ok(kit_calloc(1, 100) == NULL, "When kit_calloc() fails, we get NULL (no abort())");
    MOCKFAIL_END_TESTS();

    MOCKFAIL_START_TESTS(1, kit_realloc_diag);
    ok(kit_realloc(mem, 100) == NULL, "When kit_realloc() fails, we get NULL (no abort())");
    MOCKFAIL_END_TESTS();

    kit_free(mem);
    return exit_status();
}
