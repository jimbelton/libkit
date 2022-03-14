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

#include "kit.h"

#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define HOSTNAME_LOOKUP_INTERVAL 60    /* Lookup the hostname every 60 seconds from each thread */

const char *
kit_hostname(void)
{
    static __thread char hostname[MAXHOSTNAMELEN];
    static __thread int32_t then = -1;
    int32_t now;

    now = kit_time_cached_sec();
    if (now == 0 || now > then + HOSTNAME_LOOKUP_INTERVAL) {
        if (gethostname(hostname, sizeof(hostname)) != 0)
            snprintf(hostname, sizeof(hostname), "Amnesiac");    /* COVERAGE EXCLUSION: todo: Make gethostname() fail */
        then = now;
    }

    return hostname;
}

const char *
kit_short_hostname(void)
{
    const char *hostname;
    char *dot;

    hostname = kit_hostname();
    if ((dot = strchr(hostname, '.')) != NULL && (dot = strchr(dot + 1, '.')) != NULL)
        *dot = '\0';                                  /* COVERAGE EXCLUSION: todo: Some hostnames contain two '.'s, some don't */

    return hostname;
}
