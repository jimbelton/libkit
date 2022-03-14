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

#include "kit.h"
#include "sxe-log.h"
#include "sxe-util.h"

const struct kit_deviceid kit_deviceid_nil = {
    {0, 0, 0, 0, 0, 0, 0, 0}
};

/**
 * Convert a guid into a string of KIT_DEVICEID_STR_LEN bytes
 */
char *
kit_deviceid_to_buf(const struct kit_deviceid *deviceid, char *buf, size_t size)
{
    SXE_UNUSED_PARAMETER(size);
    SXEA6(size >= KIT_DEVICEID_STR_LEN + 1, "DeviceID buffer must have room for %u hex digits plus 1 for the NUL", KIT_DEVICEID_STR_LEN);
    kit_bin2hex(buf, (const char *)deviceid ? deviceid->bytes : kit_deviceid_nil.bytes, KIT_DEVICEID_SIZE, KIT_BIN2HEX_LOWER);
    return buf;
}

/* Return printable string of a deviceid */
const char *
kit_deviceid_to_str(const struct kit_deviceid *deviceid)
{
    static __thread char str[KIT_DEVICEID_STR_LEN + 1];

    return kit_deviceid_to_buf(deviceid, str, sizeof(str));
}

/* Build a deviceid from a string */
struct kit_deviceid *
kit_deviceid_from_str(struct kit_deviceid *deviceid, const char *str_hex)
{
    SXEE7("(deviceid=%p str_hex=%s)", deviceid, str_hex);

    if (strlen(str_hex) != KIT_DEVICEID_STR_LEN) {
        SXEL3("kit_deviceid_from_str: invalid guid str '%s'", str_hex);
        *deviceid = kit_deviceid_nil;
    }
    else
        kit_hex2bin(deviceid->bytes, str_hex, KIT_DEVICEID_STR_LEN);

    SXER7("return deviceid=%s", kit_deviceid_to_str(deviceid));
    return deviceid;
}

/* Compare two deviceids */
int
kit_deviceid_cmp(const struct kit_deviceid *deviceid1, const struct kit_deviceid *deviceid2)
{
    return deviceid1 == NULL ? (deviceid2 == NULL ? 0 : -1) : deviceid2 == NULL ? 1 : memcmp(deviceid1, deviceid2, sizeof(struct kit_deviceid));
}
