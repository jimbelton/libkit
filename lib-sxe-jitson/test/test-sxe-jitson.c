#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <tap.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "sxe-jitson.h"
#include "sxe-test-memory.h"
#include "sxe-thread.h"

static void
test_stacked_array_is_ordered(struct sxe_jitson_stack *stack, const char *json, bool expect_ordered)
{
    struct sxe_jitson *jitson;

    if (!(jitson = sxe_jitson_stack_get_jitson(stack)))
        fail("Failed to get '%s' from the stack (error == %s)", json, strerror(errno));
    if (sxe_jitson_get_type(jitson) != SXE_JITSON_TYPE_ARRAY)
        fail("'%s' is not an array", json);
    else
        is((bool)(jitson->type & SXE_JITSON_TYPE_IS_ORD), expect_ordered,
           "'%s' is %sordered", json, jitson->type & SXE_JITSON_TYPE_IS_ORD ? "" : "not ");

    sxe_jitson_free(jitson);
}

static void
test_parsing_ordered_array(const char *json, bool expect_ordered)
{
    struct sxe_jitson_source source;
    struct sxe_jitson_stack *stack  = sxe_jitson_stack_get_thread();

    sxe_jitson_source_from_string(&source, json, SXE_JITSON_FLAG_OPTIMIZE);

    if (!sxe_jitson_stack_load_json(stack, &source))
        fail("Failed to load '%s' (error == %s)", json, strerror(errno));
    else
        test_stacked_array_is_ordered(stack, json, expect_ordered);
}

static void
test_constructing_ordered_array(const char *name, bool expect_ordered, ...)
{
    struct sxe_jitson_stack *stack  = sxe_jitson_stack_get_thread();
    va_list                  varargs;
    const char              *element;

    if (!sxe_jitson_stack_open_array(stack, name)) {
        fail("Failed to open array '%s'", name);
        return;
    }

    va_start(varargs, expect_ordered);

    while ((element = va_arg(varargs, const char *)))
        if (!sxe_jitson_stack_add_string(stack, element, SXE_JITSON_TYPE_IS_REF)) {
            fail("Failed to add '%s' to array '%s'", element, name);
            va_end(varargs);
            return;
        }

    va_end(varargs);
    sxe_jitson_stack_close_array(stack, name);
    test_stacked_array_is_ordered(stack, name, expect_ordered);
}

