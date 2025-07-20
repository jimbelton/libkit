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

#include <assert.h>
#include <string.h>
#include <tap.h>

#include "kit-alloc.h"
#include "sxe-hash.h"
#include "sxe-hash-private.h"
#include "sxe-log.h"


#define HASH_SIZE  5

// These two sha_keys will map to the same int_key
#define SHA1_1ST "2ce679528627da7780f8a4fec07cb34f902468a0"
#define SHA1_2ND "2ce679528627da7780f8a4fec07cb34f902464b8"

#define SHA1_3RD "2ce679528627da7780f8a4fec07cb34f902464b7"
#define SHA1_4TH "2ce679528627da7780f8a4fec07cb34f902464b6"
#define SHA1_5TH "2CE679528627DA7780F8A4FEC07CB34F902464B5"    /* Support capital hex digits too */
#define SHA1_6TH "2CE679528627DA7780F8A4FEC07CB34F902464B4"

#define SHA1_BAD "2CE679528627DA7780F8A4FEC07CB34F902464B"

static const char   * strings[]      = {"one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"};
static const unsigned strings_number = sizeof(strings) / sizeof(strings[0]);

static void
test_hash_sha1(void)
{
    const unsigned length = SXE_HASH_SHA1_AS_HEX_LENGTH;
    SXE_HASH     * hash;

    hash = sxe_hash_new("test-hash", HASH_SIZE);

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

    is(sxe_hash_remove(hash, SHA1_1ST, length   ), SXE_HASH_KEY_NOT_FOUND, "remove non-existent key: returns expected value"   );

    /* for coverage */
    is(sxe_hash_get(hash, SHA1_BAD, length   ), SXE_HASH_KEY_NOT_FOUND,    "get    bad key: returns expected value"   );
    is(sxe_hash_remove(hash, SHA1_BAD, length   ), SXE_HASH_KEY_NOT_FOUND, "remove bad key: returns expected value"   );

    sxe_hash_delete(hash); /* for coverage */
}

typedef struct TEST_HASH_STRING_PAYLOAD
{
    char value[8];
} TEST_HASH_STRING_PAYLOAD;

static void
test_hash_variable_data(void)
{
    TEST_HASH_STRING_PAYLOAD * hash;
    unsigned                   i;
    unsigned                   id;

    hash = sxe_hash_new_plus("test_hash_variable_data", 10, sizeof(TEST_HASH_STRING_PAYLOAD), 0,
                             sizeof(TEST_HASH_STRING_PAYLOAD),
                             SXE_HASH_OPTION_UNLOCKED | SXE_HASH_OPTION_COMPUTED_HASH);

    for (i = 0; i < strings_number; i++) {
        ok((id = sxe_hash_take(hash)) != SXE_HASH_FULL, "Allocated index %u for element %u of %u", id, i + 1, strings_number);
        strlcpy(hash[id].value, strings[i], sizeof(hash[id].value));
        sxe_hash_add(hash, id);
    }

    is(sxe_hash_take(hash), SXE_HASH_FULL,                           "Hash table is full");
    ok(sxe_hash_look(hash, "one\0\0\0\0") != SXE_HASH_KEY_NOT_FOUND, "'one\\0\\0\\0\\0\\0' found in table");

    /* One of these must search a non-empty bucket to ensure full coverage
     */
    is(sxe_hash_look(hash, "eleven\0"), SXE_HASH_KEY_NOT_FOUND, "'eleven\\0\\0' correctly not found in table");
    is(sxe_hash_look(hash, "twelve\0"), SXE_HASH_KEY_NOT_FOUND, "'twelve\\0\\0' correctly not found in table");

    sxe_hash_delete(hash); /* for coverage */
}

