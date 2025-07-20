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

#include <time.h>
#include <unistd.h>

#include "kit-time.h"

static time_t            start_seconds;         // Starting value for seconds, set on first call to kit_time_get
static __thread uint32_t cached_seconds;        // Cached seconds since start_seconds for fast access
static __thread uint64_t cached_nanoseconds;    // Cached nanoseconds for fast access

const char *
kit_clocktype(void)
{
#ifdef CLOCK_MONOTONIC
    return "monotonic";
#else
    return "timeofday";
#endif
}

/* Calculate and return current seconds and nanoseconds
 */
static void
kit_time_get(uint32_t *seconds_out, uint64_t *nanoseconds_out)
{
    time_t seconds;

#ifdef CLOCK_MONOTONIC
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    seconds          = ts.tv_sec;
    *nanoseconds_out = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);
    seconds          = tv.tv_sec;
    *nanoseconds_out = tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;
#endif

    if (!start_seconds)
        start_seconds = seconds - 1;

    *seconds_out = (uint32_t)(seconds - start_seconds);    // This shouldn't roll over until the box has been up for 136 years
}

/* Return cached nanoseconds
 */
uint64_t
kit_time_cached_nsec(void)
{
    return cached_nanoseconds;
}

/* Return cached seconds since first called +1
 */
uint32_t
kit_time_cached_sec(void)
{
    return cached_seconds;
}

/* Update the cached time
 */
void
kit_time_cached_update(void)
{
    kit_time_get(&cached_seconds, &cached_nanoseconds);
}

/* Return current nanoseconds
 */
uint64_t
kit_time_nsec(void)
{
    uint32_t seconds;
    uint64_t nanoseconds;

    kit_time_get(&seconds, &nanoseconds);
    return nanoseconds;
}

/* Return current seconds since first called +1
 *
 * @note On the first call, the value 1 is returned. Subsequent calls will be >= 1
 */
uint32_t
kit_time_sec(void)
{
    uint32_t seconds;
    uint64_t nanoseconds;

    kit_time_get(&seconds, &nanoseconds);
    return seconds;
}