int
main(void)
{
    struct sxe_jitson_source source;
    struct sxe_jitson        prim_left, prim_right;
    struct sxe_jitson_stack *stack;
    struct sxe_jitson       *array, *clone, *jitson;    // Constructed jitson values are returned as non-const
    const struct sxe_jitson *element, *member;          // Accessed jitson values are returned as const
    char                    *json_out;
    size_t                   len;
    uint64_t                 start_allocations;

    tap_plan(520 + 5 * 27, TAP_FLAG_LINE_ON_OK, NULL);    // Display test line numbers in OK messages (useful for tracing)
    start_allocations = kit_memory_allocations();
    // KIT_ALLOC_SET_LOG(1);    // Turn off when done

    ok(!sxe_jitson_is_init(), "Not yet initialized");
    sxe_jitson_type_init(0, 0);    // Initialize the JSON types, and don't enable hexadecimal
    ok(sxe_jitson_is_init(),  "Now initialized");

    diag("Memory allocation failure tests");
    {
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEW_OBJECT);
        is(sxe_jitson_stack_new(1), NULL, "Failed to allocate a stack object");
        MOCKFAIL_END_TESTS();

        ok(stack = sxe_jitson_stack_new(1), "Allocated a non-thread stack");
        sxe_jitson_stack_free(stack);

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEW_JITSONS);
        is(sxe_jitson_new("0"), NULL, "Failed to allocate the thread stack's initial jitsons");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        is(sxe_jitson_new("{\"one\":1}"), NULL, "Failed to realloc the thread stack's jitsons on a string");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        is(sxe_jitson_new("\"01234567\""), NULL, "Failed to realloc the thread stack's jitsons on a long string");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        is(sxe_jitson_new("[0,1]"), NULL, "Failed to realloc the thread stack's jitsons");
        MOCKFAIL_END_TESTS();

        jitson = sxe_jitson_new("999999999");
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND_AFTER_GET);
        is(sxe_jitson_new("0"), NULL, "Failed to allocate a new stack after getting the parsed object");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        is(sxe_jitson_new("{\"x\": 0}"), NULL, "Failed to realloc the thread stack's jitsons on string inside an object");
        MOCKFAIL_END_TESTS();

        /* Don't do this at the beginning of the program (need to test failure to allocate above)
         */
        stack = sxe_jitson_stack_get_thread();

        MOCKFAIL_START_TESTS(7, MOCK_FAIL_STACK_EXPAND);
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY), "Opened an array on the stack");
        ok(!sxe_jitson_stack_add_null(stack),              "Failed to realloc to add null to an open array");
        ok(!sxe_jitson_stack_add_bool(stack, true),        "Failed to realloc to add true to an open array");
        ok(!sxe_jitson_stack_add_number(stack, 0.0),       "Failed to realloc to add 0.0 to an open array");
        ok(!sxe_jitson_stack_add_uint(stack, 0),           "Failed to realloc to add 0 to an open array");
        ok(!sxe_jitson_stack_add_reference(stack, jitson), "Failed to realloc to add a reference to an open array");
        ok(!sxe_jitson_stack_add_dup(stack, jitson),       "Failed to realloc to add a duplicate to an open array");
        sxe_jitson_stack_clear(stack);
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, sxe_factory_reserve);
        is(sxe_jitson_to_json(jitson, NULL), NULL, "Failed to encode large to JSON on realloc failure");
        errno = 0;
        MOCKFAIL_END_TESTS();
        sxe_jitson_free(jitson);
    }

    diag("Happy path parsing");
    {
        ok(jitson = sxe_jitson_new("0"),                        "Parsed '0' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "'0' is a number");
        ok(sxe_jitson_get_number(jitson) == 0.0 ,               "Value %f == 0.0", sxe_jitson_get_number(jitson));
        is(sxe_jitson_get_uint(jitson), 0,                      "Value %"PRIu64" == 0", sxe_jitson_get_uint(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new(" 666\t"),                   "Parsed ' 666\\t' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "'666' is a number");
        ok(sxe_jitson_get_number(jitson) == 666.0,              "Value %f == 666.0", sxe_jitson_get_number(jitson));
        is(sxe_jitson_get_uint(jitson), 666,                    "Value %"PRIu64" == 666", sxe_jitson_get_uint(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new(" -0.1"),                    "Parsed '-0.1' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "'-0.1'' is a number");
        ok(sxe_jitson_get_number(jitson) == -0.1,               "Value %f == -0.1", sxe_jitson_get_number(jitson));
        is(errno, 0,                                            "Error = '%s'", strerror(errno));
        is(sxe_jitson_get_uint(jitson), ~0ULL,                  "Value %"PRIu64" == ~OULL", sxe_jitson_get_uint(jitson));
        is(errno, EOVERFLOW,                                    "Error = '%s'", strerror(errno));
        sxe_jitson_free(jitson);
        errno = 0;

        ok(jitson = sxe_jitson_new("1E-100"),                   "Parsed '1E=100' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "1E100' is a number");
        ok(sxe_jitson_get_number(jitson) == 1E-100,             "Value %f == 1E100", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("0xDEADBEEF"),               "Parsed '0xDEADBEEF' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "0xDEADBEEF' is parsed as a number");
        is(sxe_jitson_get_uint(jitson), 0,                      "0x* == 0 (hex not enabled)");
        sxe_jitson_free(jitson);

        sxe_jitson_flags |= SXE_JITSON_FLAG_ALLOW_HEX;
        ok(jitson = sxe_jitson_new("0xDEADBEEF"),               "Parsed '0xDEADBEEF' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "0xDEADBEEF' is a number");
        is(sxe_jitson_get_uint(jitson), 0xDEADBEEF,             "0x%"PRIx64" == 0xDEADBEEF", sxe_jitson_get_uint(jitson));
        is(sxe_jitson_get_number(jitson), (double)0xDEADBEEF,   "%f == (double)0xDEADBEEF", sxe_jitson_get_number(jitson));
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "3735928559", "0xDEADBEEF is 3735928559 in decimal");
        kit_free(json_out);
        sxe_jitson_free(jitson);

        sxe_jitson_source_from_string(&source, "0xDEADBEEF", SXE_JITSON_FLAG_STRICT);
        ok(sxe_jitson_stack_load_json(stack, &source),          "Loaded '0xDEADBEEF' (error %s)", strerror(errno));
        ok(jitson = sxe_jitson_stack_get_jitson(stack),         "Got the loaded value");
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "0xDEADBEEF' is a number");
        is(sxe_jitson_get_uint(jitson), 0,                      "0x* == 0 (hex not enabled due to strict mode)");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("0xDEADBEEFDEADBEEF"),       "Parsed '0xDEADBEEFDEADBEEF' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "0xDEADBEEFDEADBEEF' is a number");
        is(sxe_jitson_get_uint(jitson), 0xDEADBEEFDEADBEEF,     "0x%"PRIx64" == 0xDEADBEEFDEADBEEF", sxe_jitson_get_uint(jitson));
        ok(isnan(sxe_jitson_get_number(jitson)),                "%f == NAN", sxe_jitson_get_number(jitson));
        is(errno, EOVERFLOW,                                    "Error = %s", strerror(errno));
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "16045690984833335023",
              "0xDEADBEEFDEADBEEF is 16045690984833335023 in decimal");
        kit_free(json_out);
        sxe_jitson_free(jitson);
        errno = 0;

        ok(jitson = sxe_jitson_new("0xDEADBEEFDEADBEEFDEAD"),   "Parsed '0xDEADBEEFDEADBEEFDEAD' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "0xDEADBEEFDEADBEEFDEAD' is a number");
        is(sxe_jitson_get_uint(jitson), 0xFFFFFFFFFFFFFFFF,     "0x%"PRIx64" == 0xFFFFFFFFFFFFFFFF", sxe_jitson_get_uint(jitson));
        is(errno, ERANGE,                                       "Error = %s", strerror(errno));
        sxe_jitson_free(jitson);
        errno = 0;

        ok(jitson = sxe_jitson_new("\"\""),                                "Parsed '\"\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "'\"\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "",                     "Correct value");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new(" \"x\"\n"),                            "Parsed ' \"x\"\\n' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "' \"x\"\\n' is a string");
        is_eq(sxe_jitson_get_string(jitson, &len), "x",                    "Correct value");
        is(len,                                    1,                      "Correct length");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\""),      "Parsed '\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"' (error %s)",
           strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "'\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "\"\\/\b\f\n\r\t",      "Correct value");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "\"\\u0022\\u005c/\\u0008\\u000c\\u000a\\u000d\\u0009\"",
              "Control characters are correctly encoded");
        sxe_jitson_free(jitson);
        kit_free(json_out);

        ok(jitson = sxe_jitson_new("\"\\u20aC\""),                          "Parsed '\"\\u20aC\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),             SXE_JITSON_TYPE_STRING, "'\"\\u20aC\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL),  "\xE2\x82\xAC",         "Correct UTF-8 value");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "\"\xE2\x82\xAC\"", "Valid UTC code points are not escaped");
        sxe_jitson_free(jitson);
        kit_free(json_out);

        // Checks to ensure some string specific functions are accessible to new types based on strings
        ok(jitson = sxe_jitson_new(" \"stuff\"\n"),                        "Parsed ' \"stuff\"\\n' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "' \"stuff\"\\n' is a string");
        is_eq(sxe_jitson_get_string(jitson, &len), "stuff",                "Correct value");
        is(len,                                    5,                      "Correct length");
        is(sxe_jitson_string_len(jitson),          5,                      "Correct string length via direct function");
        is(sxe_jitson_string_test(jitson),         SXE_JITSON_TEST_TRUE,   "Nonempty strings test true via direct function");
        is(sxe_jitson_string_cmp(jitson, jitson),  0,                      "Correct comparison via direct function");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new(" {\t} "),                   "Parsed ' {\\t} ' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_OBJECT, "' {\\t} ' is an object" );
        is(sxe_jitson_len(jitson),      0,                      "Correct len");
        is(sxe_jitson_size(jitson),     1,                      "Correct size");                       // Test DPT-1404b
        ok(!sxe_jitson_test(jitson),                            "Empty objects test false");
        is(sxe_jitson_object_get_member(jitson, "x", 1), NULL,  "Search for any member will fail");    // Test DPT-1404a
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "{}", "Encoded back to JSON correctly");
        kit_free(json_out);
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("{\"key\":\"value\"}"),      "Parsed '{\"key\":\"value\"}' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_OBJECT, "'{\"key\":\"value\"}' is an object");
        is(sxe_jitson_len(jitson),      1,                      "Correct len");
        is(sxe_jitson_size(jitson),     3,                      "Correct size");
        ok(sxe_jitson_test(jitson),                             "Nonempty objects test true");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("[1, 2,4]"),                "Parsed '[1, 2,4]' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_ARRAY, "'[1, 2,4]' is an array" );
        is(sxe_jitson_len(jitson),      3,                     "Correct len");
        ok(sxe_jitson_test(jitson),                            "Nonempty arrays test true");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("[]"),                      "Parsed '[]' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_ARRAY, "'[]' is an array" );
        is(sxe_jitson_len(jitson),      0,                     "Correct len");
        ok(!sxe_jitson_test(jitson),                           "Empty arrays test false");
        sxe_jitson_free(jitson);
    }

    diag("Test identifiers and parsing of terminals");
    {
        const char *ident;
        int         i;
        char        test_id[2] = "\0";

        for (i = -128; i <= 127; i++) {
            test_id[0] = (char)i;
            sxe_jitson_source_from_string(&source, test_id, 0);
            ident = sxe_jitson_source_get_identifier(&source, &len);

            if (i == '.' || i == '_' || (i >= '0' && i <= '9') || (i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z')) {
                if (ident != test_id)
                    break;
            } else if (ident != NULL)
                break;
        }

        is(i, 128, "All identifier characters are correctly classified");

        sxe_jitson_source_from_string(&source, "@", SXE_JITSON_FLAG_STRICT);
        ok(!sxe_jitson_source_peek_identifier(&source, &len), "Peek identifier fails on non-identifier character");

        sxe_jitson_source_from_string(&source, "identifier +", SXE_JITSON_FLAG_STRICT);    // Peeking allowed in STRICT mode
        ok(sxe_jitson_source_peek_identifier(&source, &len),  "Peek identifier succeeds");
        is(len, sizeof("identifier") - 1,                     "Length of identifier is correct");

        ok(jitson = sxe_jitson_new("true"),                   "Parsed 'true' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_BOOL, "'true' is a boolean" );
        is(sxe_jitson_get_bool(jitson), true,                 "Correct value");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("false"),                          "Parsed 'false' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),         SXE_JITSON_TYPE_BOOL, "'false' is a boolean" );
        is(sxe_jitson_get_bool(jitson),         false,                "Correct value");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "false",   "Encoded back to JSON correctly");
        kit_free(json_out);
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("null"),                   "Parsed 'null' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NULL, "'null' is the null type" );
        sxe_jitson_free(jitson);
    }

    diag("Cover edge cases of parsing");
    {
        ok(!sxe_jitson_new("{0:1}"),    "Failed to parse non-string key '{0:1}' (error %s)",           strerror(errno));
        ok(!sxe_jitson_new("\""),       "Failed to parse unterminated string '\"' (error %s)",         strerror(errno));
        ok(!sxe_jitson_new("\"\\"),     "Failed to parse unterminated escape '\"\\' (error %s)",       strerror(errno));
        ok(!sxe_jitson_new("\"\\u"),    "Failed to parse truncated unicode escape '\"\\u' (error %s)", strerror(errno));
        ok(!sxe_jitson_new(""),         "Failed to parse empty string '' (error %s)",                  strerror(errno));
        ok(!sxe_jitson_new("{\"k\"0}"), "Failed to parse object missing colon '{\"k\"0}' (error %s)",  strerror(errno));
        ok(!sxe_jitson_new("{\"k\":}"), "Failed to parse object missing value '{\"k\":}' (error %s)",  strerror(errno));
        ok(!sxe_jitson_new("{\"k\":0"), "Failed to parse object missing close '{\"k\":0' (error %s)",  strerror(errno));
        ok(!sxe_jitson_new("[0"),       "Failed to parse array missing close '[0' (error %s)",         strerror(errno));
        ok(!sxe_jitson_new("0."),       "Failed to parse invalid fraction '0.' (error %s)",            strerror(errno));
        ok(!sxe_jitson_new("1.0E"),     "Failed to parse invalid exponent '1.0E' (error %s)",          strerror(errno));
        ok(!sxe_jitson_new("fathead"),  "Failed to parse invalid token 'fathead' (error %s)",          strerror(errno));
        ok(!sxe_jitson_new("n"),        "Failed to parse invalid token 'n' (error %s)",                strerror(errno));
        ok(!sxe_jitson_new("twit"),     "Failed to parse invalid token 'twit' (error %s)",             strerror(errno));
        ok(!sxe_jitson_new("-x"),       "Failed to parse invalid number '-x' (error %s)",              strerror(errno));
        ok(!sxe_jitson_new("0xx"),      "Failed to parse invalid hex number '0xx' (error %s)",         strerror(errno));

        char json[65562];    // Big enough for an object with a > 64K member name
        json[len = 0] = '{';
        json[++len]   = '"';

        while (len < 65538)
            json[++len] = 'm';

        json[++len] = '"';
        ok(!sxe_jitson_new(json), "Failed to parse member name of 64K chanracters (error %s)", strerror(errno));
        errno = 0;
    }

    diag("Cover type to string");
    {
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_INVALID), "INVALID", "INVALID type");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_NUMBER),  "number",  "number");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_STRING),  "string",  "string");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_OBJECT),  "object",  "object");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_ARRAY),   "array",   "array");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_BOOL),    "bool",    "bool");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_NULL),    "null",    "null");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_MASK),    "ERROR",   "out of range type is an ERROR");
        is(errno,                                              ERANGE,    "Errno is ERANGE");
        errno = 0;
    }

    diag("Test object membership function, object duplication, and reencoding");
    {
        ok(jitson = sxe_jitson_new("{\"a\": 1, \"biglongname\": \"B\", \"c\": [2, 3], \"d\" : {\"e\": 4}, \"f\": true}"),
           "Parsed complex object (error %s)", strerror(errno));
        is(sxe_jitson_size(jitson), 16, "Object is %zu bytes", sizeof(struct sxe_jitson) * sxe_jitson_size(jitson));

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_OBJECT_GET_MEMBER);
        ok(!sxe_jitson_object_get_member(jitson, "a", 0),                   "Can't access object on failure to calloc index");
        MOCKFAIL_END_TESTS();

        ok(member = sxe_jitson_object_get_member(jitson, "a", 0),           "Object has a member 'a'");
        is(sxe_jitson_get_number(member), 1,                                "Member is the number 1");
        ok(!sxe_jitson_object_get_member(jitson, "biglongname", 1),         "Object has no member 'b'");
        ok(member = sxe_jitson_object_get_member(jitson, "biglongname", 0), "Object has a member 'biglongname'");
        is_eq(sxe_jitson_get_string(member, NULL), "B",                     "Member is the string 'B'");
        ok(member = sxe_jitson_object_get_member(jitson, "c", 0),           "Object has a member 'c'");
        is(sxe_jitson_len(member), 2,                                       "Member is an array of 2 elements");
        ok(member = sxe_jitson_object_get_member(jitson, "d", 1),           "Object has a member 'd'");
        is(sxe_jitson_len(member), 1,                                       "Member is an object with 1 member");
        ok(member = sxe_jitson_object_get_member(jitson, "f", 0),           "Object has a member 'f'");
        ok(sxe_jitson_get_bool(member),                                     "Member is 'true'");

        /* Test duplication
         */
        ok(clone = sxe_jitson_dup(jitson),                        "Duplicated the object");
        is(5, sxe_jitson_len(clone),                              "Clone has 5 members too");
        ok(member  = sxe_jitson_object_get_member(clone, "c", 0), "One of the members is 'c'");
        ok(element = sxe_jitson_array_get_element(member, 1),     "Got second element of array member 'c'");
        is(sxe_jitson_get_uint(element), 3,                       "It's the unsigned integer 3");
        is_eq(json_out = sxe_jitson_to_json(clone, NULL),
              "{\"f\":true,\"a\":1,\"c\":[2,3],\"biglongname\":\"B\",\"d\":{\"e\":4}}",
              "Encoder spat out same JSON as we took in");
        sxe_jitson_free(clone);

        is(sxe_jitson_size(jitson), 16, "Objects can be sized once indexed");
        sxe_jitson_free(jitson);
        kit_free(json_out);
    }

    diag("Test array element function and reencoding");
    {
        ok(jitson = sxe_jitson_new("[0, \"anotherlongstring\", {\"member\": null}, true]"),
           "Parsed complex array (error %s)", strerror(errno));

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_ARRAY_GET_ELEMENT);
        ok(!sxe_jitson_array_get_element(jitson, 0),                        "Can't access array on failure to malloc index");
        MOCKFAIL_END_TESTS();

        ok(element = sxe_jitson_array_get_element(jitson, 0),               "Array has a element 0");
        is(sxe_jitson_get_number(element), 0,                               "Element is the number 0");
        ok(element = sxe_jitson_array_get_element(jitson, 1),               "Array has a element 1");
        is_eq(sxe_jitson_get_string(element, NULL), "anotherlongstring",    "Element is the string 'anotherlongstring'");
        ok(element = sxe_jitson_array_get_element(jitson, 2),               "Array has a element 2");
        is(sxe_jitson_get_type(element), SXE_JITSON_TYPE_OBJECT,            "Elememt is an object");
        ok(element = sxe_jitson_array_get_element(jitson, 3),               "Array has a element 3");
        ok(sxe_jitson_get_bool(element),                                    "Element is 'true'");
        ok(!sxe_jitson_array_get_element(jitson, 4),                        "Object has no element 4");

        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[0,\"anotherlongstring\",{\"member\":null},true]",
              "Encoder spat out same JSON as we took in");
        is(sxe_jitson_size(jitson), 8, "Arrays can be sized once indexed");
        sxe_jitson_free(jitson);
        kit_free(json_out);
    }

    diag("Test bug fixes against regressions");
    {
        ok(jitson = sxe_jitson_new("{\"A.D.\": 1, \"x\": 0}"),       "Parsed problem member name (error %s)", strerror(errno));
        ok(member = sxe_jitson_object_get_member(jitson, "A.D.", 0), "Object has a member 'A.D.'");
        is(sxe_jitson_get_type(member), SXE_JITSON_TYPE_NUMBER,      "A.D.'s value is a number");
        sxe_jitson_free(jitson);

        // Test DPT-1404b (an object that contains an empty object)
        ok(jitson = sxe_jitson_new("{\"catalog\":{\"osversion-current\":{}, \"version\": 1}}"), "Parsed object containing {}");
        ok(!sxe_jitson_object_get_member(jitson, "osversion-current", 0), "Object has no member 'osversion-current'");
        sxe_jitson_free(jitson);

        // Test DPT-1408.1 (sxe_jitson_stack_clear should clear the open collection index)
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened an object on the stack");
        sxe_jitson_stack_clear(stack);
        is(stack->open , 0, "Open collection flag was cleared");

        // Test DPT-1408.2 (sxe_jitson_stack_parse_string should clear the stack on failure)
        ok(!sxe_jitson_stack_parse_json(stack, "\""), "Failed to parse an unterminated string");
        is(stack->count, 0, "Stack was cleared");

        // Test DPT-1408.3 (sxe_jitson_stack_parse_string should clear the stack on failure)
        ok(!sxe_jitson_stack_parse_json(stack, "{"), "Failed to parse a truncated object");
        is(stack->count, 0, "Stack was cleared");

        // Test DPT-1408.4 (It should be possible to construct an object with an array as a member)
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened an object on the stack");
        ok(sxe_jitson_stack_add_member_name(stack, "endpoint.certificates", SXE_JITSON_TYPE_IS_COPY), "Add a member");
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),  "Member's value is an empty array");
        ok(sxe_jitson_stack_close_collection(stack),                        "Close array - can't fail");
        ok(sxe_jitson_stack_close_collection(stack),                        "Close outer object - can't fail");
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                     "Got the object from the stack");
        sxe_jitson_free(jitson);

        // Test DPT-1408.5 (sxe_jitson_object_get_member should allow non-NUL terminated member names)
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT),       "Opened an object on the stack");
        ok(sxe_jitson_stack_add_member_number(stack, "a", 1),                     "Add member 'a' value 1");
        sxe_jitson_stack_close_collection(stack);                               // Close object
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                           "Got the object from the stack");
        ok(sxe_jitson_object_get_member(jitson, "a+", 1),                         "Got the member with non-terminated name");

        // Test DPT-1408.8 (Need to be able to duplicate into a collection)
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),        "Opened an array on the stack");
        ok(sxe_jitson_stack_add_dup(stack, jitson),                               "Duplicated previous test object in array");
        ok(sxe_jitson_stack_add_null(stack),                                      "Followed by a null");
        sxe_jitson_stack_close_collection(stack);                               // Close array
        ok(clone   = sxe_jitson_stack_get_jitson(stack),                          "Got the array from the stack");
        ok(element = sxe_jitson_array_get_element(clone, 0),                      "Got the cloned first element");
        ok(member  = sxe_jitson_object_get_member(element, "a", 0),               "Got the member 'a' from it");
        is(sxe_jitson_get_number(member), 1,                                      "It's value is correct");
        ok(!sxe_jitson_is_allocated(element), "Cloned object isn't marked allocated even tho the object cloned was");
        sxe_jitson_free(clone);

        // Test DPT-1408.9 (sxe_dup should duplicate referred to jitson values)
        struct sxe_jitson reference[1];
        sxe_jitson_make_reference(reference, jitson);
        clone = sxe_jitson_dup(reference);
        ok(!sxe_jitson_is_reference(clone), "The duped reference is not itself a reference");
        sxe_jitson_free(clone);

        sxe_jitson_free(jitson);

        // Test bug found by Dejan in DPT-1422 (more than one unicode escape sequence)
        ok(jitson = sxe_jitson_new("\"\\u000AONE\\u000ATWO\""),  "Parsed '\"\\u000AONE\\u000ATWO\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_STRING,  "'\"\\u000AONE\\u000ATWO\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "\nONE\nTWO", "Correct UTF-8 value");
        sxe_jitson_free(jitson);

        // Test DPT-1702 (can't create a reference to an allocated reference)
        struct sxe_jitson *ref;
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),       "Opened an array on the stack");
        ok(sxe_jitson_stack_add_string(stack, "hello", SXE_JITSON_TYPE_IS_COPY), "Added \"hello\" to it");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                          "Got the array from the stack");
        ok(ref       = sxe_jitson_create_reference(jitson),                      "Created a reference to the JSON [\"hello\"]");
        ok(clone     = sxe_jitson_create_reference(ref),                         "Created a clone of the reference");
        ok(json_out  = sxe_jitson_to_json(clone, NULL),                          "Converted the cloned reference to JSON text");
        is_eq(json_out, "[\"hello\"]",                                           "It's '[\"hello\"]'");
        kit_free(json_out);
        sxe_jitson_free(clone);
        sxe_jitson_free(ref);
        sxe_jitson_free(jitson);
    }

    diag("Test stack boundaries");
    {
        struct sxe_jitson *j2;

        // Verify that our NUL doesn't overflow
        ok(jitson = sxe_jitson_new("\"12345678\""),  "Parsed '\"12345678\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_STRING,  "'\"12345678\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "12345678", "Correct value");

        ok(j2 = sxe_jitson_new("\"2nd\""),  "Parsed '\"2nd\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(j2), SXE_JITSON_TYPE_STRING,  "'\"2nd\"' is a string");
        is_eq(sxe_jitson_get_string(j2, NULL), "2nd", "Correct value");

        is_eq(sxe_jitson_get_string(jitson, NULL), "12345678", "The first one is still the correct value");

        sxe_jitson_free(jitson);
        sxe_jitson_free(j2);
    }

    diag("Test simple construction");
    {
        struct sxe_jitson primitive[1];

        sxe_jitson_make_null(primitive);
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_NULL, "null is null");
        ok(!sxe_jitson_test(primitive),                          "null tests false");

        sxe_jitson_make_bool(primitive, true);
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_BOOL, "true is a bool");
        ok(sxe_jitson_test(primitive),                           "true tests true");

        sxe_jitson_make_number(primitive, 1.3E100);
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_NUMBER, "1.3E100 is a number");
        ok(sxe_jitson_test(primitive),                             "1.3E100 tests true");

        sxe_jitson_make_number(primitive, 1.0);
        is(sxe_jitson_get_uint(primitive), 1,                      "Number 1.0 is uint 1");

        sxe_jitson_make_string_ref(primitive, "hello, world");
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_STRING,    "A string_ref is a string");
        is_eq(sxe_jitson_get_string(primitive, NULL), "hello, world", "String_refs values can be retrieved");
        is(primitive->len, 0,                                         "String_refs don't know their lengths on creation");
        ok(sxe_jitson_test(primitive),                                "Non-empty string ref is true");
        is(sxe_jitson_len(primitive), 12,                             "String_ref is 12 characters");
        is(primitive->len, 12,                                        "String_refs cache their lengths");
        sxe_jitson_make_string_ref(primitive, "");
        ok(!sxe_jitson_test(primitive),                               "Empty string_ref tests false");
    }

    diag("Test object construction and duplication failures");
    {
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened an object on the stack");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                     "Got the object from the stack");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "{}",            "Look, it's an empty object");
        kit_free(json_out);

        ok(clone = sxe_jitson_dup(jitson),                     "Cloned an empty array");
        is(sxe_jitson_get_type(clone), SXE_JITSON_TYPE_OBJECT, "Clone is an '%s' (expected 'object')",
                                                               sxe_jitson_get_type_as_str(clone));
        is(sxe_jitson_len(clone), 0,                           "Clone has 0 length (it is empty)");
        sxe_jitson_free(clone);

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened another object on the stack");
        ok(sxe_jitson_stack_add_dup_members(stack, jitson),                 "Added duplicates of members of the empty object");
        ok(sxe_jitson_stack_add_member_number(stack, "pi", 3.14159),        "Added member 'pi'");
        sxe_jitson_stack_close_collection(stack);
        ok(clone = sxe_jitson_stack_get_jitson(stack),                      "Got the extended clone from the stack");
        is(sxe_jitson_len(clone), 1,                                        "It has only 1 member");
        ok(sxe_jitson_object_get_member(clone, "pi", sizeof("pi") - 1),     "It has the additional member");
        sxe_jitson_free(clone);
        sxe_jitson_free(jitson);

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT),                "Opened an object on the stack");
        ok(sxe_jitson_stack_add_member_name(stack, "null", SXE_JITSON_TYPE_IS_REF),        "Added a member name reference");
        ok(sxe_jitson_stack_add_null(stack),                                               "Added a null value");
        ok(sxe_jitson_stack_add_member_bool(stack, "bool", true),                          "Added a member 'bool' value true");
        ok(sxe_jitson_stack_add_member_number(stack, "number", 1.14159),                   "Added a member number in one call");
        ok(sxe_jitson_stack_add_member_name(stack, "hello.world", SXE_JITSON_TYPE_IS_REF), "Added long member name reference");
        ok(sxe_jitson_stack_add_string(stack, kit_strdup("hello, world"), SXE_JITSON_TYPE_IS_OWN),
           "Added long string owned reference");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson  = sxe_jitson_stack_get_jitson(stack),              "Got the object from the stack");
        ok(element = sxe_jitson_object_get_member(jitson, "bool", 0), "Got the 'bool' member");
        ok(clone   = sxe_jitson_dup(element),                         "Cloned the bool member");
        ok(sxe_jitson_is_allocated(clone),                            "Clone is allocated even though value wasn't");
        sxe_jitson_free(clone);
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL),
              "{\"hello.world\":\"hello, world\",\"number\":1.14159,\"bool\":true,\"null\":null}", "Got the expected object");
        kit_free(json_out);

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened another object on the stack");
        ok(sxe_jitson_stack_add_dup_members(stack, jitson),                 "Added duplicates of members");
        ok(sxe_jitson_stack_add_member_number(stack, "pi", 3.14159),        "Added member 'pi'");
        sxe_jitson_stack_close_collection(stack);
        ok(clone = sxe_jitson_stack_get_jitson(stack),                      "Got the extended clone from the stack");
        ok(sxe_jitson_object_get_member(clone, "hello.world", 0),           "It has a member from the cloned object");
        ok(sxe_jitson_object_get_member(clone, "pi", sizeof("pi") - 1),     "It has the additional member");
        is_eq(json_out = sxe_jitson_to_json(clone, NULL),
              "{\"bool\":true,\"null\":null,\"pi\":3.14159,\"hello.world\":\"hello, world\",\"number\":1.14159}",
              "Got the expected object");
        kit_free(json_out);
        sxe_jitson_free(clone);

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_DUP);
        ok(!sxe_jitson_dup(jitson), "Can't duplicate an object if malloc fails");
        MOCKFAIL_END_TESTS();
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_OBJECT_CLONE);
        ok(!sxe_jitson_dup(jitson), "Can't clone an object if malloc fails");
        MOCKFAIL_END_TESTS();
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STRING_CLONE);
        ok(!sxe_jitson_dup(jitson), "Can't clone an object that contains an owned string if strdup returns NULL");
        MOCKFAIL_END_TESTS();

        stack = sxe_jitson_stack_get_thread();
        stack->maximum = 1;    // Reset the initial maximum stack size to allow testing the following out of memory conditions
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened yet another object on the stack");
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        ok(!sxe_jitson_stack_add_dup_members(stack, jitson), "Can't duplicate members if stack can't be expanded");
        MOCKFAIL_END_TESTS();
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STRING_CLONE);
        ok(!sxe_jitson_stack_add_dup_members(stack, jitson), "Can't dup members that include an owned string if strdup fails");
        MOCKFAIL_END_TESTS();
        sxe_jitson_stack_clear(stack);

        sxe_jitson_free(jitson);
    }

    diag("Test array construction and string edge cases");
    {
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),  "Opened an array on the stack");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                     "Got the array from the stack");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[]",            "Look, it's an empty array");
        kit_free(json_out);

        ok(clone = sxe_jitson_dup(jitson),                "Cloned an empty array");
        is_eq(sxe_jitson_get_type_as_str(clone), "array", "Clone is an array");
        is(sxe_jitson_len(clone), 0,                      "Clone has 0 length (it is empty)");
        sxe_jitson_free(clone);
        sxe_jitson_free(jitson);

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),          "Opened an array on the stack");
        ok(sxe_jitson_stack_add_string(stack, "shortly",  SXE_JITSON_TYPE_IS_COPY), "Added a copy of a short string");
        ok(sxe_jitson_stack_add_string(stack, "longerly", SXE_JITSON_TYPE_IS_COPY), "Added a copy of a longer string");
        ok(sxe_jitson_stack_add_string(stack, "longer than 23 characters", SXE_JITSON_TYPE_IS_COPY),
           "Added a copy of a long string needing more than 2 tokens");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),    "Got the array from the stack");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[\"shortly\",\"longerly\",\"longer than 23 characters\"]",
              "Got the expected JSON");
        kit_free(json_out);

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_ARRAY_CLONE);
        ok(!sxe_jitson_dup(jitson), "Can't clone an array if malloc fails");
        MOCKFAIL_END_TESTS();

        ok(clone = sxe_jitson_dup(jitson),                "Cloned a non-empty array");
        sxe_jitson_free(jitson);
        is_eq(sxe_jitson_get_type_as_str(clone), "array", "Clone is an array");
        is(sxe_jitson_len(clone), 3,                      "Clone has expected length");
        sxe_jitson_free(clone);

        stack->maximum = 1;   // Set initial maximum back to 1
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),   "Opened an array on the stack");
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        ok(!sxe_jitson_stack_add_string(stack, "", SXE_JITSON_TYPE_IS_REF),  "Failed to add a string ref expanding in add_value");
        MOCKFAIL_END_TESTS();
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        ok(!sxe_jitson_stack_add_string(stack, "", SXE_JITSON_TYPE_IS_COPY), "Failed to add a string copy expanding in add_value");
        MOCKFAIL_END_TESTS();
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_EXPAND);
        MOCKFAIL_SET_FREQ(2);    // Fail on the second attempt to expand the stack
        ok(!sxe_jitson_stack_add_string(stack, "i'm too long", SXE_JITSON_TYPE_IS_COPY), "Failed to add a long string");
        MOCKFAIL_END_TESTS();
        ok(sxe_jitson_stack_add_null(stack),                                "Added null after failed string adds");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                                    "Got the array from the stack");
        is_eq(sxe_jitson_get_type_as_str(sxe_jitson_array_get_element(jitson, 0)), "null", "Able to retreive null from array");
        sxe_jitson_free(jitson);
    }

    diag("Test references and cloning a string that's an owned reference");
    {
        struct sxe_jitson  reference[1];

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),                  "Opened an array on the stack");
        ok(sxe_jitson_stack_add_string(stack, "one",   SXE_JITSON_TYPE_IS_COPY),            "Added a copy of a string");
        ok(sxe_jitson_stack_add_string(stack, "two",   SXE_JITSON_TYPE_IS_REF),             "Added a weak reference to a string");
        ok(sxe_jitson_stack_add_string(stack, kit_strdup("three"), SXE_JITSON_TYPE_IS_OWN), "Strong (ownership) ref to a string");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),    "Got the array from the stack");
        is(sxe_jitson_size(jitson), 4,                     "Array of 3 short strings is 4 jitsons");

        sxe_jitson_make_reference(reference, jitson);
        is(sxe_jitson_get_type(reference), SXE_JITSON_TYPE_ARRAY, "A reference to an array has type array");
        ok(sxe_jitson_test(reference),                            "A reference to a non-empty array tests true");
        is(sxe_jitson_size(reference), 1,                         "A reference to a non-empty array requires only 1 jitson");
        is(sxe_jitson_len(reference), 3,                          "A reference to an array has len of the array");
        json_out = sxe_jitson_to_json(reference, NULL);
        is_eq(json_out, "[\"one\",\"two\",\"three\"]",            "A reference to an array's json the array's");
        kit_free(json_out);
        ok(member = sxe_jitson_array_get_element(reference, 2),   "Can get a element of an array from a reference");
        is_eq(sxe_jitson_get_string(member, NULL), "three",       "Got the correct element");

        MOCKFAIL_START_TESTS(2, MOCK_FAIL_STRING_CLONE);
        ok(!sxe_jitson_dup(member), "Can't clone a string if strdup returns NULL");
        ok(!sxe_jitson_dup(jitson), "Can't clone an array that contains an owned string if strdup returns NULL");
        MOCKFAIL_END_TESTS();

        ok(clone = sxe_jitson_dup(member),                                            "Cloned the owned string");
        is_eq(sxe_jitson_get_string(member, NULL), "three",                           "Got the correct content");
        ok(sxe_jitson_get_string(member, NULL) != sxe_jitson_get_string(clone, NULL), "The owned string is duplicated");
        sxe_jitson_free(clone);
        sxe_jitson_free(reference);

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),                 "Opened another array on the stack");
        ok(sxe_jitson_stack_add_reference(stack, sxe_jitson_array_get_element(jitson, 2)), "Added a reference to an array");
        sxe_jitson_stack_close_collection(stack);
        ok(array = sxe_jitson_stack_get_jitson(stack),      "Got the array from the stack");
        ok(member = sxe_jitson_array_get_element(array, 0), "Got the reference from the array");
        is_eq(sxe_jitson_get_string(member, NULL), "three", "Got the correct element");
        is(member->type, SXE_JITSON_TYPE_REFERENCE,         "Raw type of reference is %s (expected REFERENCE)",
                                                            sxe_jitson_type_to_str(member->type));
        sxe_jitson_free(array);
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_ARRAY, "After freeing all references, array is still an array");
        sxe_jitson_free(jitson);
    }

    diag("Test a non-NUL terminated source");
    {
        sxe_jitson_source_from_buffer(&source, "{}", 1, SXE_JITSON_FLAG_STRICT);
        ok(!sxe_jitson_stack_load_json(stack, &source), "Length is respected, truncating valid JSON to invalid");
    }

    diag("Test cmp (comparison) functions");
    {
        sxe_jitson_make_uint(&prim_left,  0);
        sxe_jitson_make_uint(&prim_right, 1);
        is(sxe_jitson_cmp(&prim_left, &prim_right), -1,                   "0 cmp 1 == -1");

        sxe_jitson_make_number(&prim_right, 0.0);
        is(sxe_jitson_cmp(&prim_left, &prim_right), 0,                    "0 cmp 0.0 == 0");

        sxe_jitson_make_number(&prim_right, 0.1);
        is(sxe_jitson_cmp(&prim_left, &prim_right), -1,                   "0 cmp 0.1 == -1");

        sxe_jitson_make_uint(&prim_left, ~0ULL);
        is(sxe_jitson_cmp(&prim_left, &prim_right), SXE_JITSON_CMP_ERROR, "Cannot compare 18446744073709551615 with 0.1");

        sxe_jitson_make_number(&prim_left, 0.2);
        is(sxe_jitson_cmp(&prim_left, &prim_right), 1,                    "0.2 cmp 0.1 == 1");

        sxe_jitson_make_uint(&prim_right, 1);
        is(sxe_jitson_cmp(&prim_left, &prim_right), -1,                   "0.2 cmp 1 == -1");

        sxe_jitson_make_number(&prim_left, 2.0);
        is(sxe_jitson_cmp(&prim_left, &prim_right), 1,                    "2.0 cmp 1 == 1");

        sxe_jitson_make_number(&prim_left,  -1);
        sxe_jitson_make_uint(  &prim_right, ~0ULL);
        is(sxe_jitson_cmp(&prim_left, &prim_right), SXE_JITSON_CMP_ERROR, "Cannot compare -1 with 18446744073709551615");

        sxe_jitson_make_string_ref(&prim_left,  "10");
        sxe_jitson_make_string_ref(&prim_right, "5");
        is(sxe_jitson_cmp(&prim_left, &prim_right), -1,                   "\"10\" cmp \"5\" == -1");

        clone  = sxe_jitson_new("[1]");
        jitson = sxe_jitson_new("[]");
        is(sxe_jitson_cmp(clone, jitson), 1,                              "[1] is prefixed by []");

        sxe_jitson_free(jitson);
        jitson = sxe_jitson_new("[0,\"x\"]");
        is(sxe_jitson_cmp(clone, jitson),                 1, "[1] cmp [0,\"x\"] == 1");

        sxe_jitson_free(clone);
        clone = sxe_jitson_new("[0]");
        is(sxe_jitson_cmp(clone, jitson),                -1, "[0] prefixes [0,\"x\"]");

        sxe_jitson_free(jitson);
        jitson = sxe_jitson_new("[0]");
        is(sxe_jitson_cmp(clone, jitson),                 0, "[0] cmp [0] == 0");
        is(sxe_jitson_cmp(clone, &prim_right),      SXE_JITSON_CMP_ERROR, "Cannot compare an array to a string");
        is(sxe_jitson_cmp(clone, NULL),             SXE_JITSON_CMP_ERROR, "Cannot compare an array if there is nothing to compare to");

        sxe_jitson_free(jitson);
        sxe_jitson_free(clone);
    }

    diag("Test orderred arrays");
    {
        test_parsing_ordered_array("[]",        false);    // Trivial arrays are not considered ordered
        test_parsing_ordered_array("[null]",    false);    // Trivial arrays are not considered ordered
        test_parsing_ordered_array("[0,1]",     true);
        test_parsing_ordered_array("[0,0]",     true);
        test_parsing_ordered_array("[1,0]",     false);
        test_parsing_ordered_array("[1,\"2\"]", false);

        sxe_jitson_flags |= SXE_JITSON_FLAG_OPTIMIZE;    // Check arrays for order by default

        test_constructing_ordered_array("[]",            true,  NULL);
        test_constructing_ordered_array("[\"0\"]",       true,  "0", NULL);
        test_constructing_ordered_array("[\"0\",\"1\"]", true,  "0", "1", NULL);
        test_constructing_ordered_array("[\"0\",\"0\"]", true,  "0", "0", NULL);
        test_constructing_ordered_array("[\"1\",\"0\"]", false, "1", "0", NULL);

        SXEA1(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),    "Failed to open tuple");
        SXEA1(sxe_jitson_stack_add_uint(stack, 2),                               "Failed to add org to tuple");
        SXEA1(sxe_jitson_stack_add_uint(stack, 0),                               "Failed to add parent to tuple");
        SXEA1(sxe_jitson_stack_add_string(stack, "0:1", SXE_JITSON_TYPE_IS_REF), "Failed to add list id to tuple");
        sxe_jitson_stack_close_collection(stack);
        SXEA1((jitson = sxe_jitson_stack_get_jitson(stack)),                     "Failed to get tuple from stack");
        ok(!(jitson->type & SXE_JITSON_TYPE_IS_ORD),                             "Mixed tuple is not ordered");
        ok(!(jitson->type & SXE_JITSON_TYPE_IS_HOMO),                            "Mixed tuple is not homogenous");
        ok(jitson->type & SXE_JITSON_TYPE_IS_UNIF,                               "Mixed tuple is uniform");
        sxe_jitson_free(jitson);

        SXEA1(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),             "Failed to open tuple");
        SXEA1(sxe_jitson_stack_add_string(stack, "short", SXE_JITSON_TYPE_IS_COPY),       "Failed to add short to tuple");
        SXEA1(sxe_jitson_stack_add_string(stack, "long_string", SXE_JITSON_TYPE_IS_COPY), "Failed to add long_string to tuple");
        sxe_jitson_stack_close_collection(stack);
        SXEA1((jitson = sxe_jitson_stack_get_jitson(stack)),                              "Failed to get tuple from stack");
        ok(!(jitson->type & SXE_JITSON_TYPE_IS_ORD),                                      "Non-uniform tuple is not ordered");
        ok(jitson->type & SXE_JITSON_TYPE_IS_HOMO,                                        "Non-uniform tuple is homogenous");
        ok(!(jitson->type & SXE_JITSON_TYPE_IS_UNIF),                                     "Non-uniform tuple is not uniform");
        sxe_jitson_free(jitson);
    }

    diag("Test eq method");
    {
        sxe_jitson_make_null(&prim_left);
        is(sxe_jitson_eq(&prim_left, sxe_jitson_null), SXE_JITSON_TEST_TRUE,  "Constructed null eq builtin null");

        sxe_jitson_make_bool(&prim_left, false);
        is(sxe_jitson_eq(&prim_left, sxe_jitson_true), SXE_JITSON_TEST_FALSE, "Constructed false !eq builtin true");

        sxe_jitson_make_string_ref(&prim_left,  "not");
        sxe_jitson_make_string_ref(&prim_right, "equal");
        is(sxe_jitson_eq(&prim_left, &prim_right), SXE_JITSON_TEST_FALSE, "Compared two strings with unknown lengths");
        isnt(sxe_jitson_len(&prim_left), sxe_jitson_len(&prim_right),     "Their lengths differ");
        is(sxe_jitson_eq(&prim_left, &prim_right), SXE_JITSON_TEST_FALSE, "Equality of string of diff lengths is optimized");

        clone  = sxe_jitson_new("[null, 1]");
        jitson = sxe_jitson_new("[]");
        is(sxe_jitson_eq(clone, jitson), SXE_JITSON_TEST_FALSE, "Compare two arrays of differing lengths");

        sxe_jitson_free(jitson);
        jitson = sxe_jitson_new("[null, 1.0]");
        is(sxe_jitson_eq(clone, jitson), SXE_JITSON_TEST_TRUE,  "Compare arrays with unorderable types and mixed float/unsigned");
        sxe_jitson_free(jitson);

        jitson = sxe_jitson_new("[false, 1.0]");
        is(sxe_jitson_eq(clone, jitson), SXE_JITSON_TEST_FALSE, "Inequality of arrays with incompatible types");
        sxe_jitson_free(jitson);
        sxe_jitson_free(clone);

        clone  = sxe_jitson_new("{\"x\": 1}");
        jitson = sxe_jitson_new("{}");
        is(sxe_jitson_eq(clone, jitson), SXE_JITSON_TEST_ERROR, "Compare two objects of differing lengths isn't supported");
        sxe_jitson_free(jitson);
        sxe_jitson_free(clone);

        clone  = sxe_jitson_new("[-1]");                       // Negative numbers are stored as doubles
        jitson = sxe_jitson_new("[18446744073709551615]");     // ULONG_MAX can't be converted to a double
        is(sxe_jitson_eq(clone, jitson), SXE_JITSON_TEST_ERROR, "Compare two arrays containing incompatible types");
        sxe_jitson_free(jitson);
        sxe_jitson_free(clone);
    }

    diag("Test insertion sorted arrays");
    {
        sxe_jitson_flags &= ~SXE_JITSON_FLAG_OPTIMIZE;    // Make sure insertion sorting works without enabling optimization

        SXEA1(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY | SXE_JITSON_TYPE_MK_SORT), "Opened an array");
        is(stack->maximum, 4,                                               "Stack has expected maximum size");
        for (unsigned i = 32; i >= 1; i--) { // This forces every insert to do a memmove and guarantees the stack has to grow several times (DPT-2004)
            SXEA1(sxe_jitson_stack_add_uint(stack, i),                      "Failed to add %u to array", i);
        }
        is(stack->maximum, 64,                                              "Stack has expected maximum size after growth");
        sxe_jitson_stack_close_collection(stack);
        SXEA1((jitson = sxe_jitson_stack_get_jitson(stack)),                "Failed to get array from stack");
        ok(jitson->type & SXE_JITSON_TYPE_IS_ORD,                           "Array is ordered");
        is(sxe_jitson_get_uint(sxe_jitson_array_get_element(jitson, 0)), 1, "First element is 1");
        is(sxe_jitson_get_uint(sxe_jitson_array_get_element(jitson, 1)), 2, "Second element is 2");
        sxe_jitson_free(jitson);

        SXEA1(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY | SXE_JITSON_TYPE_MK_SORT), "Opened an array");
        SXEA1(sxe_jitson_stack_add_string(stack, "alpha", SXE_JITSON_TYPE_IS_REF),           "Failed to add 'alpha' to array");
        SXEA1(sxe_jitson_stack_add_string(stack, "beta",  SXE_JITSON_TYPE_IS_REF),           "Failed to add 'beta' to array");
        SXEA1(sxe_jitson_stack_add_string(stack, "gamma", SXE_JITSON_TYPE_IS_REF),           "Failed to add 'gamma' to array");
        SXEA1(sxe_jitson_stack_add_string(stack, "delta", SXE_JITSON_TYPE_IS_REF),           "Failed to add 'delta' to array");
        sxe_jitson_stack_close_collection(stack);
        SXEA1((jitson = sxe_jitson_stack_get_jitson(stack)),                                 "Failed to get array from stack");
        ok(jitson->type & SXE_JITSON_TYPE_IS_ORD,                                            "Array is ordered");
        is_eq(sxe_jitson_get_string(sxe_jitson_array_get_element(jitson, 2), NULL), "delta", "Third element is 'delta'");
        is_eq(sxe_jitson_get_string(sxe_jitson_array_get_element(jitson, 3), NULL), "gamma", "Fourth element is 'gamma'");
        is(sxe_jitson_size(jitson), 5,                                                       "Array is 5 jitsons long");
        sxe_jitson_free(jitson);
    }

    diag("Test stuff with optimization enabled");
    {
        sxe_jitson_flags |= SXE_JITSON_FLAG_OPTIMIZE;    // Reenable optimization

        ok(sxe_jitson_stack_open_collection(  stack, SXE_JITSON_TYPE_OBJECT),                                "Opened object");
        ok(sxe_jitson_stack_add_member_string(stack, "endpoint.os.type",    "win", SXE_JITSON_TYPE_IS_COPY), "Added os.type");
        ok(sxe_jitson_stack_add_member_string(stack, "endpoint.os.version", "10",  SXE_JITSON_TYPE_IS_COPY), "Added version");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack), "Got the constructed object from the stack");
        sxe_jitson_free(jitson);

        /* Make sure sorted array construction works if the sorted array isn't the first thing on the stack
         */
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),                            "Opened outer array");
        ok(sxe_jitson_stack_add_string(stack, "I am a very very very very long string", SXE_JITSON_TYPE_IS_COPY),
           "Added a copy of a verrrry long string");
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY | SXE_JITSON_TYPE_MK_SORT),  "Opened inner array");
        ok(sxe_jitson_stack_add_uint(stack, 2),                                                       "Added 2 to array");
        ok(sxe_jitson_stack_add_uint(stack, 1),                                                       "Added 1 to array");
        sxe_jitson_stack_close_collection(stack);
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),       "Got array from stack");
        ok(!(jitson->type & SXE_JITSON_TYPE_IS_ORD),          "Outer array is not ordered");
        ok(element = sxe_jitson_array_get_element(jitson, 1), "Got inner array");

        is(element->type & (SXE_JITSON_TYPE_IS_ORD | SXE_JITSON_TYPE_IS_UNIF | SXE_JITSON_TYPE_IS_HOMO),
           SXE_JITSON_TYPE_IS_ORD | SXE_JITSON_TYPE_IS_UNIF | SXE_JITSON_TYPE_IS_HOMO,
           "Inner array is ordered with uniformly sized, homogenously typed elements");
        is(element->uniform.type, SXE_JITSON_TYPE_NUMBER,     "And contains numbers");
        sxe_jitson_free(jitson);

        struct {
            const char *name;
            unsigned count;
            unsigned in[6];
            unsigned out[6];
        } tests[] = {
            { "umbrella.source.organization_ids", 1, { 42 }, { 42 } },
            { "umbrella.source.identity_ids", 0, { }, { } },
            { "umbrella.source.identity_type_ids", 6, { 100000, 99999, 10001, 0, 1 << 31, 1 << 30 }, { 0, 10001, 99999, 100000, 1 << 30, 1 << 31 } },
        };
        unsigned i, t;

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened a collection");
        for (t = 0; t < sizeof(tests) / sizeof(*tests); t++) {
            sxe_jitson_stack_add_member_name(stack, tests[t].name, SXE_JITSON_TYPE_IS_REF);
            ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY | SXE_JITSON_TYPE_MK_SORT), "test %u: Opened a self-sorting array", t);
            for (i = 0; i < tests[t].count; i++)
                ok(sxe_jitson_stack_add_number(stack, tests[t].in[i]), "Added %u to the array", tests[t].in[i]);
            sxe_jitson_stack_close_collection(stack);
        }
        sxe_jitson_stack_close_collection(stack);

        ok(jitson = sxe_jitson_stack_get_jitson(stack), "Got the jitson");
        is(jitson->len, t, "The jitson contains %u elements", t);

        for (t = 0; t < sizeof(tests) / sizeof(*tests); t++) {
            const struct sxe_jitson *entry = sxe_jitson_object_get_member(jitson, tests[t].name, 0);
            ok(entry, "Got object '%s'", tests[t].name);
            ok(entry->type & SXE_JITSON_TYPE_IS_ORD, "The array is ordered");
            for (i = 0; i < tests[t].count; i++)
                is(sxe_jitson_get_number(sxe_jitson_array_get_element(entry, i)), tests[t].out[i],
                   "The '%s' value %u is correct", tests[t].name, i);
        }

        sxe_jitson_free(jitson);

        jitson = sxe_jitson_new("[0,[1,2,3],[4,5,6]]");
        is(jitson->type & (SXE_JITSON_TYPE_IS_ORD | SXE_JITSON_TYPE_IS_UNIF | SXE_JITSON_TYPE_IS_HOMO), 0,
           "Array is not ordered, uniformly sized, or homogenously typed");
        sxe_jitson_free(jitson);
    }

    diag("Test stuff with optimization enabled");
    {
        sxe_jitson_flags |= SXE_JITSON_FLAG_OPTIMIZE;    // Reenable optimization

        ok(sxe_jitson_stack_open_collection(  stack, SXE_JITSON_TYPE_OBJECT),                                "Opened object");
        ok(sxe_jitson_stack_add_member_string(stack, "endpoint.os.type",    "win", SXE_JITSON_TYPE_IS_COPY), "Added os.type");
        ok(sxe_jitson_stack_add_member_string(stack, "endpoint.os.version", "10",  SXE_JITSON_TYPE_IS_COPY), "Added version");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack), "Got the constructed object from the stack");
        sxe_jitson_free(jitson);
    }

    diag("Test stack to string diagnostic function");
    {
#if !SXE_DEBUG
        skip(9, "Skipping test of debug function test_stack_to_str");
#else
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT),                              "Opened an object");
        is_eq(sxe_jitson_stack_to_str(stack), "{}",                                                      "Stack dump's right");
        ok(sxe_jitson_stack_add_member_name(stack, "endpoint.os.type", SXE_JITSON_TYPE_IS_COPY),         "Added member name 1");
        is_eq(sxe_jitson_stack_to_str(stack), "{}",                                                      "Stack dump's right");
        ok(sxe_jitson_stack_add_string(stack,  "win", SXE_JITSON_TYPE_IS_COPY),                          "Added member value");
        is_eq(sxe_jitson_stack_to_str(stack), "{\"endpoint.os.type\":\"win\"",                           "Stack dump's right");
        ok(sxe_jitson_stack_add_member_name(stack, "endpoint.os.version", SXE_JITSON_TYPE_IS_REF),       "Added member name 2");
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),                               "Opened inner array");
        is_eq(sxe_jitson_stack_to_str(stack),"{\"endpoint.os.type\":\"win\",\"endpoint.os.version\":[]", "Stack dump's right");
        sxe_jitson_stack_clear(stack);
