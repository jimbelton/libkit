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

#include <limits.h>
#include <errno.h>
#include <sxe-util.h>
#include <tap.h>

#include "kit.h"

int
main(int argc, char **argv)
{
    const char *str;
    char *endptr;

    SXE_UNUSED_PARAMETER(argc);
    SXE_UNUSED_PARAMETER(argv);

    plan_tests(85);

    diag("Verify normal valid parsing");
    {
        is(kit_strtoul("12345678", NULL, 10), 12345678, "strtoul parses correctly");
        is(errno, 0, "errno is not set");

        is(kit_strtoull("0", NULL, 0), 0, "strtoull correctly parses 0");
        is(errno, 0, "errno is not set");

        is(kit_strtoull("0x0", NULL, 0), 0, "strtoull correctly parses 0x0");
        is(errno, 0, "errno is not set");

        is(kit_strtoul("  \t  0", NULL, 10), 0, "strtoul correctly parses 0 with leading whitespace");
        is(errno, 0, "errno is not set");

        is(kit_strtoul("-1", NULL, 10), ULONG_MAX, "strtoul correctly parses -1");
        is(errno, 0, "errno is not set");

        is(kit_strtoull("-1", NULL, 10), ULLONG_MAX, "strtoull correctly parses -1");
        is(errno, 0, "errno is not set");

        is(kit_strtol("-0", NULL, 10), 0, "strtol correctly parses -0");
        is(errno, 0, "errno is not set");

        is(kit_strtoll("+0", NULL, 10), 0, "strtoll correctly parses +0");
        is(errno, 0, "errno is not set");

        is(kit_strtod("1.234", NULL), 1.234, "strtod parses correctly");
        is(errno, 0, "errno is not set");

        is(kit_strtod("   0.0000", NULL), 0, "strtod correctly parses 0");
        is(errno, 0, "errno is not set");

        str = "0.0a";
        is(kit_strtod(str, &endptr), 0, "strtod correctly parses 0");
        is(errno, 0, "errno is not set");
        is(str + 3, endptr, "strtod of 0.0a sets endpointer to a");
        is(*endptr, 'a', "strtod of 0.0a sets endpointer to a");
    }

    diag("Verify setting errno for invalid parsing");
    {
        str = "AX0BCWWW";
        is(kit_strtoul(str, &endptr, 10), 0, "invalid decimal value returns zero");
        is(errno, EINVAL, "invalid value sets errno");
        is(str, endptr, "invalid parsing sets endptr=str");

        str = "1234";
        is(kit_strtoul(str, &endptr, 10), 1234, "valid parsing to clear errno");
        is(errno, 0, "errno was cleared after valid parsing");
        is(str + 4, endptr, "valid parsing advanced endptr");

        is(kit_strtoull("0xlooojoiji", NULL, 16), 0, "invalid hex value returns zero");
        is(errno, EINVAL, "invalid value sets errno");

        is(kit_strtoul("    a0", NULL, 10), 0, "invalid decimal value after whitespace");
        is(errno, EINVAL, "invalid value sets errno");

        is(kit_strtoul("   +ostte", NULL, 10), 0, "invalid decimal value after whitespace and +");
        is(errno, EINVAL, "invalid value sets errno");

        is(kit_strtod("asd", NULL), 0, "strtod parsing invalid string");
        is(errno, EINVAL, "invalid value sets errno");

        is(kit_strtoul("xx0", NULL, 0), 0, "xx0 doesn't parse as hex");
        is(errno, EINVAL, "invalid value sets errno");

        is(kit_strtoul("  ++0", NULL, 0), 0, "Multiple +/- doesn't parse");
        is(errno, EINVAL, "invalid value sets errno");

        is(kit_strtoul("0xffffffffffffffff", NULL, 16), ULONG_MAX, "Test for upper bound");
        is(errno, 0, "errno is not set");

        is(kit_strtoul("0x1ffffffffffffffff", NULL, 16), ULONG_MAX, "Test for overflow");
        is(errno, ERANGE, "There was an overflow and errno was set to ERANGE");

        is(kit_strtoull("0xffffffffffffffff", NULL, 16), ULLONG_MAX, "Test for upper bound");
        is(errno, 0, "errno is not set");

        is(kit_strtoull("0x1ffffffffffffffff", NULL, 16), ULLONG_MAX, "Test for overflow");
        is(errno, ERANGE, "There was an overflow and errno was set to ERANGE");

        is(kit_strtoull("18446744073709551615", NULL, 10), ULLONG_MAX, "Test for upper bound");
        is(errno, 0, "errno is not set");

        is(kit_strtoull("18446744073709551616", NULL, 10), -1, "Test for overflow");
        is(errno, ERANGE, "There was an overflow and errno was set to ERANGE");
    }

    diag("Verify kit_strtou32 parsing");
    {
        is(kit_strtou32("12345678", NULL, 10), 12345678, "kit_strtou32 parses correctly");
        is(errno, 0, "errno is not set");

        is(kit_strtou32("0", NULL, 0), 0, "kit_strtou32 correctly parses 0");
        is(errno, 0, "errno is not set");

        is(kit_strtou32("0x0", NULL, 0), 0, "kit_strtou32 correctly parses 0x0");
        is(errno, 0, "errno is not set");

        is(kit_strtou32("  \t  0", NULL, 10), 0, "kit_strtou32 correctly parses 0 with leading whitespace");
        is(errno, 0, "errno is not set");

        is(kit_strtou32("4294967295", NULL, 10), UINT32_MAX, "kit_strtou32 correctly parses UINT32_MAX");
        is(errno, 0, "errno is not set");

        is(kit_strtou32("4294967296", NULL, 10), UINT32_MAX, "kit_strtou32 correctly handles overflow");
        is(errno, ERANGE, "errno is set to ERANGE on overflow");

        is(kit_strtou32("-1", NULL, 10), UINT32_MAX, "kit_strtou32 correctly handles negative value");
        is(errno, ERANGE, "errno is set to ERANGE on negative value");

        str = "1234";
        is(kit_strtou32(str, &endptr, 10), 1234, "kit_strtou32 valid parsing to clear errno");
        is(errno, 0, "errno was cleared after valid parsing");
        is(str + 4, endptr, "kit_strtou32 valid parsing advanced endptr");

        str = "4294967296";
        is(kit_strtou32(str, &endptr, 10), UINT32_MAX, "kit_strtou32 overflow parsing");
        is(errno, ERANGE, "errno is set to ERANGE on overflow");
        is(str + 10, endptr, "kit_strtou32 overflow parsing advanced endptr");
    }

    diag("Verify kit_strtou32 parsing with hexadecimal strings");
    {
        is(kit_strtou32("0x1A2B3C4D", NULL, 16), 0x1A2B3C4D, "kit_strtou32 correctly parses hexadecimal 0x1A2B3C4D");
        is(errno, 0, "errno is not set");

        is(kit_strtou32("0xFFFFFFFF", NULL, 16), UINT32_MAX, "kit_strtou32 correctly parses hexadecimal UINT32_MAX");
        is(errno, 0, "errno is not set");

        is(kit_strtou32("0x100000000", NULL, 16), UINT32_MAX, "kit_strtou32 correctly handles hexadecimal overflow");
        is(errno, ERANGE, "errno is set to ERANGE on hexadecimal overflow");

        is(kit_strtou32("0xGHIJKL", NULL, 16), 0, "kit_strtou32 correctly handles invalid hexadecimal");
        is(errno, EINVAL, "errno is set to EINVAL on invalid hexadecimal");

        str = "0x1A2B3C4D";
        is(kit_strtou32(str, &endptr, 16), 0x1A2B3C4D, "kit_strtou32 valid hexadecimal parsing to clear errno");
        is(errno, 0, "errno was cleared after valid hexadecimal parsing");
        is(str + 10, endptr, "kit_strtou32 valid hexadecimal parsing advanced endptr");
    }

    return exit_status();
}
