/* Copyright (c) 2010 Sophos Group.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <assert.h>
#include <stdio.h>
#include <time.h>

#include "kit-mock.h"
#include "kit-timestamp.h"

#define TIMESTAMP_SECONDS_SIZE sizeof("YYYYmmDDHHMMSS")

/* Since these functions will be called from the log package, they should not themselves log */

kit_timestamp_t
kit_timestamp_get(void)
{
    struct timeval tv;

    assert(gettimeofday(&tv, NULL) >= 0);
    return kit_timestamp_from_timeval(&tv);
}

char *
kit_timestamp_to_buf(kit_timestamp_t timestamp, char * buffer, unsigned size)
{
    time_t    unix_time;
    struct tm broken_time;

    assert(size >= KIT_TIMESTAMP_STRING_SIZE);
    unix_time = kit_timestamp_to_unix_time(timestamp);
    assert(gmtime_r(&unix_time, &broken_time) != NULL);
    assert(strftime(buffer, size, "%Y%m%d%H%M%S", &broken_time) != 0);
    buffer[TIMESTAMP_SECONDS_SIZE - 1] = '.';
    snprintf(&buffer[TIMESTAMP_SECONDS_SIZE], size - TIMESTAMP_SECONDS_SIZE, "%06lu",
             (timestamp & ((1 << KIT_TIMESTAMP_BITS_IN_FRACTION) - 1)) * 1000000 / KIT_TIMESTAMP_1_SEC);
    return buffer;
}
