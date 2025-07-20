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

#include <unistd.h>
#include <tap.h>

#include "kit-time.h"

int
main(void)
{
    int64_t  nsec1, nsec2;
    uint32_t sec1,  sec2;

    plan_tests(10);

    is_eq(kit_clocktype(), "monotonic", "We're using a monotonic clock");

    diag("See time cached");
    {
        kit_time_cached_update();
        ok(sec1 = kit_time_cached_sec(), "kit_time_cached_sec() returns seconds");
        usleep(100);
        is(sec2 = kit_time_cached_sec(), sec1, "kit_time_cached_sec() returns the same value multiple times");

        nsec1 = kit_time_cached_nsec();
        usleep(100);
        is(nsec2 = kit_time_cached_nsec(), nsec1, "kit_time_cached_nsec() returns the same value multiple times");
    }

    diag("See time change");
    {
        usleep(1000000);
        kit_time_cached_update();
        isnt(sec2 = kit_time_cached_sec(), sec1, "kit_time_cached_sec() returns a different value after an update");
        isnt(nsec2 = kit_time_cached_nsec(), nsec1, "kit_time_cached_nsec() returns a different value after an update");
    }

    diag("See time half-change");
    {
        usleep(1000000);
        sec1 = kit_time_sec();
        nsec1 = kit_time_nsec();

        isnt(sec1, sec2, "kit_time_sec() returns a different value when time passes");
        isnt(nsec1, nsec2, "kit_time_nsec() returns a different value when time passes");

        is(kit_time_cached_sec(), sec2, "kit_time_cached_sec() returns the same value - not updated");
        is(kit_time_cached_nsec(), nsec2, "kit_time_cached_nsec() returns the same value - not updated");
    }

    return exit_status();
}
