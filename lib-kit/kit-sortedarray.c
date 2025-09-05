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

#include <stdint.h>
#include <string.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "sxe-log.h"
#include "kit-sortedarray.h"

/**
 * Add an element to a sorted array with flags
 *
 * @param array_class Defines the type of the array
 * @param array       Pointer to the address of the array in malloced storage; *array == NULL to allocate first time
 * @param count       Pointer to the count of array elements
 * @param alloc       Pointer to the number of array slots allocated; used as the initial allocation if *array == NULL
 * @param element     Address of the element to insert
 * @param flags       KIT_SORTEDARRAY_DEFAULT or one or more of KIT_SORTEDARRAY_ALLOW_INSERTS | KIT_SORTEDARRAY_ALLOW_GROWTH
 *                    | KIT_SORTEDARRAY_ZERO_COPY
 *
 * @return Pointer to inserted element (uninitialized if array_class has flag KIT_SORTEDARRAY_ZERO_COPY), or NULL on a duplicate
 *         or error
 */
void *
kit_sortedarray_add_elem(const struct kit_sortedarray_class *array_class, void **array, unsigned *count, unsigned *alloc,
                         const void *element)
{
    unsigned    pos;
    uint8_t    *slot;
    const void *key;
    void       *new_array;
    int         cmp;
    unsigned    more = 0;
    bool        match;

    SXEA6(*array || *count == 0, "Array is unallocated but count is non zero");

    if ((pos = *count) > 0) {    // If the array is not empty
        slot = (uint8_t *)*array + (pos - 1) * array_class->elem_class.size;
        key  = (const uint8_t *)element + array_class->elem_class.keyoffset;

        if ((cmp = array_class->elem_class.cmp(slot + array_class->elem_class.keyoffset, key)) == 0)    // Already exists
            return NULL;

        if (cmp > 0) {    // Out of order
            // This (actually the memmove() below) is expensive when building large arrays
            if (array_class->flags & KIT_SORTEDARRAY_ALLOW_INSERTS) {
                pos = kit_sortedarray_find_key(array_class, *array, *count, key, &match);

                if (match)    // Already exists
                    return NULL;
            } else {
                SXEL2("Unsorted list insertions are not permitted");
                return NULL;
            }
        }
    }

    if (*count == *alloc) {
        if (!(array_class->flags & KIT_SORTEDARRAY_ALLOW_GROWTH)) {
            SXEL2("Number of elements exceed %u, the maximum allowed in this array", *alloc);
            return NULL;
        }

        more = *alloc > 100 ? *alloc / 2 : 10;
    }

    // First time through, allocate the array; if more space is needed, reallocate
    if (!*array || more) {
        if (!(new_array = MOCKERROR(kit_sortedarray_add, NULL, ENOMEM,
                                    kit_realloc(*array, (*alloc + more) * array_class->elem_class.size)))) {
            SXEL2("Failed to allocate array of %u %zu byte elements", *alloc + more, array_class->elem_class.size);
            return NULL;
        }

        *array  = new_array;
        *alloc += more;
    }

    slot = (uint8_t *)*array + pos * array_class->elem_class.size;
    SXEL7("Inserting element %s at position %u",
          (*array_class->elem_class.fmt)((const uint8_t *)element + array_class->elem_class.keyoffset), pos);

    if (pos < *count)
        memmove(slot + array_class->elem_class.size, slot, (*count - pos) * array_class->elem_class.size);

    if (!(array_class->flags & KIT_SORTEDARRAY_ZERO_COPY))
        memcpy(slot, element, array_class->elem_class.size);
#if SXE_DEBUG
    /**
     * In the case of KIT_SORTEDARRAY_ZERO_COPY -
     * corrupt memory intentionally, forcing caller
     * to populate the returned memory address
     */
    else
        memset(slot, 0xa5, array_class->elem_class.size);
#endif

    (*count)++;
    return slot;
}

/**
 * Search a sorted array for a key, returning the closest match
 *
 * @param array_class Defines the type of the array
 * @param array       The array to search
 * @param count       Number of elements in the array
 * @param key         The key to search for
 * @param match_out   Pointer to a bool set to true if there was an exact match and to false otherwise
 *
 * @return Index of the element found on exact match, the index of the first element whose key is greater, count if there is
 *         no greater element, or ~0U if the compare function supports failures and returns one.
 */
