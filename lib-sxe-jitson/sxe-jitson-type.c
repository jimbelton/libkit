/* Copyright (c) 2022 Jim Belton
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "sxe-jitson.h"
#include "sxe-jitson-const.h"
#include "sxe-jitson-oper.h"
#include "sxe-log.h"
#include "sxe-util.h"

struct sxe_jitson_type {
    const char *name;
    const void *extra;                                     // Allow the creator of a type to add extra information
    void      (*free)(            struct sxe_jitson *);    // Free any memory allocated to the value (e.g. indices)
    int       (*test)(      const struct sxe_jitson *);    // Return true (1), false (0) or error (-1)
    uint32_t  (*size)(      const struct sxe_jitson *);    // The size in "struct sxe_jitson"s of the value.
    size_t    (*len)(       const struct sxe_jitson *);    // The logical length (strlen, number of elements, number of members)
    bool      (*clone)(     const struct sxe_jitson *, struct sxe_jitson *);          // Clone (deep copy) a value's data
    char *    (*build_json)(const struct sxe_jitson *, struct sxe_factory *);         // Format a value
    int       (*cmp)(       const struct sxe_jitson *, const struct sxe_jitson *);    // Compare two values <, =, or >
    int       (*eq)(        const struct sxe_jitson *, const struct sxe_jitson *);    // Equal (1) or not (0), -1 on error
};

const struct sxe_jitson *sxe_jitson_false = NULL;
const struct sxe_jitson *sxe_jitson_true  = NULL;
const struct sxe_jitson *sxe_jitson_null  = NULL;

uint32_t sxe_jitson_flags = 0;    // Flags passed at initialization

static uint32_t                type_count   = 0;
static struct sxe_jitson_type *jitson_types = NULL;
static struct sxe_factory      type_factory[1];

/**
 * Register a type.
 *
 * @param name       The name of the type. e.g. "bool" is the name of the boolean type whose values are true and false
 * @param free       Free a value of this type. e.g. sxe_jitson_free_base is often a good default
 * @param test       Test a value of this type, returning SXE_JITSON_TEST_TRUE, SXE_JITSON_TEST_FALSE, or SXE_JITSON_TEST_ERROR.
 * @param size       Return the size of a value of this type. e.g. sxe_jitson_size_1 for types guaranteed to fit in one jitson
 * @param len        Return the length of a value. e.g. sxe_jitson_type_len or NULL for types that don't have a length.
 * @param clone      Deep copy a value of this type. NULL if the type can be copied without extra work (e.g. numbers, bools)
 * @param build_json Convert a value back into JSON using the given factory
 * @param cmp        Determine whether a value is <, =, or > another value. NULL for types that can't be ordered.
 * @param eq         Determine whether two values are equal. NULL to use cmp or if no comparisons are supported.
 */
uint32_t
sxe_jitson_type_register(const char *name,
                         void      (*free)(            struct sxe_jitson *),
                         int       (*test)(      const struct sxe_jitson *),
                         uint32_t  (*size)(      const struct sxe_jitson *),
                         size_t    (*len)(       const struct sxe_jitson *),
                         bool      (*clone)(     const struct sxe_jitson *, struct sxe_jitson *),
                         char *    (*build_json)(const struct sxe_jitson *, struct sxe_factory *),
                         int       (*cmp)(       const struct sxe_jitson *, const struct sxe_jitson *),
                         int       (*eq)(        const struct sxe_jitson *, const struct sxe_jitson *))
{
    uint32_t type = type_count++;

    SXEA1(jitson_types, ":sxe_jitson_type_init has not been called");
    SXEA1(jitson_types = (struct sxe_jitson_type *)sxe_factory_reserve(type_factory, type_count * sizeof(*jitson_types)),
          "Couldn't allocate %u jitson types", type_count);
    jitson_types[type].name       = name;
    jitson_types[type].extra      = NULL;
    jitson_types[type].free       = free;
    jitson_types[type].test       = test;
    jitson_types[type].size       = size;
    jitson_types[type].len        = len;
    jitson_types[type].clone      = clone;
    jitson_types[type].build_json = build_json;
    jitson_types[type].cmp        = cmp;
    jitson_types[type].eq         = eq;

    sxe_jitson_oper_increase_num_types(type);
    return type;
}

/* Get extra type info set below
 */
const void *
sxe_jitson_type_get_extra(unsigned type)
{
    return jitson_types[type].extra;
}

/* Set extra type info used only by the caller
 */
