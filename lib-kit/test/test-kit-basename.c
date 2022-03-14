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
#include <string.h>
#include <tap.h>

#include "kit.h"

int
main(int argc, char **argv)
{
    const char *path, *res;

    SXE_UNUSED_PARAMETER(argc);
    SXE_UNUSED_PARAMETER(argv);

    plan_tests(3);

    path = "filename";
    res = kit_basename(path);
    is_eq(path, res, "kit_basename() returns same value for a relative filename");

    path = "path/filename";
    res = kit_basename(path);
    ok(!strcmp(res, "filename"), "kit_basename() returns proper base for a relative path");

    path = "/path/path2/filename";
    res = kit_basename(path);
    ok(!strcmp(res, "filename"), "kit_basename() returns proper base for an absolute path");

    return exit_status();
}
