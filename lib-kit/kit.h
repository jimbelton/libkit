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

/* Shared header for kit files. This is intended to be generic tool kit, not specific to the problem being solved. */

#ifndef KIT_H
#define KIT_H

#if __FreeBSD__
#include <sys/types.h>
#endif

#include <netinet/in.h>
#include <resolv.h>
#include <stdbool.h>
#include <string.h>

#include "kit-sortedarray.h"    // For backward compatibility
#include "kit-time.h"           // For backward compatibility

#define KIT_UNSIGNED_MAX (~0U)

#define KIT_BASE_DECODE_DEFAULT         0x00
#define KIT_BASE_DECODE_SKIP_WHITESPACE 0x01

#define KIT_MD5_SIZE                    16
#define KIT_MD5_STR_LEN                 (2 * KIT_MD5_SIZE)

#define KIT_GUID_SIZE                   KIT_MD5_SIZE
#define KIT_GUID_STR_LEN                KIT_MD5_STR_LEN

#define KIT_DEVICEID_SIZE               8
#define KIT_DEVICEID_STR_LEN            (2 * KIT_DEVICEID_SIZE)

#define KIT_UDP_DELAY                   0x01    // Get the delay (RQT) in msec for all packets received on this socket
#define KIT_UDP_TTLTOS                  0x02    // Get the TTL and TOS fields for all packets received on this socket
#define KIT_UDP_DST_ADDR                0x04    // Get the original destination address for packets received on this socket
#define KIT_UDP_TRANSPARENT             0x08    // Set the UDP socket for use with a transparent proxy

enum kit_bin2hex_fmt {
    KIT_BIN2HEX_UPPER,
    KIT_BIN2HEX_LOWER,
};

struct kit_guid {
    uint8_t  bytes[KIT_GUID_SIZE];
};

struct kit_md5 {
    uint8_t  bytes[KIT_MD5_SIZE];
};

struct kit_deviceid {
    uint8_t  bytes[KIT_DEVICEID_SIZE];
};

struct kit_udp_ttltos {
    uint8_t ttl;
    uint8_t tos;
};

static inline struct kit_udp_ttltos *
kit_udp_ttltos_init(struct kit_udp_ttltos *self)
{
    self->ttl = 0;
    self->tos = 0;
    return self;
}

typedef void *(*kit_realloc_ptr_t)(void *, size_t);

extern const struct kit_guid kit_guid_nil;    // The nil GUID (All bytes are 0)
extern const struct kit_deviceid kit_deviceid_nil;    // The nil DEVICEID (All bytes are 0)

/* Check if a device ID is null (all bytes are zero) */
static inline bool
kit_deviceid_is_null(const struct kit_deviceid *deviceid)
{
    return memcmp(deviceid, &kit_deviceid_nil, sizeof(struct kit_deviceid)) == 0;
}

/* Check if a GUID is null (all bytes are zero) */
static inline bool
kit_guid_is_null(const struct kit_guid *guid)
{
   return memcmp(guid, &kit_guid_nil, sizeof(struct kit_guid)) == 0;
}

#include "kit-base-encode-proto.h"
#include "kit-basename-proto.h"
#include "kit-guid-proto.h"
#include "kit-deviceid-proto.h"
#include "kit-hostname-proto.h"
#include "kit-strto-proto.h"
#include "kit-udp-proto.h"

#endif