void
sxe_jitson_type_set_extra(unsigned type, const void *extra)
{
    jitson_types[type].extra = extra;
}

const char *
sxe_jitson_type_to_str(unsigned type)
{
    if (type > type_count) {
        errno = ERANGE;
        return "ERROR";
    }

    return jitson_types[type].name;
}

unsigned
sxe_jitson_get_type(const struct sxe_jitson *jitson)
{
    if (sxe_jitson_is_reference(jitson))
        return sxe_jitson_get_type(jitson->jitref);

    return jitson->type & SXE_JITSON_TYPE_MASK;
}

/* Most types can use this as a free function, and those that don't should call it after any special work they do.
 */
void
sxe_jitson_free_base(struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_IS_OWN) {    // If this jitson contains a reference to a value or index that it owns
        // Atomically nullify the reference in case there is a race, though calling code should ensure this is not the case
        void *reference = __sync_lock_test_and_set(&jitson->index, NULL);
        kit_free(reference);
    }

    jitson->type = SXE_JITSON_TYPE_INVALID;
}

uint32_t
sxe_jitson_size_1(const struct sxe_jitson *jitson)
{
    SXE_UNUSED_PARAMETER(jitson);
    return 1;
}

static int
sxe_jitson_null_test(const struct sxe_jitson *jitson)
{
    SXE_UNUSED_PARAMETER(jitson);
    return SXE_JITSON_TEST_FALSE;
}

static char *
sxe_jitson_null_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    SXE_UNUSED_PARAMETER(jitson);
    sxe_factory_add(factory, "null", 4);
    return sxe_factory_look(factory, NULL);
}

static int
sxe_jitson_null_eq(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    SXE_UNUSED_PARAMETER(left);
    SXE_UNUSED_PARAMETER(right);
    return SXE_JITSON_TEST_TRUE;      // There's only one value, so nulls always equal eachother
}

uint32_t
sxe_jitson_type_register_null(void)
{
    return sxe_jitson_type_register("null", sxe_jitson_free_base, sxe_jitson_null_test, sxe_jitson_size_1, NULL, NULL,
                                    sxe_jitson_null_build_json, NULL, sxe_jitson_null_eq);
}

static int
sxe_jitson_bool_test(const struct sxe_jitson *jitson)
{
    return jitson->boolean ? SXE_JITSON_TEST_TRUE : SXE_JITSON_TEST_FALSE;
}

static char *
sxe_jitson_bool_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    if (jitson->boolean)
        sxe_factory_add(factory, "true", 4);
    else
        sxe_factory_add(factory, "false", 5);

    return sxe_factory_look(factory, NULL);
}

static int
sxe_jitson_bool_eq(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    return left->boolean == right->boolean ? SXE_JITSON_TEST_TRUE : SXE_JITSON_TEST_FALSE;
}

uint32_t
sxe_jitson_type_register_bool(void)
{
    return sxe_jitson_type_register("bool", sxe_jitson_free_base, sxe_jitson_bool_test, sxe_jitson_size_1, NULL, NULL,
                                    sxe_jitson_bool_build_json, NULL, sxe_jitson_bool_eq);
}

static int
sxe_jitson_number_test(const struct sxe_jitson *jitson)
{
    return jitson->number != 0.0 ? SXE_JITSON_TEST_TRUE : SXE_JITSON_TEST_FALSE;
}

#define NUMBER_MAX_LEN 24    // Enough space for the largest double and the largest uint64_t

static char *
sxe_jitson_number_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    char    *ret;
    unsigned len;

    if ((ret = sxe_factory_reserve(factory, NUMBER_MAX_LEN)) == NULL)
        return NULL;

    if (jitson->type & SXE_JITSON_TYPE_IS_UINT) {
        len = snprintf(ret, NUMBER_MAX_LEN + 1, "%"PRIu64, sxe_jitson_get_uint(jitson));
        SXEA6(len <= NUMBER_MAX_LEN, "As a string, numeric value %"PRIu64" is more than %u characters long",
              sxe_jitson_get_uint(jitson), NUMBER_MAX_LEN);
    } else {
        /* The format %.17G comes from:
        *    https://stackoverflow.com/questions/16839658/printf-width-specifier-to-maintain-precision-of-floating-point-value
        *
        * Using .17G reveals rounding errors in the encoding of fractions. For example:
        *    got:      "pi":3.1415899999999999,"number":1.1415900000000001
        *    expected: "pi":3.14159,           "number":1.14159
        *
        * Reducing the format to .16G rounds the numbers back to what was orinally parsed.
        */
        len = snprintf(ret, NUMBER_MAX_LEN + 1, "%.16G", sxe_jitson_get_number(jitson));
        SXEA6(len <= NUMBER_MAX_LEN, "As a string, numeric value %.16G is more than %u characters long",
              sxe_jitson_get_number(jitson), NUMBER_MAX_LEN);
    }

    sxe_factory_commit(factory, len);
    return sxe_factory_look(factory, NULL);
}

