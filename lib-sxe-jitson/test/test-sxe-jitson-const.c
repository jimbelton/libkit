/* Test the sxe-jitson const extension
 */

#include <stdio.h>
#include <tap.h>

#include "kit-mockfail.h"
#include "sxe-jitson-const.h"
#include "sxe-thread.h"

static bool
number_cast_func(struct sxe_jitson_stack *stack, const struct sxe_jitson *from)
{
    const char   *string;
    char         *end;
    unsigned long value;
    size_t        len;
    unsigned      index;

    if (sxe_jitson_get_type(from) != SXE_JITSON_TYPE_STRING)
        return false;

    value = strtoul(string = sxe_jitson_get_string(from, &len), &end, 10);

    if ((size_t)(end - string) != len)
        return false;

    if ((index = sxe_jitson_stack_expand(stack, 1)) == SXE_JITSON_STACK_ERROR)
        return false;

    sxe_jitson_make_uint(&stack->jitsons[index], value);
    return true;
}

static bool
string_cast_func(struct sxe_jitson_stack *stack, const struct sxe_jitson *from)
{
    unsigned index;
    char     buf[12];

    SXEA1(sxe_jitson_get_type(from) == SXE_JITSON_TYPE_NUMBER, "Expected a number");
    SXEA1((index = sxe_jitson_stack_expand(stack, 1)) != SXE_JITSON_STACK_ERROR, "Expected to expand the stack");

    snprintf(buf, sizeof(buf), "%lu", sxe_jitson_get_uint(from));
    sxe_jitson_make_string_ref(&stack->jitsons[index], kit_strdup(buf));
    stack->jitsons[index].type |= SXE_JITSON_TYPE_IS_OWN; // Set as owned so that sxe_jitson_free_base() will free the string
    return true;
}

int
main(void)
{
    struct sxe_jitson_source source;
    struct sxe_jitson_stack *stack;
    struct sxe_jitson       *jitson;               // Constructed jitson values are returned as non-const
    const struct sxe_jitson *element;
    char                    *json_out;
    uint64_t                 start_allocations;

    plan_tests(24);
    start_allocations = kit_memory_allocations();
    // KIT_ALLOC_SET_LOG(1);    // Turn off when done

    sxe_jitson_initialize(0, 0);                              // Initialize the JSON types, and don't enable hexadecimal
    stack                 = sxe_jitson_stack_get_thread();

    diag("Test the constants extension");
    {
        is(sxe_jitson_new("[NONE,BIT0,BIT1]"), NULL, "Failed to parsed an array containing unknown constants");

        ok(sxe_jitson_stack_open_collection(  stack, SXE_JITSON_TYPE_OBJECT),        "Opened an object on the stack");
        ok(sxe_jitson_stack_add_member_string(stack, "NAME", "two-jitson-value", 0), "Added a name to the object");
        ok(sxe_jitson_stack_add_member_uint(  stack, "BIT0", 1),                     "Added a bit flag");
        ok(sxe_jitson_stack_add_member_uint(  stack, "BIT1", 2),                     "Added another bit flag");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack), "Got the object from the stack");
        sxe_jitson_const_initialize(jitson);

        /* Test the failure case where a duplicated constant value is > 1 jitson and the allocation fails.
         */
        stack = sxe_jitson_stack_get_thread();
        stack->maximum = 1;   // Set initial maximum back to 1
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        MOCKFAIL_SET_SKIP(1);    // Skip 1st call, but fail thereafter
        is(sxe_jitson_new("[NAME,BIT0,BIT1]"), NULL, "Failed to parse constants on alloc failure");
        MOCKFAIL_END_TESTS();

        ok(jitson = sxe_jitson_new("[NAME,BIT0,BIT1]"),                                  "Parsed array containing constants");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[\"two-jitson-value\",1,2]", "Constants were correctly replaced");
        kit_free(json_out);
        sxe_jitson_free(jitson);

        is(sxe_jitson_new("bull"), NULL, "Failed to parse a 1st letter misspelling of a keyword");

        sxe_jitson_source_from_string(&source, "[NAME,BIT0,BIT1]", SXE_JITSON_FLAG_STRICT);
        is(sxe_jitson_stack_load_json(stack, &source), NULL, "Failed to parse constants due to strict mode");
    }

    diag("Test cast operators");
    {
        struct sxe_jitson number_cast, string_cast;

        sxe_jitson_const_register_cast(&number_cast, "number", number_cast_func);
        ok(jitson = sxe_jitson_new("number(\"1\")"),        "Cast \"1\" to a number");
        is_eq(sxe_jitson_get_type_as_str(jitson), "number", "number(\"1\") is a number");
        is(sxe_jitson_get_uint(jitson),           1,        "number(\"1\") is 1");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("[number(\"1\")]"),        "Parsed an array containing a cast");
        ok(element = sxe_jitson_array_get_element(jitson, 0), "Got its element");
        is_eq(sxe_jitson_get_type_as_str(element), "number",  "Its element is a number");
        is(sxe_jitson_get_uint(element),           1,         "Its element is 1");
        sxe_jitson_free(jitson);

        ok(!sxe_jitson_new("number"),                         "Expect a ( after a cast name");
        ok(!sxe_jitson_new("number("),                        "Expect a JSON value after a cast name");
        ok(!sxe_jitson_new("number(null)"),                   "Expect cast function to fail if argument is not a string");
        ok(!sxe_jitson_new("number(\"1\""),                   "Expect a ) after the JSON value after a cast name");

        /* The following should not leak the memory allocated by the cast function
         */
        sxe_jitson_const_register_cast(&string_cast, "string", string_cast_func);
        ok(!sxe_jitson_new("string(1"), "Expect a ) after the JSON value after a cast name");
    }

    sxe_jitson_const_finalize();
    sxe_jitson_type_fini();
    sxe_thread_memory_free(SXE_THREAD_MEMORY_ALL);
        is(kit_memory_allocations(), start_allocations, "No memory was leaked");
    return exit_status();
}
