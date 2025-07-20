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

#include <stdio.h>
#include <unistd.h>    /* For snprintf on Windows */
#include <inttypes.h>

#include "kit-timestamp.h"
#include "sxe-hash.h"

#define KEY_SIZE 8

int
main(int argc, char * argv[])
{
    struct SXE_HASH_SHA1 *hash;
    kit_timestamp_t       start_time;
    unsigned              i;
    unsigned              id;
    char                  key[KEY_SIZE + 1];

    (void)argv;

    if (argc == 1) {
        fprintf(stderr, "To benchmark hash, run: build-linux-32-release/test-sxe-hash-bench with options:\n");
        fprintf(stderr, "    -x = xxh32 hash of 8 byte keys\n");
        exit(0);
    }

    hash = sxe_hash_new_plus("testhash", 1 << 16, KEY_SIZE, 0, 8, SXE_HASH_OPTION_UNLOCKED | SXE_HASH_OPTION_COMPUTED_HASH);
    start_time = kit_timestamp_get();

    for (i = 0; i < 10000; i++) {
        id = sxe_hash_take(hash);
        snprintf((char *)&hash[id], sizeof(key), "%08x", i);
        sxe_hash_add(hash, id);
    }

    printf("xx32h: hashed %lu 8 byte keys per second\n", KIT_TIMESTAMP_FROM_UNIX_TIME(i) / (kit_timestamp_get() - start_time));
    return 0;
}
