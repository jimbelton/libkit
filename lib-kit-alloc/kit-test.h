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

/* Boiler-plate code for libkit/libtap based test programs
 */
#ifndef KIT_TEST_H
#define KIT_TEST_H 1

#include <kit-alloc.h>
#include <stdint.h>
#include <tap.h>

#define KIT_TEST_SUCCESS

/* If a test program is built with the SXE_DEBUG flag set, enable jemalloc junk filling to catch bugs
 */
#if SXE_DEBUG
const char *malloc_conf = KIT_MALLOC_CONF_JUNK_FILL;
#endif

/**
 * Plan a number of tap tests, run under memory leak checking
 *
 * @param num_tests Number of tests
 *
 * @note Must be followed by a single matching kit_test_exit call
 */
#define kit_test_plan(num_tests)                               \
{                                                              \
    tap_plan((num_tests) + 1, TAP_FLAG_LINE_ON_OK, NULL);      \
    uint64_t kit_start_allocations = kit_memory_allocations();

/**
 * Test intermediate number of allocations
 *
 * @param exp_alloc Expected number of unfreed allocations at the current point in the test
 *
 * @note This counts as one of your tap tests
 */
#define kit_allocation_test(exp_alloc) \
    is(kit_memory_allocations(), kit_start_allocations + (exp_alloc), "Got expected number of allocations")

/**
 * Complete tap tests run under memory leak checking
 *
 * @param exp_alloc Expected number of unfreed allocations (normally 0)
 *
 * @note Must followed a single matching kit_test_plan call
 */
#define kit_test_exit(exp_alloc)                                                                                        \
    is(kit_memory_allocations() - kit_start_allocations, (exp_alloc), "All memory allocations were freed after tests"); \
    return exit_status();                                                                                               \
}

#endif
