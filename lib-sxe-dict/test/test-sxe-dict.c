#include <inttypes.h>
#include <tap.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "kit-test.h"
#include "sxe-dict.h"
#include "sxe-hash.h"

static bool     visit_all = true;
static unsigned visits    = 0;

static bool
my_visit(const void *key, size_t key_size, const void **value, void *user)
{
    const struct sxe_dict *dict = user;

    if (dict->flags & SXE_DICT_FLAG_KEYS_HASHED)
        is(*(const uint64_t *)key, sxe_hash_64("one", 3),          "Correct hash");
    else
        is_eq(key, "longname",                                     "Correct key");

    is(key_size, 8,                                                "Correct size");
    is(*value, dict->flags & SXE_DICT_FLAG_KEYS_HASHED ? 1 : 1026, "Correct value");
    visits += 1;
    return visit_all;
}

int
main(void) {
    struct sxe_dict  dictator[1];
    struct sxe_dict *dic;
    const void     **value_ptr;
    const void      *value;

    kit_test_plan(40);
    // KIT_ALLOC_SET_LOG(1);    // Turn off when done

    MOCKFAIL_START_TESTS(1, sxe_dict_new);
    ok(!sxe_dict_new(0), "sxe_dict_new failed to allocate");
    MOCKFAIL_END_TESTS();

    dic = sxe_dict_new(0);
    is(dic->table, NULL,                                "Empty dictionary has no table");
    ok((value = sxe_dict_find(dic, "ABC",  3)) == NULL, "Before adding ABC, expected NULL, found: %"PRIiPTR, (intptr_t)value);

    MOCKFAIL_START_TESTS(1, sxe_dict_add);
    ok(!sxe_dict_add(dic, "ABC", 3), "sxe_dict_add failed to allocate initial table");
    MOCKFAIL_END_TESTS();

    value_ptr = sxe_dict_add(dic, "ABC", 3);
    is(*value_ptr, NULL, "New entry should not have a value");
    *value_ptr = (const void *)100;
    is(dic->size, 1,    "Size after 1 insert is 1");

    MOCKFAIL_START_TESTS(1, sxe_dict_resize);
    ok(!sxe_dict_add(dic, "DE", 2), "sxe_dict_add failed to expand table");
    MOCKFAIL_END_TESTS();

    value_ptr = sxe_dict_add(dic, "DQ", 2);
    *value_ptr = (const void *)200;
    is(dic->size, 2, "Size after 2 inserts is 2");

    value_ptr = sxe_dict_add(dic, "FGHI", 4);
    *value_ptr = (const void *)300;

    /* The following is because after doubling to 2, 1 and 2 ended up in bucket 0, but 3 ends up in bucket 1.
     */
    is(dic->size, 2, "Size after 3 inserts is 2");

    ok((value = sxe_dict_find(dic, "ABC",  3)), "ABC found");
    is(value, 100,                              "It's value is 100");
    ok((value = sxe_dict_find(dic, "DQ",   0)), "DQ found (and test passing 0 length to use strlen)");
    is(value, 200,                              "It's value is 200");
    ok((value = sxe_dict_find(dic, "FGHI", 4)), "FGHI found");
    is(value, 300,                              "It's value is 300");

    /* Make sure adding a key that's already in the table, a new entry is not added.
     */
    value_ptr = sxe_dict_add(dic, "ABC", 0);
    is(*value_ptr, (const void *)100, "Got expected value for 'ABC'");

    sxe_dict_free(dic);

    ok(sxe_dict_init(dictator, 2, 100, 2, SXE_DICT_FLAG_KEYS_NOCOPY), "Constructed a dictionary that stores keys by reference");
    value_ptr = sxe_dict_add(dictator, "longname", 0);
    *value_ptr = (const void *)1026;
    is(sxe_dict_find(dictator, "different bucket", 0), NULL, "Looked up a non-existent key that lands in a different bucket");
    is(sxe_dict_find(dictator, "same bucket", 0),      NULL, "Looked up a non-existent key that lands in the used bucket");
    ok(sxe_dict_walk(dictator, my_visit, dictator),          "Visited all nodes");
    is(visits, 1,                                            "Visited all (1) nodes");

    visit_all = false;    // From now on, short circuit walks after visiting one entry
    
    visits = 0;
    sxe_dict_forEach(dictator, my_visit, dictator);    // Make sure old name is supported for backward compatibility
    is(visits, 1, "Visited 1 node");
    sxe_dict_fini(dictator);
    
    ok(sxe_dict_init(dictator, 0, 100, 2, SXE_DICT_FLAG_KEYS_STRING), "Constructed a dictionary that stores keys as strings");
    value_ptr  = sxe_dict_add(dictator, "longname", 0);
    *value_ptr = (const void *)1026;
    value_ptr  = sxe_dict_add(dictator, "short", 0);
    *value_ptr = (const void *)1027;
    is(sxe_dict_find(dictator, "longname", 8), 1026,                  "Found 'longname'");
    visits = 0;
    ok(!sxe_dict_walk(dictator, my_visit, dictator),                  "Walk was ended early");
    is(visits, 1,                                                     "Visited 1 node and short circuited");
    sxe_dict_fini(dictator);

    ok(sxe_dict_init(dictator, 0, 100, 2, SXE_DICT_FLAG_KEYS_HASHED), "Constructed a dictionary that stores keys as hash sums");
    value_ptr = sxe_dict_add(dictator, "one", 0);
    *value_ptr = (const void *)1;
    value_ptr = sxe_dict_add(dictator, "two", 0);
    *value_ptr = (const void *)2;
    is(sxe_dict_find(dictator, "one", 3), 1,                          "Found 'one'");
    sxe_dict_walk(dictator, my_visit, dictator);
    sxe_dict_fini(dictator);

    sxe_dict_free(NULL);
    kit_test_exit(0);
}