unsigned
kit_sortedarray_find_key(const struct kit_sortedarray_class *array_class, const void *array, unsigned count,
                             const void *key, bool *match_out)
{
    const struct kit_sortedelement_class *type = &array_class->elem_class;
    unsigned                              i, lim, pos;
    int                                   cmp;

    *match_out = false;

    for (pos = 0, lim = count; lim; lim >>= 1) {
        i   = pos + (lim >> 1);
        cmp = type->cmp(key, (const uint8_t *)array + type->size * i + type->keyoffset);

        if (cmp == 0) {
            *match_out = true;
            pos = i;
            break;
        }

        if (cmp == INT_MAX && (array_class->flags & KIT_SORTEDARRAY_CMP_CAN_FAIL)) {
            SXEL2("(me=?, count=%u, key=?) // return ~0U due to comparison failure", count);
            return ~0U;
        }

        if (cmp > 0) {
            pos = i + 1;
            lim--;
        }
    }

    if (type->fmt) {
        SXEA6(pos == count || type->cmp(key, (const uint8_t *)array + type->size * pos + type->keyoffset) <= 0,
              "Unexpected pos %u looking for %s, landed on %s", pos, (*type->fmt)(key),
              (*type->fmt)((const uint8_t *)array + type->size * pos + type->keyoffset));
        SXEL7("(me=?, count=%u, key=%s) // return %u, val %s, prev %s, next %s", count, (*type->fmt)(key), pos,
              pos < count     ? type->fmt((const uint8_t *)array + type->size * pos + type->keyoffset)       : "NOT FOUND",
              pos > 0         ? type->fmt((const uint8_t *)array + type->size * (pos - 1) + type->keyoffset) : "NONE",
              pos + 1 < count ? type->fmt((const uint8_t *)array + type->size * (pos + 1) + type->keyoffset) : "NONE");
    } else
        SXEL7("(me=?, count=%u, key=?) // return %u", count, pos);

    return pos;
}

/**
 * Search a sorted array for a key, returning the matching element
 *
 * @param array_class Defines the type of the array
 * @param array       The array to search
 * @param count       Number of elements in the array
 * @param key         The key to search for
 *
 * @return A pointer to the element or NULL on failure to match
 */
const void *
kit_sortedarray_get_elem(const struct kit_sortedarray_class *array_class, const void *array, unsigned count, const void *key)
{
    bool match = false;

    SXEA6(array || count == 0, "kit_sortedarray_get called with array %p and count %u", array, count);
    unsigned pos = array ? kit_sortedarray_find_key(array_class, array, count, key, &match) : count;

    return match ? (const uint8_t *)array + array_class->elem_class.size * pos : NULL;
}

#define SORTEDARRAY_ELEM(arr_class, array, idx) ((const char *)(array) + (idx) * (arr_class)->elem_class.size)
#define SORTEDARRAY_KEY( arr_class, array, idx) (SORTEDARRAY_ELEM(arr_class, array, idx) + (arr_class)->elem_class.keyoffset)

/**
 * Visit every element of the left array that is in the right array
 *
 * @param array_class Defines the type of the arrays, including the visit function
 * @param left        The left side array
 * @param left_count  The number of elements in the left side array
 * @param right       The right side array
 * @param right_count The number of elements in the right side array
 *
 * @return true if the arrays were entirely intersected, false if the intersection was terminated by the type's visit function
 *         or due to an error
 */
