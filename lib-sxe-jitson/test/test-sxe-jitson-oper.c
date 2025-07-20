/* Test the sxe-jitson operator extension
 */

#include <string.h>
#include <tap.h>

#include "kit-mockfail.h"
#include "sxe-jitson-oper.h"
#include "sxe-jitson-in.h"
#include "sxe-jitson-intersect.h"
#include "sxe-log.h"
#include "sxe-thread.h"

static unsigned len_op = ~0U;

static struct sxe_jitson jitson_true = {
    .type    = SXE_JITSON_TYPE_BOOL,
    .boolean = true
};

static struct sxe_jitson jitson_false = {
    .type    = SXE_JITSON_TYPE_BOOL,
    .boolean = false
};

static struct sxe_jitson jitson_null = {
    .type = SXE_JITSON_TYPE_NULL
};

static const struct sxe_jitson *
and_op_default(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    if (!sxe_jitson_test(left))
        return &jitson_false;

    return sxe_jitson_test(right) ? &jitson_true : &jitson_false;
}

static const struct sxe_jitson *
len_op_default(const struct sxe_jitson *arg)
{
    if (!sxe_jitson_supports_len(arg)) {
        SXEL2(": Type %s doesn't support operator '%s'", sxe_jitson_get_type_as_str(arg), sxe_jitson_oper_get_name(len_op));
        errno = EOPNOTSUPP;
        return NULL;
    }

    return sxe_jitson_create_uint(sxe_jitson_len(arg));
}

static const struct sxe_jitson *
in_op_string_caseless(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    if (sxe_jitson_get_type(left) != SXE_JITSON_TYPE_STRING) {
        SXEL2(": Can't look for a %s in a string", sxe_jitson_get_type_as_str(left));
        errno = EOPNOTSUPP;
        return NULL;
    }

    char *found = strcasestr(sxe_jitson_get_string(right, NULL), sxe_jitson_get_string(left, NULL));

    if (!found)
        return &jitson_null;

    return sxe_jitson_create_string_ref(found);
}

/* Wonky override to take the length of a number.
 */
static const struct sxe_jitson *
len_op_number(const struct sxe_jitson *arg)
{
    char buf[16];

    return sxe_jitson_create_number(snprintf(buf, sizeof(buf), "%g", sxe_jitson_get_number(arg)));
}

static const char *string = "this STRING is 33 characters long";

