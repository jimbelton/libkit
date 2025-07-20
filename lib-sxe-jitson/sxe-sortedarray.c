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
#include "sxe-sortedarray.h"

/**
 * Add an element to a sorted array.
 *
 * @param type    Object definining the type of elements of the array
 * @param array   Pointer to the address of the array in malloced storage; *array == NULL to allocate first time
 * @param count   Pointer to the count of array elements
 * @param alloc   Pointer to the number of array slots allocated; used as the initial allocation if *array == NULL
 * @param element Address of the element to insert
 *
 * @return Pointer to inserted element (uninitialized if type has flag SXE_SORTEDARRAY_ZERO_COPY), or NULL on a duplicate or
 *         error
 *
 * @note Affected by type->flags SXE_SORTEDARRAY_ALLOW_INSERTS, SXE_SORTEDARRAY_ALLOW_GROWTH, and SXE_SORTEDARRAY_ZERO_COPY. If
 *       SXE_SORTEDARRAY_ZERO_COPY is set caller SHOULD assign the returned pointer a value immediately to avoid a possible
 *       unsorted array.
 */
void *
sxe_sortedarray_add(const struct sxe_sortedarray_class *type, void **array, unsigned *count, unsigned *alloc,
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
        slot = (uint8_t *)*array + (pos - 1) * type->size;
        key  = (const uint8_t *)element + type->keyoffset;

        if ((cmp = type->cmp(slot + type->keyoffset, key)) == 0)    // Already exists
            return NULL;

        if (cmp > 0) {    // Out of order
            // This (actually the memmove() below) is expensive when building large pref blocks
            if (type->flags & SXE_SORTEDARRAY_ALLOW_INSERTS) {
                pos = sxe_sortedarray_find(type, *array, *count, key, &match);

                if (match)    // Already exists
                    return NULL;
            } else {
                SXEL2("Unsorted list insertions are not permitted");
                return NULL;
            }
        }
    }

    if (*count == *alloc) {
        if (!(type->flags & SXE_SORTEDARRAY_ALLOW_GROWTH)) {
            SXEL2("Number of elements exceed %u, the maximum allowed in this array", *alloc);
            return NULL;
        }

        more = *alloc > 100 ? *alloc / 2 : 10;
    }

    // First time through, allocate the array; if more space is needed, reallocate
    if (!*array || more) {
        if (!(new_array = MOCKERROR(sxe_sortedarray_add, NULL, ENOMEM, kit_realloc(*array, (*alloc + more) * type->size)))) {
            SXEL2("Failed to allocate array of %u %zu byte elements", *alloc + more, type->size);
            return NULL;
        }

        *array  = new_array;
        *alloc += more;
    }

    slot = (uint8_t *)*array + pos * type->size;
    SXEL7("Inserting element %s at position %u", (*type->fmt)((const uint8_t *)element + type->keyoffset), pos);

    if (pos < *count)
        memmove(slot + type->size, slot, (*count - pos) * type->size);

    if (!(type->flags & SXE_SORTEDARRAY_ZERO_COPY))
        memcpy(slot, element, type->size);
#if SXE_DEBUG
    /**
     * In the case of SXE_SORTEDARRAY_ZERO_COPY -
     * corrupt memory intentionally, forcing caller
     * to populate the returned memory address
     */
    else
        memset(slot, 0xa5, type->size);
#endif

    (*count)++;
    return slot;
}

/**
 * Search a sorted array for a key, returning the closest match
 *
 * @param type      Defines the type of the elements
 * @param array     The array to search
 * @param count     Number of elements in the array
 * @param key       The key to search for
 * @param match_out Pointer to a bool set to true if there was an exact match and to false otherwise
 *
 * @return Index of the element found on exact match, the index of the first element whose key is greater, count if there is
 *         no greater element, or ~0U if the compare function supports failures and returns one.
 */
