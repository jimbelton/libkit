/* Copyright (c) 2021 Jim Belton
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

/* SXE jitson stacks are factories for building sxe-jitson.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "sxe-hash.h"
#include "sxe-jitson.h"
#include "sxe-jitson-const.h"
#include "sxe-log.h"
#include "sxe-sortedarray.h"
#include "sxe-thread.h"
#include "sxe-unicode.h"

#define JITSON_STACK_INIT_SIZE 1       // The initial numer of tokens in a per thread stack
#define JITSON_STACK_MAX_INCR  4096    // The maximum the stack will grow by

/* A per thread stack is kept for parsing. It's per thread for lockless thread safety, and automatically grows as needed.
 */
static unsigned                          jitson_stack_init_size = JITSON_STACK_INIT_SIZE;
static __thread struct sxe_jitson_stack *jitson_stack           = NULL;

/* Hook to allow parser to push unmatched identifiers onto the stack. This is a non-standard extension
 */
bool (*sxe_jitson_stack_push_ident)(struct sxe_jitson_stack *stack, const char *ident, size_t len) = NULL;

static bool
sxe_jitson_stack_make(struct sxe_jitson_stack *stack, unsigned init_size)
{
    SXEA1(SXE_JITSON_TOKEN_SIZE == 16, "Expected token size 16, got %zu", SXE_JITSON_TOKEN_SIZE);

    if (!stack)
        return false;

    memset(stack, 0, sizeof(*stack));
    stack->maximum = init_size;
    stack->jitsons = MOCKERROR(MOCK_FAIL_STACK_NEW_JITSONS, NULL, ENOMEM, kit_malloc((size_t)init_size * sizeof(*stack->jitsons)));
    return stack->jitsons ? true : false;
}

struct sxe_jitson_stack *
sxe_jitson_stack_new(unsigned init_size)
{
    struct sxe_jitson_stack *stack = MOCKERROR(MOCK_FAIL_STACK_NEW_OBJECT, NULL, ENOMEM, kit_malloc(sizeof(*stack)));

    if (!sxe_jitson_stack_make(stack, init_size)) {
        kit_free(stack);
        return NULL;
    }

    return stack;
}

/**
 * Extract the jitson parsed or constructed on a stack
 *
 * @param The stack
 *
 * @return The parsed or constructed jitson
 *
 * @note Aborts if there is no jitson on the stack or if there is a partially constructed one
 */
struct sxe_jitson *
sxe_jitson_stack_get_jitson(struct sxe_jitson_stack *stack)
{
    struct sxe_jitson *ret = stack->jitsons;
    size_t             size;

    SXEA1(stack->jitsons, "Can't get a jitson from an empty stack");
    SXEA1(!stack->open,   "Can't get a jitson there's an open collection");
    SXEE6("(stack=%p)", stack);

    if (stack->borrow) {
        SXEA1(stack->borrow < stack->count, "Can't get a jitson from a borrowed stack that hasn't been grown");
        size = (stack->count - stack->borrow) * sizeof(*stack->jitsons);

        if ((ret = kit_malloc(size))) {
            memcpy(ret, &stack->jitsons[stack->borrow], size);
            ret->type   |= SXE_JITSON_TYPE_ALLOCED;
            stack->count = stack->borrow;
        }

        goto OUT;
    }

    if (stack->maximum > stack->count)
        ret = kit_realloc(ret, stack->count * sizeof(*stack->jitsons)) ?: stack->jitsons;

    stack->jitsons = NULL;
    stack->count   = 0;
    ret->type     |= SXE_JITSON_TYPE_ALLOCED;    // The token at the base of the stack is marked as allocated

OUT:
    SXER6("return %p; // type=%s", ret, ret ? sxe_jitson_type_to_str(sxe_jitson_get_type(ret)) : "NONE");
    return ret;
}

/**
 * Clear the content of a parse stack
 */
void
sxe_jitson_stack_clear(struct sxe_jitson_stack *stack)
{
    stack->count = stack->borrow;
    stack->open  = 0;
}

/**
 * Temporarily borrow a stack
 *
 * @param stack The stack to borrow
 * @param iou   A stack object used to store an IOU for the stack
 */
void
sxe_jitson_stack_borrow(struct sxe_jitson_stack *stack, struct sxe_jitson_stack *iou)
{
    memcpy(iou, stack, sizeof(*iou));
    stack->open   = 0;
    stack->last   = 0;
    stack->borrow = stack->count;
}

/**
 * Return a temporarily borrowed stack
 *
 * @param stack The stack that was borrowed
 * @param iou   A stack object used to store an IOU for the stack
 */
void
sxe_jitson_stack_return(struct sxe_jitson_stack *stack, const struct sxe_jitson_stack *iou)
{
    SXEA1(stack->borrow == iou->count, "The size of the stack differs from its size when borrowed");
    stack->open   = iou->open;
    stack->last   = iou->last;
    stack->borrow = iou->borrow;
    stack->count  = iou->count;
}

/**
 * Return a per thread stack, constructing it on first call.
 *
 * @note The stack can be freed after the thread exits by calling sxe_thread_memory_free
 */
struct sxe_jitson_stack *
sxe_jitson_stack_get_thread(void)
{
    /* Allocate the per thread stack; once allocated, we can't free it because its being tracked
     */
    if (!jitson_stack) {
        jitson_stack = sxe_thread_malloc(sizeof(*jitson_stack), (void (*)(void *))sxe_jitson_stack_free, NULL);

        if (!sxe_jitson_stack_make(jitson_stack, jitson_stack_init_size)) {
            SXEL2(": failed to create a sxe-jitson per thread stack");
            return NULL;
        }
    }

    SXEL6(": return %p; // count=%u, open=%u", jitson_stack, jitson_stack ? jitson_stack->count : 0,
          jitson_stack ? jitson_stack->open : 0);
    return jitson_stack;
}

void
sxe_jitson_stack_free(struct sxe_jitson_stack *stack)
{
    kit_free(stack->jitsons);
    kit_free(stack);
}

/* Reserve space on stack, expanding it if needed to make room for at least 'more' new values
 *
 * @return The index of the first new slot on the stack, or SXE_JITSON_STACK_ERROR on error (ENOMEM)
 */
