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

#include "tzcode.h"

int
main(void)
{
    struct tm  my_tm, local_tm;
    timezone_t my_tz;
    time_t     my_time, local_time;
    char       buf1[TZ_ASCTIME_BUF_SIZE], buf2[TZ_ASCTIME_BUF_SIZE];

    plan_tests(9);
    tzset();          // Set the timezone using the TZ variable passed by make, which must match the one allocated below

    diag("Test the tz_tzalloc, tz_tzfree, tz_localtime_rz and tz_asctime_r");
    {
        isnt(my_time = time(NULL), (time_t)-1,               "Got the time");
        is(ctime_r(&my_time, buf1), buf1,                    "Converted it to a string using libc");
        ok(my_tz = tz_tzalloc(":America/Vancouver"),         "Got the timezone definition using lib-tzcode");
        is(tz_localtime_rz(my_tz, &my_time, &my_tm), &my_tm, "Got the broken down time using the timezone definition");
        is(tz_asctime_r(&my_tm, buf2), buf2,                 "Converted it to a string using lib-tzcode");
        is_eq(buf1, buf2,                                    "String representations match");

        my_tm.tm_isdst  = 0;                                   // No daylight savings time for UTC
        isnt(local_time = tz_mktime_z(NULL, &my_tm), (time_t)-1, "Converted local time using the UTC timezone definition");
        is(gmtime_r(&local_time, &local_tm), &local_tm,          "Converted back to broken down time using libc");
        is(my_tm.tm_hour, local_tm.tm_hour,                      "The hours are the same");

        tz_tzfree(my_tz);
    }

    return exit_status();
}
