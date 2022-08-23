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

#ifndef KIT_ALLOC_PRIVATE_H
#define KIT_ALLOC_PRIVATE_H

#include "kit-alloc.h"

/* Wrapper macros for logging used only in kit-alloc
 */
#define KIT_ASSERT(con, ...)   do { if (!(con)) (*kit_alloc_assert)(__FILE__,__LINE__,#con,__VA_ARGS__); } while (0)

#ifdef MAK_DEBUG
#   define KIT_CHECK(con, ...) do { if (!(con)) (*kit_alloc_assert)(__FILE__,__LINE__,#con,__VA_ARGS__); } while (0)
#   define KIT_TRACE(...)      do { (*kit_alloc_trace)(__FILE__,__LINE__,__VA_ARGS__); } while (0)
#else
#   define KIT_CHECK(con, ...) do {} while (0)
#   define KIT_TRACE(...)      do {} while (0)
#endif

/* Kit log will plug in functions here when initialized
 */
extern void (*kit_alloc_assert)(const char * file, int line, const char * con, const char * format, ...);
extern void (*kit_alloc_trace)( const char * file, int line, const char * format, ...);

extern void kit_memory_init_internal(bool hard);

#endif
