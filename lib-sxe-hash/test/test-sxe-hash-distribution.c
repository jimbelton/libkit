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

#include "sxe-hash-private.h"
#include "sxe-log.h"
#include "sxe-pool.h"
#include "tap.h"
#include "sha1sum.h"

#define HASH_SIZE                    10000
#define NBUCKETS                     (HASH_SIZE + 2)    // HASH_SIZE + SXE_HASH_BUCKET_RESERVED
#define MAX_ALLOWED_PER_BUCKET_INDEX 7

/* Prehashed key and unsigned value
 */
struct sum_value_pair {
    unsigned sum;
    unsigned value;
};

/* Unhashed key only
 */
struct key {
    char hex[10];
};

int
main(void)
{
    struct sum_value_pair * pairs;
    struct key *            keys;
    unsigned                i;
    unsigned                id;
    unsigned                bucket;
    int                     counter[NBUCKETS];
    char                    key[10];

    plan_tests(2);
    sxe_log_set_level(SXE_LOG_LEVEL_DEBUG);

    memset(counter, 0, NBUCKETS * sizeof(int));
    pairs = sxe_hash_new("in-place-hash", HASH_SIZE, sizeof(struct sum_value_pair), 0, sizeof(pairs->sum),
                         SXE_HASH_OPTION_UNLOCKED | SXE_HASH_OPTION_PREHASHED, NULL);

    for (i = 0; i < HASH_SIZE; i++) {
        snprintf(key, sizeof(key), "%08x", i);
        id = sxe_hash_take(pairs);
        pairs[id].sum   = (*sxe_hash_sum)(key, sizeof(key));
        pairs[id].value = i;
        sxe_hash_add(pairs, id);

        bucket = sxe_pool_index_to_state(pairs, id);
        SXEA1(bucket < NBUCKETS, "Bucket index %u (id %u) is out of range", bucket, id);
        counter[bucket]++;

        if (counter[bucket] > MAX_ALLOWED_PER_BUCKET_INDEX) {
            diag("Count at bucket index %u is greater than %u", counter[bucket], MAX_ALLOWED_PER_BUCKET_INDEX);
            break;
        }
    }

    is(i, HASH_SIZE, "%u items pre-hashed and no bucket has more that %u entries", i, MAX_ALLOWED_PER_BUCKET_INDEX);

    memset(counter, 0, NBUCKETS * sizeof(int));
    keys = sxe_hash_new("computed-hash", HASH_SIZE, sizeof(struct key), 0, sizeof(keys->hex), SXE_HASH_OPTION_COMPUTED_HASH,
                        NULL);

    for (i = 0; i < HASH_SIZE; i++)
    {
        id = sxe_hash_take(keys);
        snprintf(keys[id].hex, sizeof(keys[id].hex), "%08x", i);
        sxe_hash_add(keys, id);

        bucket = sxe_pool_index_to_state(keys, id);
        SXEA1(bucket < NBUCKETS, "Bucket index %u is out of range", bucket);
        counter[bucket]++;

        if (counter[bucket] > MAX_ALLOWED_PER_BUCKET_INDEX + 1) {
            diag("Count at bucket index %u is greater than %u", counter[bucket], MAX_ALLOWED_PER_BUCKET_INDEX + 1);
            break;
        }
    }

    is(i, HASH_SIZE, "%u items hashed on demand and no bucket has more that %u entries", i, MAX_ALLOWED_PER_BUCKET_INDEX + 1);
    return exit_status();
}
