/*
 * Copyright (c) 2022 Cisco Systems, Inc. and its affiliates
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sxe-jitson.h"
#include "sxe-jitson-const.h"
#include "sxe-jitson-in.h"
#include "sxe-jitson-oper.h"
#include "sxe-jitson-range.h"
#include "sxe-log.h"

uint32_t SXE_JITSON_TYPE_RANGE = SXE_JITSON_TYPE_INVALID;    // Need to register the type to get a valid value

static bool
sxe_jitson_range_cast(struct sxe_jitson_stack *stack, const struct sxe_jitson *from)
{
    char    *string;
    unsigned idx = stack->count;

    SXEA1(SXE_JITSON_TYPE_RANGE != SXE_JITSON_TYPE_INVALID, "Can't cast to a range until sxe_jitson_range_register is called");

    if (sxe_jitson_get_type(from) != SXE_JITSON_TYPE_ARRAY) {
        SXEL2("Can't cast a %s to a range", sxe_jitson_get_type_as_str(from));
        return false;
    }

    if (sxe_jitson_len(from) != 2) {
        SXEL2("Expected a range array to have 2 elements, got %zu", sxe_jitson_len(from));
        return false;
    }

    if (sxe_jitson_cmp(sxe_jitson_array_get_element(from, 0), sxe_jitson_array_get_element(from, 1)) > 0) {
        string = sxe_jitson_to_json(from, NULL);
        SXEL2("Expected the begining and end of range '%s' to be ordered", string);
        kit_free(string);
        return false;
    }

    if (!sxe_jitson_stack_dup_at_index(stack, idx, from, 0))
        return false;

    /* A range is exactly an array with the type changed, but all the same flags.
     */
    stack->jitsons[idx].type = SXE_JITSON_TYPE_RANGE | (stack->jitsons[idx].type & ~SXE_JITSON_TYPE_MASK);
    return true;
}

static int
sxe_jitson_range_test(const struct sxe_jitson *jitson)
{
    SXE_UNUSED_PARAMETER(jitson);
    return SXE_JITSON_TEST_TRUE;    // Ranges always test true for evaluation purposes
}

static char *
sxe_jitson_range_build_json(const struct sxe_jitson *range, struct sxe_factory *factory)
{
    if (sxe_factory_add(factory, "range(", sizeof("range(") - 1) < 0 || sxe_jitson_array_build_json(range, factory) == NULL
     || sxe_factory_add(factory, ")", 1) < 0) {
        SXEL2("Failed to allocate memory for range as a string");
        return NULL;
    }

    return sxe_factory_look(factory, NULL);
}

/* Implement value IN range -> true or null
 */
static const struct sxe_jitson *
sxe_jitson_range_in(const struct sxe_jitson *json_value, const struct sxe_jitson *range)
{
    SXEL2A6((range->type & SXE_JITSON_TYPE_MASK) == SXE_JITSON_TYPE_RANGE,
            ": Internal error: Expected right side of IN to be a range value");

    if (sxe_jitson_cmp(sxe_jitson_array_get_element(range, 0), json_value) <= 0
     && sxe_jitson_cmp(json_value, sxe_jitson_array_get_element(range, 1)) <= 0)
        return sxe_jitson_true;

    return sxe_jitson_null;    // On error or if out of range, return null
}

void
sxe_jitson_range_register(void)
{
    union sxe_jitson_oper_func func;
    static struct sxe_jitson   cast_constant;

    if (SXE_JITSON_TYPE_RANGE != SXE_JITSON_TYPE_INVALID)    // Already registered
        return;

    SXE_JITSON_TYPE_RANGE = sxe_jitson_type_register("range", sxe_jitson_array_free, sxe_jitson_range_test,
                                                     sxe_jitson_array_size, sxe_jitson_len_base, sxe_jitson_array_clone,
                                                     sxe_jitson_range_build_json, NULL, sxe_jitson_array_eq);
    sxe_jitson_const_register_cast(&cast_constant, "range", sxe_jitson_range_cast);
    func.binary = sxe_jitson_range_in;
    sxe_jitson_oper_add_to_type(sxe_jitson_oper_in, SXE_JITSON_TYPE_RANGE, func);
}

static bool
range_error(const char *before, const struct sxe_jitson *from, const struct sxe_jitson *to, const char *after)
{
    char *json_from = sxe_jitson_to_json(from, NULL);
    char *json_to   = sxe_jitson_to_json(to,   NULL);

    SXEL2("%s[%s,%s]%s", before, json_from, json_to, after);
    kit_free(json_to);
    kit_free(json_from);
    return false;
}

/**
 * Construct a range on a stack
 *
 * @param stack   The stack to add the range to
 * @param from/to The beginning and end of the range, expected to be comparable and in order
 *
 * @return true on success, false on failure
 */
bool
sxe_jitson_stack_add_range(struct sxe_jitson_stack *stack, const struct sxe_jitson *from, const struct sxe_jitson *to)
{
    unsigned idx = stack->count;

    _Static_assert(SXE_JITSON_CMP_ERROR > 0,                "Expected compare error value to be > 0");
    SXEA1(SXE_JITSON_TYPE_RANGE != SXE_JITSON_TYPE_INVALID, "Can't add a range until sxe_jitson_range_init is called");

    if (sxe_jitson_cmp(from, to) > 0)
        return range_error("Expected the begining and end of range ", from, to, " to be ordered");

    if (!sxe_jitson_stack_open_array(stack, "for range") || !sxe_jitson_stack_add_dup(stack, from)
     || !sxe_jitson_stack_add_dup(stack, to) || !sxe_jitson_stack_close_array(stack, "for range"))
        return range_error("Failed to add range ", from, to, " to the stack");

    /* A range is exactly an array with the type changed, but all the same flags.
     */
    SXEA6(sxe_jitson_cmp(&stack->jitsons[idx + 1], &stack->jitsons[idx + 2]) <= 0, "Expected constructed range to be ordered");
    stack->jitsons[idx].type = SXE_JITSON_TYPE_RANGE | (stack->jitsons[idx].type & ~SXE_JITSON_TYPE_MASK);
    return true;
}
