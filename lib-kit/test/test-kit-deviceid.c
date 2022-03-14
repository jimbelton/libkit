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
    plan_tests(12);

    struct kit_deviceid deviceid = {
        .bytes = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}
    };
    char deviceidstr[] = "0001020304050607";
    const char *str;

    ok(str = kit_deviceid_to_str(&deviceid), "Called kit_deviceid_to_str");
    is_eq(str, deviceidstr, "Printed string matches expected");

    ok(str = kit_deviceid_to_str(NULL), "Called kit_deviceid_to_str with NULL");
    is_eq(str, "0000000000000000", "Printed string for NULL matches expected");

    /* Test creating deviceid from string */
    struct kit_deviceid deviceid2;
    kit_deviceid_from_str(&deviceid2, deviceidstr);
    ok(kit_deviceid_cmp(&deviceid, &deviceid2) == 0, "deviceid from string as expected");
    is_eq(kit_deviceid_to_str(&deviceid2), deviceidstr, "Print of build deviceid as expected");

    char deviceidstrbad[] = "bad string";
    kit_deviceid_from_str(&deviceid2, deviceidstrbad);
    ok(str = kit_deviceid_to_str(&deviceid2), "Called kit_deviceid_to_str");
    is_eq(str, "0000000000000000", "Printed string matches expected");
    ok(kit_deviceid_cmp(&deviceid2, &kit_deviceid_nil) == 0, "Deviceid from bad string as expected");
    ok(kit_deviceid_cmp(&deviceid,  &deviceid2)        != 0, "Deviceids should not match");
    ok(kit_deviceid_cmp(&deviceid,  &deviceid)         == 0, "Deviceid compare for same pointer");
    ok(kit_deviceid_cmp(&deviceid,  NULL)          != 0, "Deviceid compare for NULL pointer");

    return exit_status();
}
