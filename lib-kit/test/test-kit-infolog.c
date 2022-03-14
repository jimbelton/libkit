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

#include <string.h>
#include <tap.h>

#include "kit-infolog.h"

int
main(void)
{
    int  output;
    int  stderr_saved;
    char temp[] = "test-infolog.temp.XXXXXX";
    char buf[4096];
    char *bufptr;
    int count;

    plan_tests(4);

    SXEA1(output = mkstemp(temp),                   "Failed to create temporary output file");
    SXEA1((stderr_saved = dup(STDERR_FILENO)) >= 0, "Failed to saved STDERR_FILENO");
    dup2(output, STDERR_FILENO);
    kit_infolog_printf("%2048s", "this should be truncated");
    dup2(stderr_saved, STDERR_FILENO);
    SXEA1(lseek(output, 0, SEEK_SET) == 0, "Failed to rewind output file");

    ok(read(output, buf, sizeof(buf)) >= KIT_INFOLOG_MAX_LINE, "Output at least %u", KIT_INFOLOG_MAX_LINE);
    is_strncmp(&buf[KIT_INFOLOG_MAX_LINE - 4], "...\n", 4,     "... at end of truncated line");
    diag("%s/%s", getcwd(buf, sizeof(buf)), temp);

    dup2(output, STDERR_FILENO);
    for (count = 0; count < 20; count++) {
        kit_infolog_printf("This is an identical line");
    }
    SXEA1(lseek(output, 0, SEEK_SET) == 0, "Failed to rewind output file");

    ok(read(output, buf, sizeof(buf)) > 0, "Read all data from kit_infolog");
    diag("Data1:\n%s", buf);

    bufptr = buf;
    count = 0;
    while ((bufptr = strstr(bufptr, "identical")) != NULL) {
        count++;
        bufptr++;
    }
    is(count, 11, "kit_infolog burst was limited");
    diag("Data2:\n%s", buf);

    unlink(temp);
    return exit_status();
}