unsigned
sxe_jitson_stack_expand(struct sxe_jitson_stack *stack, unsigned more)
{
    unsigned expanded = stack->count + more;

    if (expanded > stack->maximum) {
        unsigned new_maximum;

        if (expanded < JITSON_STACK_MAX_INCR)
            new_maximum = ((expanded - 1) / stack->maximum + 1) * stack->maximum;
        else
            new_maximum = ((expanded - 1) / JITSON_STACK_MAX_INCR + 1) * JITSON_STACK_MAX_INCR;

        struct sxe_jitson *new_jitsons = MOCKERROR(MOCK_FAIL_STACK_EXPAND, NULL, ENOMEM,
                                                  kit_realloc(stack->jitsons, (size_t)new_maximum * sizeof(*stack->jitsons)));

        if (!new_jitsons) {
            SXEL2(": Failed to expand the stack to %u jitsons from %u", new_maximum, stack->jitsons ? stack->maximum : 0);
            return SXE_JITSON_STACK_ERROR;
        }

        stack->maximum = new_maximum;
        stack->jitsons = new_jitsons;    // If the array moved, point current into the new one.
    }
    else if (!stack->jitsons && !(stack->jitsons = MOCKERROR(MOCK_FAIL_STACK_EXPAND_AFTER_GET, NULL, ENOMEM,
                                                            kit_malloc(((size_t)stack->maximum * sizeof(*stack->jitsons)))))) {
        SXEL2(": Failed to allocate %u jitsons for the stack", stack->maximum);
        return SXE_JITSON_STACK_ERROR;
    }

    stack->count = expanded;
    return expanded - more;
}

/**
 * Load a JSON string from a source, returning true on success, false on error
 * See https://www.json.org/json-en.html
 */
bool
sxe_jitson_stack_load_string(struct sxe_jitson_stack *stack, struct sxe_jitson_source *source)
{
    struct sxe_jitson *jitson;
    unsigned           i, idx, space, unicode;
    char               c, utf8[4];

    if (sxe_jitson_source_get_nonspace(source) != '"') {
        errno = EINVAL;
        return false;
    }

    if ((idx = sxe_jitson_stack_expand(stack, 1)) == SXE_JITSON_STACK_ERROR)
        return false;

    jitson       = &stack->jitsons[idx];
    jitson->type = SXE_JITSON_TYPE_STRING;
    jitson->len  = 0;
    space        = sizeof(jitson->string);    // Amount of space left in the current jitson

    while ((c = sxe_jitson_source_get_char(source)) != '"') {
        if (c == '\0') {   // No terminating "
            errno = EINVAL;
            goto ERROR;
        }

        i = 1;    // If its not UTF-8, it takes 1 byte

        if (c == '\\')
            switch (c = sxe_jitson_source_get_char(source)) {
            case '"':
            case '\\':
            case '/':
                jitson->string[jitson->len++] = c;
                break;

            case 'b':
                jitson->string[jitson->len++] = '\b';
                break;

            case 'f':
                jitson->string[jitson->len++] = '\f';
                break;

            case 'n':
                jitson->string[jitson->len++] = '\n';
                break;

            case 'r':
                jitson->string[jitson->len++] = '\r';
                break;

            case 't':
                jitson->string[jitson->len++] = '\t';
                break;

            case 'u':
                for (i = 0, unicode = 0; i < 4; i++)
                    switch (c = sxe_jitson_source_get_char(source)) {
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        unicode = (unicode << 4) + c - '0';
                        break;

                    case 'a':
                    case 'b':
                    case 'c':
                    case 'd':
                    case 'e':
                    case 'f':
                        unicode = (unicode << 4) + c - 'a' + 10;
                        break;

                    case 'A':
                    case 'B':
                    case 'C':
                    case 'D':
                    case 'E':
                    case 'F':
                        unicode = (unicode << 4) + c - 'A' + 10;
                        break;

                    default:
                        errno = EILSEQ;
                        goto ERROR;
                    }

                i = sxe_unicode_to_utf8(unicode, utf8);    // Return is number of bytes in the utf8 string
                jitson->string[jitson->len++] = utf8[0];
                break;

            default:
                errno = EILSEQ;
                goto ERROR;
            }
        else
            jitson->string[jitson->len++] = c;

        if (--space < i) {    // Used up a byte. If there's not enough space left including an extra byte for the trailing '\0'
            if (sxe_jitson_stack_expand(stack, 1) == SXE_JITSON_STACK_ERROR)
                goto ERROR;

            jitson = &stack->jitsons[idx];    // In case the jitsons were moved by realloc
            space += sizeof(*jitson);         // Got 16 more bytes of string space
        }

        if (--i > 0) {    // For UTF-8 strings > 1 character, copy the rest of the string
            memcpy(&jitson->string[jitson->len], &utf8[1], i);
            jitson->len += i;
            space       -= i;
        }
    }

    jitson->string[jitson->len] = '\0';
    return true;

ERROR:
    stack->count = idx;    // Discard any data that this function added to the stack
    return false;
}

/**
 * Duplicate a jitson value onto the stack
 *
 * @param stack to duplicate to
 * @param idx   the stack index where the duplicate is to be made
 * @param value the jitson value to duplicate
 * @param size  the size (in jitsons) of the value or 0 to compute it
 *
 * @return false on out of memory expanding stack or if deep copying the value fails (usually implying out of memory)
 */
bool
sxe_jitson_stack_dup_at_index(struct sxe_jitson_stack *stack, unsigned idx, const struct sxe_jitson *value, unsigned size)
{
    size = size ?: sxe_jitson_size(value);

    if (idx + size > stack->count && sxe_jitson_stack_expand(stack, idx + size - stack->count) == SXE_JITSON_STACK_ERROR)
        return false;

    memcpy(&stack->jitsons[idx], value, size * sizeof(*value));
    bool ret = sxe_jitson_clone(value, &stack->jitsons[idx]);      // If the type requires a deep clone, do it
    stack->jitsons[idx].type &= ~SXE_JITSON_TYPE_ALLOCED;          // Clear the allocation flag if any
    return ret;
}

/* Character classes used by the parser
 */
#define INV 0    // Invalid (control character, non-ASCII character, etc.)
#define SYM 1    // A standalone symbol not used by JSON
#define QOT 2    // A quote-like character
#define DIG 4    // A decimal digit
#define ALP 5    // An alphabetic character, including _ and upper and lower case letters

