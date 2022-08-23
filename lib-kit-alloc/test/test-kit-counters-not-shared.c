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

#include <pthread.h>
#include <tap.h>

#include "kit-alloc.h"
#include "kit-counters.h"
#include "kit-port.h"

kit_counter_t KIT_COUNT;

static void
static_thread_cleanup(void *v)
{
    if ((intptr_t)v == 2345 && kit_counter_get(KIT_COUNT) == 6)
        kit_counter_incr(KIT_COUNT);
    else
        kit_counter_zero(KIT_COUNT);
}

static void *
static_thread(__unused void *v)
{
    static pthread_key_t cleanup_key;

    kit_counters_init_thread(1);

    pthread_key_create(&cleanup_key, static_thread_cleanup);
    pthread_setspecific(cleanup_key, (void *)(intptr_t)2345);

    kit_counter_add(KIT_COUNT, 5);
    kit_counters_fini_thread(1);

    return NULL;
}

static void
dynamic_thread_cleanup(void *v)
{
    if ((intptr_t)v == 1234 && kit_counter_get(KIT_COUNT) == 8)
        kit_counter_incr(KIT_COUNT);
    else
        kit_counter_zero(KIT_COUNT);
}

static void *
dynamic_thread(__unused void *v)
{
    static pthread_key_t cleanup_key;
    unsigned slot;

    slot = kit_counters_init_dynamic_thread();

    pthread_key_create(&cleanup_key, dynamic_thread_cleanup);
    pthread_setspecific(cleanup_key, (void *)(intptr_t)1234);

    kit_counter_incr(KIT_COUNT);
    kit_counters_fini_dynamic_thread(slot);

    return NULL;
}

int
main(void)
{
    pthread_t thr;
    void *ret;

    plan_tests(7);
    KIT_ALLOC_SET_LOG(1);

    kit_counters_initialize(KIT_COUNTERS_MAX, 2, false);    /* Don't allow shared counters to be used */
    KIT_COUNT = kit_counter_new("kit.counter");
    ok(kit_counter_isvalid(KIT_COUNT), "Created kit.counter");
    kit_counter_incr(KIT_COUNT);

    if (ok(pthread_create(&thr, NULL, static_thread, NULL) == 0, "Created a static thread with counters"))
        pthread_join(thr, &ret);
    is(kit_counter_get(KIT_COUNT), 7, "Static thread-specific cleanup was able to adjust counters");

    diag("Setting up for dynamic threads and creating one to bump counters by 0, 1 and 10");
    kit_counters_prepare_dynamic_threads(1);

    if (ok(pthread_create(&thr, NULL, dynamic_thread, NULL) == 0, "Created a dynamic thread with counters"))
        pthread_join(thr, &ret);

    is(kit_counter_get(KIT_COUNT), 9, "Dynamic thread-specific cleanup was able to adjust counters");
    is(kit_num_counters(), 7, "Number of counters is as expected (1 + 6 memory counters)");

    is(kit_memory_allocations(), 3, "3 allocations for counters");

    return exit_status();
}
