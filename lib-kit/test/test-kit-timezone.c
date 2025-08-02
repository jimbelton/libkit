/*
 * Copyright (c) 2024 Cisco Systems, Inc. and its affiliates
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
#include <string.h>
#include <tap.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "kit-timezone.h"

static bool
tm_eq(const struct tm *left, const struct tm *right)
{
    return left->tm_sec    == right->tm_sec && left->tm_min   == right->tm_min && left->tm_hour   == right->tm_hour
        && left->tm_mday   == right->tm_mday && left->tm_mon  == right->tm_mon && left->tm_year   == right->tm_year
        && left->tm_wday   == right->tm_wday && left->tm_yday == right->tm_yday && left->tm_isdst == right->tm_isdst
        && left->tm_gmtoff == right->tm_gmtoff;
}

static time_t    gm_time;
static struct tm gm_tm;

static void *
thread_start(void *path)
{
    const struct kit_timezone *timezone;
    struct tm                  test_tm;

    ok(timezone = kit_timezone_load(path, 0),                        "Got test_zoneinfo");
    ok(kit_timezone_lock(timezone),                                  "Locked the timezone");
    ok(kit_timezone_time_to_localtime(timezone, gm_time, &test_tm),  "Got the local time in the test zone");
    ok(tm_eq(&gm_tm, &test_tm),                                      "It's in UTC");
    kit_timezone_unlock(timezone);
    return NULL;
}

int
main(void)
{
    struct tm                  local_tm, test_tm;
    const struct kit_timezone *local_tz, *timezone;
    size_t                     len;
    pthread_t                  thread;
    char                       path[PATH_MAX];

    plan_tests(31);
    kit_memory_initialize(KIT_MEMORY_ABORT_ON_ENOMEM | KIT_MEMORY_CHECK_OVERFLOWS);
    uint64_t start_allocations = kit_memory_allocations();    // Clocked the initial # of memory allocations
//  KIT_ALLOC_SET_LOG(1);

    unlink("test_zoneinfo");
    kit_timezone_initialize(1);    // Check for new updates if zoneinfo is older than 1 second

    diag("Verify a invalid timezone can be loaded but not used");
    {
        ok(!(local_tz = kit_timezone_load("America/nowhere", sizeof("America/nowhere") - 1)), "Failed to get timezone for nowhere");
        ok(!kit_timezone_time_to_localtime(local_tz, gm_time, &local_tm), "Unable to get the local time in nowhere");
    }

    diag("Verify that a timezone can be loaded and used");
    {
        ok(local_tz = kit_timezone_load("America/Vancouverxxx", sizeof("America/Vancouver") - 1), "Got zoneinfo for Vancouver");
        gm_time = time(NULL);
        ok(kit_timezone_time_to_localtime(local_tz, gm_time, &local_tm), "Got the local time in Vancouver");
        SXEA1(gmtime_r(&gm_time, &gm_tm),                                "Failed to get the UTC time");

        if (gm_tm.tm_hour > local_tm.tm_hour) {
            is(gm_tm.tm_yday, local_tm.tm_yday,                                                "Days are the same");
            ok(gm_tm.tm_hour - local_tm.tm_hour == 7 || gm_tm.tm_hour - local_tm.tm_hour == 8, "Times differ by 7 or 8");
        }
        else {
            is(gm_tm.tm_yday, local_tm.tm_yday + 1,                                                      "UTC is on to the next day");
            ok(gm_tm.tm_hour + 24 - local_tm.tm_hour == 7 || gm_tm.tm_hour + 24 - local_tm.tm_hour == 8, "Times differ by 7 or 8");
        }

        is_eq(kit_timezone_get_name(local_tz, &len), "America/Vancouver", "Got the correct name");
        is(len, sizeof("America/Vancouver") - 1,                          "Got the correct name length");
    }

    diag("Verify files not present and added");
    {
        SXEA1(getcwd(path, sizeof(path)),                                   "Failed to get the current working directory path");
        SXEA1(strlcat(path, "/test_zoneinfo", sizeof(path)) < sizeof(path), "Failed to append /test_zoneinfo");
        is(kit_timezone_load(path, 0), NULL,                                "Correctly couldn't get test_zoneinfo");

        SXEA1(system("cp /usr/share/zoneinfo/America/Vancouver ./test_zoneinfo") == 0, "Failed to copy Vancouver zoneinfo");
        is(kit_timezone_load(path, 0), NULL,                                           "Still couldn't get test_zoneinfo");

        ok(!kit_timezone_load("America/nowhere", sizeof("America/nowhere") - 1), "Got no zoneinfo for nowhere");
        ok(!kit_timezone_load("America/nowhere", sizeof("America/nowhere") - 1), "Got no zoneinfo for nowhere again");
        sleep(2);
        ok(!kit_timezone_load("America/nowhere", sizeof("America/nowhere") - 1), "Got no zoneinfo for nowhere yet again (after 2 seconds)");

        ok(timezone = kit_timezone_load(path, 0),                       "Got test_zoneinfo");
        ok(kit_timezone_time_to_localtime(timezone, gm_time, &test_tm), "Got the local time in the test zone");
        ok(tm_eq(&local_tm, &test_tm),                                  "Same as the local time in Vancouver");
    }

    diag("Verify files changing and thread safety");
    {
        SXEA1(system("cp /usr/share/zoneinfo/UTC ./test_zoneinfo") == 0, "Failed to copy UTC zoneinfo");
        ok(timezone = kit_timezone_load(path, 0),                        "Got test_zoneinfo");
        ok(kit_timezone_lock(timezone),                                  "Locked the timezone");
        ok(kit_timezone_time_to_localtime(timezone, gm_time, &test_tm),  "Got the local time in the test zone");
        ok(tm_eq(&local_tm, &test_tm),                                   "Still same as the local time in Vancouver");

        SXEA1(pthread_create(&thread, NULL, thread_start, path) == 0, "Failed to create a thread");
        sleep(2);                                                   // Delay before unlocking to make sure thread waits to lock
        kit_timezone_unlock(timezone);
        SXEA1(pthread_join(thread, NULL)                        == 0, "Failed to wait for thread to terminate");
    }

    diag("Verify coverage");
    {
        unlink("test_zoneinfo");
        ok(kit_timezone_lock(timezone),  "Able to lock the test zone when the zoneinfo file was deleted");
        kit_timezone_unlock(timezone);
        sleep(2);
        ok(!kit_timezone_lock(timezone), "Failed to lock the test zone after the zoneinfo file was deleted");
        ok(!kit_timezone_lock(timezone), "Still failed to lock the test zone after the zoneinfo file was deleted");

        ok(kit_timezone_lock(local_tz), "Able to lock the America/Vancouver timezone");

        SXEA1(system("touch ./test_zoneinfo") == 0, "Failed to create an empty zoneinfo");
        sleep(2);
        ok(!kit_timezone_lock(timezone), "Failed to lock the test zone after when the zoneinfo file was invalid");

        MOCKFAIL_START_TESTS(1, kit_timezone_load);
        ok(!kit_timezone_load("UTC", 0), "Failed to load UTC on simulated failure to add to cache");
        MOCKFAIL_END_TESTS();
    }

    kit_timezone_finalize();

    is(kit_memory_allocations(), start_allocations, "All memory allocations were freed");
    return exit_status();
}