int
main(void)
{
    struct sxe_jitson          arg, element;
    union sxe_jitson_oper_func func;
    struct sxe_jitson_stack   *stack;
    struct sxe_jitson         *collection, *coll_rhs;
    const struct sxe_jitson   *result;
    char                      *json_str;
    const char                *substring;
    uint64_t                   start_allocations;
    unsigned                   and_op;

    tap_plan(90, TAP_FLAG_LINE_ON_OK, NULL);    // Display test line numbers in OK messages (useful for tracing)
    start_allocations = kit_memory_allocations();
    // KIT_ALLOC_SET_LOG(1);    // Turn off when done

    sxe_jitson_type_init(SXE_JITSON_MIN_TYPES, SXE_JITSON_FLAG_OPTIMIZE);

    diag("Test the operator extension");
    {
        func.binary = and_op_default;
        is(and_op   = sxe_jitson_oper_register("&&", SXE_JITSON_OPER_BINARY, func), 1, "First operator is 1");
        is(sxe_jitson_oper_apply_binary(&jitson_true, and_op, &jitson_true),  &jitson_true,  "true && true is true");
        is(sxe_jitson_oper_apply_binary(&jitson_true, and_op, &jitson_false), &jitson_false, "true && false is false");

        sxe_jitson_make_string_ref(&arg, string);
        func.unary = len_op_default;
        is(len_op  = sxe_jitson_oper_register("len", SXE_JITSON_OPER_UNARY, func), 2, "Second operator is 2");
        is(sxe_jitson_oper_apply_unary(len_op, &jitson_true), NULL,                   "len true is invalid");
        ok(result = sxe_jitson_oper_apply_unary(len_op, &arg),                        "len string is implemented");
        is(sxe_jitson_get_number(result), 33,                                         "len string is 33");
        sxe_jitson_free(result);

        sxe_jitson_make_string_ref(&element, "string");
        sxe_jitson_in_init();
        is(sxe_jitson_oper_in, 3,                                                     "Third operator is 3");
        ok(result = sxe_jitson_oper_apply_binary(&element, sxe_jitson_oper_in, &arg), "substring in string is implemented");
        is_eq(sxe_jitson_get_type_as_str(result), "null",                             "substring not found");

        /* Override the standard implementation of string IN to ignore case
         */
        func.binary = in_op_string_caseless;
        sxe_jitson_oper_add_to_type(sxe_jitson_oper_in, SXE_JITSON_TYPE_STRING, func);
        ok(result = sxe_jitson_oper_apply_binary(&element, sxe_jitson_oper_in, &arg), "substring in string is implemented");
        is_strncmp(substring = sxe_jitson_get_string(result, NULL), "STRING", 6,      "Return begins with the substring");
        is(substring, string + strlen("this "),                                       "And points into the containing string");
        sxe_jitson_free(result);

        sxe_jitson_make_number(&arg, 33);
        ok(!sxe_jitson_oper_apply_binary(&element, sxe_jitson_oper_in, &arg), "Substring in a number is not implemented");
        is(errno, EOPNOTSUPP,                                                 "Got the expected error (%s)", strerror(errno));

        sxe_jitson_make_number(&arg, 666);
        func.unary = len_op_number;
        sxe_jitson_oper_add_to_type(len_op, SXE_JITSON_TYPE_NUMBER, func);
        ok(result = sxe_jitson_oper_apply_unary(len_op, &arg), "length of number is now implemented");
        is(sxe_jitson_get_uint(result), 3,                     "Returned length of 666 is 3");
        sxe_jitson_free(result);

        func.unary = NULL;    // No default
        is(sxe_jitson_oper_register("~", SXE_JITSON_OPER_UNARY, func), 4, "Fourth operator is 4");
        ok(!sxe_jitson_oper_apply_unary(4, &arg),                         "~ of number is not implemented");

        func.binary = NULL;    // No default
        is(sxe_jitson_oper_register("||", SXE_JITSON_OPER_BINARY, func), 5,     "Fifth operator is 5");
        ok(!sxe_jitson_oper_apply_binary(sxe_jitson_true, 5, sxe_jitson_false), "|| of bools is not implemented");
    }

    diag("Test the default IN operator");
    {
        collection = sxe_jitson_new("[true, 1]");
        ok(sxe_jitson_eq(sxe_jitson_in(sxe_jitson_true, collection), sxe_jitson_true), "Found 'true'");
        sxe_jitson_make_number(&element, 1.1);
        ok(sxe_jitson_eq(sxe_jitson_in(&element, collection), sxe_jitson_null),        "Didn't find 1.1");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("{\"one\": 1, \"zero\": 0}");
        is(sxe_jitson_in(&element, collection), NULL,                                  "Can't use a number as a key");
        sxe_jitson_make_string_ref(&element, "one");
        sxe_jitson_make_number(&arg, 1);
        ok(sxe_jitson_eq(sxe_jitson_in(&element, collection), &arg),                   "Found \"one\" in object");
        sxe_jitson_make_string_ref(&element, "zero");
        sxe_jitson_make_number(&arg, 0);
        ok(sxe_jitson_eq(sxe_jitson_in(&element, collection), &arg),                   "Found \"zero\" in object");
        sxe_jitson_free(collection);

        ok(result = sxe_jitson_in(&element, sxe_jitson_null),                          "Able to look for a number in 'null'");
        is_eq(sxe_jitson_get_type_as_str(result), "null",                              "And get 'null' back");

        collection = sxe_jitson_new("[\"a\", \"bsearchable\", \"is\", \"list\", \"this\"]");
        ok(sxe_jitson_eq(sxe_jitson_in(&element, collection), sxe_jitson_null),        "Didn't find \"one\" in sorted list");
        sxe_jitson_make_string_ref(&element, "is");
        ok(sxe_jitson_test(sxe_jitson_in(&element, collection)),                       "Found \"is\" in sorted list");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[]");
        ok(sxe_jitson_eq(sxe_jitson_in(&element, collection), sxe_jitson_null),        "Didn't find \"one\" in empty list");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[0,1,2,3,4,5,6]");
        sxe_jitson_make_number(&element, 1.0);
        ok(sxe_jitson_test(sxe_jitson_in(&element, collection)),                       "Found 1 in [0,1,2,3,4,5,6]");
        sxe_jitson_make_number(&element, 0);
        ok(sxe_jitson_test(sxe_jitson_in(&element, collection)),                       "Found 0 in [0,1,2,3,4,5,6]");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[0,[1,2,3],[4,5,6]]");
        ok(sxe_jitson_eq(sxe_jitson_in(&element, collection), sxe_jitson_true),        "0 IN [0,[1,2,3],[4,5,6]] -> true");
        sxe_jitson_make_uint(&element, 1);
        result = sxe_jitson_in(&element, collection);
        is_eq(json_str = result ? sxe_jitson_to_json(result, NULL) : NULL, "[1,2,3]",  "1 IN [0,[1,2,3],[4,5,6]] -> [1,2,3]");
        kit_free(json_str);
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[\"one\",\"two\"]");
        sxe_jitson_make_string_ref(&element, "on");
        ok(sxe_jitson_eq(sxe_jitson_in(&element, collection), sxe_jitson_null),        "\"on\" IN [\"one\",\"two\"] -> null");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[3.14, 6.66]");
        sxe_jitson_make_uint(&element, 18446744073709551615ULL);
        result = sxe_jitson_in(&element, collection);
        is(result, NULL,                                                               "max uint64_t IN [3.14, 6.66] -> error");
        sxe_jitson_free(collection);
    }

    diag("Test the default INTERSECT operator");
    {
        sxe_jitson_intersect_init();
        ok(!sxe_jitson_intersect(sxe_jitson_null, sxe_jitson_true),           "Can't intersect values that aren't arrays");

        collection = sxe_jitson_new("[]");
        ok(!sxe_jitson_intersect(sxe_jitson_null, collection),                "Can't intersect a value that isn't an array");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[2, 1]");
        ok(!sxe_jitson_intersect(collection, sxe_jitson_true),                "Can't intersect a value that's not a array");

        coll_rhs = sxe_jitson_new("[1, 2]");
        ok(!sxe_jitson_intersect(sxe_jitson_true, coll_rhs),                  "Can't intersect non-array with ordered array");
        MOCKFAIL_START_TESTS(1, SXE_JITSON_INTERSECT_OPEN);
        ok(!sxe_jitson_intersect(collection, coll_rhs),                       "Can't intersect w/ordered on failure to open");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, SXE_JITSON_INTERSECT_ADD);
        ok(!sxe_jitson_intersect(collection, coll_rhs),                       "Can't intersect w/ordered on failure to add");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, SXE_JITSON_INTERSECT_GET);
        ok(!sxe_jitson_intersect(collection, coll_rhs),                       "Can't intersect w/ordered on failure to get");
        MOCKFAIL_END_TESTS();
        sxe_jitson_free(coll_rhs);

        coll_rhs = sxe_jitson_new("[1]");
        MOCKFAIL_START_TESTS(1, SXE_JITSON_INTERSECT_OPEN);
        ok(!sxe_jitson_intersect(collection, coll_rhs),                       "Can't intersect on failure to open result");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, SXE_JITSON_INTERSECT_ADD);
        ok(!sxe_jitson_intersect(collection, coll_rhs),                       "Can't intersect on failure to add to result");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, SXE_JITSON_INTERSECT_GET);
        ok(!sxe_jitson_intersect(collection, coll_rhs),                       "Can't intersect on failure to get result");
        MOCKFAIL_END_TESTS();

        stack = sxe_jitson_stack_get_thread();                              // Get the tread stack
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),    "Opened an outside collection");
        ok(result = sxe_jitson_intersect(collection, coll_rhs),               "Intersected [2, 1] with [1]");
        is_eq(sxe_jitson_get_type_as_str(result), "array",                    "Result is an array");
        is(sxe_jitson_len(result), 1,                                         "Of one element");
        is(sxe_jitson_get_number(sxe_jitson_array_get_element(result, 0)), 1, "Which is 1");
        sxe_jitson_free(result);
        sxe_jitson_free(coll_rhs);
        sxe_jitson_stack_close_collection(stack);                           // Close the outside collection
        ok(coll_rhs = sxe_jitson_stack_get_jitson(stack),                     "Got the outside collection");
        sxe_jitson_free(coll_rhs);

        coll_rhs   = collection;
        collection = sxe_jitson_new("[1, 3]");
        ok(result = sxe_jitson_intersect(collection, coll_rhs),               "Intersected [1, 3] with [2, 1]");
        is_eq(sxe_jitson_get_type_as_str(result), "array",                    "Result is an array");
        is(sxe_jitson_len(result), 1,                                         "Of one element");
        is(sxe_jitson_get_number(sxe_jitson_array_get_element(result, 0)), 1, "Which is 1");
        sxe_jitson_free(result);
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[{}]");
        coll_rhs   = sxe_jitson_new("[{}]");
        ok(!sxe_jitson_intersect(collection, coll_rhs),                       "Can't intersect arrays containing objects");
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[1,2,3]");
        coll_rhs   = sxe_jitson_new("[2,4]");
        ok(result = sxe_jitson_intersect(collection, coll_rhs),               "Intersected ordered arrays [1,2,3] and [2,4]");
        is(1, sxe_jitson_len(result),                                         "Intersection has 1 element");
        is(2, sxe_jitson_get_uint(sxe_jitson_array_get_element(result, 0)),   "It's 2");
        sxe_jitson_free(result);
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[3,2,1]");
        coll_rhs   = sxe_jitson_new("[4,2]");
        MOCKFAIL_START_TESTS(1, SXE_JITSON_INTERSECT_ADD);
        ok(!sxe_jitson_intersect(collection, coll_rhs),                       "Can't intersect unordered arrays if add fails");
        MOCKFAIL_END_TESTS();

        ok(result = sxe_jitson_intersect(collection, coll_rhs),               "Intersected unordered arrays [3,2,1] and [4,2]");
        is(1, sxe_jitson_len(result),                                         "Intersection has 1 element");
        is(2, sxe_jitson_get_uint(sxe_jitson_array_get_element(result, 0)),   "It's 2");
        sxe_jitson_free(result);
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[0, 18446744073709551615]");    // 2nd element can only be represented as a uint
        coll_rhs   = sxe_jitson_new("[3.14159, 4.4]");               // 1st element can only be represented as a double
        ok(!sxe_jitson_intersect(collection, coll_rhs),                "Can't intersect ordered arrays with incomparable types");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[18446744073709551615, 0]");    // Same as above but LHS list is unordered
        ok(!sxe_jitson_intersect(collection, coll_rhs),                "Unordered arrays with ordered with incomparable types");
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[{},{}]");                      // Technically an unordered array
        coll_rhs   = sxe_jitson_new("[{},{}]");                      // Technically an unordered array
        ok(!sxe_jitson_intersect(collection, coll_rhs),                "Can't intersect unordered arrays with incomparable types");
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[\"one-long\",\"two\"]");                           // A non-uniform sorted array
        coll_rhs   = sxe_jitson_new("[\"three-long\",\"two\"]");                         // Another non-uniform sorted array
        ok(result = sxe_jitson_intersect(collection, coll_rhs),                            "Intersected non-uniform arrays");
        is(1, sxe_jitson_len(result),                                                      "Intersection has 1 element");
        is_eq(sxe_jitson_get_string(sxe_jitson_array_get_element(result, 0), NULL), "two", "Got expected value");
        sxe_jitson_free(result);
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[\"one-long\",\"two-long\"]");    // A sorted array of size 32 elements
        coll_rhs   = sxe_jitson_new("[\"three\",\"two\"]");            // A sorted array of size 16 elements
        ok(result = sxe_jitson_intersect(collection, coll_rhs),          "Intersected uniform arrays of different sized elements");
        is(0, sxe_jitson_len(result),                                    "Intersection has no elements");
        sxe_jitson_free(result);
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);
    }

    diag("Test the INTERSECT_TEST operator");    // INTERSECT_TEST is optimized to just test wether there is an intersection
    {
        is(sxe_jitson_intersect_test(sxe_jitson_true, sxe_jitson_false), NULL,  "Can't test intersect booleans");

        collection = sxe_jitson_new("[]");
        is(sxe_jitson_intersect_test(sxe_jitson_true, collection), NULL,        "Can't test intersect a boolean with an array");
        is(sxe_jitson_intersect_test(collection, collection), sxe_jitson_false, "No common elements");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[1]");
        is(sxe_jitson_intersect_test(collection, collection), sxe_jitson_true,  "Common elements");
        sxe_jitson_free(collection);

        coll_rhs   = sxe_jitson_new("[3.14159, 4.4]");               // 1st element can only be represented as a double
        collection = sxe_jitson_new("[0, 18446744073709551615]");    // 2nd element can only be represented as a uint
        ok(!sxe_jitson_intersect_test(collection, coll_rhs), "Can't test intersect ordered arrays with incomparable types");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[18446744073709551615]");       // Element can only be represented as a uint
        ok(!sxe_jitson_intersect_test(collection, coll_rhs), "Can't test intersect unsorted/sorted arrays with incomparables");
        sxe_jitson_free(coll_rhs);

        coll_rhs = sxe_jitson_new("[4.4, 3.14159]");                 // Unsorted elements can only be represented as doubles
        ok(!sxe_jitson_intersect_test(collection, coll_rhs), "Can't test intersect unsorted arrays with incomparables");
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[1, 2]");
        is(sxe_jitson_intersect_test(sxe_jitson_true, collection), NULL,       "Can't test intersect boolean and sorted array");
        is(sxe_jitson_intersect_test(collection, collection), sxe_jitson_true, "Common elements in sorted arrays");
        coll_rhs = sxe_jitson_new("[0, 3]");
        is(sxe_jitson_intersect_test(collection, coll_rhs), sxe_jitson_false,  "No common elements in sorted arrays");
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[\"short\", \"snort\"]");                // Ordered strings stored in 1 jitson
        coll_rhs   = sxe_jitson_new("[\"long string\", \"null string\"]");    // Ordered strings stored in 2 jitsons
        is(sxe_jitson_intersect_test(collection, coll_rhs), sxe_jitson_false, "Insta-false when sizes differ");
        sxe_jitson_free(coll_rhs);
        sxe_jitson_free(collection);

        coll_rhs   = sxe_jitson_new("[\"null string\", \"snort\"]");           // Different ordered strings, nonuniform sizes
        collection = sxe_jitson_new("[\"long string\", \"short\"]");           // Ordered strings, nonuniform sizes
        is(sxe_jitson_intersect_test(collection, collection), sxe_jitson_true, "Common elements in nonuniform sorted arrays");
        is(sxe_jitson_intersect_test(collection, coll_rhs), sxe_jitson_false,  "No common elems in nonuniform sorted arrays");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[]");                                     // Unordered
        is(sxe_jitson_intersect_test(collection, coll_rhs), sxe_jitson_false,  "No common elems in unsorted/sorted arrays");
        sxe_jitson_free(collection);

        collection = sxe_jitson_new("[\"snort\"]");                            // 1 element arrays are alway unordered
        is(sxe_jitson_intersect_test(collection, coll_rhs), sxe_jitson_true,   "Common elems in unsorted/sorted arrays");
        is(sxe_jitson_intersect_test(coll_rhs, collection), sxe_jitson_true,   "Sorted array intersected with unsorted");
        sxe_jitson_free(collection);
        sxe_jitson_free(coll_rhs);
    }

    sxe_jitson_oper_fini();
    sxe_jitson_type_fini();
    sxe_thread_memory_free(SXE_THREAD_MEMORY_ALL);
    is(kit_memory_allocations(), start_allocations, "No memory was leaked");
    return exit_status();
}