static void
test_hash_sha1_reconstruct(void)
{
    const unsigned length = SXE_HASH_SHA1_AS_HEX_LENGTH;
    SXE_HASH     * hash;

    hash = sxe_hash_new("test-hash-reconstruct", HASH_SIZE);

    is(sxe_hash_set   (hash, SHA1_1ST, length, 1), 4                     , "set keys 1: Inserted at index 4"                   );
    is(sxe_hash_set   (hash, SHA1_2ND, length, 2), 3                     , "set keys 2: Inserted at index 3"                   );
    is(sxe_hash_set   (hash, SHA1_3RD, length, 3), 2                     , "set keys 3: Inserted at index 2"                   );
    is(sxe_hash_set   (hash, SHA1_4TH, length, 4), 1                     , "set keys 4: Inserted at index 1"                   );
    is(sxe_hash_set   (hash, SHA1_5TH, length, 5), 0                     , "set keys 5: Inserted at index 0"                   );

    /* Reconstruct the hash, reuse the memory block in fact */
    sxe_hash_reconstruct(hash);

    /* Insert only two entries first */
    is(sxe_hash_set   (hash, SHA1_2ND, length, 2), 4                     , "set keys 2: Inserted at index 4"                   );
    is(sxe_hash_set   (hash, SHA1_4TH, length, 4), 3                     , "set keys 4: Inserted at index 3"                   );

    is(sxe_hash_get   (hash, SHA1_1ST, length   ), SXE_HASH_KEY_NOT_FOUND, "reconstruct keys: First sha not available yet"     );
    is(sxe_hash_get   (hash, SHA1_3RD, length   ), SXE_HASH_KEY_NOT_FOUND, "reconstruct keys: Third sha not available yet"     );
    is(sxe_hash_get   (hash, SHA1_5TH, length   ), SXE_HASH_KEY_NOT_FOUND, "reconstruct keys: Fifth sha not available yet"     );
    is(sxe_hash_get   (hash, SHA1_2ND, length   ), 2                     , "reconstruct keys: Second sha is correct"           );
    is(sxe_hash_get   (hash, SHA1_4TH, length   ), 4                     , "reconstruct keys: Fourth sha is correct"           );

    /* Insert the others */
    is(sxe_hash_set   (hash, SHA1_3RD, length, 7), 2                     , "set keys 3: Inserted at index 2"                   );
    is(sxe_hash_set   (hash, SHA1_5TH, length, 8), 1                     , "set keys 5: Inserted at index 1"                   );
    is(sxe_hash_set   (hash, SHA1_1ST, length, 9), 0                     , "set keys 1: Inserted at index 0"                   );
    is(sxe_hash_set   (hash, SHA1_6TH, length, 6), SXE_HASH_FULL         , "insert too many keys: Failed to insert key"        );

    is(sxe_hash_get   (hash, SHA1_3RD, length   ), 7                     , "reconstruct keys: Third sha is correct"            );
    is(sxe_hash_get   (hash, SHA1_1ST, length   ), 9                     , "reconstruct keys: First sha is correct"            );
    is(sxe_hash_get   (hash, SHA1_5TH, length   ), 8                     , "reconstruct keys: Fifth sha is correct"            );

    sxe_hash_delete(hash); /* for coverage */
}

static unsigned
my_hash_sum(const void *key, size_t length)
{
    unsigned result = 0;

    memcpy(&result, key, length > sizeof(result) ?  sizeof(result) : length);
    return result;
}

static void
test_hash_override_sum(void)
{
    unsigned default_sum = sxe_hash_sum("Hello, world.", 0);

    sxe_hash_override_sum(my_hash_sum);
    ok(default_sum != sxe_hash_sum("Hello, world.", 0), "After overriding to my_hash_sum, sum is different");
    sxe_hash_use_xxh32();
    is(sxe_hash_sum("Hello, world.", 0), default_sum,   "After setting back to xxh32, sum is the same");
}

int
main(void)
{
    plan_tests(59);

    uint64_t start_allocations = kit_memory_allocations();
    // KIT_ALLOC_SET_LOG(1);    // Turn off when done

    test_hash_sha1();
    test_hash_variable_data();
    test_hash_sha1_reconstruct();
    test_hash_override_sum();

    is(kit_memory_allocations(), start_allocations, "No memory was leaked");
    return exit_status();
}
