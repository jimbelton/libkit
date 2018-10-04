#include <stdint.h>
#include <string.h>
#include <sxe-log.h>

#include "kit.h"
#include "kit-alloc.h"

static void *kit_sortedarray_realloc(void *memory, size_t size) {
    return kit_realloc(memory, size);
}

static void *(*reallocate)(void *, size_t) = kit_sortedarray_realloc;

/**
 * Allow use of an alternate reallocator
 */
kit_realloc_ptr_t
kit_sortedarray_override_realloc(void *(*new_realloc)(void *, size_t))
{
    void *(*old_realloc)(void *, size_t) = reallocate;
    reallocate = new_realloc;
    return old_realloc;
}

/**
 * Add an element to a sorted array
 *
 * @param class      Object definining the class of elements of the array
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
kit_sortedarray_add(const struct kit_sortedelement_class *class, void **array, unsigned *count, unsigned *alloc,
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
        slot = (uint8_t *)*array + (pos - 1) * class->size;
        key  = (const uint8_t *)element + class->keyoffset;

        if ((cmp = class->cmp(slot + class->keyoffset, key)) == 0)    // Already exists
            return false;

        if (cmp > 0) {    // Out of order
            // This (actually the memmove() below) is expensive when building large pref blocks
            if (flags & KIT_SORTEDARRAY_ALLOW_INSERTS) {
                pos  = kit_sortedarray_find(class, *array, *count, key);
                slot = (uint8_t *)*array + pos * class->size;

                if (pos < *count && class->cmp(slot + class->keyoffset, key) == 0)    // Already exists
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
        if (!(new_array = (*reallocate)(*array, (*alloc + more) * class->size))) {
            SXEL2("Failed to allocate array of %u %zu byte elements", *alloc + more, class->size);
            return false;
        }

        *array  = new_array;
        *alloc += more;
    }

    slot = (uint8_t *)*array + pos * class->size;
    SXEL7("Inserting element %s at position %u", (*class->fmt)((const uint8_t *)element + class->keyoffset), pos);

    if (pos < *count)
        memmove(slot + class->size, slot, (*count - pos) * class->size);

    if (!(flags & KIT_SORTEDARRAY_ZERO_COPY))
        memcpy(slot, element, class->size);

    (*count)++;
    return slot;
}

unsigned
kit_sortedarray_find(const struct kit_sortedelement_class *class, const void *array, unsigned count, const void *key)
{
    unsigned i, lim, pos;
    int cmp;

    for (pos = 0, lim = count; lim; lim >>= 1) {
        i   = pos + (lim >> 1);
        cmp = class->cmp(key, (const uint8_t *)array + class->size * i + class->keyoffset);

        if (cmp == 0) {
            pos = i;
            break;
        }

        if (cmp > 0) {
            pos = i + 1;
            lim--;
        }
    }

    SXEA6(pos == count || class->cmp(key, (const uint8_t *)array + class->size * pos + class->keyoffset) <= 0,
          "Unexpected pos %u looking for %s, landed on %s", pos, (*class->fmt)(key),
          (*class->fmt)((const uint8_t *)array + class->size * pos + class->keyoffset));

    SXEL7("%s(me=?, count=%u, key=%s) // return %u, val %s, prev %s, next %s", __FUNCTION__, count, (*class->fmt)(key), pos,
          pos < count     ? class->fmt((const uint8_t *)array + class->size * pos + class->keyoffset)       : "NOT FOUND",
          pos > 0         ? class->fmt((const uint8_t *)array + class->size * (pos - 1) + class->keyoffset) : "NONE",
          pos + 1 < count ? class->fmt((const uint8_t *)array + class->size * (pos + 1) + class->keyoffset) : "NONE");

    return pos;
}

const void *
kit_sortedarray_get(const struct kit_sortedelement_class *class, const void *array, unsigned count, const void *key)
{
    SXEA6(array || count == 0, "kit_sortedarray_get called with a NULL array and count %u", count);
    unsigned pos = array ? kit_sortedarray_find(class, array, count, key) : count;

    if (pos < count && class->cmp(key, (const uint8_t *)array + class->size * pos + class->keyoffset) == 0)
        return (const uint8_t *)array + class->size * pos;

    return NULL;
}
