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

#include <ctype.h>
#include <string.h>

#include "sxe-jitson.h"
#include "sxe-jitson-ident.h"
#include "sxe-log.h"

/* This is a 256 bit little endian bitmask. For each unsigned char, the bit is set if the char is valid in an identifier
 */
static uint64_t identifier_chars[4] = {0x03FF400000000000, 0x07FFFFFE87FFFFFE, 0x0000000000000000, 0x00000000000000};

/**
 * Construct a source from possibly non-NUL terminated JSON
 *
 * @param source The source to construct
 * @param string JSON to be parsed
 * @param len    Length of the JSON to be parsed
 * @param flags  Flags affecting the parsing. 0 for strict JSON, or one or more of SXE_JITSON_FLAG_ALLOW_HEX,
 *               SXE_JITSON_FLAG_ALLOW_CONSTS or SXE_JITSON_FLAG_ALLOW_IDENTS
 *
 * @note SXE_JITSON_FLAG_ALLOW_IDENTS has no effect if sxe_jitson_ident_register has not been called
 */
void
sxe_jitson_source_from_buffer(struct sxe_jitson_source *source, const char *string, size_t len, uint32_t flags)
{
    source->json  = string;
    source->next  = string;
    source->end   = string + len;
    source->flags = flags;
    source->file  = NULL;
    source->line  = 0;
}

/**
 * Construct a source from a JSON string
 *
 * @param source The source to construct
 * @param string A JSON string to be parsed
 * @param flags  Flags affecting the parsing. 0 for strict JSON, or one or more of SXE_JITSON_FLAG_ALLOW_HEX,
 *               SXE_JITSON_FLAG_ALLOW_CONSTS or SXE_JITSON_FLAG_ALLOW_IDENTS
 *
 * @note SXE_JITSON_FLAG_ALLOW_IDENTS has no effect if sxe_jitson_ident_register has not been called
 */
void
sxe_jitson_source_from_string(struct sxe_jitson_source *source, const char *string, uint32_t flags)
{
    source->json  = string;
    source->next  = string;
    source->end   = (const char *)~0ULL;    // Just use the NUL terminator. This allows us to not take the strlen.
    source->flags = flags;
    source->file  = NULL;
    source->line  = 0;
}

/**
 * Set file/line information for diagnostics
 *
 * @param file File name or NULL
 * @param line Line number or 0 to not track
 */
void
sxe_jitson_source_set_file_line(struct sxe_jitson_source *source, const char *file, unsigned line)
{
    source->file  = file;
    source->line  = line;
}

/* Return the next character in the source without consuming it, or '\0' on end of data
 */
unsigned char
sxe_jitson_source_peek_char(const struct sxe_jitson_source *source)
{
    return source->next >= source->end ? '\0' : *source->next;
}

/* Get the next character in the source, returning '\0' on end of data
 */
char
sxe_jitson_source_get_char(struct sxe_jitson_source *source)
{
    if (source->next >= source->end || (source->end == (const char *)~0ULL && *source->next == '\0'))
        return '\0';

    if (source->line && *source->next == '\n')
        source->line++;

    return *source->next++;
}

/* Skip whitespace characters in the source, returning a peek at the first nonspace character or '\0' on end of data
 */
char
sxe_jitson_source_peek_nonspace(struct sxe_jitson_source *source)
{
    while (source->next < source->end && isspace(*source->next)) {
        if (source->line && *source->next == '\n')
            source->line++;

        source->next++;
    }

    return sxe_jitson_source_peek_char(source);
}

/* Skip whitespace characters in the source, returning the first nonspace character or '\0' on end of data
 */
char
sxe_jitson_source_get_nonspace(struct sxe_jitson_source *source)
{
    char c;

    while (isspace(c = sxe_jitson_source_get_char(source))) {
    }

    return c;
}

/**
 * Peek at identifier characters in a source until a non-identifier character is reached
 *
 * @param source     Source to parse
 * @param len_out    Pointer to variable set to the length of the identifier; not set if identifier is not found
 *
 * @return Identifier or NULL if first character is a non-identifier
 *
 * @note If the first identifier character has stricter limitations than subsequent characters, you must check that outside
 */
const char *
sxe_jitson_source_peek_identifier(const struct sxe_jitson_source *source, size_t *len_out)
{
    const char   *next = source->next;
    unsigned char c    = next >= source->end ? '\0' : (unsigned char)*next;

    /* While the next character's bit is set in the 256 bit identifier_chars bitmask
     */
    for (; identifier_chars[c >> 6] & (1UL << (c & 0x3f)); c = next >= source->end ? '\0' : *next)
        next++;

    return (*len_out = next - source->next) ? source->next : NULL;
}

/**
 * Peek at characters in a source until a whitespace character is reached
 *
 * @param source     Source to parse
 * @param len_out    Pointer to variable set to the length of the token; not set if a token is not found
 *
 * @return Token or NULL on end of source
 */
