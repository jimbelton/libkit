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
#include "kit.h"

/* Cache time for fast access */
static __thread uint32_t cached_seconds;
static __thread uint64_t cached_nanoseconds;

const char *
kit_clocktype(void)
{
#ifdef CLOCK_MONOTONIC
    return "monotonic";
#else
    return "timeofday";
#endif
}

/* Calculate and return current seconds and nanoseconds */
static void
kit_time_get(uint32_t *seconds, uint64_t *nanoseconds)
{
#ifdef CLOCK_MONOTONIC
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return;    /* COVERAGE EXCLUSION: todo: test clock_gettime() failure... can it fail? */

    if (seconds != NULL)
        *seconds = ts.tv_sec;

    if (nanoseconds != NULL)
        *nanoseconds = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#else
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0)
        return;

    if (seconds != NULL)
        *seconds = tv.tv_sec;

    if (nanoseconds != NULL)
        *nanoseconds = tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;
#endif
}

uint64_t
kit_time_cached_nsec(void)
{
    return cached_nanoseconds;
}

uint32_t
kit_time_cached_sec(void)
{
    return cached_seconds;
}

/* Update the cached time values */
void
kit_time_cached_update(void)
{
    kit_time_get(&cached_seconds, &cached_nanoseconds);
}

/* Return current nanoseconds */
uint64_t
kit_time_nsec(void)
{
    uint64_t nanoseconds = 0;

    kit_time_get(NULL, &nanoseconds);

    return nanoseconds;
}

/* Return current seconds */
uint32_t
kit_time_sec(void)
{
    uint32_t seconds = 0;

    kit_time_get(&seconds, NULL);

    return seconds;
}