#endif
    }

    diag("Test ability to extend types and construct complex types");
    {
        is(NULL, sxe_jitson_type_get_extra(SXE_JITSON_TYPE_NULL),      "Initially, types have no extra data");
        sxe_jitson_type_set_extra(SXE_JITSON_TYPE_NULL, "NULL");
        is_eq("NULL", sxe_jitson_type_get_extra(SXE_JITSON_TYPE_NULL), "Type has expected extra data");

        SXEA1(sxe_jitson_stack_open_array(stack, "pair"),                                                   "Failed to open");
        SXEA1(sxe_jitson_stack_expand(stack, 1) != SXE_JITSON_STACK_ERROR,                                  "Failed to grow");
        SXEA1(sxe_jitson_make_uint(&stack->jitsons[stack->count - 1], 2),                                   "Failed to make 2");
        SXEA1(sxe_jitson_stack_add_value(stack, 1, SXE_JITSON_TYPE_NUMBER, NULL) != SXE_JITSON_STACK_ERROR, "Failed to add 2");
        SXEA1(sxe_jitson_stack_expand(stack, 1) != SXE_JITSON_STACK_ERROR,                                  "Failed to grow");
        SXEA1(sxe_jitson_make_uint(&stack->jitsons[stack->count - 1], 1),                                   "Failed to make 1");
        SXEA1(sxe_jitson_stack_add_value(stack, 1, SXE_JITSON_TYPE_NUMBER, NULL) != SXE_JITSON_STACK_ERROR, "Failed to add 1");
        SXEA1(sxe_jitson_stack_close_array(stack, "pair"),                                                  "Failed to close");
        SXEA1(jitson = sxe_jitson_stack_get_jitson(stack),                                                  "Failed to get");

        ok(!(jitson->type & SXE_JITSON_TYPE_IS_ORD), "Pair is not ordered");
        ok(jitson->type & SXE_JITSON_TYPE_IS_HOMO,   "Pair is homogenous");
        ok(jitson->type & SXE_JITSON_TYPE_IS_UNIF,   "Pair is uniform");
        sxe_jitson_free(jitson);
    }

    diag("Test for the bugs that led to DPT-3052");
    {
        ok(jitson = sxe_jitson_new("[\"I-fujitsu\\u30b5\\u30a4\\u30c8\",\"x\"]"),  "Parsed a string with unicode characters");
        ok(element = sxe_jitson_array_get_element(jitson, 0),                      "Got the first element");
        is_eq(sxe_jitson_get_string(element, NULL), "I-fujitsu\u30b5\u30a4\u30c8", "It looks right");
        ok(element = sxe_jitson_array_get_element(jitson, 1),                      "Got the next element");
        is_eq(sxe_jitson_get_type_as_str(element), "string",                       "It's a string");
        is_eq(sxe_jitson_get_string(element, NULL), "x",                           "It's the right one");
        sxe_jitson_free(jitson);

        /* Test that if a JSON string includes UTF-8 characters (technically not valid JSON) we continue to behave "as expected"
         */
        SXEA1(strlen("Navegaci\u00f3n expl\u00edcita") == 22, "Expected length");
        ok(jitson = sxe_jitson_new("[\"Navegaci\u00f3n expl\u00edcita\",\"x\"]"),     "Parsed a string with UTF-8 characters");
        ok(element = sxe_jitson_array_get_element(jitson, 0),                         "Got the first element");
        is_eq(sxe_jitson_get_string(element, NULL), "Navegaci\u00f3n expl\u00edcita", "It looks right");
        is(sxe_jitson_size(element), 2,                                               "23 byte string fits in 2 jitsons");
        ok(element = sxe_jitson_array_get_element(jitson, 1),                         "Got the next element");
        is_eq(sxe_jitson_get_type_as_str(element), "string",                          "It's a string");
        is_eq(sxe_jitson_get_string(element, NULL), "x",                              "It's the right one");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("[\"\\u624d\\u8302\\u3055\\u3093\\u30c6\\u30b9\\u30c8\\u30a4\\u30f3\\u30bf\\u30fc\\u30cd\\u30c3\\u30c82\",\"x\",\"y\"]"),
           "Parsed another string with unicode characters");
        ok(element = sxe_jitson_array_get_element(jitson, 0),                                     "Got the first element");
        is_eq(sxe_jitson_get_string(element, NULL), "\u624d\u8302\u3055\u3093\u30c6\u30b9\u30c8\u30a4\u30f3\u30bf\u30fc\u30cd\u30c3\u30c82", "It looks right");
        ok(element = sxe_jitson_array_get_element(jitson, 1),                                     "Got the next element");
        is_eq(sxe_jitson_get_type_as_str(element), "string",                                      "It's a string");
        is_eq(sxe_jitson_get_string(element, NULL), "x",                                          "It's the right one");
        sxe_jitson_free(jitson);
    }

    diag("Test thread-local indexing");
    {
        sxe_jitson_flags &= ~SXE_JITSON_FLAG_OPTIMIZE;    // Make sure optimization not enabled to test indexing

        ok(sxe_jitson_stack_open_local_object(stack, "obj"),                "Opened an object on the stack");
        ok(sxe_jitson_stack_add_member_name(stack, "ids", SXE_JITSON_TYPE_IS_COPY), "Add a member");
        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),  "Member's value is an array");
        ok(sxe_jitson_stack_add_uint(stack, 11),                            "Added 11 to the array");
        ok(sxe_jitson_stack_add_uint(stack, 12),                            "Added 12 to the array");
        ok(sxe_jitson_stack_close_collection(stack),                        "Close array - can't fail");
        ok(sxe_jitson_stack_close_collection(stack),                        "Close outer object - can't fail");
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                     "Got the object from the stack");
        ok(sxe_jitson_is_local(jitson),                                     "Object is thread-local");
        ok(!(jitson->type & SXE_JITSON_TYPE_INDEXED),                       "Object is not indexed");
        ok(member = sxe_jitson_object_get_member(jitson, "ids", 0),         "Got first member of the object 'ids'");
        ok(jitson->type & SXE_JITSON_TYPE_INDEXED,                          "Object is now indexed");
        is(sxe_jitson_len(member), 2,                                       "Member is an array of 2 elements");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "{\"ids\":[11,12]}", "Object is right");
        kit_free(json_out);
        sxe_jitson_stack_clear(stack);
        is(stack->count, 0,                                                 "Stack was cleared");
        sxe_jitson_free(jitson);

        ok(sxe_jitson_stack_open_local_array(stack, "ids"),                 "Opened an array on the stack");
        ok(sxe_jitson_stack_add_uint(stack, 21),                            "Added 21 to the array");
        ok(sxe_jitson_stack_add_uint(stack, 22),                            "Added 22 to the array");
        ok(sxe_jitson_stack_close_collection(stack),                        "Close array - can't fail");
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                     "Got the array from the stack");
        ok(sxe_jitson_is_local(jitson),                                     "Array is thread-local");
        ok(!(jitson->type & SXE_JITSON_TYPE_INDEXED),                       "Array is not indexed");
        ok(element = sxe_jitson_array_get_element(jitson, 0),               "Got first element of array 'ids'");
        ok(jitson->type & SXE_JITSON_TYPE_INDEXED,                          "Array is now indexed");
        is(sxe_jitson_get_uint(element), 21,                                "It's the unsigned integer 21");
        ok(element = sxe_jitson_array_get_element(jitson, 1),               "Got second element of array 'ids'");
        is(sxe_jitson_get_uint(element), 22,                                "It's the unsigned integer 22");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[21,22]",       "Array is right");
        kit_free(json_out);
        sxe_jitson_stack_clear(stack);
        is(stack->count, 0,                                                 "Stack was cleared");
        sxe_jitson_free(jitson);
    }

    diag("Tests for DPT-3141 (yet another unicode bug)");
    {
        ok(jitson = sxe_jitson_new("[\"Lagerh\\u00e4user ALLOW WLAN\",666]"), "Parsed [23 character UTF-8 string,666]");
        ok(element = sxe_jitson_array_get_element(jitson, 0),                 "Got the first element");
        is(sxe_jitson_len(element), 23,                                       "It's length is correct");
        ok(element = sxe_jitson_array_get_element(jitson, 1),                 "Got the second element");
        is(sxe_jitson_get_uint(element), 666,                                 "It's the number of the beast");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("[\"Lagerhxxuser ALLOW WLAN\",667]"),      "Parsed [23 character string,667]");
        ok(element = sxe_jitson_array_get_element(jitson, 0),                 "Got the first element");
        is(sxe_jitson_len(element), 23,                                       "It's length is correct");
        ok(element = sxe_jitson_array_get_element(jitson, 1),                 "Got the second element");
        is(sxe_jitson_get_uint(element), 667,                                 "It's the number of the beast + 1");
        sxe_jitson_free(jitson);

        unsigned   i;
        char       json_buf[64];
        const char head[] = "[\"abcdefghijklmnopqrstuvwxyz";
