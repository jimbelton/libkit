/* Copyright (c) 2010 Sophos Group.
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

#ifndef __KIT_TIMESTAMP_H__
#define __KIT_TIMESTAMP_H__

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#define KIT_TIMESTAMP_STRING_SIZE               sizeof("YYYYmmDDHHMMSS.uuuuuu")
#define KIT_TIMESTAMP_BITS_IN_FRACTION          20
#define KIT_TIMESTAMP_1_SEC                     ((kit_timestamp_t)(1UL << KIT_TIMESTAMP_BITS_IN_FRACTION))
#define KIT_TIMESTAMP_FROM_UNIX_TIME(unix_time) ((kit_timestamp_t)(unix_time) << KIT_TIMESTAMP_BITS_IN_FRACTION)

/* kit_timestamp_t is a single continuously increasing 64 bit timestamp that replaces SXE_TIME in the libkit library.
 *
 * SXE_TIME was 32bits of seconds since 1970 (time_t) followed by 32bits representing the fraction of the second in nanoseconds.
 * kit_timestamp is 44 bits of the time_t followed by 20 bits needed to represent the fraction of the second in microseconds.
 */
typedef uint64_t kit_timestamp_t;

static inline time_t
kit_timestamp_to_unix_time(kit_timestamp_t timestamp)
{
    return (time_t)(timestamp >> KIT_TIMESTAMP_BITS_IN_FRACTION);
}

static inline kit_timestamp_t
kit_timestamp_from_timeval(const struct timeval *tv)
{
    return ((kit_timestamp_t)tv->tv_sec << KIT_TIMESTAMP_BITS_IN_FRACTION)
         + ((kit_timestamp_t)tv->tv_usec * KIT_TIMESTAMP_1_SEC / 1000000);
}

static inline void
kit_timestamp_to_timeval(kit_timestamp_t timestamp, struct timeval *tv_out)
{
    tv_out->tv_sec  = timestamp >> KIT_TIMESTAMP_BITS_IN_FRACTION;
    tv_out->tv_usec = (timestamp & ((1 << KIT_TIMESTAMP_BITS_IN_FRACTION) - 1)) * 1000000 / KIT_TIMESTAMP_1_SEC;
}

#include "kit-timestamp-proto.h"

#endif
