/*
 * Copyright (c) 2023 Cisco Systems, Inc. and its affiliates
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

#include "sxe-jitson-const.h"
#include "sxe-dict.h"

uint32_t sxe_jitson_const_type_cast = SXE_JITSON_TYPE_INVALID;    // Used by the parser to detect a cast operator

static const struct sxe_jitson *jitson_constants       = NULL;
static struct sxe_dict         *jitson_builtin_symbols = NULL;
static struct sxe_dict         *jitson_all_symbols     = NULL;
static struct sxe_jitson        json_builtins[3];

/* Internal function called by sxe_jitson_initialize to create the symbol table for JSON builtins
 */
void
sxe_jitson_builtins_initialize_private(void)
{
    const void **value_ptr;

    SXEA1(!jitson_builtin_symbols,                  "Can't initialize jitson builtins twice");
    SXEA1(jitson_builtin_symbols = sxe_dict_new(3), "Failed to allocate symbol table for JSON builtins");

    /* Construct JSON builtin values
     */
    sxe_jitson_make_bool(&json_builtins[0], true);
    sxe_jitson_make_bool(&json_builtins[1], false);
    sxe_jitson_make_null(&json_builtins[2]);

    value_ptr  = sxe_dict_add(jitson_builtin_symbols, "true", sizeof("true") - 1);
    *value_ptr = &json_builtins[0];
    value_ptr  = sxe_dict_add(jitson_builtin_symbols, "false", sizeof("false") - 1);
    *value_ptr = &json_builtins[1];
    value_ptr  = sxe_dict_add(jitson_builtin_symbols, "null", sizeof("null") - 1);
    *value_ptr = &json_builtins[2];

    jitson_all_symbols = jitson_builtin_symbols;    // Until sxe_jitson_const_initialize is called
}

/* Internal function called by sxe_jitson_finalize to free space used for JSON builtins
 */
void
sxe_jitson_builtins_finalize_private(void)
{
    if (jitson_all_symbols == jitson_builtin_symbols)
        jitson_all_symbols = NULL;

    sxe_dict_free(jitson_builtin_symbols);
    jitson_builtin_symbols = NULL;
}

/**
 * Initialize the constants module
 *
 * @param constants A set of identifiers to be replaced with constant jitson values when parsing or NULL for JSON builtins only.
 *
 * @note Values are duplicated in the parsed jitson. If you want to include a large object or array, consider making the value
 *       a reference to it if it may appear more than once in the parsed JSON, but then beware the lifetime of the referenced
 *       object or array.
 */
void
sxe_jitson_const_initialize(const struct sxe_jitson *constants)
{
    const void **value_ptr;
    const char  *name;
    size_t       len, num_const;
    unsigned     i;

    SXEA1(jitson_builtin_symbols,                       "Can't initialize jitson constants before calling sxe_jitson_initialize");
    SXEA1(jitson_all_symbols == jitson_builtin_symbols, "Can't initialize jitson constants twice");

    jitson_constants  = constants;
    sxe_jitson_flags |= SXE_JITSON_FLAG_ALLOW_CONSTS;
    num_const         = constants ? sxe_jitson_len(constants) : 0;
    SXEA1(jitson_all_symbols = sxe_dict_new(3 + num_const), "Failed to allocate symbol table for JSON builtins and constants");

    value_ptr  = sxe_dict_add(jitson_all_symbols, "true", sizeof("true") - 1);
    *value_ptr = &json_builtins[0];
    value_ptr  = sxe_dict_add(jitson_all_symbols, "false", sizeof("false") - 1);
    *value_ptr = &json_builtins[1];
    value_ptr  = sxe_dict_add(jitson_all_symbols, "null", sizeof("null") - 1);
    *value_ptr = &json_builtins[2];

    for (constants++, i = 0; i < num_const; i++) {
        name       = sxe_jitson_get_string(constants, &len);
        constants += sxe_jitson_size(constants);
        value_ptr  = sxe_dict_add(jitson_all_symbols, name, len);
        *value_ptr = constants;
        constants += sxe_jitson_size(constants);
    }

    if (sxe_jitson_const_type_cast == SXE_JITSON_TYPE_INVALID)    // Create the cast pseudo type if not already done
        sxe_jitson_const_type_cast = sxe_jitson_type_register("cast", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

void
sxe_jitson_const_finalize(void)
{
    SXEA1(jitson_all_symbols && jitson_all_symbols != jitson_builtin_symbols,
          "Can't finalize constants if they weren't initialized");
    sxe_jitson_free(jitson_constants);
    sxe_dict_free(jitson_all_symbols);
    jitson_all_symbols = jitson_builtin_symbols;    // Revert back to the JSON builtins
}

/**
 * Register a type cast operator
 *
 * @param cast A jitson used to hold the cast function for the parser
 * @param type Name of the type
 * @param func The cast function to be applied to the JSON that follows the operator
 */
void
sxe_jitson_const_register_cast(struct sxe_jitson *cast, const char *type, sxe_jitson_castfunc_t func)
{
    const void **value_ptr;

    SXEA1(sxe_jitson_const_type_cast != SXE_JITSON_TYPE_INVALID,
         "sxe_jitson_const_initialize must be called before sxe_jitson_const_register_cast");
    cast->type     = sxe_jitson_const_type_cast;
    cast->castfunc = func;
    value_ptr      = sxe_dict_add(jitson_all_symbols, type, strlen(type));    // SonarQube False Positive
    SXEA1(!*value_ptr, "Attempt to reregister cast for type '%s'", type);
    *value_ptr = cast;
}

/**
 * Look up a name and return the matching builtin or constant value
 *
 * @param flags If SXE_JITSON_FLAG_ALLOW_CONSTS is set, return constants from initialization, else return JSON builtins only
 * @param name  The name of the builtin or constant
 * @param len   The length of the name
 *
 * @return The constant value, or NULL if not found
 */
const struct sxe_jitson *
sxe_jitson_const_get(unsigned flags, const char *name, size_t len)
{
    return sxe_dict_find(flags & SXE_JITSON_FLAG_ALLOW_CONSTS ? jitson_all_symbols : jitson_builtin_symbols, name, len);
}

