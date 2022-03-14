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

#include "kit.h"

int
main(void)
{
    plan_tests(16);

    struct kit_guid guid = {
        .bytes = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}
    };
    struct kit_md5 md5 = {
        .bytes = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}
    };

    char guidstr[] = "000102030405060708090a0b0c0d0e0f";
    const char *str;

    ok(str = kit_guid_to_str(&guid), "Called kit_guid_to_str");
    is_strncmp(str, guidstr, strlen(str), "Printed string matches expected");

    ok(str = kit_guid_to_str(NULL), "Called kit_guid_to_str with NULL");
    is_strncmp(str, "00000000000000000000000000000000", strlen(str), "Printed string for NULL matches expected");

    ok(str = kit_md5_to_str(NULL), "Called kit_md5_to_str with NULL");
    is_strncmp(str, "00000000000000000000000000000000", strlen(str), "Printed string for NULL matches expected");

    ok(str = kit_md5_to_str(&md5), "Called kit_md5_to_str with NULL");
    is_strncmp(str, guidstr, strlen(str), "Printed string matches expected");

    /* Test creating guid from string */
    struct kit_guid guid2;
    kit_guid_from_str(&guid2, guidstr);
    ok(kit_guid_cmp(&guid, &guid2) == 0, "guid from string as expected");
    is_strncmp(kit_guid_to_str(&guid2), guidstr, KIT_GUID_STR_LEN, "Print of build guid as expected");

    char guidstrbad[] = "bad string";
    kit_guid_from_str(&guid2, guidstrbad);
    ok(str = kit_guid_to_str(&guid2), "Called kit_guid_to_str");
    is_strncmp(str, "00000000000000000000000000000000", strlen(str), "Printed string matches expected");
    ok(kit_guid_cmp(&guid2, &kit_guid_nil) == 0, "guid from bad string as expected");
    ok(kit_guid_cmp(&guid,  &guid2)        != 0, "guids should not match");
    ok(kit_guid_cmp(&guid,  &guid)         == 0, "Guid compare for same pointer");
    ok(kit_guid_cmp(&guid,  NULL)          != 0, "Guid compare for NULL pointer");

    return exit_status();
}