static int
sxe_jitson_number_cmp(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    uint64_t cast_uint;
    double   cast_float;

    if (left->type & SXE_JITSON_TYPE_IS_UINT) {
        if (right->type & SXE_JITSON_TYPE_IS_UINT)
            return sxe_uint64_cmp(left->integer, right->integer);

        if ((double)(cast_uint = (uint64_t)right->number) == right->number)
            return sxe_uint64_cmp(left->integer, cast_uint);

        if ((uint64_t)(cast_float = (double)left->integer) == left->integer)
            return cast_float < right->number ? -1 : cast_float == right->number ? 0 : 1;

        SXEL2(": Cannot compare %"PRIu64" with %.16g", left->integer, right->number);
        return SXE_JITSON_CMP_ERROR;
    }

    if (!(right->type & SXE_JITSON_TYPE_IS_UINT))
        return left->number < right->number ? -1 : left->number == right->number ? 0 : 1;

    if ((double)(cast_uint = (uint64_t)left->number) == left->number)
        return sxe_uint64_cmp(cast_uint, right->integer);

    if ((uint64_t)(cast_float = (double)right->integer) == right->integer)
        return left->number < cast_float ? -1 : left->number == cast_float ? 0 : 1;

    SXEL2(": Cannot compare %.16g with %"PRIu64, left->number, right->integer);
    return SXE_JITSON_CMP_ERROR;
}

uint32_t
sxe_jitson_type_register_number(void)
{
    return sxe_jitson_type_register("number", sxe_jitson_free_base, sxe_jitson_number_test, sxe_jitson_size_1, NULL, NULL,
                                    sxe_jitson_number_build_json, sxe_jitson_number_cmp, NULL);
}

size_t
sxe_jitson_string_len(const struct sxe_jitson *jitson)
{
    size_t len;

    if (jitson->type & SXE_JITSON_TYPE_IS_KEY)    // Object keys use the len field to store a link offset
        return jitson->type & SXE_JITSON_TYPE_IS_REF ? strlen(jitson->reference) : strlen(jitson->string);    // SonarQube False Positive

    if (jitson->len == 0 && (jitson->type & SXE_JITSON_TYPE_IS_REF)) {
        len = strlen(jitson->reference);    // SonarQube False Positive

        if ((uint32_t)len == len)    // Can't cache the length if > 4294967295
            /* This is thread safe because the assignment is atomic and the referenced string is immutable
             */
            ((struct sxe_jitson *)(uintptr_t)jitson)->len = (uint32_t)len;

        return len;
    }

    return jitson->len;
}

static uint32_t
sxe_jitson_string_size(const struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_IS_REF)
        return 1;

    return 1
         + (uint32_t)((sxe_jitson_string_len(jitson) + SXE_JITSON_TOKEN_SIZE - SXE_JITSON_STRING_SIZE) / SXE_JITSON_TOKEN_SIZE);
}

int
sxe_jitson_string_test(const struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_IS_REF ? *(const uint8_t *)jitson->reference : jitson->len)
        return SXE_JITSON_TEST_TRUE;
    else
        return SXE_JITSON_TEST_FALSE;
}

static bool
sxe_jitson_string_clone(const struct sxe_jitson *jitson, struct sxe_jitson *clone)
{
    /* If the jitson owns the string it's refering to, it must be duplicated
     */
    if ((jitson->type & SXE_JITSON_TYPE_IS_OWN)
     && (clone->reference = MOCKERROR(MOCK_FAIL_STRING_CLONE, NULL, ENOMEM, kit_strdup(jitson->reference))) == NULL) {
        SXEL2("Failed to duplicate a %zu byte string", strlen(jitson->string));
        return false;
    }

    return true;
}

