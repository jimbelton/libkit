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

#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <tap.h>

#include "sxe-hash.h"
#include "sxe-log.h"
#include "sxe-pool.h"

#define HASH_SIZE                    10000
#define NBUCKETS                     (HASH_SIZE + 2) /* HASH_SIZE + SXE_HASH_BUCKET_RESERVED */
#define MAX_ALLOWED_PER_BUCKET_INDEX 7

int
main(void)
{
    SXE_HASH    * hash;
    unsigned      i;
    unsigned      id;
    unsigned      bucket;
    int           counter[NBUCKETS];

    plan_tests(1);
    sxe_log_set_level(SXE_LOG_LEVEL_DEBUG);

    memset(counter, 0, NBUCKETS * sizeof(int));
    hash = sxe_hash_new_plus("xxh32", HASH_SIZE, sizeof(SXE_HASH), 0, 8, SXE_HASH_OPTION_COMPUTED_HASH);

    for (i = 0; i < HASH_SIZE; i++)
    {
        id = sxe_hash_take(hash);
        snprintf((char *)&hash[id], 9, "%08x", i);
        sxe_hash_add(hash, id);

        bucket = sxe_pool_index_to_state(hash, id);
        SXEA1(bucket < NBUCKETS, "Bucket index %u is out of range", bucket);
        counter[bucket]++;

        if (counter[bucket] > MAX_ALLOWED_PER_BUCKET_INDEX + 1) {
            diag("Count at bucket index %u is greater than %u", counter[bucket], MAX_ALLOWED_PER_BUCKET_INDEX + 1);
            break;
        }
    }

    is(i, HASH_SIZE, "%u items xxh32 hashed and no bucket has more that %u entries", i, MAX_ALLOWED_PER_BUCKET_INDEX + 1);
    sxe_hash_delete(hash);
    return exit_status();
}
