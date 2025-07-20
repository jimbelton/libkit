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

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "kit-test.h"        // To enable junk filling when build with SXE_DEBUG == 1
#include "sxe-util.h"

unsigned long long initial_mallocs = ~0UL;

static void
check_counters(const char *why, int m, int c, int r, int f, int fail)
{
    initial_mallocs = initial_mallocs == ~0ULL ? kit_counter_get(KIT_COUNTER_MEMORY_MALLOC) : initial_mallocs;
    m              += initial_mallocs;

    is(kit_counter_get(KIT_COUNTER_MEMORY_MALLOC), m, "%s, the KIT_COUNTER_MEMORY_MALLOC value is %d", why, m);
    is(kit_counter_get(KIT_COUNTER_MEMORY_CALLOC), c, "%s, the KIT_COUNTER_MEMORY_CALLOC value is %d", why, c);
    is(kit_counter_get(KIT_COUNTER_MEMORY_REALLOC), r, "%s, the KIT_COUNTER_MEMORY_REALLOC value is %d", why, r);
    is(kit_counter_get(KIT_COUNTER_MEMORY_FREE), f, "%s, the KIT_COUNTER_MEMORY_FREE value is %d", why, f);
    is(kit_counter_get(KIT_COUNTER_MEMORY_FAIL), fail, "%s, the KIT_COUNTER_MEMORY_FAIL value is %d", why, fail);
}

struct counter_gather {
    unsigned long bytes;
    unsigned long malloc;
    unsigned long calloc;
    unsigned long realloc;
    unsigned long free;
    unsigned long fail;
    unsigned long wtf;
};

static void
counter_callback(void *v, const char *key, const char *val)
{
    struct counter_gather *cg = v;
    unsigned i;
    char *end;

    const struct {
        const char *key;
        unsigned long *val;
    } map[] = {
        { "memory.bytes", &cg->bytes },
        { "memory.malloc", &cg->malloc },
        { "memory.calloc", &cg->calloc },
        { "memory.realloc", &cg->realloc },
        { "memory.free", &cg->free },
        { "memory.fail", &cg->fail },
        { NULL, &cg->wtf },
    };

    for (i = 0; i < sizeof(map) / sizeof(*map); i++)
        if (map[i].key == NULL || strcmp(key, map[i].key) == 0) {
            if (map[i].key == NULL)
                SXEL1("Unexpected counter_callback key '%s'", key);
            *map[i].val = strtoul(val, &end, 0);
            if (*end)
                *map[i].val = 666;
            break;
        }
}

static int  length = 0;
static char buffer[4096];

static __printflike(1, 2) int
test_printf(const char *format, ...)
{
    va_list ap;
    int     newlen;

    va_start(ap, format);
    newlen = vsnprintf(buffer + length, sizeof(buffer) - length, format, ap);
    va_end(ap);

    if (newlen > 0) {
        length = newlen < (int)(sizeof(buffer) - length) ? length + newlen : (int)sizeof(buffer);
    }

    return newlen;
}

