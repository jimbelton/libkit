/* Copyright (c) 2016 by Jim Belton
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

#include <string.h>
#include <tap.h>

#include "kit-process.h"

int
main(int argc, char ** argv)
{
    intptr_t child_pid;
    intptr_t dead_pid;
    int      status;

    if (argc != 2 || strcmp(argv[1], "child process") != 0)
        plan_tests(2);

    /* If this is the spawned process, exit with the low byte of the PID as the status
     */
    if (argc > 1) {
        int pid = getpid();
        return pid & 0xFF;
    }

    if ((child_pid = kit_spawnl(P_NOWAIT, argv[0], argv[0], "child process", NULL)) < 0) {
        fail("Failed to spawn a child process");
    }
    else if ((dead_pid = kit_cwait(&status, child_pid, WAIT_CHILD)) < 0) {
        fail("Expect cwait to return child pid %p, got %p", (void *)child_pid, (void *)dead_pid);
    }
    else {
        fprintf(stderr, "Status %x\n", status);
        is(status >> 8, child_pid & 0xFF, "Expected the high byte of the status to be the low byte of child pid");
    }

    if ((child_pid = kit_spawnl(P_NOWAIT, "gorporola", "gorporola", "child process", NULL)) < 0) {
        fail("Failed to spawn a child process");
    }
    else if ((dead_pid = kit_cwait(&status, child_pid, WAIT_CHILD)) < 0) {
        fail("Expect cwait to return child pid %p, got %p", (void *)child_pid, (void *)dead_pid);
    }
    else {
        fprintf(stderr, "Status %x\n", status);
        is(status >> 8, 1, "Expected the high byte of the status to be 1 (exit value on error from kit_spawnl)");
    }

    return exit_status();
}