/* Map from char to character class. Characters special to JSON are often a class of their own.
 */
static char jitson_class[256] = { INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, SYM, '"', SYM, SYM, SYM, SYM, QOT, '(', ')', SYM, SYM, SYM, '-', SYM, SYM,
                                  DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, DIG, SYM, SYM, SYM, SYM, SYM, SYM,
                                  SYM, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,
                                  ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, '[', SYM, ']', SYM, ALP,
                                  QOT, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP,
                                  ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, ALP, '{', SYM, '}', SYM, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
                                  INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV };

/**
 * Load a JSON onto a sxe-jitson stack.
 * See https://www.json.org/json-en.html
 *
 * @param stack  The stack to load onto
 * @param source A sxe_jitson_source object
 *
 * @return true if the JSON was successfully parsed or false on error
 *
 * @note On error, any jitson values partially parsed onto the stack will be cleared.
 */
bool
sxe_jitson_stack_load_json(struct sxe_jitson_stack *stack, struct sxe_jitson_source *source)
{
    const struct sxe_jitson *jitson, *cast_arg = NULL;
    const char              *token;
    char                    *endptr;
    size_t                   len;
    unsigned                 current, idx, previous;
    bool                     is_uint;
    char                     c;

    if ((c = sxe_jitson_source_peek_nonspace(source)) == '\0') {    // Nothing but whitespace
        errno = ENODATA;
        return false;
    }

    if ((idx = sxe_jitson_stack_expand(stack, 1)) == SXE_JITSON_STACK_ERROR)    // Get an empty jitson
        return false;

    stack->last = idx;    // Keep track of the last value loaded on the stack

    switch (jitson_class[(unsigned char)c]) {
    case '"':              // It's a string
        stack->count--;    // Return the jitson just allocated. The load_string function will get it back.
        return sxe_jitson_stack_load_string(stack, source);

    case '{':    // It's an object
        sxe_jitson_source_consume(source, 1);
        stack->jitsons[idx].type = SXE_JITSON_TYPE_OBJECT;
        stack->jitsons[idx].len  = 0;

        if ((c = sxe_jitson_source_peek_nonspace(source)) == '}') {    // If it's an empty object, return it
            sxe_jitson_source_consume(source, 1);
            stack->jitsons[idx].integer = 1;                           // Save the size in jitsons
            return true;
        }

        do {
            if (c != '"')
                goto INVALID;

            previous = stack->count;

            if (!sxe_jitson_stack_load_string(stack, source))    // Member name must be a string
                goto ERROR;

            if (sxe_jitson_source_get_nonspace(source) != ':')
                goto INVALID;

            if (!sxe_jitson_stack_load_json(stack, source))    // Value can be any JSON value
                goto ERROR;

            stack->jitsons[idx].len++;
            stack->jitsons[previous].type |= SXE_JITSON_TYPE_IS_KEY;
            c = sxe_jitson_source_get_nonspace(source);
        } while (c == ',' && (c = sxe_jitson_source_peek_nonspace(source)));

        if (c == '}') {
            stack->jitsons[idx].integer = stack->count - idx;    // Store the size = offset past the object
            return true;
        }

        goto INVALID;

    case '[':    // It's an array
        sxe_jitson_source_consume(source, 1);
        stack->jitsons[idx].type  = SXE_JITSON_TYPE_ARRAY;
        stack->jitsons[idx].len   = 0;

        if (source->flags & SXE_JITSON_FLAG_OPTIMIZE)    // If optimization is turned on, detect the following properties
            stack->jitsons[idx].type |= SXE_JITSON_TYPE_IS_ORD | SXE_JITSON_TYPE_IS_UNIF | SXE_JITSON_TYPE_IS_HOMO;

        if (sxe_jitson_source_peek_nonspace(source) == ']') {   // If it's an empty array, return it
            sxe_jitson_source_consume(source, 1);
            stack->jitsons[idx].type   &= ~SXE_JITSON_TYPE_IS_ORD;    // Empty arrays are not considered ordered
            stack->jitsons[idx].integer = 1;                          // Offset past the empty array (not used if optimized)
            return true;
        }

        do {
            previous = stack->last;
            current  = stack->count;    // Index of JSON value about to be loaded

            if (!sxe_jitson_stack_load_json(stack, source))    // Value can be any JSON value
                goto ERROR;

            /* If optimization is enabled and there's at least one element already in the array.
             */
            if ((source->flags & SXE_JITSON_FLAG_OPTIMIZE) && stack->jitsons[idx].len > 0) {
                /* If the array is currently ordered and the previous element is greater than the current one or they can't be
                 * compared, clear the ordered flag.
                 */
                if ((stack->jitsons[idx].type & SXE_JITSON_TYPE_IS_ORD)
                 && sxe_jitson_cmp(&stack->jitsons[previous], &stack->jitsons[current]) > 0)
                    stack->jitsons[idx].type &= ~SXE_JITSON_TYPE_IS_ORD;

                /* If the array is currently uniform and the previous element's size differs from the current one, clear the
                 * uniform flag.
                 */
                if ((stack->jitsons[idx].type & SXE_JITSON_TYPE_IS_UNIF) && (current - previous) != (stack->count - current))
                    stack->jitsons[idx].type &= ~SXE_JITSON_TYPE_IS_UNIF;

                /* If the array is currently homogenous and the previous element's type differs from the current one, clear the
                 * homogenous flag.
                 */
                if ((stack->jitsons[idx].type & SXE_JITSON_TYPE_IS_HOMO)
                 && stack->jitsons[previous].type != stack->jitsons[current].type)
                    stack->jitsons[idx].type &= ~SXE_JITSON_TYPE_IS_HOMO;
            }

            stack->jitsons[idx].len++;
        } while ((c = sxe_jitson_source_get_nonspace(source)) == ',');

        if (c != ']')
            goto INVALID;

        if (stack->jitsons[idx].len <= 1)    // Arrays with  a single element are not considered ordered
            stack->jitsons[idx].type &= ~SXE_JITSON_TYPE_IS_ORD;

        if (stack->jitsons[idx].len && (stack->jitsons[idx].type & SXE_JITSON_TYPE_IS_UNIF)) {
            stack->jitsons[idx].uniform.size = sizeof(struct sxe_jitson) * (stack->count - (idx + 1)) / stack->jitsons[idx].len;
            stack->jitsons[idx].uniform.type = stack->jitsons[idx].type & SXE_JITSON_TYPE_IS_HOMO
                                               ? stack->jitsons[stack->open].type : SXE_JITSON_TYPE_INVALID;    // Mixed list
        }
        else
            stack->jitsons[idx].integer = stack->count - idx;    // Store the offset past the object

        return true;

    case '-':
    case DIG:
        if ((token = sxe_jitson_source_get_number(source, &len, &is_uint)) == NULL)
            goto ERROR;

        if (is_uint) {
            stack->jitsons[idx].type    = SXE_JITSON_TYPE_NUMBER | SXE_JITSON_TYPE_IS_UINT;
            stack->jitsons[idx].integer = strtoul(token, &endptr,
                                                  sxe_jitson_source_get_flags(source) & SXE_JITSON_FLAG_ALLOW_HEX ? 0 : 10);
            SXEA6(endptr - token == (ptrdiff_t)len, "strtoul failed to parse '%.*s'", (int)len, token);
        } else {
            stack->jitsons[idx].type   = SXE_JITSON_TYPE_NUMBER;
            stack->jitsons[idx].number = strtod(token, &endptr);
            SXEA6(endptr - token == (ptrdiff_t)len, "strtod failed to parse '%.*s'", (int)len, token);
        }

        return true;

    case ALP:
        token = sxe_jitson_source_get_identifier(source, &len);

        if ((jitson = sxe_jitson_const_get(sxe_jitson_source_get_flags(source), token, len))) {
            if (jitson->type == sxe_jitson_const_type_cast) {    // If it's a cast
                struct sxe_jitson_stack iou;

                if (sxe_jitson_source_get_nonspace(source) != '(') {
                    SXEL2("Expected '(' after type name %.*s", (int)len, token);
                    goto ERROR;
                }

                stack->count--;    // Return the allocated jitson to the stack
                sxe_jitson_stack_borrow(stack, &iou);

                if (sxe_jitson_stack_load_json(stack, source))
                    cast_arg = sxe_jitson_stack_get_jitson(stack);

                sxe_jitson_stack_return(stack, &iou);

                if (!cast_arg) {
                    SXEL2("Expected JSON value after %.*s", (int)len, token);
                    goto ERROR;
                }

                if (sxe_jitson_source_get_nonspace(source) != ')') {
                    SXEL2("Expected ')' after JSON value that follows type name %.*s", (int)len, token);
                    goto ERROR;
                }

                if (!(*jitson->castfunc)(stack, cast_arg))
                    goto ERROR;

                sxe_jitson_free(cast_arg);
                return true;
            }

            return sxe_jitson_stack_dup_at_index(stack, idx, jitson, 0);    // Duplicate the constant's value
        }

        if (sxe_jitson_stack_push_ident && (sxe_jitson_source_get_flags(source) & SXE_JITSON_FLAG_ALLOW_IDENTS)) {
            stack->count--;    // Return the allocated jitson to the stack

            if (!sxe_jitson_stack_push_ident(stack, token, len))
                goto ERROR;

            return true;
        }

#if SXE_DEBUG
        if (sxe_jitson_source_get_flags(source) & SXE_JITSON_FLAG_ALLOW_CONSTS)
            SXEL6(": Identifier '%.*s' is neither a JSON keyword nor a registered constant", (int)len, token);
        else
            SXEL6(": Identifier '%.*s' is not a JSON keyword", (int)len, token);
#endif

        __FALLTHROUGH;
    default:
        break;
    }

INVALID:
    errno = EINVAL;

ERROR:
    sxe_jitson_free(cast_arg);
    stack->count = idx;    // Discard any data that this function added to the stack
    return false;
}

