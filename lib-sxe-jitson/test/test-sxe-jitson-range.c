/* Test the sxe-jitson range extension
 */

#include <tap.h>

#include "kit-mockfail.h"
#include "sxe-jitson-const.h"
#include "sxe-jitson-in.h"
#include "sxe-jitson-oper.h"
#include "sxe-jitson-range.h"
#include "sxe-thread.h"

int
main(void)
{
    struct sxe_jitson        value, to;
    struct sxe_jitson       *jitson;
    struct sxe_jitson_stack *stack;
    char                    *json;
    uint64_t                 start_allocations;

    plan_tests(29);
    start_allocations = kit_memory_allocations();
    // KIT_ALLOC_SET_LOG(1);    // Turn off when done

    sxe_jitson_initialize(0, 0);          // Initialize the JSON types, and don't enable hexadecimal by default
    sxe_jitson_const_initialize(NULL);    // Don't add any constants, but allow casts to be added.
    sxe_jitson_in_init();
    sxe_jitson_range_register();
    sxe_jitson_range_register();          // Registration is idempotent
    stack = sxe_jitson_stack_get_thread();

    diag("Happy path cases");
    {
        ok(jitson = sxe_jitson_new("range([1,5])"),                                     "Parsed an range from 'range([1,5])'");
        ok(sxe_jitson_test(jitson),                                                     "Ranges always test true");
        is_eq(json = sxe_jitson_to_json(jitson, NULL),                 "range([1,5])",  "Back to expected JSON text");
        is(sxe_jitson_in(sxe_jitson_make_uint(&value, 0),     jitson), sxe_jitson_null, "0 is not IN range([1,5])");
        is(sxe_jitson_in(sxe_jitson_make_uint(&value, 1),     jitson), sxe_jitson_true, "1 is IN range([1,5])");
        is(sxe_jitson_in(sxe_jitson_make_number(&value, 3.5), jitson), sxe_jitson_true, "1 is IN range([1,5])");
        is(sxe_jitson_in(sxe_jitson_make_uint(&value, 1),     jitson), sxe_jitson_true, "3.5 is IN range([1,5])");
        is(sxe_jitson_in(sxe_jitson_make_number(&value, 5.0), jitson), sxe_jitson_true, "5.0 is IN range([1,5])");
        is(sxe_jitson_in(sxe_jitson_make_uint(&value, ~0ULL), jitson), sxe_jitson_null, "%llu is not IN range([1,5])", ~0ULL);
        kit_free(json);
        sxe_jitson_free(jitson);

        sxe_jitson_make_uint(  &value, 1);
        sxe_jitson_make_number(&to,    5.0);
        ok(sxe_jitson_stack_add_range(stack, &value, &to),                              "Added a range from 'range([1,5.0])'");
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                                 "Got it from the stack");
        ok(sxe_jitson_test(jitson),                                                     "Ranges always test true");
        is_eq(json = sxe_jitson_to_json(jitson, NULL),                 "range([1,5])",  "Back to expected JSON text");
        is(sxe_jitson_in(sxe_jitson_make_uint(&value, 0),     jitson), sxe_jitson_null, "0 is not IN range([1,5]");
        is(sxe_jitson_in(sxe_jitson_make_uint(&value, 1),     jitson), sxe_jitson_true, "1 is IN range([1,5]");
        is(sxe_jitson_in(sxe_jitson_make_number(&value, 3.5), jitson), sxe_jitson_true, "1 is IN range([1,5]");
        is(sxe_jitson_in(sxe_jitson_make_uint(&value, 1),     jitson), sxe_jitson_true, "3.5 is IN range([1,5]");
        is(sxe_jitson_in(sxe_jitson_make_number(&value, 5.0), jitson), sxe_jitson_true, "5.0 is IN range([1,5]");
        is(sxe_jitson_in(sxe_jitson_make_uint(&value, ~0ULL), jitson), sxe_jitson_null, "%llu is not IN range([1,5]", ~0ULL);
        kit_free(json);
        sxe_jitson_free(jitson);
    }

    diag("Failure cases");
    {
        ok(!sxe_jitson_new("range(\"1,5\")"),   "Can't parse a range from 'range(\"1,5\")'");
        ok(!sxe_jitson_new("range([1,2,3])"),   "Can't parse a range array that has more than 2 elements");
        ok(!sxe_jitson_new("range([5,1])"),     "Can't parse a range array whose elements are out of order");
        ok(!sxe_jitson_new("range([1,\"2\"])"), "Can't parse a range array whose elements are incomparable");

        sxe_jitson_make_string_ref(&value, "one");
        sxe_jitson_make_string_ref(&to,    "five");
        ok(!sxe_jitson_stack_add_range(stack, &value, &to), "Can't add a range that isn't ordered");

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND_AFTER_GET);
        MOCKFAIL_SET_SKIP(1);
        ok(!sxe_jitson_new("range([1,5])"), "Failed to parse a range on failure to allocate memory");
        MOCKFAIL_END_TESTS();

        ok(jitson = sxe_jitson_new("range([1,5])"), "Parsed an range from 'range([1,5])'");
        MOCKFAIL_START_TESTS(1, sxe_factory_reserve);
        ok(!sxe_jitson_to_json(jitson, NULL), "Failed to convert to JSON text on failure to allocate memory");
        MOCKFAIL_END_TESTS();
        sxe_jitson_free(jitson);

        sxe_jitson_make_uint(  &value, 1);
        sxe_jitson_make_number(&to,    5.0);
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND_AFTER_GET);
        ok(!sxe_jitson_stack_add_range(stack, &value, &to), "Failed to construct a range on failure to allocate memory");
        MOCKFAIL_END_TESTS();
    }

    sxe_jitson_oper_fini();
    sxe_jitson_const_finalize();
    sxe_jitson_finalize();
    sxe_thread_memory_free(SXE_THREAD_MEMORY_ALL);
    is(kit_memory_allocations(), start_allocations, "No memory was leaked");
    return exit_status();
}