const char *
sxe_jitson_source_peek_token(struct sxe_jitson_source *source, size_t *len_out)
{
    const char *next, *token;

    if (!sxe_jitson_source_peek_nonspace(source))    // EOF
        return NULL;

    token    = source->next;
    *len_out = 1;

    /* While the next character is not EOF or whitespace
     */
    for (next = token + 1; next < source->end && *next && !isspace(*next); next++)
        (*len_out)++;

    return token;
}

/**
 * Get identifier characters until a non-identifier character is reached.
 *
 * @param source  Source to parse
 * @param len_out Set to the length of the identifier or 0 if there is no valid identifier
 *
 * @return Pointer to the identifier or NULL if there is no valid identifier
 *
 * @note If the first identifier character has stricter limitations than subsequent characters, you must check that outside
 */
const char *
sxe_jitson_source_get_identifier(struct sxe_jitson_source *source, size_t *len_out)
{
    const char *identifier = sxe_jitson_source_peek_identifier(source, len_out);

    sxe_jitson_source_consume(source, *len_out);
    return identifier;
}

/**
 * Get characters until a non-number character is reached.
 *
 * @param source      Source to parse
 * @param len_out     Set to the length of the number as a string
 * @param is_uint_out Set to true if the value is a uint, false if not
 *
 * @return Pointer to the number as a string or NULL if there is no valid number
 */
const char *
sxe_jitson_source_get_number(struct sxe_jitson_source *source, size_t *len_out, bool *is_uint_out)
{
    const char *number = source->next;

    *is_uint_out = true;

    if (sxe_jitson_source_peek_char(source) == '-') {
        *is_uint_out = false;
        source->next++;
    }

    if (!isdigit(sxe_jitson_source_peek_char(source)))
        goto INVALID;

    source->next++;

    /* If hex is allowed and the number starts with '0x', its a hexadecimal unsigned integer
     */
    if ((source->flags & SXE_JITSON_FLAG_ALLOW_HEX) && *number == '0' && sxe_jitson_source_peek_char(source) == 'x') {
        for (source->next++; isxdigit(sxe_jitson_source_peek_char(source)); source->next++) {
        }

        if (source->next - number <= 2)    // Can't be just '0x'
            goto INVALID;

        *len_out = source->next - number;
        return number;
    }

    while (isdigit(sxe_jitson_source_peek_char(source)))
        source->next++;

    if (sxe_jitson_source_peek_char(source) == '.') {    // If there's a fraction
        *is_uint_out = false;
        source->next++;

        if (!isdigit(sxe_jitson_source_peek_char(source)))
            goto INVALID;

        while (isdigit(sxe_jitson_source_peek_char(source)))
            source->next++;
    }

    if (sxe_jitson_source_peek_char(source) == 'E' || sxe_jitson_source_peek_char(source) == 'e') {    // If there's an exponent
        *is_uint_out = false;
        source->next++;

        if (sxe_jitson_source_peek_char(source) == '-' || sxe_jitson_source_peek_char(source) == '+')
            source->next++;

        if (!isdigit(sxe_jitson_source_peek_char(source)))
            goto INVALID;

        while (isdigit(sxe_jitson_source_peek_char(source)))
            source->next++;
    }

    *len_out = source->next - number;
    return number;

INVALID:
    errno = EINVAL;
    return NULL;
}

/**
 * Get a literal, non-JSON character string
 *
 * @param source      Source to parse
 * @param len_out     If not NULL, set to the length of the literal as a string including the double quotes
 *
 * @return Pointer to the literal as a string or NULL if there is no valid literal
 */
const char *
sxe_jitson_source_get_literal(struct sxe_jitson_source *source, size_t *len_out)
{
    const char *literal = source->next;
    char        character;

    if (sxe_jitson_source_peek_char(source) != '"')
        goto INVALID;

    source->next++;

    while ((character = sxe_jitson_source_get_char(source)) != '"')
        if (character == '\0')
            goto INVALID;

    if (len_out)
        *len_out = source->next - literal;

    return literal;

INVALID:
    errno = EINVAL;
    return NULL;
}

/**
 * Return up to 63 characters of the source or "" on EOF, for use in diagnostic messages.
 */
const char *
sxe_jitson_source_left(const struct sxe_jitson_source *source)
{
    static __thread char buf[65];    // An extra byte is needed just to shut up GCC

    if (*source->next == '\0' || source->next >= source->end)
        return "";

    if ((uintptr_t)source->end - (uintptr_t)source->next > 63) {
        strncpy(buf, source->next, 64);    // SonarQube False Positive

        if (buf[63])    // There are more than 63 characters
            strcpy(&buf[60], "...");    // SonarQube False Positive
    }
    else {
        memcpy(buf, source->next, source->end - source->next);
        buf[source->end - source->next] = '\0';
    }

    return buf;
}