/**
 * Parse a JSON from a string onto a sxe-jitson stack.
 *
 * @return Pointer into the json string to the character after the JSON parsed or NULL on error
 *
 * @note On error, any jitson values partially parsed onto the stack will be cleared.
 */
const char *
sxe_jitson_stack_parse_json(struct sxe_jitson_stack *stack, const char *json)
{
    struct sxe_jitson_source source;

    sxe_jitson_source_from_string(&source, json, sxe_jitson_flags);

    if (!sxe_jitson_stack_load_json(stack, &source))
        return NULL;

    return json + sxe_jitson_source_get_consumed(&source);
}

/**
 * Add or prepare to add a value to a collection
 *
 * @param stack     The stack on which the collection is being constructed
 * @param size      Size in jitsons of the element, or 0 if the value will be added by the caller and its size is unknown
 * @param elem_type The type of the element being added
 * @param element   Pointer to the element to add (required for _TYPE_MK_SORT) or NULL if the caller will add the value
 *
 * @return The index of the new element or SXE_JITSON_STACK_ERROR on error
 *
 * @note This function can be called by types that are constructing internal objects, but use with care.  To facilitate this,
 *       if a size is passed but element is NULL, this function is assumed to be called after adding the element.
 */
unsigned
sxe_jitson_stack_add_value(struct sxe_jitson_stack *stack, unsigned size, uint32_t elem_type, const struct sxe_jitson *element)
{
    SXEA1(stack->open,      "Can't add a value when there is no array or object under construction");
    SXEA6(size || !element, "Can't copy an element if the size is not known");

    unsigned idx, len;
    unsigned collection = stack->open - 1;    // Can't use a pointer in case the stack is moved
    unsigned type       = stack->jitsons[collection].type;
    bool     is_array   = (type & SXE_JITSON_TYPE_MASK) == SXE_JITSON_TYPE_ARRAY;

    SXEA1(is_array || (type & SXE_JITSON_TYPE_MASK) == SXE_JITSON_TYPE_OBJECT, "Values can only be added to arrays or objects");
    SXEA1(is_array || stack->jitsons[collection].partial.no_value, "Must add member name to an object before adding a value");

    if ((len = stack->jitsons[collection].len)) {    // If there's at least one element
        if ((type & SXE_JITSON_TYPE_IS_UNIF)         // If all elements so far are of uniform size
         && size != (stack->count - (element ? 0 : size) - (collection + 1)) / stack->jitsons[collection].len) {    // No longer
            SXEA1(!(type & SXE_JITSON_TYPE_MK_SORT), "Insertion sorted lists must have uniformly sized elements");
            stack->jitsons[collection].type &= ~SXE_JITSON_TYPE_IS_UNIF;
        }

        if ((type & SXE_JITSON_TYPE_IS_HOMO) && (stack->jitsons[collection + 1].type & SXE_JITSON_TYPE_MASK) != elem_type)
            stack->jitsons[collection].type &= ~SXE_JITSON_TYPE_IS_HOMO;    // Type changed
    }

    /* Keep track of the last value just added or about to be added
     */
    if (element) {
        if ((stack->last = sxe_jitson_stack_expand(stack, size)) == SXE_JITSON_STACK_ERROR)
            return SXE_JITSON_STACK_ERROR;
    }
    else
        stack->last = stack->count - size;

    idx = stack->last;

    /* If there's at least one element already in the array
     */
    if (len) {
        if (type & SXE_JITSON_TYPE_MK_SORT) {    // If insertion sorting is desired
            SXEA6(element, "Elements must be provided for insertion sorted lists");

            if (sxe_jitson_cmp(&stack->jitsons[idx - size], element) > 0) {    // Insertion is required
                struct sxe_sortedarray_class elem_class;
                unsigned                     i;
                bool                         match;

                elem_class.size      = size * sizeof(*element);
                elem_class.keyoffset = 0;
                elem_class.cmp       = (int (*)(const void *, const void *))sxe_jitson_cmp;
                elem_class.fmt       = NULL;
                elem_class.flags     = SXE_SORTEDARRAY_CMP_CAN_FAIL;

                i = sxe_sortedarray_find(&elem_class, &stack->jitsons[collection + 1], len, element, &match);
                SXEA6(i < len,  "If insertion is required, array index must be found and < len");
                idx = collection + 1 + size * i;    // Turn the array index into a stack index
                memmove(&stack->jitsons[idx + 1], &stack->jitsons[idx], (len - i) * size * sizeof(*element));
            }
        }
        else if (type & SXE_JITSON_TYPE_IS_ORD) {
            if (element) {
                if (sxe_jitson_cmp(&stack->jitsons[stack->jitsons[collection].partial.last], element) > 0)
                    stack->jitsons[collection].type &= ~SXE_JITSON_TYPE_IS_ORD;
            }
            else if (size) {
                if (sxe_jitson_cmp(&stack->jitsons[stack->jitsons[collection].partial.last], &stack->jitsons[stack->last]) > 0)
                    stack->jitsons[collection].type &= ~SXE_JITSON_TYPE_IS_ORD;
            }
        }
    }

    if (element)
        memcpy(&stack->jitsons[idx], element, size * sizeof(*element));

    if (is_array) {
        if (size)
            stack->jitsons[collection].partial.last = stack->last;
    }
    else
        stack->jitsons[collection].partial.no_value = false;

    stack->jitsons[collection].len++;
    return idx;
}