static char *
sxe_jitson_string_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    const char *string;
    char       *buffer;
    size_t      len;
    unsigned    first, i;

    len    = sxe_jitson_len(jitson);
    string = sxe_jitson_get_string(jitson, NULL);
    sxe_factory_add(factory, "\"", 1);

    for (first = i = 0; i < len; i++)
        /* If the character is a control character or " or \, encode it as a unicode escape sequence.
         * (unsigned char) casts are used to allow any UTF8 encoded string.
         */
        if ((unsigned char)string[i] <= 0x1F || string[i] == '"' || string[i] == '\\') {
            if (first < i)
                sxe_factory_add(factory, &string[first], i - first);

            if ((buffer = sxe_factory_reserve(factory, sizeof("\\u0000"))) == NULL)
                return NULL;    /* COVERAGE EXCLUSION: Memory allocation failure */

            snprintf(buffer, sizeof("\\u0000"), "\\u00%02x", (unsigned char)string[i]);
            SXEA6(strlen(buffer) == sizeof("\\u0000") - 1, "Unicode escape sequence should always be 6 characters long");
            sxe_factory_commit(factory, sizeof("\\u0000") - 1);
            first = i + 1;
        }

    if (first < len)
        sxe_factory_add(factory, &string[first], len - first);

    sxe_factory_add(factory, "\"", 1);
    return sxe_factory_look(factory, NULL);
}

int
sxe_jitson_string_cmp(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    const char *left_str  = left->type  & SXE_JITSON_TYPE_IS_REF ? left->reference  : left->string;
    const char *right_str = right->type & SXE_JITSON_TYPE_IS_REF ? right->reference : right->string;
    int         ret       = strcmp(left_str, right_str);

    return ret < 0 ? -1 : ret == 0 ? 0 : 1;
}

static int
sxe_jitson_string_eq(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    if (left->len && right->len && left->len != right->len)    // If both lengths are known and different, not equal!
        return SXE_JITSON_TEST_FALSE;

    const char *left_str  = left->type  & SXE_JITSON_TYPE_IS_REF ? left->reference  : left->string;
    const char *right_str = right->type & SXE_JITSON_TYPE_IS_REF ? right->reference : right->string;
    return strcmp(left_str, right_str) == 0 ? SXE_JITSON_TEST_TRUE : SXE_JITSON_TEST_FALSE;
}

uint32_t
sxe_jitson_type_register_string(void)
{
    return sxe_jitson_type_register("string", sxe_jitson_free_base, sxe_jitson_string_test, sxe_jitson_string_size,
                                    sxe_jitson_string_len, sxe_jitson_string_clone, sxe_jitson_string_build_json,
                                    sxe_jitson_string_cmp, sxe_jitson_string_eq);
}

static uint32_t
sxe_jitson_size_indexed(const struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_INDEXED)    // Once an array or object is indexed, it's size is at the end of the index
        return jitson->index[jitson->len];

    return (uint32_t)jitson->integer;    // Prior to indexing, the offset past the end of the object/array is stored here
}

/**
 * Determine the size of an array or array-like jitson (e.g. a range)
 *
 * @param jitson An array or array-like jitson
 *
 * @return The size in jitsons
 */
uint32_t
sxe_jitson_array_size(const struct sxe_jitson *jitson)
{
    if (jitson->type & SXE_JITSON_TYPE_IS_UNIF)    // Uniform arrays have their element sizes in bytes; round up to jitsons
        return 1 + jitson->len * ((jitson->uniform.size + sizeof(*jitson) - 1) / sizeof(*jitson));

    return sxe_jitson_size_indexed(jitson);
}

int
sxe_jitson_test_len(const struct sxe_jitson *jitson)
{
    return jitson->len != 0 ? SXE_JITSON_TEST_TRUE : SXE_JITSON_TEST_FALSE;
}

/**
 * Length function for types that store their lengths in the len field
 *
 * @param jitson Jitson of a type that stores its type in the len field (e.g. an array)
 */
size_t
sxe_jitson_len_base(const struct sxe_jitson *jitson)
{
    return jitson->len;
}

/**
 * Free a jitson array or array-like jitson (e.g. a range)
 *
 * @param jitson An array or array-like jitson
 *
 * @note The free is done without using the index so that referenced data owned by it's members will be freed.
 */
void
sxe_jitson_array_free(struct sxe_jitson *jitson)
{
    struct sxe_jitson *element;
    unsigned           i;
    uint32_t           offset, size;

    for (i = 0, offset = 1; i < jitson->len; i++, offset += size) {
        size = sxe_jitson_size(element = jitson + offset);
        jitson_types[element->type & SXE_JITSON_TYPE_MASK].free(element);
    }

    sxe_jitson_free_base(jitson);
}

/**
 * Determine the size of an array or array-like jitson (e.g. a range)
 *
 * @param jitson An array or array-like jitson
 *
 * @return The size in jitsons
 */
