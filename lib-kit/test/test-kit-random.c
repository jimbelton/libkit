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

/*
 * This test mainly for coverage purposes, verifying randomness is impossible.
 * As kit-random is a simplified wrapper for kit-arc4random it is more
 * thoroughly tested in test-kit-arc4random.c.
 */

#include <fcntl.h>
#include <string.h>
#include <tap.h>

#include "kit-random.h"

int
main(void)
{
    int seedfd;

    plan_tests(1);

    ok(seedfd = open("/dev/urandom", O_RDONLY), "Opened seed");

    kit_random_init(seedfd);
    kit_random32();
    kit_random16();
    kit_random8();

    return exit_status();
}
