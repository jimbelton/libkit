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

#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <tap.h>

#include "kit-arc4random.h"

int
main(void)
{
    int p[2], status;
    uint8_t buf[100];
    char ch;

    plan_tests(8);

    kit_arc4random_init(open("/dev/urandom", O_RDONLY));
    ok(!kit_arc4random_internals_initialized(), "kit_arc4random internals are not initialized after kit_arc4random_init()");
    kit_arc4random();
    ok(kit_arc4random_internals_initialized(), "kit_arc4random internals are initialized after kit_arc4random() use");
    kit_arc4random_stir();
    kit_arc4random_buf(buf, 100);
    ok(kit_arc4random_uniform(5) < 5, "kit_arc4random_uniform returned valid random number");

    ok(pipe(p) == 0, "Created an unnamed pipe");
    if (fork()) {
        close(p[1]);
        is(read(p[0], &ch, 1), 1, "Read the first result");
        ok(!ch, "kit_arc4random child internals are not initialized after fork()");
        is(read(p[0], &ch, 1), 1, "Read the second result");
        ok(ch, "kit_arc4random child internals are initialized after kit_arc4random() use");
        close(p[0]);
        wait(&status);
    } else {
        close(p[0]);
        ch = kit_arc4random_internals_initialized();
        if (write(p[1], &ch, 1) != 1)
            diag("Write failed");
        kit_arc4random();
        ch = kit_arc4random_internals_initialized();
        if (write(p[1], &ch, 1) != 1)
            diag("Write failed");
        close(p[1]);
    }

    return exit_status();
}
