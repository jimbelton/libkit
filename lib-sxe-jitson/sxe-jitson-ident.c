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

#include <stdio.h>
#include <string.h>

#include "sxe-jitson-ident.h"

uint32_t SXE_JITSON_TYPE_IDENT = SXE_JITSON_TYPE_INVALID;    // Need to register the type to get a valid value

static const struct sxe_jitson * (*sxe_jitson_ident_lookup)(const char *name, size_t len) = NULL;    // name -> value function

/* Hook the identifier lookup function, returning the previous function or NULL if no previous function was set
 */
sxe_jitson_ident_lookup_t
sxe_jitson_ident_lookup_hook(const struct sxe_jitson *(*my_ident_lookup)(const char *, size_t))
{
    sxe_jitson_ident_lookup_t prev = sxe_jitson_ident_lookup;

    sxe_jitson_ident_lookup = my_ident_lookup;
    return prev;
}

/**
 * Called back from sxe-jitson-stack when an unrecognized identifier has been found
 */
bool
sxe_jitson_ident_push_stack(struct sxe_jitson_stack *stack, const char *ident, size_t len)
{
    unsigned idx;

    SXEA6((unsigned)((SXE_JITSON_STRING_SIZE + len) / SXE_JITSON_TOKEN_SIZE)
       == (SXE_JITSON_STRING_SIZE + len) / SXE_JITSON_TOKEN_SIZE, "String length overflows the maximum jitson count");

    /* Reserve space for the fixed header, the identifier and the trailing '\0', rounded up to the nearest jitson
     */
    if ((idx = sxe_jitson_stack_expand(stack, (unsigned)((SXE_JITSON_STRING_SIZE + len) / SXE_JITSON_TOKEN_SIZE) + 1))
     == SXE_JITSON_STACK_ERROR)
        return false;

    stack->jitsons[idx].type = SXE_JITSON_TYPE_IDENT;
    stack->jitsons[idx].len  = (uint32_t)len;
    memcpy(&stack->jitsons[idx].string, ident, len);
    stack->jitsons[idx].string[len] = '\0';
    return true;
}

static const struct sxe_jitson *
sxe_jitson_ident_get_value(const struct sxe_jitson *jitson)
{
    const char *ident;
    size_t      len;

    if (!sxe_jitson_ident_lookup) {
        SXEL2(": No lookup function registered");
        return NULL;
    }

    ident = sxe_jitson_ident_get_name(jitson, &len);
    return sxe_jitson_ident_lookup(ident, len);
}

static int
sxe_jitson_ident_test(const struct sxe_jitson *jitson)
{
    return (jitson = sxe_jitson_ident_get_value(jitson)) ? sxe_jitson_test(jitson) : SXE_JITSON_TEST_ERROR;
}

/* Identifiers are stored like strings: Up to 8 chars in the first jitson, then up to 16 in each subsequent jitson.
 */
static uint32_t
sxe_jitson_ident_size(const struct sxe_jitson *jitson)
{
    return 1 + (jitson->len + SXE_JITSON_STRING_SIZE) / SXE_JITSON_TOKEN_SIZE;
}

//static size_t
//sxe_jitson_ident_len(const struct sxe_jitson *jitson)
//{
//    return types[sxe_jitson_get_type(jitson->jitref)].len(jitson->jitref);
//}

static char *
sxe_jitson_ident_build_json(const struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    char  *buf;
    size_t len = strlen(jitson->string);    // SonarQube False Positive

    if (!(buf = sxe_factory_reserve(factory, len)))
        return NULL;

    memcpy(buf, jitson->string, len);
    sxe_factory_commit(factory, len);
    return sxe_factory_look(factory, NULL);
}

/* Private hook into the jitson parser
 */
extern bool (*sxe_jitson_stack_push_ident)(struct sxe_jitson_stack *stack, const char *ident, size_t len);

/* Call at initialization after sxe_jitson_type_init to register the identifier type
 */
uint32_t
sxe_jitson_ident_register(void)
{
    if (SXE_JITSON_TYPE_IDENT != SXE_JITSON_TYPE_INVALID)    // Already registered
        return SXE_JITSON_TYPE_IDENT;

    SXE_JITSON_TYPE_IDENT = sxe_jitson_type_register("identifier", sxe_jitson_free_base, sxe_jitson_ident_test,
                                                     sxe_jitson_ident_size, NULL, NULL, sxe_jitson_ident_build_json, NULL, NULL);
    sxe_jitson_stack_push_ident = sxe_jitson_ident_push_stack;    // Hook in to the parser
    sxe_jitson_flags           |= SXE_JITSON_FLAG_ALLOW_IDENTS;
    return SXE_JITSON_TYPE_IDENT;
}

const char *
sxe_jitson_ident_get_name(const struct sxe_jitson *ident, size_t *len_out)
{
    if (len_out)
        *len_out = ident->len;

    return ident->string;
}