unsigned
sxe_sortedarray_find(const struct sxe_sortedarray_class *type, const void *array, unsigned count, const void *key,
                     bool *match_out)
{
    unsigned i, lim, pos;
    int      cmp;

    *match_out = false;

    for (pos = 0, lim = count; lim; lim >>= 1) {
        i   = pos + (lim >> 1);
        cmp = type->cmp(key, (const uint8_t *)array + type->size * i + type->keyoffset);

        if (cmp == 0) {
            *match_out = true;
            pos = i;
            break;
        }

        if (cmp == INT_MAX && (type->flags & SXE_SORTEDARRAY_CMP_CAN_FAIL)) {
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
    }
    else
        SXEL7("(me=?, count=%u, key=?) // return %u", count, pos);

    return pos;
}

const void *
sxe_sortedarray_get(const struct sxe_sortedarray_class *class, const void *array, unsigned count, const void *key)
{
    bool match = false;

    SXEA6(array || count == 0, "sxe_sortedarray_get called with a NULL array and count %u", count);
    unsigned pos = array ? sxe_sortedarray_find(class, array, count, key, &match) : count;

    return match ? (const uint8_t *)array + class->size * pos : NULL;
}

#define SORTEDARRAY_ELEM(type, array, idx) ((const char *)(array) + (idx) * (type)->size)
#define SORTEDARRAY_KEY( type, array, idx) (SORTEDARRAY_ELEM(type, array, idx) + (type)->keyoffset)

/**
 * Visit every element of the left array that is in the right array
 *
 * @param type        An object defining the type of element in the array, including the visit function
 * @param left        The left side array
 * @param left_count  The number of elements in the left side array
 * @param right       The right side array
 * @param right_count The number of elements in the right side array
 *
 * @return true if the arrays were entirely intersected, false if the intersection was terminated by the type's visit function
 *         or due to an error
 */
bool
sxe_sortedarray_intersect(struct sxe_sortedarray_class *type, const void *left, unsigned left_count, const void *right,
                          unsigned right_count)
{
    unsigned idx, median;
    bool     match;

    if (left_count == 0 || right_count == 0)
        return true;

    if (left_count == 1) {
        if (sxe_sortedarray_find(type, right, right_count, (const char *)left + type->keyoffset, &match) == ~0U
         && (type->flags & SXE_SORTEDARRAY_CMP_CAN_FAIL))
            return false;

        if (match && !type->visit(type->value, left))    // Visit element
            return false;

        return true;
    }

    median = left_count / 2;

    if ((idx = sxe_sortedarray_find(type, right, right_count, SORTEDARRAY_KEY(type, left, median), &match)) == ~0U
     && (type->flags & SXE_SORTEDARRAY_CMP_CAN_FAIL))
            return false;

    if (median > 0 && idx > 0)    // If median is not the 1st element in left and there are elements in right before the match
        if (!sxe_sortedarray_intersect(type, left, median, right, idx))
            return false;

    if (match) {
        if (!type->visit(type->value, SORTEDARRAY_ELEM(type, left, median)))    // Visit element
            return false;

        median++;                                                          // Move past the median in the left array
        idx++;                                                             // Move past the match in the right array

        if (median == left_count - 1) {    // There's exactly one left element after the (previous) median
            if (idx < right_count) {       // There's at least one right element after the match
                if (sxe_sortedarray_find(type, SORTEDARRAY_ELEM(type, right, idx), right_count - idx,
                                         SORTEDARRAY_KEY(type, left, median), &match) == ~0U
                 && (type->flags & SXE_SORTEDARRAY_CMP_CAN_FAIL))
                    return false;

                if (match && !type->visit(type->value, SORTEDARRAY_ELEM(type, left, median)))    // Visit element
                    return false;
            }

            return true;
        }
    }
    else
        median++;    // Move past the median in the left array

    if (idx < right_count)
        return sxe_sortedarray_intersect(type, SORTEDARRAY_KEY(type, left, median), left_count - median,
                                         SORTEDARRAY_KEY(type, right, idx), right_count - idx);

    return true;
}


/**
 * Delete key element from sorted array, returning true if the key is found and removed and decrementing count.
 *
 * @param type      Defines the type of the elements
 * @param array     The array to search
 * @param count     Number of elements in the array
 * @param key       The key to delete
 *
 * @return Boolean value equaling true if the key is found and deleted successfully, false otherwise.
 */
bool
sxe_sortedarray_delete(const struct sxe_sortedarray_class *type, void *array, unsigned *count, const void *key)
{
    bool     match;
    unsigned pos;

    if (!array || *count == 0)
        return false;

    pos = sxe_sortedarray_find(type, array, *count, key, &match);

    if (!match)
        return false;

    memmove((uint8_t *)array + pos * type->size, (uint8_t *)array + (pos + 1) * type->size, (size_t)(*count - pos - 1) * type->size);
    (*count)--;

    return true;
}
