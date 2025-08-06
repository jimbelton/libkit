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

#include "kit-mockfail.h"
#include "kit-sortedarray.h"
#include "sxe-jitson-in.h"
#include "sxe-jitson-intersect.h"
#include "sxe-jitson-oper.h"

unsigned sxe_jitson_oper_intersect      = 0;
unsigned sxe_jitson_oper_intersect_test = 0;

static __thread const struct sxe_jitson *array_left;    // The indexed array in which the visited elements are found
static __thread const struct sxe_jitson *array_right;   // The indexed array being intersected with it
static __thread struct sxe_jitson_stack iou;

static int
indexed_element_cmp(const void *void_left, const void *void_right)
{
    const struct sxe_jitson *elem_left  = &array_left[ *(const unsigned *)void_left];
    const struct sxe_jitson *elem_right = &array_right[*(const unsigned *)void_right];

    return sxe_jitson_cmp(elem_left, elem_right);
}

static inline bool
jitson_stack_add(struct sxe_jitson_stack *stack, const struct sxe_jitson *element)
{
    return sxe_jitson_size(element) == 1 ? sxe_jitson_stack_add_dup(stack, element) : sxe_jitson_stack_add_reference(stack, element);
}

static bool
intersect_add_indexed_element(void *void_stack, const void *void_element)
{
    struct sxe_jitson_stack *stack   = void_stack;
    const struct sxe_jitson *element = &array_left[*(const unsigned *)void_element];

    return MOCKERROR(SXE_JITSON_INTERSECT_ADD_INDEXED, false, ENOMEM, jitson_stack_add(stack, element));
}

static bool
intersect_add_element(void *void_stack, const void *element)
{
    struct sxe_jitson_stack *stack = void_stack;

    return MOCKERROR(SXE_JITSON_INTERSECT_ADD, false, ENOMEM, jitson_stack_add(stack, element));
}

static struct sxe_jitson_stack *
get_stack_open_array(void)
{
    struct sxe_jitson_stack *stack = sxe_jitson_stack_get_thread();

    sxe_jitson_stack_borrow(stack, &iou);

    /* Open an empty array for the intersection.
     */
    if (!MOCKERROR(SXE_JITSON_INTERSECT_OPEN, false, ENOMEM, sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY))) {
        SXEL2(": Failed to open array for result of INTERSECT expression");
        sxe_jitson_stack_return(stack, &iou);
        return NULL;
    }

    return stack;
}

static bool
add_to_array(struct sxe_jitson_stack *stack, const struct sxe_jitson *element)
{
    if (!MOCKERROR(SXE_JITSON_INTERSECT_ADD, false, ENOMEM, jitson_stack_add(stack, element))) {
        SXEL2(": Failed to add a duplicate of or reference to an element in an INTERSECT expression");
        return false;
    }

    return true;
}

static const struct sxe_jitson *
close_array_and_get(struct sxe_jitson_stack *stack)
{
    const struct sxe_jitson *jitson;

    sxe_jitson_stack_close_collection(stack);

    if (!(jitson = MOCKERROR(SXE_JITSON_INTERSECT_GET, NULL, ENOMEM, sxe_jitson_stack_get_jitson(stack))))
        SXEL2(": Failed to get array for result of INTERSECT expression");

    sxe_jitson_stack_return(stack, &iou);
    return jitson;
}

/* Implementation of INTERSECT for an ordered sxe-jitson array type
 */
