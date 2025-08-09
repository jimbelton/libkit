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

#ifndef __SXE_HASH_H__
#define __SXE_HASH_H__

#include <stdint.h>

#include "sxe-pool.h"
#include "sxe-util.h"

#define SXE_HASH_KEY_NOT_FOUND       ~0U
#define SXE_HASH_FULL                ~0U
#define SXE_HASH_SHA1_AS_HEX_LENGTH  (2 * sizeof(struct SXE_HASH_SHA1))

#define SXE_HASH_OPTION_UNLOCKED      0
#define SXE_HASH_OPTION_LOCKED        SXE_BIT_OPTION(0)
#define SXE_HASH_OPTION_PREHASHED     0
#define SXE_HASH_OPTION_COMPUTED_HASH SXE_BIT_OPTION(1)

struct SXE_HASH_SHA1 {
    uint32_t word[5];
};

/* Classic hash: prehashed SHA1 key to unsigned value
 */
typedef struct SXE_HASH_KEY_VALUE_PAIR {
    struct SXE_HASH_SHA1 sha1;
    unsigned             value;
} SXE_HASH_KEY_VALUE_PAIR;

typedef struct SXE_HASH {
    void      * pool;
    unsigned    count;
    unsigned    size;
    unsigned    key_offset;
    unsigned    key_size;
    unsigned    options;
    unsigned (* hash_key)(const void * key, size_t size);
} SXE_HASH;

typedef unsigned (*SXE_HASH_FUNC)(    const void *, size_t);                // Type signature for a 32 bit hash function
typedef void     (*SXE_HASH_128_FUNC)(const void *, size_t, uint8_t *);    // Type signature for a 128 bit hash function

extern uint32_t (*sxe_hash_sum)(const void *key, size_t length);
extern void     (*sxe_hash_128)(const void *key, size_t length, uint8_t *hash_out);

#include "lib-sxe-hash-proto.h"
#endif