bool
kit_sortedarray_intersect(const struct kit_sortedarray_class *array_class, const void *left, unsigned left_count,
                          const void *right, unsigned right_count)
{
    unsigned idx, median;
    bool     match;

    if (left_count == 0 || right_count == 0)
        return true;

    if (left_count == 1) {
        idx = kit_sortedarray_find_key(array_class, right, right_count,
                                           (const char *)left + array_class->elem_class.keyoffset, &match);

        if (idx == ~0U && (array_class->flags & KIT_SORTEDARRAY_CMP_CAN_FAIL))
            return false;

        if (match && !array_class->visit(array_class->value, left))    // Visit element
            return false;

        return true;
    }

    median = left_count / 2;
    idx    = kit_sortedarray_find_key(array_class, right, right_count, SORTEDARRAY_KEY(array_class, left, median), &match);

    if (idx == ~0U && (array_class->flags & KIT_SORTEDARRAY_CMP_CAN_FAIL))
        return false;

    if (median > 0 && idx > 0)    // If median is not the 1st element in left and there are elements in right before the match
        if (!kit_sortedarray_intersect(array_class, left, median, right, idx))
            return false;

    if (match) {
        if (!array_class->visit(array_class->value, SORTEDARRAY_ELEM(array_class, left, median)))    // Visit element
            return false;

        median++;                                                          // Move past the median in the left array
        idx++;                                                             // Move past the match in the right array

        if (median == left_count - 1) {    // There's exactly one left element after the (previous) median
            if (idx < right_count) {       // There's at least one right element after the match
                if (kit_sortedarray_find_key(array_class, SORTEDARRAY_ELEM(array_class, right, idx), right_count - idx,
                                                 SORTEDARRAY_KEY(array_class, left, median), &match) == ~0U
                 && (array_class->flags & KIT_SORTEDARRAY_CMP_CAN_FAIL))
                    return false;

                if (match && !array_class->visit(array_class->value, SORTEDARRAY_ELEM(array_class, left, median)))    // Visit element
                    return false;
            }

            return true;
        }
    } else
        median++;    // Move past the median in the left array

    if (idx < right_count)
        return kit_sortedarray_intersect(array_class, SORTEDARRAY_KEY(array_class, left, median), left_count - median,
                                         SORTEDARRAY_KEY(array_class, right, idx), right_count - idx);

    return true;
}

/**
 * Delete key element from sorted array, returning true if the key is found and removed
 *
 * @param array_class Defines the type of the array
 * @param array       The array to search
 * @param count       A pointer to the number of elements in the array, which will be decremented on success
 * @param key         The key to delete
 *
 * @return Boolean value equaling true if the key is found and deleted successfully, false otherwise.
 */
bool
kit_sortedarray_delete_elem(const struct kit_sortedarray_class *array_class, void *array, unsigned *count, const void *key)
{
    bool     match;
    unsigned pos;

    if (!array || *count == 0)
        return false;

    pos = kit_sortedarray_find_key(array_class, array, *count, key, &match);

    if (!match)
        return false;

    memmove((uint8_t *)array + pos * array_class->elem_class.size, (uint8_t *)array + (pos + 1) * array_class->elem_class.size,
            (size_t)(*count - pos - 1) * array_class->elem_class.size);
    (*count)--;

    return true;
}

/* The following deprecated interfaces are provided for backward compatibility
 */

#define KIT_SORTEDELEMENT_CLASS_TO_ARRAY_CLASS(elem_class, array_flags)             \
    struct kit_sortedarray_class array_class;                                       \
    memcpy(&array_class.elem_class, elem_class, sizeof(array_class.elem_class));    \
    array_class.visit = NULL;                                                       \
    array_class.value = NULL;                                                       \
    array_class.flags = (array_flags);

void *
kit_sortedarray_add(const struct kit_sortedelement_class *elem_class, void **array, unsigned *count, unsigned *alloc,
                         const void *element, unsigned flags)
{
    KIT_SORTEDELEMENT_CLASS_TO_ARRAY_CLASS(elem_class, flags);
    return kit_sortedarray_add_elem(&array_class, array, count, alloc, element);
}

unsigned
kit_sortedarray_find(const struct kit_sortedelement_class *elem_class, const void *array, unsigned count, const void *key,
                     bool *match_out)
{
    KIT_SORTEDELEMENT_CLASS_TO_ARRAY_CLASS(elem_class, KIT_SORTEDARRAY_DEFAULT);
    return kit_sortedarray_find_key(&array_class, array, count, key, match_out);
}

const void *
kit_sortedarray_get(const struct kit_sortedelement_class *elem_class, const void *array, unsigned count, const void *key)
{
    KIT_SORTEDELEMENT_CLASS_TO_ARRAY_CLASS(elem_class, KIT_SORTEDARRAY_DEFAULT);
    return kit_sortedarray_get_elem(&array_class, array, count, key);
}

bool
kit_sortedarray_delete(const struct kit_sortedelement_class *elem_class, void *array, unsigned *count, const void *key)
{
    KIT_SORTEDELEMENT_CLASS_TO_ARRAY_CLASS(elem_class, KIT_SORTEDARRAY_DEFAULT);
    return kit_sortedarray_delete_elem(&array_class, array, count, key);
}