static const struct sxe_jitson *
sxe_jitson_intersect_ordered_array(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    struct kit_sortedarray_class elem_type;
    struct sxe_jitson_stack     *stack;
    const struct sxe_jitson     *elem_lhs, *elem_rhs, *json = NULL;
    size_t                       i, len_lhs;

    SXEA6(sxe_jitson_get_type(right) == SXE_JITSON_TYPE_ARRAY,
          "Right hand side of an array INTERSECT expression cannot be JSON type %s", sxe_jitson_get_type_as_str(right));
    SXEA6(right->type & SXE_JITSON_TYPE_IS_ORD, "Right hand side of an ordered array INTERSECT expression must be ordered");

    if (sxe_jitson_get_type(left) != SXE_JITSON_TYPE_ARRAY) {
        SXEL2(": Left hand side of an INTERSECT expression cannot be JSON type %s", sxe_jitson_get_type_as_str(left));
        return NULL;
    }

    if (!(stack = get_stack_open_array()))
        return NULL;

    len_lhs = sxe_jitson_len(left);

    if (left->type & SXE_JITSON_TYPE_IS_ORD) {    // Left hand size is ordered
        elem_type.keyoffset = 0;
        elem_type.fmt       = NULL;
        elem_type.value     = stack;
        elem_type.flags     = KIT_SORTEDARRAY_CMP_CAN_FAIL;

        /* If both arrays contain uniformly sized elements
            */
        if ((left->type & SXE_JITSON_TYPE_IS_UNIF) && (right->type & SXE_JITSON_TYPE_IS_UNIF)) {
            if (left->uniform.size != right->uniform.size)
                goto EARLY_OUT;

            elem_type.size  = left->uniform.size;
            elem_type.cmp   = (int (*)(const void *, const void *))sxe_jitson_cmp;
            elem_type.visit = intersect_add_element;

            if (!kit_sortedarray_intersect(&elem_type, &left[1], left->len, &right[1], right->len))
                goto ERROR_OUT;

            goto EARLY_OUT;
        }

        /* If both arrays contain non-uniformly sized elements
         */
        if (!(left->type & SXE_JITSON_TYPE_IS_UNIF) && !(right->type & SXE_JITSON_TYPE_IS_UNIF)) {
            if (!(left->type & SXE_JITSON_TYPE_INDEXED))     // If the left array isn't indexed, index it
                sxe_jitson_array_get_element(left, 0);

            if (!(right->type & SXE_JITSON_TYPE_INDEXED))    // If the right array isn't indexed, index it
                sxe_jitson_array_get_element(right, 0);

            array_left      = left;
            array_right     = right;
            elem_type.size  = sizeof(left->index[0]);
            elem_type.cmp   = indexed_element_cmp;
            elem_type.visit = intersect_add_indexed_element;

            if (!kit_sortedarray_intersect(&elem_type, left->index, left->len, right->index, right->len))
                goto ERROR_OUT;    /* COVERAGE EXCLUSION: Ordered arrays of non-uniform size with incomparable elements */

            goto EARLY_OUT;
        }
    }

    for (i = 0; i < len_lhs; i++) {
        elem_lhs = sxe_jitson_array_get_element(left, i);

        if ((elem_rhs = sxe_jitson_in(elem_lhs, right)) == NULL                 // Error in IN operation
         || (elem_rhs != sxe_jitson_null && !add_to_array(stack, elem_lhs)))    // Found but failed to add to result array
            goto ERROR_OUT;
    }

EARLY_OUT:
    if ((json = close_array_and_get(stack)))
        return json;

ERROR_OUT:
    sxe_jitson_stack_clear(stack);
    return NULL;
}

/* Implementation of INTERSECT for sxe-jitson array type
 */
static const struct sxe_jitson *
sxe_jitson_intersect_array(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    struct sxe_jitson_stack     *stack;
    const struct sxe_jitson     *elem_lhs, *elem_rhs, *json = NULL;
    size_t                       i, j, len_lhs, len_rhs;
    int                          ret;

    SXEA6(sxe_jitson_get_type(right) == SXE_JITSON_TYPE_ARRAY,
          "Right hand side of an array INTERSECT expression cannot be JSON type %s", sxe_jitson_get_type_as_str(right));

    if (right->type & SXE_JITSON_TYPE_IS_ORD)
        return sxe_jitson_intersect_ordered_array(left, right);
    else if (left->type & SXE_JITSON_TYPE_IS_ORD)
        return sxe_jitson_intersect_ordered_array(right, left);

    if (sxe_jitson_get_type(left) != SXE_JITSON_TYPE_ARRAY) {
        SXEL2(": Left hand side of an INTERSECT expression cannot be JSON type %s", sxe_jitson_get_type_as_str(left));
        return NULL;
    }

    if (!(stack = get_stack_open_array()))
        return NULL;

    len_lhs = sxe_jitson_len(left);

    for (i = 0; i < len_lhs; i++) {
        elem_lhs = sxe_jitson_array_get_element(left, i);

        for (j = 0, len_rhs = sxe_jitson_len(right); j < len_rhs; j++) {
            elem_rhs = sxe_jitson_array_get_element(right, j);

            if ((ret = sxe_jitson_eq(elem_lhs, elem_rhs)) == SXE_JITSON_TEST_ERROR) {
                SXEL2(": Failed to compare elements of types %s and %s when INTERSECTing arrays",
                        sxe_jitson_get_type_as_str(elem_lhs), sxe_jitson_get_type_as_str(elem_rhs));
                goto ERROR_OUT;
            }
            else if (ret == SXE_JITSON_TEST_TRUE && !add_to_array(stack, elem_lhs))
                goto ERROR_OUT;
        }
    }

    if ((json = close_array_and_get(stack)))
        return json;

ERROR_OUT:
    sxe_jitson_stack_clear(stack);
    return NULL;
}

