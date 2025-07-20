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

#include <string.h>

#include "sxe-jitson-in.h"
#include "sxe-jitson-oper.h"
#include "sxe-sortedarray.h"

unsigned sxe_jitson_oper_in = 0;

static __thread const struct sxe_jitson *array;    // Pointer to indexed array

/* Compare a value to an indexed array element
 */
static int
compare_value_to_element(const void *void_value, const void *void_element_offset)
{
    const struct sxe_jitson *value   = void_value;
    const struct sxe_jitson *element = &array[*(const uint32_t *)void_element_offset];

    return sxe_jitson_cmp(value, element);
}

/* Implementation of IN for sxe-jitson arrays
 */
const struct sxe_jitson *
sxe_jitson_in_array(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    struct sxe_sortedarray_class elem_type;
    const struct sxe_jitson     *element, *result;
    size_t                       len;
    unsigned                     i;
    uint32_t                     left_type;

    SXEA6(sxe_jitson_get_type(right) == SXE_JITSON_TYPE_ARRAY,
          "Right hand side of an array IN expression cannot be JSON type %s", sxe_jitson_get_type_as_str(right));

    if (right->len == 0)
        return sxe_jitson_null;

    left_type = sxe_jitson_get_type(left);

    /* If the array is ordered and of one homogenous type and the type being looked up is the type of the array, use bsearch for
     * O(log n).
     */
    if ((right->type & SXE_JITSON_TYPE_IS_ORD) && (right->type & SXE_JITSON_TYPE_IS_HOMO)
     && left_type == sxe_jitson_get_type(&right[1])) {
        unsigned idx;
        bool     match;

        elem_type.keyoffset = 0;
        elem_type.fmt       = NULL;
        elem_type.flags     = SXE_SORTEDARRAY_CMP_CAN_FAIL;

        if (right->type & SXE_JITSON_TYPE_IS_UNIF) {    // If array is of uniform elements, it's simple
            elem_type.size = right->uniform.size;
            elem_type.cmp  = (int (*)(const void *, const void *))sxe_jitson_cmp;
            idx            = sxe_sortedarray_find(&elem_type, &right[1], right->len, left, &match);
        }
        else {
            if (!(right->type & SXE_JITSON_TYPE_INDEXED))   // If the array needs indexing
                sxe_jitson_array_get_element(right, 0);

            array          = right;
            elem_type.size = sizeof(left->index[0]);
            elem_type.cmp  = compare_value_to_element;
            idx            = sxe_sortedarray_find(&elem_type, array->index, array->len, left, &match);
        }

        if (idx == ~0U)    // Error occurred in find
            return NULL;

        return match ? sxe_jitson_true : sxe_jitson_null;
    }

    for (i = 0, len = sxe_jitson_len(right); i < len; i++) {    // For each element in the array
        element = sxe_jitson_array_get_element(right, i);

        if (left_type == sxe_jitson_get_type(element)) {
            if (sxe_jitson_eq(left, element) == SXE_JITSON_TEST_TRUE)
                return sxe_jitson_true;    // Return true so false values don't cause false negatives
        }

        /* If not the same type, look for the LHS value in the element (transitive IN operation)
         */
        else if ((result = sxe_jitson_in(left, element)) && sxe_jitson_get_type(result) != SXE_JITSON_TYPE_NULL)
            return element;    // Safe to return, because a containing value cannot test false
    }

    return sxe_jitson_null;
}

/* Default implementation of IN for sxe-jitson standard types
 */
static const struct sxe_jitson *
sxe_jitson_in_default(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    const char *string;
    size_t      len;
    uint32_t    json_type;

    switch (json_type = sxe_jitson_get_type(right)) {
    case SXE_JITSON_TYPE_OBJECT:
    case SXE_JITSON_TYPE_STRING:
        if (sxe_jitson_get_type(left) != SXE_JITSON_TYPE_STRING) {
            SXEL2(": invalid check for a JSON value of type %s in a%s", sxe_jitson_type_to_str(sxe_jitson_get_type(left)),
                  json_type == SXE_JITSON_TYPE_OBJECT ? "n object" : " string");
            return NULL;
        }

        /* Only for objects, the value is returned and may be 0 or "", which are treated as false. This is done so that IN can
         * be used as a safe [] operator. If you must test whether a key is in an object, use '(key IN object) != null'
         */
        if (json_type == SXE_JITSON_TYPE_OBJECT) {
            string = sxe_jitson_get_string(left, &len);
            return sxe_jitson_object_get_member(right, string, len) ?: sxe_jitson_null;
        }

        string = strstr(sxe_jitson_get_string(right, NULL), sxe_jitson_get_string(left, NULL));
        return string ? sxe_jitson_true : sxe_jitson_null;

    case SXE_JITSON_TYPE_NULL:    // Allow "element IN (member IN object)" when member IN object is NULL.
        return sxe_jitson_null;

    default:
        SXEL2(": invalid check for inclusion in a JSON value of type %s", sxe_jitson_type_to_str(json_type));
        return NULL;
    }
}

/* Initialize the SXE in operator
 */
void
sxe_jitson_in_init(void)
{
    union sxe_jitson_oper_func func_default = {.binary = sxe_jitson_in_default},
                               func_array   = {.binary = sxe_jitson_in_array};

    sxe_jitson_oper_in = sxe_jitson_oper_register("IN", SXE_JITSON_OPER_BINARY | SXE_JITSON_OPER_TYPE_RIGHT, func_default);
    sxe_jitson_oper_add_to_type(sxe_jitson_oper_in, SXE_JITSON_TYPE_ARRAY, func_array);
}

/**
 * Determine whether one json value is in another
 *
 * @param left  The key or value to be checked for
 * @param right The value to be looked in
 *
 * @return The value found or sxe_jitson_bool_true if found, sxe_jitson_null if not, or NULL on error
 *
 * @note The value found may test as false (e.g. '"a" IN {"a":0}'); CRL will only treat a null return from IN as false,
 *       but if you save the result of an IN operation to a variable, be aware that it might have a value like 0 or "".
 */
const struct sxe_jitson *
sxe_jitson_in(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    SXEA6(sxe_jitson_oper_in, "sxe_jitson_in_init has not been called");
    return sxe_jitson_oper_apply_binary(left, sxe_jitson_oper_in, right);
}


