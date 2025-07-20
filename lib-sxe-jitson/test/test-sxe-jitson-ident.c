/* Test the sxe-jitson identifier extension
 */

#include <tap.h>

#include "kit-mockfail.h"
#include "sxe-jitson-const.h"
#include "sxe-jitson-ident.h"
#include "sxe-thread.h"

static const struct sxe_jitson *my_value = NULL;

static const struct sxe_jitson *
my_lookup(const char *ident, size_t len)
{
    SXE_UNUSED_PARAMETER(ident);
    SXE_UNUSED_PARAMETER(len);
    return my_value;
}

int
main(void)
{
    struct sxe_jitson_source source;
    struct sxe_jitson_stack  *stack;
    struct sxe_jitson        *jitson;     // Constructed jitson values are returned as non-const
    const struct sxe_jitson  *element;    // Accessed jitson values are returned as const
    char                     *string;
    size_t                    len;
    uint64_t                  start_allocations;

    plan_tests(27);
    start_allocations = kit_memory_allocations();
    // KIT_ALLOC_SET_LOG(1);    // Turn off when done

    ok(!sxe_jitson_is_init(),         "Not yet initialized");
    sxe_jitson_type_init(0, 0);     // Initialize the JSON types, and don't enable hexadecimal
    sxe_jitson_ident_register();
    sxe_jitson_ident_register();    // Registration is idempotent
    stack = sxe_jitson_stack_get_thread();

    ok(jitson = sxe_jitson_new("[NONE,length_8,identifier]"), "Parsed an array containing unknown identifiers");
    ok(element = sxe_jitson_array_get_element(jitson, 0),     "Got the first element");
    is(SXE_JITSON_TYPE_IDENT, sxe_jitson_get_type(element),   "It's an identifier");
    is_eq(sxe_jitson_ident_get_name(element, &len), "NONE",   "It's 'NONE'");
    is(len, 4,                                                "It's length is 4");
    sxe_jitson_free(jitson);

    sxe_jitson_source_from_string(&source, "[NONE,length_8,identifier]", SXE_JITSON_FLAG_STRICT);
    is(sxe_jitson_stack_load_json(stack, &source), NULL, "Failed to parse identifiers due to strict mode");

    ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened an object on the stack");
    ok(sxe_jitson_stack_add_member_uint(stack, "NONE", 0),              "Added value for NONE to the object");
    sxe_jitson_stack_close_collection(stack);
    ok(jitson = sxe_jitson_stack_get_jitson(stack), "Got the object from the stack");
    sxe_jitson_const_initialize(jitson);    // Set the constants

    /* Test the failure case where an identifier is > 7 characters and jitson allocation fails.
     */
    stack->maximum = 1;    // Reset the thread stack's initial stack size to 1
    MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
    MOCKFAIL_SET_FREQ(3);
    is(sxe_jitson_new("[NONE,BIT0,identifier]"), NULL, "Failed to parse constants on alloc failure");
    MOCKFAIL_END_TESTS();

    ok(jitson = sxe_jitson_new("[NONE,BIT0,identifier]"),           "Parsed array of a constant and an unknown identifier");
    ok(element = sxe_jitson_array_get_element(jitson, 0),           "Got the first element");
    is(sxe_jitson_get_type(element), SXE_JITSON_TYPE_NUMBER,        "It's a number");
    is(sxe_jitson_get_uint(element), 0,                             "It's 0");
    ok(element = sxe_jitson_array_get_element(jitson, 2),           "Got the third element");
    is_eq(sxe_jitson_get_type_as_str(element), "identifier",        "It's an indentifier");
    is_eq(string = sxe_jitson_to_json(element, NULL), "identifier", "identifier translated to string correctly");
    kit_free(string);
    is_eq(sxe_jitson_ident_get_name(element, &len), "identifier", "It's 'identifier'");
    is(len, 10,                                                   "Its length is 10");

    MOCKFAIL_START_TESTS(1, sxe_factory_reserve);
    is(string = sxe_jitson_to_json(element, NULL), NULL,       "identifier got null on malloc failure");
    MOCKFAIL_END_TESTS();

    sxe_jitson_free(jitson);

    diag("Test lookup and test functions");
    {
        ok(jitson = sxe_jitson_new("identifier"),          "Constructed an identifier");
        is(sxe_jitson_test(jitson), SXE_JITSON_TEST_ERROR, "With no lookup, test returns SXE_JITSON_TEST_ERROR");
        is(sxe_jitson_ident_lookup_hook(my_lookup), NULL,  "Hooked in the lookup function");
        is(sxe_jitson_test(jitson), SXE_JITSON_TEST_ERROR, "With not found, test returns SXE_JITSON_TEST_ERROR");
        my_value = sxe_jitson_true;
        is(sxe_jitson_test(jitson), SXE_JITSON_TEST_TRUE,  "With true found, test returns SXE_JITSON_TEST_TRUE");
        sxe_jitson_free(jitson);
    }

    sxe_jitson_const_finalize();
    sxe_jitson_type_fini();
    sxe_thread_memory_free(SXE_THREAD_MEMORY_ALL);
    is(kit_memory_allocations(), start_allocations, "No memory was leaked");

    return exit_status();
}