static bool
intersect_check_element(void *found_out, const void *element)
{
    SXE_UNUSED_PARAMETER(element);
    *(bool *)found_out = true;
    return false;                 // Found a match so end the intersect test
}

/* Implementation of INTERSECT_TEST for an ordered sxe-jitson array type
 */
static const struct sxe_jitson *
sxe_jitson_intersect_test_ordered_array(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    struct kit_sortedarray_class elem_type;
    const struct sxe_jitson     *elem_lhs, *elem_rhs;
    size_t                       i, len_lhs;
    bool                         found;

    SXEA6(sxe_jitson_get_type(right) == SXE_JITSON_TYPE_ARRAY,
          "Right hand side of an array INTERSECT expression cannot be JSON type %s", sxe_jitson_get_type_as_str(right));
    SXEA6(right->type & SXE_JITSON_TYPE_IS_ORD, "Right hand side of an ordered array INTERSECT expression must be ordered");

    if (sxe_jitson_get_type(left) != SXE_JITSON_TYPE_ARRAY) {
        SXEL2(": Left hand side of an INTERSECT expression cannot be JSON type %s", sxe_jitson_get_type_as_str(left));
        return NULL;
    }

    len_lhs = sxe_jitson_len(left);

    if (left->type & SXE_JITSON_TYPE_IS_ORD) {    // Left hand size is ordered
        elem_type.keyoffset = 0;
        elem_type.fmt       = NULL;
        found               = false;
        elem_type.value     = &found;
        elem_type.flags     = KIT_SORTEDARRAY_CMP_CAN_FAIL;

        /* If both arrays contain uniformly sized elements
         */
        if ((left->type & SXE_JITSON_TYPE_IS_UNIF) && (right->type & SXE_JITSON_TYPE_IS_UNIF)) {
            if (left->uniform.size != right->uniform.size)    // Different sized elements can't intersect
                return sxe_jitson_false;

            elem_type.size  = left->uniform.size;
            elem_type.cmp   = (int (*)(const void *, const void *))sxe_jitson_cmp;
            elem_type.visit = intersect_check_element;

            if (!kit_sortedarray_intersect(&elem_type, &left[1], left->len, &right[1], right->len)) {   // Incomplete intersect
                if (found)
                    return sxe_jitson_true;

                return NULL;
            }

            return sxe_jitson_false;
        }

        /* If both arrays contain non-uniformly sized elements
         */
        if (!(left->type & SXE_JITSON_TYPE_IS_UNIF) && !(right->type & SXE_JITSON_TYPE_IS_UNIF)) {
            if (!(left->type & SXE_JITSON_TYPE_INDEXED))     // If the left array isn't indexed, index it
                sxe_jitson_array_get_element(left, 0);

            if (!(right->type & SXE_JITSON_TYPE_INDEXED))    // If the right array isn't indexed, index it
                sxe_jitson_array_get_element(right, 0);

            array_left      = left;
            array_right     = right;
            elem_type.size  = sizeof(left->index[0]);
            elem_type.cmp   = indexed_element_cmp;
            elem_type.visit = intersect_check_element;

            if (!kit_sortedarray_intersect(&elem_type, left->index, left->len, right->index, right->len)) {    // Incomplete
                if (found)
                    return sxe_jitson_true;

                return NULL;    /* COVERAGE EXCLUSION: Ordered arrays of non-uniform size with incomparable elements */
            }

            return sxe_jitson_false;
        }
    }

    for (i = 0; i < len_lhs; i++) {
        elem_lhs = sxe_jitson_array_get_element(left, i);

        if ((elem_rhs = sxe_jitson_in_array(elem_lhs, right)) == NULL)    // Error in IN operation
            return NULL;
        else if (elem_rhs != sxe_jitson_null)
            return sxe_jitson_true;
    }

    return sxe_jitson_false;
}

/* Implementation of INTERSECT_TEST for sxe-jitson array type
 */
