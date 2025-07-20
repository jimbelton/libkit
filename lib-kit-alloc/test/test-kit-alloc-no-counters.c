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

#include <tap.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"

int
main(void)
{
    void *mem;

    plan_tests(6);
    kit_memory_initialize(0);

    MOCKFAIL_START_TESTS(3, kit_malloc_diag);
    is(kit_malloc(16), NULL,        "Test malloc failure");
    is(kit_calloc(1, 16), NULL,     "Test calloc failure");
    is(kit_realloc(NULL, 16), NULL, "Test realloc failure");
    MOCKFAIL_END_TESTS();

#if SXE_DEBUG || SXE_COVERAGE
#   define EXP_ALLOCS 2
#else
#   define EXP_ALLOCS 0
#endif

    is(kit_memory_allocations(), EXP_ALLOCS,       "Failed allocations count as allocations");
    ok(mem = kit_memalign_diag(16, 16, "file", 1), "Allocated memory using kit_memalign_diag SSL callback function");
    kit_free(mem);
    is(kit_memory_allocations(), EXP_ALLOCS,       "kit_memalign and kit_free balance eachother out");

    return exit_status();
}
