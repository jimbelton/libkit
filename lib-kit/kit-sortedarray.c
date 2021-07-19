#include <mockfail.h>
#include <stdint.h>
#include <string.h>
#include <sxe-log.h>

#include "kit.h"
#include "kit-alloc.h"

/**
 * Add an element to a sorted array
 *
 * @param type       Object definining the type of elements of the array
 * @param array      Pointer to the address of the array in malloced storage; *array == NULL to allocate first time
 * @param count      Pointer to the count of array elements
 * @param alloc      Pointer to the number of array slots allocated; used as the initial allocation if *array == NULL
 * @param element    Address of the element to insert
 * @param flags      KIT_SORTEDARRAY_DEFAULT or a combination of KIT_SORTEDARRAY_ALLOW_INSERTS | KIT_SORTEDARRAY_ALLOW_GROWTH
 *                                                             | KIT_SORTEDARRAY_ZERO_COPY
 *
 * @return Pointer to inserted element (uninitialized if KIT_SORTEDARRAY_ZERO_COPY passed), or NULL on a duplicate or error
 */
void *
kit_sortedarray_add(const struct kit_sortedelement_class *type, void **array, unsigned *count, unsigned *alloc,
                    const void *element, unsigned flags)
{
    unsigned    pos;
    uint8_t    *slot;
    const void *key;
    int         cmp;
    unsigned    more = 0;
    void       *new_array;

    SXEA6(*array || *count == 0, "Array is unallocated but count is non zero");

    if ((pos = *count) > 0) {    // If the array is not empty
        slot = (uint8_t *)*array + (pos - 1) * type->size;
        key  = (const uint8_t *)element + type->keyoffset;

        if ((cmp = type->cmp(slot + type->keyoffset, key)) == 0)    // Already exists
            return false;

        if (cmp > 0) {    // Out of order
            // This (actually the memmove() below) is expensive when building large pref blocks
            if (flags & KIT_SORTEDARRAY_ALLOW_INSERTS) {
                pos  = kit_sortedarray_find(type, *array, *count, key);
                slot = (uint8_t *)*array + pos * type->size;

                if (pos < *count && type->cmp(slot + type->keyoffset, key) == 0)    // Already exists
                    return NULL;
            } else {
                SXEL2("Unsorted list insertions are not permitted");
                return NULL;
            }
        }
    }

    if (*count == *alloc) {
        if (!(flags & KIT_SORTEDARRAY_ALLOW_GROWTH)) {
            SXEL2("Number of elements exceed %u, the maximum allowed in this array", *alloc);
            return NULL;
        }

        more = *alloc > 100 ? *alloc / 2 : 10;
    }

    // First time through, allocate the array; if more space is needed, reallocate
    if (!*array || more) {
        if (!(new_array = MOCKFAIL(kit_sortedarray_add, NULL, kit_realloc(*array, (*alloc + more) * type->size)))) {
            SXEL2("Failed to allocate array of %u %zu byte elements", *alloc + more, type->size);
            return false;
        }

        *array  = new_array;
        *alloc += more;
    }

    slot = (uint8_t *)*array + pos * type->size;
    SXEL7("Inserting element %s at position %u", (*type->fmt)((const uint8_t *)element + type->keyoffset), pos);

    if (pos < *count)
        memmove(slot + type->size, slot, (*count - pos) * type->size);

    if (!(flags & KIT_SORTEDARRAY_ZERO_COPY))
        memcpy(slot, element, type->size);

    (*count)++;
    return slot;
}

unsigned
kit_sortedarray_find(const struct kit_sortedelement_class *type, const void *array, unsigned count, const void *key)
{
    unsigned i, lim, pos;
    int cmp;

    for (pos = 0, lim = count; lim; lim >>= 1) {
        i   = pos + (lim >> 1);
        cmp = type->cmp(key, (const uint8_t *)array + type->size * i + type->keyoffset);

        if (cmp == 0) {
            pos = i;
            break;
        }

        if (cmp > 0) {
            pos = i + 1;
            lim--;
        }
    }

    SXEA6(pos == count || type->cmp(key, (const uint8_t *)array + type->size * pos + type->keyoffset) <= 0,
          "Unexpected pos %u looking for %s, landed on %s", pos, (*type->fmt)(key),
          (*type->fmt)((const uint8_t *)array + type->size * pos + type->keyoffset));

    SXEL7("%s(me=?, count=%u, key=%s) // return %u, val %s, prev %s, next %s", __FUNCTION__, count, (*type->fmt)(key), pos,
          pos < count     ? type->fmt((const uint8_t *)array + type->size * pos + type->keyoffset)       : "NOT FOUND",
          pos > 0         ? type->fmt((const uint8_t *)array + type->size * (pos - 1) + type->keyoffset) : "NONE",
          pos + 1 < count ? type->fmt((const uint8_t *)array + type->size * (pos + 1) + type->keyoffset) : "NONE");

    return pos;
}

const void *
kit_sortedarray_get(const struct kit_sortedelement_class *type, const void *array, unsigned count, const void *key)
{
    SXEA6(array || count == 0, "kit_sortedarray_get called with a NULL array and count %u", count);
    unsigned pos = array ? kit_sortedarray_find(type, array, count, key) : count;

    if (pos < count && type->cmp(key, (const uint8_t *)array + type->size * pos + type->keyoffset) == 0)
        return (const uint8_t *)array + type->size * pos;

    return NULL;
}
