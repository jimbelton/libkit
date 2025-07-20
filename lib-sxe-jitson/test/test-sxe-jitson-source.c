/* Test the sxe-jitson source object
 */

#include <errno.h>
#include <tap.h>

#include "sxe-jitson.h"
#include "sxe-thread.h"

int
main(void)
{
    struct sxe_jitson_source source;
    size_t                   len;
    uint64_t                 start_allocations;

    plan_tests(28);
    start_allocations = kit_memory_allocations();
    // KIT_ALLOC_SET_LOG(1);    // Turn off when done

    diag("Test line number handling");
    {
        sxe_jitson_source_from_buffer(&source, "a\nb\n\nc", 5, SXE_JITSON_FLAG_STRICT);    // c is not included in the buffer
        sxe_jitson_source_set_file_line(&source, NULL, 1);
        is(sxe_jitson_source_get_nonspace(&source), 'a',   "Read 'a'");
        is(source.line, 1,                                 "On line 1");
        is(sxe_jitson_source_get_char(&source), '\n',      "Read '\\n'");
        is(source.line, 2,                                 "Now on line 2");
        is(sxe_jitson_source_get_nonspace(&source), 'b',   "Read 'b'");
        is(sxe_jitson_source_peek_char(&source), '\n',     "Peeked at '\\n'");
        is(source.line, 2,                                 "Still on line 2");
        sxe_jitson_source_consume(&source, 1);
        is(source.line, 3,                                 "Consumed '\\n' and now on line 3");
        is(sxe_jitson_source_peek_nonspace(&source), '\0', "Skipped final newline and returned EOF");
        is(sxe_jitson_source_get_char(&source), '\0',      "Read '\\0' at EOF");
    }

    diag("Test new error case");
    {
        struct sxe_jitson_stack *stack = sxe_jitson_stack_get_thread();

        sxe_jitson_source_from_string(&source, "not a string", SXE_JITSON_FLAG_STRICT);
        ok(!sxe_jitson_stack_load_string(stack, &source), "Successfully failed to load a non-string");
        is(EINVAL, errno,                                 "errno is EINVAL");
    }

    diag("Test get literal function");
    {
        const char *literal;

        sxe_jitson_source_from_string(&source, "\"boo\"", SXE_JITSON_FLAG_STRICT);
        is_eq(literal = sxe_jitson_source_get_literal(&source, &len), "\"boo\"", "Got the expected literal string");
        is(len, sizeof("\"boo\"") - 1,                                           "Got the expected length");

        sxe_jitson_source_from_string(&source, "not a literal", SXE_JITSON_FLAG_STRICT);
        ok(!sxe_jitson_source_get_literal(&source, NULL), "Correctly detected non-literal string");

        sxe_jitson_source_from_string(&source, "\"unterminated", SXE_JITSON_FLAG_STRICT);
        ok(!sxe_jitson_source_get_literal(&source, NULL), "Correctly detected unterminated literal string");
    }

    diag("Test diagnostic function");
    {
        sxe_jitson_source_from_string(&source,
                                      "01234567890123456789012345678901234567890123456789012345678901234567890123456789",
                                      SXE_JITSON_FLAG_STRICT);
        is_eq(sxe_jitson_source_left(&source), "012345678901234567890123456789012345678901234567890123456789...",
              "Correctly truncated source left to 60 characters");
        sxe_jitson_source_from_string(&source, "012345678901234567890123456789012345678901234567890123456789012",
                                      SXE_JITSON_FLAG_STRICT);
        is_eq(sxe_jitson_source_left(&source), "012345678901234567890123456789012345678901234567890123456789012",
              "Correctly didn't truncated source with only 63 characters");
        sxe_jitson_source_from_buffer(&source, "012345678901234567890123456789012345678901234567890123456789012", 63,
                                      SXE_JITSON_FLAG_STRICT);
        is_eq(sxe_jitson_source_left(&source), "012345678901234567890123456789012345678901234567890123456789012",
              "Correctly didn't truncated source with only 63 characters");
        sxe_jitson_source_from_string(&source, "", SXE_JITSON_FLAG_STRICT);
        is_eq(sxe_jitson_source_left(&source), "", "Correctly returned \"\" for empty source");
    }

    diag("Test peek token function");
    {
        sxe_jitson_source_from_string(&source, " ?=", SXE_JITSON_FLAG_STRICT);
        is_eq(sxe_jitson_source_peek_token(&source, &len), "?=", "Correctly peeked at token in string source");
        is(len, 2,                                               "Cerrectly determined token's length");

        sxe_jitson_source_from_buffer(&source, " ?=garbage", 3, SXE_JITSON_FLAG_STRICT);
        is_eq(sxe_jitson_source_peek_token(&source, &len), "?=garbage", "Correctly peeked at token in buffer source");
        is(len, 2,                                                      "Cerrectly determined token's length");

        sxe_jitson_source_from_buffer(&source, " ?=", 1, SXE_JITSON_FLAG_STRICT);
        is(sxe_jitson_source_peek_token(&source, &len), NULL, "Correctly detected lack of token in buffer source");
    }

    diag("Test repeated sxe_jitson_source_get_char()");
    {
        /* Although nothing currently does this, ensure that repeated calls at end-of-string work */
        unsigned i;

        sxe_jitson_source_from_string(&source, "x\0y", SXE_JITSON_FLAG_STRICT);
        is(sxe_jitson_source_get_char(&source), 'x', "Got 'x' from the source");
        for (i = 0; i < 100; i++)
            if (sxe_jitson_source_get_char(&source))
                break;
        is(i, 100, "Read '\\0' from the end of string 100 times");
    }

    sxe_thread_memory_free(SXE_THREAD_MEMORY_ALL);
    is(kit_memory_allocations(), start_allocations, "No memory was leaked");
    return exit_status();
}