static const struct sxe_jitson *
sxe_jitson_intersect_test_array(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    const struct sxe_jitson     *elem_lhs, *elem_rhs;
    size_t                       i, j, len_lhs, len_rhs;
    int                          ret;

    SXEA6(sxe_jitson_get_type(right) == SXE_JITSON_TYPE_ARRAY,
          "Right hand side of an array INTERSECT expression cannot be JSON type %s", sxe_jitson_get_type_as_str(right));

    if (right->type & SXE_JITSON_TYPE_IS_ORD)
        return sxe_jitson_intersect_test_ordered_array(left, right);
    else if (left->type & SXE_JITSON_TYPE_IS_ORD)
        return sxe_jitson_intersect_test_ordered_array(right, left);

    if (sxe_jitson_get_type(left) != SXE_JITSON_TYPE_ARRAY) {
        SXEL2(": Left hand side of an INTERSECT_TEST expression cannot be JSON type %s", sxe_jitson_get_type_as_str(left));
        return NULL;
    }

    len_lhs = sxe_jitson_len(left);

    for (i = 0; i < len_lhs; i++) {
        elem_lhs = sxe_jitson_array_get_element(left, i);

        for (j = 0, len_rhs = sxe_jitson_len(right); j < len_rhs; j++) {
            elem_rhs = sxe_jitson_array_get_element(right, j);

            if ((ret = sxe_jitson_eq(elem_lhs, elem_rhs)) == SXE_JITSON_TEST_ERROR) {
                SXEL2(": Failed to compare elements of types %s and %s when testing INTERSECTion of arrays",
                        sxe_jitson_get_type_as_str(elem_lhs), sxe_jitson_get_type_as_str(elem_rhs));
                return NULL;
            } else if (ret == SXE_JITSON_TEST_TRUE)
                return sxe_jitson_true;
        }
    }

    return sxe_jitson_false;
}

/* Default function to determine whether two json values intersect, which falls back to calling intersect
 */
static const struct sxe_jitson *
sxe_jitson_intersect_test_default(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    const struct sxe_jitson *result, *ret = NULL;

    result = sxe_jitson_oper_apply_binary(left, sxe_jitson_oper_intersect, right);

    if (result) {
        if (sxe_jitson_len(result))    /* COVERAGE EXCLUSION: Requires a type that implement INTERSECT but not INTERSECT_TEST */
            ret = sxe_jitson_true;     /* COVERAGE EXCLUSION: Requires a type that implement INTERSECT but not INTERSECT_TEST */
        else
            ret = sxe_jitson_false;    /* COVERAGE EXCLUSION: Requires a type that implement INTERSECT but not INTERSECT_TEST */

        sxe_jitson_free(result);       /* COVERAGE EXCLUSION: Requires a type that implement INTERSECT but not INTERSECT_TEST */
    }

    return ret;
}

/* Initialize the SXE intersect operator
 */
void
sxe_jitson_intersect_init(void)
{
    union sxe_jitson_oper_func func              = {.binary = sxe_jitson_intersect_array},
                               func_test_default = {.binary = sxe_jitson_intersect_test_default},
                               func_test_array   = {.binary = sxe_jitson_intersect_test_array};

    SXEA1(sxe_jitson_is_init(), "sxe_jitson_initialize needs to be called first");
    sxe_jitson_oper_intersect = sxe_jitson_oper_register("INTERSECT", SXE_JITSON_OPER_BINARY | SXE_JITSON_OPER_TYPE_RIGHT,
                                                         sxe_jitson_oper_func_null);
    sxe_jitson_oper_add_to_type(sxe_jitson_oper_intersect, SXE_JITSON_TYPE_ARRAY, func);

    sxe_jitson_oper_intersect_test = sxe_jitson_oper_register("INTERSECT_TEST",
                                                              SXE_JITSON_OPER_BINARY | SXE_JITSON_OPER_TYPE_RIGHT,
                                                              func_test_default);
    sxe_jitson_oper_add_to_type(sxe_jitson_oper_intersect_test, SXE_JITSON_TYPE_ARRAY, func_test_array);
}

/**
 * Determine the intersection of two json values
 *
 * @param left/right The values to be intersected
 *
 * @return The possibly empty intersection or NULL on error
 */
const struct sxe_jitson *
sxe_jitson_intersect(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    SXEA6(sxe_jitson_oper_intersect, "sxe_jitson_intersect_init has not been called");
    return sxe_jitson_oper_apply_binary(left, sxe_jitson_oper_intersect, right);
}

/**
 * Determine whether two json values intersect (optimized function)
 *
 * @param left  The key or value to be checked for
 * @param right The value to be looked in
 *
 * @return sxe_jitson_bool_true if found, sxe_jitson_false if not, or NULL on error
 */
const struct sxe_jitson *
sxe_jitson_intersect_test(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    SXEA6(sxe_jitson_oper_intersect, "sxe_jitson_intersect_init has not been called");
    return sxe_jitson_oper_apply_binary(left, sxe_jitson_oper_intersect_test, right);
}
