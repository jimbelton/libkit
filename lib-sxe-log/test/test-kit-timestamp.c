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

#include <inttypes.h>
#include <string.h>
#include <tap.h>
#include <time.h>

#include "kit-mock.h"
#include "kit-timestamp.h"

static struct timeval test_tv;

static int
test_mock_gettimeofday(struct timeval * __restrict tv, __timezone_ptr_t tz)
{
    (void)tz;
    memcpy(tv, &test_tv, sizeof(test_tv));
    return 0;
}

static void
test_timeval_conversions(unsigned tv_sec, unsigned tv_usec, unsigned tv_sec_expected, unsigned tv_usec_expected)
{
    kit_timestamp_t timestamp;

    test_tv.tv_sec  = tv_sec;
    test_tv.tv_usec = tv_usec;
    timestamp       = kit_timestamp_from_timeval(&test_tv);
    kit_timestamp_to_timeval(timestamp, &test_tv);
    is(test_tv.tv_sec , tv_sec_expected , "Convert to and from timeval: timeval seconds is %u",  tv_sec );
    is(test_tv.tv_usec, tv_usec_expected, "Convert to and from timeval: timeval useconds is %u", tv_usec);
}

int
main(void)
{
    time_t          expected;
    time_t          actual;
    kit_timestamp_t timestamp;
    char            buffer[KIT_TIMESTAMP_STRING_SIZE];

    plan_tests(21);

    time(&expected);
    actual = kit_timestamp_to_unix_time(kit_timestamp_get());
    ok((actual == expected) || (actual == expected + 1), "Actual time (%lu) is as expected (%lu)", actual, expected);

    is_eq(kit_timestamp_to_buf(0xFFFFFUL, buffer, sizeof(buffer)), "19700101000000.999999",
          "Formatted kit timestamp as expected (%s)", buffer);
    is_eq(kit_timestamp_to_buf(0x7FFFFUL, buffer, sizeof(buffer)), "19700101000000.499999",
          "Formatted kit timestamp as expected (%s)", buffer);
    kit_timestamp_to_buf(987654UL << KIT_TIMESTAMP_BITS_IN_FRACTION, buffer, sizeof(buffer));
    is_eq(&buffer[sizeof("YYYYmmDDHHMMSS") - 1], ".000000",
          "Zeroed fractional part formatted as expected (string=%s, timestamp=%" PRIx64 ")", buffer,
          987654UL << KIT_TIMESTAMP_BITS_IN_FRACTION);

    MOCK_SET_HOOK(gettimeofday, test_mock_gettimeofday);

    test_tv.tv_sec  = 0;
    test_tv.tv_usec = 0;
    timestamp       = kit_timestamp_get();
    is(timestamp, 0, "tv(0,0) -> timestamp 0");

    test_tv.tv_usec = 999999;
    timestamp       = kit_timestamp_get();
    ok(KIT_TIMESTAMP_1_SEC - 1 - timestamp < KIT_TIMESTAMP_1_SEC,
       "tv(0,999999) -> timestamp ~%" PRIu64 " (got %" PRIu64 ", epsilon %" PRIu64 ")",
       KIT_TIMESTAMP_1_SEC - 1, timestamp, KIT_TIMESTAMP_1_SEC);

    test_tv.tv_sec  = 1;
    test_tv.tv_usec = 0;
    timestamp       = kit_timestamp_get();
    is(timestamp, KIT_TIMESTAMP_1_SEC, "tv(1, 0) -> timestamp 2^20");

    test_timeval_conversions(1, 500000, 1, 500000);
    test_timeval_conversions(2,      0, 2,      0);
    test_timeval_conversions(3,      1, 3,      0); /* rounds down; challenge for you, figure out why! */
    test_timeval_conversions(4, 999999, 4, 999998); /* rounds down; challenge for you, figure out why! */
    test_timeval_conversions(5,      5, 5,      4); /* rounds down; challenge for you, figure out why! */
    test_timeval_conversions(6,   4294, 6,   4293); /* rounds down; challenge for you, figure out why! */
    test_timeval_conversions(7,   4295, 7,   4294); /* rounds down; challenge for you, figure out why! */

    return exit_status();
}