/**
 * Check optimization after adding a variably sized value (copied string or array) to an array
 *
 * @param stack The stack the array is being constructed on
 * @param flags The flags in effect, needed in case optimization is disabled
 *
 * @return true; always succeeds, but the return allows chained construction
 *
 * @note This function can be called by types that are constructing internal objects, but use with care.
 */
bool
sxe_jitson_stack_value_added(struct sxe_jitson_stack *stack, unsigned flags)
{
    struct sxe_jitson *collection = &stack->jitsons[stack->open - 1];    // Safe to use a pointer because no stack expansion

    /* If optimization is enabled and the collection is an array
     */
    if (flags & SXE_JITSON_FLAG_OPTIMIZE && (collection->type & SXE_JITSON_TYPE_MASK) == SXE_JITSON_TYPE_ARRAY) {
        if (collection->len > 1) {    // If there are at least 2 elements
            /* If the array is currently ordered and the previous element is greater than the current one or they can't be
             * compared, clear the ordered flag.
             */
            if ((collection->type & SXE_JITSON_TYPE_IS_ORD)
             && sxe_jitson_cmp(&stack->jitsons[collection->partial.last], &stack->jitsons[stack->last]) > 0)
                collection->type &= ~SXE_JITSON_TYPE_IS_ORD;
        }

        collection->partial.last = stack->last;
    }

    return true;
}

/**
 * Begin construction of an object on a stack
 *
 * @param type SXE_JITSON_TYPE_OBJECT, SXE_JITSON_TYPE_ARRAY, or SXE_JITSON_TYPE_ARRAY | SXE_JITSON_TYPE_MK_SORT
 *
 * @return true on success, false on allocation failure
 *
 * @note SXE_JITSON_TYPE_ARRAY | SXE_JITSON_TYPE_MK_SORT allows you to construct a sorted array out of order. This makes
 *       construction inefficient, but can speed evalation of membership and intersection operations. Elements must be of a
 *       single orderable type and must be of uniform fixed size, or additions will fail.
 */
