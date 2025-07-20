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

#include <errno.h>
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "kit.h"
#include "kit-mockfail.h"
#include "kit-time.h"

#define HOSTNAME_LOOKUP_INTERVAL 60    /* Lookup the hostname every 60 seconds from each thread */

/**
 * Get the hostname (efficiently if the cuurent thread updates the cached kit-time with kit_time_cached_update)
 *
 * @note Once a non-zero value is returned by kit_time_cached_sec, this function will not update the hostname again until the
 *       cached seconds value increases by 60s. This means less system calls are made but changes will take a minute to notice.
 */
const char *
kit_hostname(void)
{
    static __thread char     hostname[MAXHOSTNAMELEN];
    static __thread uint32_t then = 0;
    uint32_t                 now;

    now = kit_time_cached_sec();

    if (then == 0 || now > then + HOSTNAME_LOOKUP_INTERVAL) {
        if (MOCKERROR(kit_hostname, -1, EFAULT, gethostname(hostname, sizeof(hostname))) != 0)
            snprintf(hostname, sizeof(hostname), "Amnesiac");

        then = now;
    }

    return hostname;
}