int
main(void)
{
    size_t talloc1, talloc2, talloc3;
    size_t alloc1, alloc2, alloc3;
    struct counter_gather cg;
    char *ptr1, *ptr2;
    int failures;

    tap_plan(103, TAP_FLAG_LINE_ON_OK, NULL);

    // KIT_ALLOC_SET_LOG(1);    // Turn off when done
    is(kit_memory_allocations(), 0, "Expected no allocations before initializing counters");
    kit_memory_set_flags(KIT_MEMORY_CHECK_OVERFLOWS);       // Need overflow checks to ensure coverage
    kit_counters_initialize(KIT_COUNTERS_MAX, 1, false);    // This calls kit_memory_initialize(~0U)
    kit_memory_set_assert_on_enomem(true);
    is(kit_memory_allocations(), 2, "Expected number of allocations for memory counters");
    check_counters("After initializing the counters", 0, 0, 0, 0, 0);
    alloc1  = kit_allocated_bytes();
    talloc1 = kit_thread_allocated_bytes();

    ptr1 = kit_malloc(100);
#if SXE_DEBUG
    is(*(uint64_t *)ptr1,   0xa5a5a5a5a5a5a5a5ULL, "Malloc junk fills in a debug test");
#else
    ok(*(uint64_t *)ptr1 != 0xa5a5a5a5a5a5a5a5ULL, "Malloc doesn't junk fill in a release test");
#endif
    check_counters("After one malloc", 1, 0, 0, 0, 0);
    alloc2 = kit_allocated_bytes();
    talloc2 = kit_thread_allocated_bytes();
    ok(alloc2 - alloc1 >= 100, "kit_allocated_bytes() reports that alloc2 (%zu) is at least 100 greater than alloc1 (%zu)", alloc2, alloc1);
    ok(talloc2 - talloc1 >= 100, "kit_thread_allocated_bytes() reports that talloc2 (%zu) is at least 100 greater than talloc1 (%zu)", talloc2, talloc1);

    ptr2 = kit_malloc_diag(0, __FILE__, __LINE__);    // Call via the wrapper (used in release build by openssl)
    ok(ptr1 != NULL, "kit_malloc(0) returns a pointer");
    check_counters("After malloc(0)", 2, 0, 0, 0, 0);

    kit_free(ptr2);
    check_counters("After a free()", 2, 0, 0, 1, 0);

    ptr2 = kit_calloc(2, 100);
    check_counters("After an additional calloc", 2, 1, 0, 1, 0);
    alloc3 = kit_allocated_bytes();
    talloc3 = kit_thread_allocated_bytes();
    ok(alloc3 - alloc2 >= 200, "kit_allocated_bytes() reports that alloc3 (%zu) is at least 200 greater than alloc2 (%zu)", alloc3, alloc2);
    ok(talloc3 - talloc2 >= 200, "kit_thread_allocated_bytes() reports that talloc3 (%zu) is at least 200 greater than talloc2 (%zu)", talloc3, talloc2);

    kit_free_diag(ptr1, __FILE__, __LINE__);    // Call via the wrapper (used in release build by openssl)
    check_counters("After one free()", 2, 1, 0, 2, 0);

    ptr2 = kit_realloc(ptr2, 10);
    check_counters("After a realloc()", 2, 1, 1, 2, 0);

    ptr1 = kit_realloc(ptr2, 0);
    ok(ptr1 == NULL, "realloc(..., 0) returns NULL");
    check_counters("After a realloc(..., 0)", 2, 1, 1, 3, 0);

    ptr1 = kit_realloc_diag(NULL, 0, __FILE__, __LINE__);    // Call via the wrapper (used in release build by openssl)
    check_counters("After a realloc(NULL, 0)", 3, 1, 1, 3, 0);

    kit_free(NULL);
    check_counters("After a free(NULL)", 3, 1, 1, 3, 0);

    kit_memory_set_assert_on_enomem(false);    // Failures tested after this point, so don't abort

    failures = 0;
    MOCKFAIL_START_TESTS(1, kit_realloc_diag);
    is(kit_reduce(ptr1, 1), ptr1, "When kit_reduce() fails to realloc(), it returns the same pointer");
    failures++;
    MOCKFAIL_END_TESTS();

    kit_reduce(ptr1, 0);
    check_counters("After free()ing the last pointer", 3, 1, 1, 4, failures);

    ok(!kit_reduce(NULL, 0), "kit_reduce(NULL, 0) is a no-op");

    ok(ptr1 = kit_strndup("hello, world.", 5), "Duplicated up to 5 bytes of 'hello, world'");
    is_eq(ptr1, "hello",                       "Duplicated string is just 'hello'");
    kit_free(ptr1);

    diag("Checking counter gatherer");
    memset(&cg, 0xa5, sizeof(cg));
    cg.wtf = 0;
    kit_counters_mib_text("memory", &cg, counter_callback, -1, 0);
    ok(cg.bytes > 200,                        "Allocated more than 200 bytes");
    is(cg.malloc - initial_mallocs, 4,        "Malloc says 4 more than the initial value");
    is(cg.calloc,                   1,        "Calloc says 1");
    is(cg.realloc,                  1,        "Realloc says 1");
    is(cg.free,                     5,        "Free says 5");
    is(cg.fail,                     failures, "Fail says %d", failures);
    is(cg.wtf,                      0,        "WTF says 0");

    void *mem = malloc(1);    // CONVENTION EXCLUSION: Force some glibc memory growth
    ok(kit_memory_log_growth(test_printf), "Logged growth in allocated memory");
    is_strncmp(buffer, "Maximum memory allocated via jemalloc", sizeof("Maximum memory allocated via jemalloc") - 1,
               "Expected start of output found");
    length = 0;
    ok(kit_memory_log_stats(test_printf, NULL), "Created a statistics file");
    is_strncmp(buffer, "___ Begin jemalloc statistics ___", sizeof("___ Begin jemalloc statistics ___") - 1,
               "Expected start of output found");
    free(mem);    // CONVENTION EXCLUSION: Free the memory allocated with glibc above

    diag("Test kit-alloc-analyze");
    {
#if SXE_DEBUG
        KIT_ALLOC_SET_LOG(1);           // Enable kit-alloc logging in debug builds
#endif
        mem  = kit_memalign(16, 16);
        ptr1 = tap_shell("../../bin/kit-alloc-analyze test-kit-alloc.t.out", NULL);

#if SXE_DEBUG
        is_strncmp(ptr1, "Unmatched allocs:\n", sizeof("Unmatched allocs:\n") - 1, "There was an unmatched allocation");
        is_strstr( ptr1, "kit_memalign(16,16)",                                    "It was a call to kit_memalign");
#else
        is_eq(ptr1, "kit_alloc debug logging may not be enabled\n", "kit-alloc-analyze requires a debug build");
        skip(1, "Non debug build");
#endif
        free(ptr1);    // CONVENTION EXCLUSION: OK to use in test code

        kit_free(mem);
        ptr1 = tap_shell("../../bin/kit-alloc-analyze test-kit-alloc.t.out", NULL);

#if SXE_DEBUG
        is_eq(ptr1, "", "There were no unmatched allocations");
#else
        is_eq(ptr1, "kit_alloc debug logging may not be enabled\n", "kit-alloc-analyze requires a debug build");
#endif
        free(ptr1);    // CONVENTION EXCLUSION: OK to use in test code
    }

    diag("Test zero length allocations, calloc overflows, and error cases");
    {
        ok(mem = kit_memalign(16, 0),                  "Allocated 0 bytes of aligned memory");
        kit_free(mem);
        ok(mem = kit_calloc(0, 0),                     "Allocated 0 bytes of zeroed memory");
        kit_free(mem);
        ok(mem = kit_calloc(0, 1),                     "Allocated 0 x 1 bytes of zeroed memory");
        kit_free(mem);
        is(kit_calloc(2, 0x8000000000000000ULL), NULL, "Can't allocate 2 * 0x8000000000000000 bytes");
        is(errno, ENOMEM,                              "Errno '%s' is the expected 'Out of memory'", strerror(errno));

        failures = kit_counter_get(KIT_COUNTER_MEMORY_FAIL);
        MOCKFAIL_START_TESTS(2, kit_malloc_diag);
        is(kit_malloc(16), NULL, "Test malloc failure");
        is(kit_counter_get(KIT_COUNTER_MEMORY_FAIL), failures + 1, "Expected the failure count to increase");
        MOCKFAIL_END_TESTS();
    }

    diag("Test memory guards");
    {
        ok(ptr1 = kit_memalign(32, 32),                          "Allocated memory at 32 byte alignment");
        is((uintptr_t)ptr1 & 0x1F, 0,                            "Memory is actually 32 byte aligned");
        ok(ptr1 - (char *)kit_memory_check(ptr1, &alloc1) == 32, "Expected a double guard before 32 byte aligned memory");
        is(alloc1, 32,                                           "Expected the correct allocated size to be set");

        ok(ptr2 = kit_realloc(ptr1, 64),                         "Reallocated memory (%s aligned)",
                                                                 ((uintptr_t)ptr2 & 0x1F) == 0 ? "still" : "no longer");
        ok(ptr2 - (char *)kit_memory_check(ptr2, &alloc1) == 32, "Expected a double guard before 64 byte reallocated memory");
        is(alloc1, 64,                                           "Expected the new correct allocated size to be set");
        kit_free(ptr2);
    }

    diag("Safe kit functions tests");
    {
        mem = NULL;
        mem = kit_malloc_safe(mem, 32);
        ok(mem != NULL, "Allocated 32 bytes using safe malloc");
        kit_free_safe(&mem);
        ok(mem == NULL, "Free'd 32 bytes using safe free");

        mem = kit_calloc_safe(mem, 32, 2);
        ok(mem != NULL, "Allocated 64 bytes using safe calloc");
        kit_free_safe(&mem);
        ok(mem == NULL, "Free'd 64 bytes using safe free");
    }

    diag("Test kit_strdup");
    {
        ok(mem = kit_strdup("hello, world"), "Duplicated a string");
        is_eq(mem, "hello, world",           "It's correct");
        kit_free(mem);
    }

    is(kit_counter_get_data(KIT_COUNTERS_INVALID, -1), 0, "The invalid counter has not been touched");
    return exit_status();
}