bool
sxe_jitson_stack_open_collection(struct sxe_jitson_stack *stack, uint32_t type)
{
    unsigned idx;
    uint32_t type_check = type & ~SXE_JITSON_TYPE_IS_LOCAL;

    SXE_USED_IN_DEBUG(type_check);

    SXEA6(type_check == SXE_JITSON_TYPE_ARRAY || type_check == SXE_JITSON_TYPE_OBJECT
       || type_check == (SXE_JITSON_TYPE_ARRAY | SXE_JITSON_TYPE_MK_SORT), "Only arrays and objects can be constructed");

    if (stack->open)    // If there's already an open collection, prepare to add the new collection as a value
        sxe_jitson_stack_add_value(stack, 0, type, NULL);

    if ((idx = sxe_jitson_stack_expand(stack, 1)) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    if ((type & SXE_JITSON_TYPE_MASK) == SXE_JITSON_TYPE_ARRAY   // If it's an array and either optimized or sorted
     && ((sxe_jitson_flags & SXE_JITSON_FLAG_OPTIMIZE) || (type & SXE_JITSON_TYPE_MK_SORT)))
        stack->jitsons[idx].type = type | SXE_JITSON_TYPE_IS_ORD | SXE_JITSON_TYPE_IS_UNIF | SXE_JITSON_TYPE_IS_HOMO;
    else
        stack->jitsons[idx].type = type;

    stack->jitsons[idx].len                = 0;
    stack->jitsons[idx].partial.no_value   = false;          // If it's an array, sets last to 0, which is harmless
    stack->jitsons[idx].partial.collection = stack->open;
    stack->open                            = idx + 1;
    return true;
}

/**
 * Add a string to the stack.
 *
 * @param stack  The jitson stack
 * @param string The string
 * @param type   SXE_JITSON_TYPE_IS_COPY, SXE_JITSON_TYPE_IS_REF, or SXE_JITSON_TYPE_IS_OWN
 *
 * @return true on success, false on out of memory (ENOMEM) or copied string too long (ENAMETOOLONG)
 *
 * @note When called internally, type may include SXE_JITSON_TYPE_IS_KEY
 */
bool
sxe_jitson_stack_push_string(struct sxe_jitson_stack *stack, const char *string, uint32_t type)
{
    unsigned idx, unwind;

    if ((unwind = sxe_jitson_stack_expand(stack, 1)) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    type                        = type & SXE_JITSON_TYPE_IS_OWN ? SXE_JITSON_TYPE_IS_REF | type : type;
    stack->jitsons[unwind].type = SXE_JITSON_TYPE_STRING | type;

    if (type & SXE_JITSON_TYPE_IS_REF) {    // Not a copy (a reference, possibly giving ownership to the object)
        stack->jitsons[unwind].reference = string;
        stack->jitsons[unwind].len       = 0;
        return true;
    }

    size_t len = strlen(string);    // SonarQube False Positive

    if ((uint32_t)len != len) {
        errno        = ENAMETOOLONG;    /* COVERAGE EXCLUSION: Copied string > 4294967295 characters */
        stack->count = unwind;          /* COVERAGE EXCLUSION: Copied string > 4294967295 characters */
        return false;                   /* COVERAGE EXCLUSION: Copied string > 4294967295 characters */
    }

    stack->jitsons[unwind].len = (uint32_t)len;

    if (len < SXE_JITSON_STRING_SIZE) {
        memcpy(stack->jitsons[unwind].string, string, len + 1);
        return true;
    }

    memcpy(stack->jitsons[unwind].string, string, SXE_JITSON_STRING_SIZE);

    for (string += SXE_JITSON_STRING_SIZE, len -= SXE_JITSON_STRING_SIZE; ; len -= SXE_JITSON_TOKEN_SIZE) {
        if ((idx = sxe_jitson_stack_expand(stack, 1)) == SXE_JITSON_STACK_ERROR) {
            stack->count = unwind;
            return false;
        }

        if (len < SXE_JITSON_TOKEN_SIZE) {
            memcpy(&stack->jitsons[idx], string, len + 1);
            return true;
        }

        memcpy(&stack->jitsons[idx], string, SXE_JITSON_TOKEN_SIZE);
        string += SXE_JITSON_TOKEN_SIZE;
    }

    return true;
}

/**
 * Add a string to the stack in reverse order
 *
 * @param stack  The jitson stack
 * @param string The string
 * @param len    The length of the string or 0 to compute it on the fly
 *
 * @return true on success, false on out of memory (ENOMEM) or string too long (ENAMETOOLONG)
 */
bool
sxe_jitson_stack_push_string_reversed(struct sxe_jitson_stack *stack, const char *string, size_t len)
{
    unsigned idx, i;

    len = len ?: strlen(string);

    if ((uint32_t)len != len) {
        errno = ENAMETOOLONG;    /* COVERAGE EXCLUSION: Copied string > 4294967295 characters */
        return false;            /* COVERAGE EXCLUSION: Copied string > 4294967295 characters */
    }

    /* Length to number of jitson tokens needed: 0-7 -> 1, 8-23 -> 2, 24-39 -> 3, ...
     */
    idx = sxe_jitson_stack_expand(stack, (len + 2 * SXE_JITSON_TOKEN_SIZE - SXE_JITSON_STRING_SIZE) / SXE_JITSON_TOKEN_SIZE);    // SonarQube False Positive

    if (idx == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    stack->jitsons[idx].type = SXE_JITSON_TYPE_STRING | SXE_JITSON_TYPE_IS_COPY | SXE_JITSON_TYPE_REVERSED;
    stack->jitsons[idx].len  = len;    // SonarQube False Positive

    for (i = 0; i < len; i++)
        stack->jitsons[idx].string[len - i - 1] = string[i];

    stack->jitsons[idx].string[len] = '\0';
    return true;
}

/**
 * Add a member name to the object being constructed on the stack
 *
 * @param stack The jitson stack
 * @param name  The member name
 * @param type  SXE_JITSON_TYPE_IS_COPY, SXE_JITSON_TYPE_IS_REF, or SXE_JITSON_TYPE_IS_OWN
 *
 * @return true on success, false on out of memory (ENOMEM) or copied string too long (ENAMETOOLONG)
 */
bool
sxe_jitson_stack_add_member_name(struct sxe_jitson_stack *stack, const char *name, uint32_t type)
{
    unsigned object;

    SXEA1(stack->open,                                    "Can't add a member name when there is no object under construction");
    SXEA1((stack->jitsons[object = stack->open - 1].type & ~SXE_JITSON_TYPE_IS_LOCAL) == SXE_JITSON_TYPE_OBJECT, "Member names can only be added to objects");
    SXEA1(!stack->jitsons[object].partial.no_value,                                "Member name already added without a value");
    SXEA1(!(type & ~(SXE_JITSON_TYPE_IS_REF | SXE_JITSON_TYPE_IS_OWN | SXE_JITSON_TYPE_IS_LOCAL)),            "Unexected type flags 0x%x", (unsigned)type);

    stack->jitsons[object].partial.no_value = 1;    // (uint8_t)true
    return sxe_jitson_stack_push_string(stack, name, type | SXE_JITSON_TYPE_IS_KEY);
}

/**
 * Add a string to the object or array being constructed on the stack
 *
 * @param stack  The jitson stack
 * @param string The NUL terminated string
 * @param type   SXE_JITSON_TYPE_IS_COPY, SXE_JITSON_TYPE_IS_REF, or SXE_JITSON_TYPE_IS_OWN
 *
 * @return true on success, false on out of memory (ENOMEM) or copied string too long (ENAMETOOLONG)
 */
bool
sxe_jitson_stack_add_string(struct sxe_jitson_stack *stack, const char *string, uint32_t type)
{
    SXEA1(stack->open, "Can't add a value when there is no array or object under construction");
    SXEA1(!(type & ~(SXE_JITSON_TYPE_IS_REF | SXE_JITSON_TYPE_IS_OWN)), "Unexected type flags 0x%x", (unsigned)type);
    type = type == SXE_JITSON_TYPE_IS_OWN ? SXE_JITSON_TYPE_IS_OWN | SXE_JITSON_TYPE_IS_REF : type;

    if (type & SXE_JITSON_TYPE_IS_REF) {    // String references are fixed size, allowing sorted array construction
        struct sxe_jitson string_ref;

        sxe_jitson_make_string_ref(&string_ref, string);
        string_ref.type |= type & SXE_JITSON_TYPE_IS_OWN;
        return sxe_jitson_stack_add_value(stack, 1, SXE_JITSON_TYPE_STRING, &string_ref) != SXE_JITSON_STACK_ERROR;
    }

    if (sxe_jitson_stack_add_value(stack, 0, SXE_JITSON_TYPE_STRING, NULL) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory */

    stack->last = stack->count;    // Keep track of the last value loaded on the stack

    if (!sxe_jitson_stack_push_string(stack, string, type)) {
        stack->jitsons[stack->open - 1].len--;    // On error, remove the partial element added above in _add_value
        return false;
    }

    sxe_jitson_stack_value_added(stack, sxe_jitson_flags);
    return true;
}

/**
 * Add a null value to the array or object being constructed on the stack
 *
 * @param stack The jitson stack
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_null(struct sxe_jitson_stack *stack)
{
    return sxe_jitson_stack_add_value(stack, 1, SXE_JITSON_TYPE_NULL, sxe_jitson_null) != SXE_JITSON_STACK_ERROR;
}

/**
 * Add a boolean value to the array or object being constructed on the stack
 *
 * @param stack   The jitson stack
 * @param boolean The boolean value
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_bool(struct sxe_jitson_stack *stack, bool boolean)
{
    return sxe_jitson_stack_add_value(stack, 1, SXE_JITSON_TYPE_BOOL, boolean ? sxe_jitson_true : sxe_jitson_false)
        != SXE_JITSON_STACK_ERROR;
}

/**
 * Add a number to the array or object being constructed on the stack
 *
 * @param stack  The jitson stack
 * @param number The numeric value
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_number(struct sxe_jitson_stack *stack, double number)
{
    struct sxe_jitson value;

    sxe_jitson_make_number(&value, number);

    return sxe_jitson_stack_add_value(stack, 1, SXE_JITSON_TYPE_NUMBER, &value) != SXE_JITSON_STACK_ERROR;
}

/**
 * Add an unsigned integer to the array or object being constructed on the stack
 *
 * @param stack The jitson stack
 * @param uint  The unsigned integer value
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_uint(struct sxe_jitson_stack *stack, uint64_t uint)
{
    struct sxe_jitson value;

    sxe_jitson_make_uint(&value, uint);

    return sxe_jitson_stack_add_value(stack, 1, SXE_JITSON_TYPE_NUMBER, &value) != SXE_JITSON_STACK_ERROR;
}

/**
 * Add a reference value to the array or object being constructed on the stack
 *
 * @param stack The jitson stack
 * @param to    The jitson to add a reference to
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_reference(struct sxe_jitson_stack *stack, const struct sxe_jitson *to)
{
    struct sxe_jitson value;

    sxe_jitson_make_reference(&value, to);

    return sxe_jitson_stack_add_value(stack, 1, to->type, &value) != SXE_JITSON_STACK_ERROR;
}

/**
 * Add a duplicate of a value to the array or object being constructed on the stack
 *
 * @param stack The jitson stack
 * @param to    The jitson to add a duplicate of
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_dup(struct sxe_jitson_stack *stack, const struct sxe_jitson *value)
{
    unsigned size = sxe_jitson_size(value);

    if (!sxe_jitson_stack_dup_at_index(stack, stack->count, value, size))
        return false;

    SXEA1(sxe_jitson_stack_add_value(stack, size, value->type, NULL) != SXE_JITSON_STACK_ERROR, "Can't fail in this case");
    sxe_jitson_stack_value_added(stack, sxe_jitson_flags);
    return true;
}

/**
 * Add duplicates of all members of an object to the object being constructed on the stack
 *
 * @param stack  The jitson stack
 * @param jitson The object whose members are to be duplicated
 *
 * @return true on success, false if memory allocation failed
 */
bool
sxe_jitson_stack_add_dup_members(struct sxe_jitson_stack *stack, const struct sxe_jitson *jitson)
{
    unsigned len, object = stack->open - 1;
    uint32_t idx, size;

    SXEA1(stack->open,                                           "Can't add members when no object is under construction");
    SXEA1((stack->jitsons[object].type & ~SXE_JITSON_TYPE_IS_LOCAL) == SXE_JITSON_TYPE_OBJECT, "Members can only be added to an object");
    SXEA1(!stack->jitsons[object].partial.no_value,              "Member name already added without a value");

    jitson = sxe_jitson_is_reference(jitson) ? jitson->jitref : jitson;
    SXEA1((jitson->type & SXE_JITSON_TYPE_MASK) == SXE_JITSON_TYPE_OBJECT, "Can't add members from JSON type %s",
                                                                           sxe_jitson_get_type_as_str(jitson));

    if ((len = jitson->len) == 0)
        return true;

    size = sxe_jitson_size(jitson) - 1;    // Don't include the object itself

    if ((idx = sxe_jitson_stack_expand(stack, size)) == SXE_JITSON_STACK_ERROR)
        return false;

    memcpy(&stack->jitsons[idx], jitson + 1, size * sizeof(*jitson));

    if (!sxe_jitson_object_clone_members(jitson, &stack->jitsons[idx - 1], len))
        return false;

    stack->jitsons[object].len += len;
    return true;
}

/**
 * Finish construction of an object or array on a stack
 *
 * @note Aborts if the object is not a collection under construction, has an incomplete nested object, or is an object and has a
 *       member name without a matching value.
 *
 * @return true. This is returned to allow further calls (e.g. sxe_jitson_stack_get_jitson) to be chained with &&.
 */
bool
sxe_jitson_stack_close_collection(struct sxe_jitson_stack *stack)
{
    SXEA1(stack->open, "There must be an open collection on the stack");

    unsigned           idx        = stack->open - 1;
    struct sxe_jitson *collection = &stack->jitsons[idx];    // Safe to use a pointer because no stack expansion is done

    SXEA1(collection->type != SXE_JITSON_TYPE_OBJECT || !collection->partial.no_value,
          "Index %u is an object with a member name with no value", idx);
    SXEA1(collection->partial.collection < stack->open, "Previous collection is not < the one being closed");

    stack->open = collection->partial.collection;

    if (collection->len && (collection->type & (SXE_JITSON_TYPE_MASK | SXE_JITSON_TYPE_IS_UNIF))
                        == (SXE_JITSON_TYPE_ARRAY | SXE_JITSON_TYPE_IS_UNIF)) {
        collection->uniform.size = sizeof(struct sxe_jitson) * (stack->count - (idx + 1)) / collection->len;
        collection->uniform.type = collection->type & SXE_JITSON_TYPE_IS_HOMO ? (SXE_JITSON_TYPE_MASK & collection[1].type)
                                                                              : SXE_JITSON_TYPE_INVALID;    // Mixed list
    }
    else
        collection->integer = stack->count - idx;    // Store the offset past the object or array

    if (stack->open)
        sxe_jitson_stack_value_added(stack, sxe_jitson_flags);

    return true;
}

/**
 * Push a concatenation of two arrays to the stack.
 *
 * @param stack  The jitson stack
 * @param array1 The first array
 * @param array2 The second array
 * @param type   SXE_JITSON_TYPE_REF if contatination refers to two arrays, SXE_JITSON_TYPE_OWN if it owns their storage
 *
 * @return true on success, false on out of memory (ENOMEM) or if concatenation's length exceeds 4294967295 (EOVERFLOW)
 */
bool
sxe_jitson_stack_push_concat_array(struct sxe_jitson_stack *stack, const struct sxe_jitson *array1,
                                   const struct sxe_jitson *array2, uint32_t type)
{
    size_t   len;
    unsigned idx;

    SXEA6(sxe_jitson_get_type(array1) == SXE_JITSON_TYPE_ARRAY && sxe_jitson_get_type(array2) == SXE_JITSON_TYPE_ARRAY,
          "Array arguments must be arrays, not %s and %s", sxe_jitson_get_type_as_str(array1), sxe_jitson_get_type_as_str(array2));
    SXEA6(type == SXE_JITSON_TYPE_IS_REF || type == SXE_JITSON_TYPE_IS_OWN,
          "Type must be SXE_JITSON_TYPE_IS_REF or SXE_JITSON_TYPE_IS_OWN, not %"PRIu32, type);

    len = sxe_jitson_len_array(array1) + sxe_jitson_len_array(array2);

    if (len != (uint32_t)len) {
        SXEL2(": concatenated array len %zu exceeds %u", len, ~0U);
        errno = EOVERFLOW;
        return false;
    }

    if ((idx = sxe_jitson_stack_expand(stack, 2)) == SXE_JITSON_STACK_ERROR)
        return false;    /* COVERAGE EXCLUSION: Out of memory condition */

    stack->jitsons[idx].type            = SXE_JITSON_TYPE_ARRAY | SXE_JITSON_TYPE_IS_REF | type;
    stack->jitsons[idx].len             = len;       // SonarQube False Positive
    (&stack->jitsons[idx].reference)[0] = array1;
    (&stack->jitsons[idx].reference)[1] = array2;    // SonarQube False Positive
    return true;
}

#if SXE_DEBUG

/* Recursively format a possibly partial collection on the stack using a string factory
 */
static unsigned
sxe_jitson_stack_to_str_iterator(const struct sxe_jitson_stack *stack, unsigned i, struct sxe_factory *factory)
{
    char    *text;
    size_t   len;
    unsigned j;
    bool     is_object = false;

    if (sxe_jitson_get_type(&stack->jitsons[i]) == SXE_JITSON_TYPE_ARRAY)
        sxe_factory_add(factory, "[", 1);
    else if (sxe_jitson_get_type(&stack->jitsons[i]) == SXE_JITSON_TYPE_OBJECT) {
        sxe_factory_add(factory, "{", 1);
        is_object = true;
    }
    else {
        text = sxe_jitson_to_json(&stack->jitsons[i], &len);
        sxe_factory_add(factory, text, len);
        kit_free(text);
        return i + sxe_jitson_size(&stack->jitsons[i]);
    }

    unsigned count = stack->jitsons[i].len;

    for (i++, j = 0; j < count; j++) {    // For each element or member of the collection
        if (j > 0)
            sxe_factory_add(factory, ",", 1);

        if (i >= stack->count)
            return i;

        if (is_object) {
            SXEA1(sxe_jitson_get_type(&stack->jitsons[i]) == SXE_JITSON_TYPE_STRING
               && (stack->jitsons[i].type & SXE_JITSON_TYPE_IS_KEY), "Object members must be preceded by a key");
            i = sxe_jitson_stack_to_str_iterator(stack, i, factory);
            sxe_factory_add(factory, ":", 1);

            if (i >= stack->count)
                return i;
        }

        if ((i = sxe_jitson_stack_to_str_iterator(stack, i, factory)) >= stack->count)
            return i;
    }

    if (is_object)
        sxe_factory_add(factory, "}", 1);
    else
        sxe_factory_add(factory, "]", 1);

    return i;
}

/* Diagnostic function that dumps a stack as a string in a static per thread buffer which is overwritten on the next call
 */
const char *
sxe_jitson_stack_to_str(const struct sxe_jitson_stack *stack)
{
    static __thread char buf[4096];
    struct sxe_factory   factory;
    char                *text;
    size_t               len;

    sxe_factory_alloc_make(&factory, 0, 0);
    sxe_jitson_stack_to_str_iterator(stack, 0, &factory);
    text = sxe_factory_remove(&factory, &len);

    if (len < sizeof(buf)) {
        memcpy(buf, text, len);
        buf[len] = '\0';
    }
    else {
        memcpy(buf, text, sizeof(buf) - 4);
        memcpy(&buf[sizeof(buf) - 4], "...", 4);
    }

    kit_free(text);
    return buf;
}

#endif