bool
sxe_jitson_array_clone(const struct sxe_jitson *jitson, struct sxe_jitson *clone)
{
    size_t   size;
    unsigned i, len;

    if ((len = jitson->len) == 0)
        return true;

    if (jitson->type & SXE_JITSON_TYPE_INDEXED) {
        if (!(clone->index = MOCKERROR(MOCK_FAIL_ARRAY_CLONE, NULL, ENOMEM, kit_malloc((size = (len + 1) * sizeof(jitson->index[0]))))))
        {
            SXEL2("Failed to allocate %zu bytes to clone an array", (len + 1) * sizeof(jitson->index[0]));
            return false;
        }

        memcpy(clone->index, jitson->index, size);
    }

    for (i = 0; i < len; i++)
        if (!sxe_jitson_clone(sxe_jitson_array_get_element(jitson, i),
                              (struct sxe_jitson *)(uintptr_t)sxe_jitson_array_get_element(clone, i))) {
            while (i > 0)    // On error, free any allocations done
                sxe_jitson_free(SXE_CAST_NOCONST(struct sxe_jitson *, sxe_jitson_array_get_element(clone, --i)));

            if (clone->type & SXE_JITSON_TYPE_INDEXED)
                kit_free(clone->index);

            return false;
        }

    return true;
}

/**
 * Convert an array or array-like jitson (e.g. a range) to a JSON string
 *
 * @param jitson An array or array-like jitson
 *
 * @return A pointer to the string
 */
char *
sxe_jitson_array_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    unsigned i, len;

    sxe_factory_add(factory, "[", 1);

    if ((len = jitson->len) > 0) {
        for (i = 0; i < len - 1; i++) {
            sxe_jitson_build_json(sxe_jitson_array_get_element(jitson, i), factory);
            sxe_factory_add(factory, ",", 1);
        }

        sxe_jitson_build_json(sxe_jitson_array_get_element(jitson, len - 1), factory);
    }

    sxe_factory_add(factory, "]", 1);
    return sxe_factory_look(factory, NULL);
}

static int
sxe_jitson_array_cmp(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    unsigned i;
    int      ret;

    for (i = 0; i < left->len; i++) {
        if (i >= right->len)    // Right is a prefix of left
            return 1;

        if ((ret = sxe_jitson_cmp(sxe_jitson_array_get_element(left, i), sxe_jitson_array_get_element(right, i))))
            return ret;
    }

    if (i < right->len)    // Left is a prefix of right
        return -1;

    return 0;
}

/**
 * Test two arrays or array-like jitsons (e.g. ranges) for equality
 *
 * @param left/right Arrays or array-like jitsons to compare
 *
 * @return SXE_JITSON_TEST_TRUE if the same, SXE_JITSON_TEST_FALSE if not.
 */
int
sxe_jitson_array_eq(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    unsigned i;
    int      ret;

    if (left->len != right->len)
        return SXE_JITSON_TEST_FALSE;

    for (i = 0; i < left->len; i++)
        if ((ret = sxe_jitson_eq(sxe_jitson_array_get_element(left, i), sxe_jitson_array_get_element(right, i)))
         != SXE_JITSON_TEST_TRUE)
            return ret;

    return SXE_JITSON_TEST_TRUE;
}

uint32_t
sxe_jitson_type_register_array(void)
{
    return sxe_jitson_type_register("array", sxe_jitson_array_free, sxe_jitson_test_len, sxe_jitson_array_size,
                                    sxe_jitson_len_base, sxe_jitson_array_clone, sxe_jitson_array_build_json,
                                    sxe_jitson_array_cmp, sxe_jitson_array_eq);
}

/**
 * Free any data allocated by a jitson value that is contained in a larger value
 *
 * @note This should only be called on a contained jitson when the container is being freed; the containee itself isn't freed
 */
void
sxe_jitson_free_containee(struct sxe_jitson *containee)
{
    jitson_types[containee->type & SXE_JITSON_TYPE_MASK].free(containee);
}

/* Free a jitson object without using the index so that referenced data owned by it's members will be freed.
 */
static void
sxe_jitson_free_object(struct sxe_jitson *jitson)
{
    struct sxe_jitson *element;
    unsigned           i, len;
    uint32_t           offset, size;

    len = jitson->len * 2;    // Objects are pairs of member name/value

    for (i = 0, offset = 1; i < len; i++, offset += size) {
        size = sxe_jitson_size(element = jitson + offset);
        sxe_jitson_free_containee(element);
    }

    sxe_jitson_free_base(jitson);
}

