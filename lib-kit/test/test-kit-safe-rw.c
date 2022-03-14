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

#include <sxe-util.h>
#include <fcntl.h>
#include <tap.h>
#include <errno.h>

#include "kit-safe-rw.h"

int
main(int argc, char **argv)
{
    char ch, buf[70000];
    ssize_t ret;
    unsigned i;
    int p[2];

    SXE_UNUSED_PARAMETER(argc);
    SXE_UNUSED_PARAMETER(argv);

    plan_tests(11);

    for (i = 0, ch = 'a'; i < sizeof(buf) - 1; i++, ch = ch == 'z' ? 'a' : ch + 1)
        buf[i] = ch;
    buf[i] = '\0';

    ok(pipe2(p, O_NONBLOCK) == 0, "Created a non-blocking pipe");
    ret = kit_safe_write(p[1], buf, sizeof(buf), 1);
    ok(ret != sizeof(buf), "Writing to one end of the pipe was truncated");
    is(errno, ETIMEDOUT, "errno is set to ETIMEOUT");
    close(p[0]);
    close(p[1]);

    ret = kit_safe_write(-1, buf, 10, 1);
    ok(ret != 10, "Writing to a dead file descriptor fails");
    is(errno, EBADF, "errno is set to EBADF");

    ok(pipe2(p, O_NONBLOCK) == 0, "Created a non-blocking pipe");
    ret = kit_safe_write(p[1], buf, 10, 1);
    ok(ret == 10, "Wrote 10 bytes to the pipe");
    ret = kit_safe_read(p[0], buf, 10);
    ok(ret == 10, "Read 10 bytes from the pipe");
    close(p[1]);
    ret = kit_safe_read(p[0], buf + 10, 10);
    ok(ret == 0, "Closed the other end, then read 0 bytes from the pipe");
    close(p[0]);

    ret = kit_safe_read(-1, buf, 10);
    ok(ret != 10, "Reading from a dead file descriptor fails");
    is(errno, EBADF, "errno is set to EBADF");

    return exit_status();
}
