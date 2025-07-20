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

/* This is the minimal public API. It must be in sync with private.h */

#include <time.h>

#define TZ_ASCTIME_BUF_SIZE 26    // Must be the same as STD_ASCTIME_BUF_SIZE in asctime.c

typedef void *timezone_t;

/* Patched in localtime.c to allow malloc and free to be overriden
 */
extern void *(*tz_malloc)(size_t size);
extern void  (*tz_free)(void *ptr);

/* Prototypes of useful functions. tz_asctime_r is provided for testing. Note the tz_ prefixing to prevent collisions.
 */
char      *tz_asctime_r(struct tm const *restrict timeptr, char *restrict buf);
struct tm *tz_localtime_rz(timezone_t restrict, time_t const *restrict, struct tm *restrict);
time_t     tz_mktime_z(timezone_t restrict, struct tm *restrict);
timezone_t tz_tzalloc(char const *);
void       tz_tzfree(timezone_t);