/* Helper for cloning the members of one object into another
 */
bool
sxe_jitson_object_clone_members(const struct sxe_jitson *jitson, struct sxe_jitson *clone, unsigned len)
{
    const struct sxe_jitson *content;
    size_t                   size;
    unsigned                 i;
    uint32_t                 index, stop;
    bool                     key;

    for (i = 0, index = 1; i < len; i++, index += size + sxe_jitson_size(&clone[index + size])) {    // For each member
        content = &jitson[index];
        size    = sxe_jitson_size(content);

        /* Clone the key and the value
         */
        if (!(key = sxe_jitson_clone(content, &clone[index])) || !sxe_jitson_clone(content + size, &clone[index + size])) {
            stop = index;

            /* For each member already processed
             */
            for (i = 0, index = 1; index < stop; i++, index += size + sxe_jitson_size(&clone[index + size])) {
                SXEA6(i < len, "We missed our stop!");
                size = sxe_jitson_size(&clone[index]);
                sxe_jitson_free(&clone[index]);
                sxe_jitson_free(&clone[index + size]);
            }

            SXEA6(index == stop, "We skipped our stop!");

            if (key)    // If the last key was cloned, free it
                sxe_jitson_free(&clone[index]);

            return false;
        }
    }

    return true;
}

static bool
sxe_jitson_object_clone(const struct sxe_jitson *jitson, struct sxe_jitson *clone)
{
    size_t   size;
    unsigned len;

    if ((len = jitson->len) == 0)
        return true;

    if (jitson->type & SXE_JITSON_TYPE_INDEXED) {    // If the object is already indexed, must clone it's index
        size = (len + 1) * sizeof(jitson->index[0]);

        if (!(clone->index = MOCKERROR(MOCK_FAIL_OBJECT_CLONE, NULL, ENOMEM, kit_malloc(size)))) {
            SXEL2("Failed to allocate %zu bytes to clone an object", (len + 1) * sizeof(jitson->index[0]));
            return false;
        }

        SXEA6(clone->type & SXE_JITSON_TYPE_INDEXED, "Clone of object should already be marked indexed");
        memcpy(clone->index, jitson->index, size);
    }

    if (!sxe_jitson_object_clone_members(jitson, clone, len)) {
        if (clone->type & SXE_JITSON_TYPE_INDEXED) {    // If the index was cloned, free it up
            kit_free(clone->index);
            clone->index = NULL;
        }

        return false;
    }

    return true;
}

static char *
sxe_jitson_object_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    unsigned i, index, len;
    bool     first;

    if ((len = jitson->len) == 0) {
        sxe_factory_add(factory, "{}", 2);
        return sxe_factory_look(factory, NULL);
    }

    if (!(jitson->type & SXE_JITSON_TYPE_INDEXED))    // Force indexing. Would it be better to walk the unindexed object?
        sxe_jitson_object_get_member(jitson, "", 0);

    sxe_factory_add(factory, "{", 1);

    for (first = true, i = 0; i < len; i++) {                                  // For each bucket
        for (index = jitson->index[i]; index; index = jitson[index].link) {    // For each member in the bucket
            if (!first)
                 sxe_factory_add(factory, ",", 1);

            sxe_jitson_build_json(&jitson[index], factory);                                      // Output the member name
            sxe_factory_add(factory, ":", 1);
            sxe_jitson_build_json(&jitson[index] + sxe_jitson_size(&jitson[index]), factory);    // Output the value
            first = false;
        }
    }

    sxe_factory_add(factory, "}", 1);
    return sxe_factory_look(factory, NULL);
}

/**
 * Register the JSON object type
 *
 * @note Objects cannot currently be compared
 */
uint32_t
sxe_jitson_type_register_object(void)
{
    return sxe_jitson_type_register("object", sxe_jitson_free_object, sxe_jitson_test_len, sxe_jitson_size_indexed,
                                    sxe_jitson_len_base, sxe_jitson_object_clone, sxe_jitson_object_build_json, NULL, NULL);
}

static int
sxe_jitson_reference_test(const struct sxe_jitson *jitson)
{
    return jitson_types[sxe_jitson_get_type(jitson->jitref)].test(jitson->jitref);
}

static size_t
sxe_jitson_reference_len(const struct sxe_jitson *jitson)
{
    return jitson_types[sxe_jitson_get_type(jitson->jitref)].len(jitson->jitref);
}

char *
sxe_jitson_reference_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    return jitson_types[sxe_jitson_get_type(jitson->jitref)].build_json(jitson->jitref, factory);
}

