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

#define _GNU_SOURCE              // For asprintf
#include <assert.h>
#include <string.h>

#include "sxe-alloc.h"
#include "sxe-hash.h"
#include "sxe-hash-private.h"
#include "sxe-log.h"
#include "sha1.h"
#include "tap.h"

#define HASH_SIZE  5

typedef struct TEST_HASH_STRING_ELEMENT
{
    const char * key;
    unsigned     value;
} TEST_HASH_STRING_ELEMENT;

static unsigned
my_hash_string_sum(const void *key, unsigned size)
{
    SXE_UNUSED_PARAMETER(size);

    return (*sxe_hash_sum)(key, strlen(key));
}

static void
test_hash_string(void)
{
    TEST_HASH_STRING_ELEMENT * hash;
    char *                     strings[HASH_SIZE];
    unsigned                   i;
    unsigned                   id;

    hash = sxe_hash_new("test-hash-string", HASH_SIZE, sizeof(TEST_HASH_STRING_ELEMENT),
                        offsetof(TEST_HASH_STRING_ELEMENT, key), SXE_HASH_KEYS_ARE_STRINGS, SXE_HASH_OPTION_COMPUTED_HASH,
                        my_hash_string_sum);

    for (i = 0; i < HASH_SIZE; i++) {
        SXEA1(asprintf(&strings[i], "%u", i + 1) > 0,   "Couldn't asprintf a number to string");
        ok((id = sxe_hash_take(hash)) != SXE_HASH_FULL, "Allocated index %u for element %u of %u", id, i + 1, HASH_SIZE);
        hash[id].key   = strings[i];
        hash[id].value = i + 1;
        sxe_hash_add(hash, id);
    }

    is(sxe_hash_take(hash), SXE_HASH_FULL,                        "Hash table is full");
    ok((id = sxe_hash_look(hash, "1")) != SXE_HASH_KEY_NOT_FOUND, "'1' found in table");
    is(hash[id].value, 1,                                         "It's value is 1");
    is(sxe_hash_look(hash, "11"),         SXE_HASH_KEY_NOT_FOUND, "'11' correctly not found in table");
/*
    is(sxe_hash_set   (hash, SHA1_1ST, length, 1), 4                     , "set keys 1: Inserted at index 4"                   );
    is(sxe_hash_get   (hash, SHA1_1ST, length   ), 1                     , "set keys 1: Got correct value for first sha"       );
    is(sxe_hash_get   (hash, SHA1_2ND, length   ), SXE_HASH_KEY_NOT_FOUND, "set keys 1: Second sha is not in hash pool yet"    );
    is(sxe_hash_get   (hash, SHA1_3RD, length   ), SXE_HASH_KEY_NOT_FOUND, "set keys 1: Third sha is not in hash pool yet"     );

    is(sxe_hash_set   (hash, SHA1_2ND, length, 2), 3                     , "set keys 2: Inserted at index 3"                   );
    is(sxe_hash_get   (hash, SHA1_1ST, length   ), 1                     , "set keys 2: Still got correct value for first sha" );
    is(sxe_hash_get   (hash, SHA1_2ND, length   ), 2                     , "set keys 2: Got correct value for second sha"      );
    is(sxe_hash_get   (hash, SHA1_3RD, length   ), SXE_HASH_KEY_NOT_FOUND, "set keys 2: Third sha is not in hash pool yet"     );

    is(sxe_hash_set   (hash, SHA1_3RD, length, 3), 2                     , "set keys 3: Inserted at index 2"                   );
    is(sxe_hash_get   (hash, SHA1_1ST, length   ), 1                     , "set keys 3: Still got correct value for first sha" );
    is(sxe_hash_get   (hash, SHA1_2ND, length   ), 2                     , "set keys 3: Still got correct value for second sha");
    is(sxe_hash_get   (hash, SHA1_3RD, length   ), 3                     , "set keys 3: Got correct value for third sha"       );

    is(sxe_hash_set   (hash, SHA1_4TH, length, 4), 1                     , "insert too many keys: Inserted at index 1"         );
    is(sxe_hash_set   (hash, SHA1_5TH, length, 5), 0                     , "insert too many keys: Inserted at index 0"         );
    is(sxe_hash_set   (hash, SHA1_6TH, length, 6), SXE_HASH_FULL         , "insert too many keys: Failed to insert key"        );

    is(sxe_hash_remove(hash, SHA1_1ST, length   ), 1                     , "remove keys: Remove returns the correct value"     );
    is(sxe_hash_remove(hash, SHA1_2ND, length   ), 2                     , "remove keys: Remove returns the correct value"     );
    is(sxe_hash_get   (hash, SHA1_1ST, length   ), SXE_HASH_KEY_NOT_FOUND, "remove keys: First sha has been deleted"           );
    is(sxe_hash_get   (hash, SHA1_2ND, length   ), SXE_HASH_KEY_NOT_FOUND, "remove keys: Second sha has been deleted"          );
    is(sxe_hash_get   (hash, SHA1_3RD, length   ), 3                     , "remove keys: Still got correct value for third sha");
*/

    sxe_hash_delete(hash);

    for (i = 0; i < HASH_SIZE; i++)
        free(strings[i]);    // CONVENTION EXCLUSION: allocated by asprintf
}

int
main(void)
{
    plan_tests(10);

    uint64_t start_allocations = sxe_allocations;

    sxe_alloc_diagnostics = true;

#ifndef SXE_DISABLE_XXHASH
    test_hash_string();
#else
    skip(58, "Building without XXH32 support");
#endif

    is(sxe_allocations, start_allocations, "No memory was leaked");
    return exit_status();
}