#       define HEAD_AND_UNICODE_LEN (sizeof(head) - 1 + sizeof("\\u00e4") - 1)

        /* Test the above string with \u00e4 inserted in every possible place
         */
        for (i = 2; i < sizeof(head); i++) {
            memcpy(json_buf,                             head,      i);
            memcpy(json_buf + i,                         "\\u00e4", sizeof("\\u00e4") - 1);
            strcpy(json_buf + i + sizeof("\\u00e4") - 1, head + i);
            snprintf(json_buf + HEAD_AND_UNICODE_LEN, sizeof(json_buf) - HEAD_AND_UNICODE_LEN, "\",%u]", i);

            ok(jitson = sxe_jitson_new(json_buf),                 "Parsed string with inserted 2 byte UTF-8");
            ok(element = sxe_jitson_array_get_element(jitson, 0), "Got the first element");
            is(sxe_jitson_len(element), 28,                       "It's length is correct");
            ok(element = sxe_jitson_array_get_element(jitson, 1), "Got the second element");
            is(sxe_jitson_get_uint(element), i,                   "It's the correct number");
            sxe_jitson_free(jitson);
        }
    }

    diag("Tests for DPT-3261 - sxe_jitson_stack_push_string_reversed");
    {
        ok(sxe_jitson_stack_open_array(stack, "pair"),                        "Opened array");
        ok(sxe_jitson_stack_add_value(stack, 0, SXE_JITSON_TYPE_STRING, NULL) != SXE_JITSON_STACK_ERROR,
           "Prepared to add the value");
        ok(sxe_jitson_stack_push_string_reversed(stack, "reverse", 0),        "Pushed reversed string");
        SXEA1(sxe_jitson_stack_value_added(stack, sxe_jitson_flags),          "Failed to add a value that's already pushed");
        ok(sxe_jitson_stack_add_uint(stack, 777),                             "Added a number");
        SXEA1(sxe_jitson_stack_close_array(stack, "pair"),                    "Failed to close the array");
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                       "Got the array from the stack");
        ok(element = sxe_jitson_array_get_element(jitson, 0),                 "Got the first element");
        is_eq(sxe_jitson_get_type_as_str(element), "string",                  "Reversed string is a string");
        ok(sxe_jitson_get_type(element) | SXE_JITSON_TYPE_REVERSED,           "Reversed flag is set");
        is_eq(sxe_jitson_get_string(element, &len), "esrever",                "Reversed string is 'esrever'");
        is(len, 7,                                                            "Length of 'esrever' is 7");
        is(sxe_jitson_get_uint(sxe_jitson_array_get_element(jitson, 1)), 777, "Got expected guard value");
        sxe_jitson_free(jitson);

        ok(sxe_jitson_stack_open_array(stack, "pair"),                                   "Opened array");
        ok(sxe_jitson_stack_add_value(stack, 0, SXE_JITSON_TYPE_STRING, NULL) != SXE_JITSON_STACK_ERROR,
           "Prepared to add the value");
        ok(sxe_jitson_stack_push_string_reversed(stack, "i'm backwards, you wooly", 24), "Pushed the reversed string");
        SXEA1(sxe_jitson_stack_value_added(stack, sxe_jitson_flags),                     "Failed to add value");
        ok(sxe_jitson_stack_add_uint(stack, 888),                                        "Added a number");
        SXEA1(sxe_jitson_stack_close_array(stack, "pair"),                               "Failed to close the array");
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                                  "Got the array from the stack");
        ok(element = sxe_jitson_array_get_element(jitson, 0),                            "Got the first element");
        is_eq(sxe_jitson_get_string(element, &len), "yloow uoy ,sdrawkcab m'i",          "Reversed string is correct");
        is(len, 24,                                                                      "Length is 24");
        is(sxe_jitson_get_uint(sxe_jitson_array_get_element(jitson, 1)), 888,            "Got expected guard value");
        sxe_jitson_free(jitson);
    }

    diag("Tests for DPT-3064 - Concatenate arrays");
    {
        ok(array  = sxe_jitson_new("[1, 2]"),                                                "Created array 1");
        ok(jitson = sxe_jitson_new("[3, 4]"),                                                "Created array 2");
        ok(sxe_jitson_stack_push_concat_array(stack, array, jitson, SXE_JITSON_TYPE_IS_OWN), "Pushed the concatenation");
        ok(clone = sxe_jitson_stack_get_jitson(stack),                                       "Got concatenated array from stack");
        is(sxe_jitson_len(clone), 4,                                                         "It's got 4 elements");
        is(sxe_jitson_get_uint(sxe_jitson_array_get_element(clone, 0)), 1,                   "Got the correct first element");
        is(sxe_jitson_get_uint(sxe_jitson_array_get_element(clone, 3)), 4,                   "Got the correct fourth element");
        is(sxe_jitson_size(clone), 2,                                                        "Concatenations take 2 jitsons");
        is_eq(json_out = sxe_jitson_to_json(clone, NULL), "[1,2,3,4]",                       "Array is right");
        is_eq(sxe_jitson_get_type_as_str(*(struct sxe_jitson **)(clone + 1)), "array",       "Memory layout looks correct");
        kit_free(json_out);
        sxe_jitson_free(clone);

        prim_left.type = prim_right.type = SXE_JITSON_TYPE_ARRAY;
        prim_left.len  = prim_right.len  = ~0;
        ok(!sxe_jitson_stack_push_concat_array(stack, &prim_left, &prim_right, SXE_JITSON_TYPE_IS_REF), "Overflow error");
        is(errno, EOVERFLOW,                                                                            "It's EOVERFLOW");
    }

    sxe_jitson_type_fini();
    sxe_thread_memory_free(SXE_THREAD_MEMORY_ALL);
    is(kit_memory_allocations(), start_allocations, "No memory was leaked");

    return exit_status();
}