uint32_t
sxe_jitson_type_register_reference(void)
{
    return sxe_jitson_type_register("reference", sxe_jitson_free_base, sxe_jitson_reference_test, sxe_jitson_size_1,
                                    sxe_jitson_reference_len, NULL, sxe_jitson_reference_build_json, NULL, NULL);
}

/**
 * Initialize the types and register INVALID (0) and all JSON types: null, bool, number, string, array, object, and pointer.
 *
 * @param mintypes  The minimum number of types to preallocate. Should be SXE_JITSON_MIN_TYPES + the number of additional types
 * @param flags     SXE_JITSON_FLAG_STRICT    for standard JSON or a combination of:
 *                  SXE_JITSON_FLAG_ALLOW_HEX to allow hexadecimal (and octal) unsigned integers
 *                  SXE_JITSON_FLAG_OPTIMIZE  to optimize while parsing, slowing it, but reducing space and speeding evaluation
 */
void
sxe_jitson_initialize(uint32_t mintypes, uint32_t flags)
{
    sxe_factory_alloc_make(type_factory, mintypes < SXE_JITSON_MIN_TYPES ? SXE_JITSON_MIN_TYPES : mintypes, 256);
    jitson_types = (struct sxe_jitson_type *)sxe_factory_reserve(type_factory, mintypes * sizeof(*jitson_types));
    SXEA1(sxe_jitson_type_register("INVALID", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL) == 0,
          "Type 0 is the 'INVALID' type");
    SXEA1(sxe_jitson_type_register_null()      == SXE_JITSON_TYPE_NULL,      "Type 1 is 'null'");
    SXEA1(sxe_jitson_type_register_bool()      == SXE_JITSON_TYPE_BOOL,      "Type 2 is 'bool'");
    SXEA1(sxe_jitson_type_register_number()    == SXE_JITSON_TYPE_NUMBER,    "Type 3 is 'number'");
    SXEA1(sxe_jitson_type_register_string()    == SXE_JITSON_TYPE_STRING,    "Type 4 is 'string'");
    SXEA1(sxe_jitson_type_register_array()     == SXE_JITSON_TYPE_ARRAY,     "Type 5 is 'array'");
    SXEA1(sxe_jitson_type_register_object()    == SXE_JITSON_TYPE_OBJECT,    "Type 6 is 'object'");
    SXEA1(sxe_jitson_type_register_reference() == SXE_JITSON_TYPE_REFERENCE, "Type 7 is 'reference'");
    sxe_jitson_flags |= flags;

    sxe_jitson_builtins_initialize_private();

    /* The following constant values are used to avoid malloc/free of temporary values
     */
    sxe_jitson_true  = sxe_jitson_const_get(SXE_JITSON_FLAG_STRICT, "true",  sizeof("true")  - 1);
    sxe_jitson_false = sxe_jitson_const_get(SXE_JITSON_FLAG_STRICT, "false", sizeof("false") - 1);
    sxe_jitson_null  = sxe_jitson_const_get(SXE_JITSON_FLAG_STRICT, "null",  sizeof("null")  - 1);
}

/**
 * Finalize memory used for type and clear the types variables
 */
void
sxe_jitson_finalize(void)
{
    kit_free(sxe_factory_remove(type_factory, NULL));
    jitson_types = NULL;
    type_count   = 0;

    sxe_jitson_builtins_finalize_private();
}

bool
sxe_jitson_is_init(void)
{
    return jitson_types ? true : false;
}

/**
 * Free a jitson object if it was allocated
 *
 * @note All other threads that might access the object must remove their references to it before this function is called,
 *       unless all just in time operations (i.e. string length of a referenced string, construction of indeces) can be
 *       guaranteed to have already happened.
 */
void
sxe_jitson_free(const struct sxe_jitson *jitson)
{
    if (jitson && (jitson->type & SXE_JITSON_TYPE_ALLOCED)) {    // If it was allocated, it's safe to free it
        struct sxe_jitson *mutable = SXE_CAST_NOCONST(struct sxe_jitson *, jitson);

        jitson_types[jitson->type & SXE_JITSON_TYPE_MASK].free(mutable);
        mutable->type = SXE_JITSON_TYPE_INVALID;
        kit_free(mutable);
    }
}

/**
 * Test a jitson object
 *
 * @return SXE_JITSON_TEST_TRUE, SXE_JITSON_TEST_FALSE, or SXE_JITSON_TEST_ERROR
 *
 * @note For standard JSON types, tests never return SXE_JITSON_TEST_ERROR.
 */
int
sxe_jitson_test(const struct sxe_jitson *jitson)
{
    return jitson_types[jitson->type & SXE_JITSON_TYPE_MASK].test(jitson);
}

/**
 * Determine the size in jitson tokens of a jitson object
 */
uint32_t
sxe_jitson_size(const struct sxe_jitson *jitson)
{
    return jitson_types[jitson->type & SXE_JITSON_TYPE_MASK].size(jitson);
}

/**
 * Determine whether a jitson supports taking its length
 */
bool
sxe_jitson_supports_len(const struct sxe_jitson *jitson)
{
    return jitson_types[jitson->type & SXE_JITSON_TYPE_MASK].len != NULL;
}

/**
 * Determine the length of a jitson value. For strings, this is the string length, for collections, the number of members.
 */
size_t
sxe_jitson_len(const struct sxe_jitson *jitson)
{
    SXEA1(sxe_jitson_supports_len(jitson), "Type %s does not support taking its length", sxe_jitson_get_type_as_str(jitson));
    return jitson_types[jitson->type & SXE_JITSON_TYPE_MASK].len(jitson);
}

bool
sxe_jitson_clone(const struct sxe_jitson *jitson, struct sxe_jitson *clone)
{
    uint32_t type = jitson->type & SXE_JITSON_TYPE_MASK;
    bool     success;

    SXEE6("(jitson=%p,clone=%p) // type=%u", jitson, clone, type);
    success = !jitson_types[type].clone || jitson_types[type].clone(jitson, clone);    // Succeeds if no clone or clone succeeds
    SXER6("return %s;", success ? "true" : "false");
    return success;
}

/**
 * Build a JSON string from a jitson object
 *
 * @param jitson  The jitson object to encode in JSON
 * @param factory The factory object used to build the JSON string
 */
char *
sxe_jitson_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    return jitson_types[jitson->type & SXE_JITSON_TYPE_MASK].build_json(jitson, factory);
}

/**
 * Compare two jitson objects
 *
 * @param left/right The objects to compare
 *
 * @return -1 if left < right, 0 if left == right, 1 if left > right, or SXE_JITSON_CMP_ERROR if the values can't be compared
 *
 * @note SXE_JITSON_CMP_ERROR will be returned if either jitson is NULL, the dereferenced jitsons have different types, or the
 *       type doesn't support comparisons
 */
int
sxe_jitson_cmp(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    if (!left || !right)    // Garbage in, error out
        return SXE_JITSON_CMP_ERROR;

    left  = sxe_jitson_is_reference(left)  ? left->jitref  : left;     // OK because refs to refs are not allowed
    right = sxe_jitson_is_reference(right) ? right->jitref : right;    // OK because refs to refs are not allowed

    uint32_t type = left->type & SXE_JITSON_TYPE_MASK;

    if (type != (right->type & SXE_JITSON_TYPE_MASK) || jitson_types[type].cmp == NULL)
        return SXE_JITSON_CMP_ERROR;

    return jitson_types[type].cmp(left, right);
}

/**
 * Determine whether two jitson values are equal
 *
 * @param left/right The values to compare
 *
 * @return SXE_JITSON_TEST_TRUE if equal, SXE_JITSON_TEST_FALSE if not equal, or SXE_JITSON_TEST_ERROR if the type is incomparable
 */
int
sxe_jitson_eq(const struct sxe_jitson *left, const struct sxe_jitson *right)
{
    int ret;

    SXEA1(left && right, "both left and right values must be provided");
    left  = sxe_jitson_is_reference(left)  ? left->jitref  : left;     // OK because refs to refs are not allowed
    right = sxe_jitson_is_reference(right) ? right->jitref : right;    // OK because refs to refs are not allowed

    uint32_t type = left->type & SXE_JITSON_TYPE_MASK;

    if (type != (right->type & SXE_JITSON_TYPE_MASK))
        return SXE_JITSON_TEST_FALSE;

    if (jitson_types[type].eq)    // If there is an eq, favor it, because it may be optimized over cmp
        return jitson_types[type].eq(left, right);

    if (jitson_types[type].cmp) {
        if ((ret = jitson_types[type].cmp(left, right)) == SXE_JITSON_CMP_ERROR)
            return SXE_JITSON_TEST_ERROR;

        return ret == 0 ? SXE_JITSON_TEST_TRUE : SXE_JITSON_TEST_FALSE;
    }

    return SXE_JITSON_TEST_ERROR;
}
