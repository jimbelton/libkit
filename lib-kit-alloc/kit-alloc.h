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

#ifndef KIT_ALLOC_H
#define KIT_ALLOC_H

#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>

#include "kit-counters.h"
#include "kit-port.h"

struct kit_memory_counters {
    kit_counter_t bytes;
    kit_counter_t calloc;
    kit_counter_t fail;
    kit_counter_t free;
    kit_counter_t malloc;
    kit_counter_t realloc;
};

extern struct kit_memory_counters kit_memory_counters;
extern size_t                     kit_memory_max_allocated;

#define KIT_COUNTER_MEMORY_BYTES     (kit_memory_counters.bytes)
#define KIT_COUNTER_MEMORY_CALLOC    (kit_memory_counters.calloc)
#define KIT_COUNTER_MEMORY_FAIL      (kit_memory_counters.fail)
#define KIT_COUNTER_MEMORY_FREE      (kit_memory_counters.free)
#define KIT_COUNTER_MEMORY_MALLOC    (kit_memory_counters.malloc)
#define KIT_COUNTER_MEMORY_REALLOC   (kit_memory_counters.realloc)

#ifdef MAK_DEBUG
/* Use bin/kit-alloc-analyze to parse kit_alloc_diagnostics lines */
extern int kit_alloc_diagnostics;
#define KIT_ALLOC_SET_LOG(n) do { kit_alloc_diagnostics = (n); } while (0)    // Turn kit_alloc log messages on/off in debug
#define KIT_ALLOC_SOURCE_PROTO , const char *file, int line
#define KIT_ALLOC_MANGLE(name) name ## _diag

#define kit_malloc(size)       kit_malloc_diag(size, __FILE__, __LINE__)          /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_memalign(al, size) kit_memalign_diag(al, size, __FILE__, __LINE__)    /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_reduce(ptr, size)  kit_reduce_diag(ptr, size, __FILE__, __LINE__)     /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_strdup(txt)        kit_strdup_diag(txt, __FILE__, __LINE__)           /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_calloc(num, size)  kit_calloc_diag(num, size, __FILE__, __LINE__)     /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_realloc(ptr, size) kit_realloc_diag(ptr, size, __FILE__, __LINE__)    /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_free(ptr)          kit_free_diag(ptr, __FILE__, __LINE__)             /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_strndup(txt, size) kit_strndup_diag(txt, size, __FILE__, __LINE__)    /* CONVENTION EXCLUSION: these are supposed to look like functions */

#else
#define KIT_ALLOC_SET_LOG(n) do { } while (0)
#define KIT_ALLOC_SOURCE_PROTO
#define KIT_ALLOC_MANGLE(name) name
#define KIT_ALLOC_SUFFIX

extern void *kit_malloc_diag(size_t size, const char *file, int line);
extern void *kit_memalign_diag(size_t alignment, size_t size, const char *file, int line);
extern void *kit_realloc_diag(void *ptr, size_t size, const char *file, int line);
extern void  kit_free_diag(void *ptr, const char *file, int line);
#endif

extern void kit_memory_initialize(bool assert_on_enomem);
extern bool kit_memory_is_initialized(void);
extern size_t kit_allocated_bytes(void);
extern uint64_t kit_thread_allocated_bytes(void);
extern __attribute__((malloc)) void *KIT_ALLOC_MANGLE(kit_malloc)(size_t size KIT_ALLOC_SOURCE_PROTO);
extern __attribute__((malloc)) void *KIT_ALLOC_MANGLE(kit_memalign)(size_t alignment, size_t size KIT_ALLOC_SOURCE_PROTO);
extern void *KIT_ALLOC_MANGLE(kit_reduce)(void *ptr, size_t size KIT_ALLOC_SOURCE_PROTO);
extern __attribute__((malloc)) char *KIT_ALLOC_MANGLE(kit_strdup)(const char *txt KIT_ALLOC_SOURCE_PROTO);
extern __attribute__((malloc)) void *KIT_ALLOC_MANGLE(kit_calloc)(size_t num, size_t size KIT_ALLOC_SOURCE_PROTO);
extern void *KIT_ALLOC_MANGLE(kit_realloc)(void *ptr, size_t size KIT_ALLOC_SOURCE_PROTO);
extern void KIT_ALLOC_MANGLE(kit_free)(void *ptr KIT_ALLOC_SOURCE_PROTO);
extern __attribute__((malloc)) char *KIT_ALLOC_MANGLE(kit_strndup)(const char *txt, size_t size KIT_ALLOC_SOURCE_PROTO);
extern uint64_t kit_memory_allocations(void);
extern bool kit_memory_log_growth(__printflike(1, 2) int (*printer)(const char *format, ...));
extern bool kit_memory_log_stats(__printflike(1, 2) int (*printer)(const char *format, ...), const char *options);

/* The following macro is useful for finding allocations and frees in third party libraries that don't use the kit interface
 */
#ifdef MAK_DEBUG

extern __thread uint64_t kit_thread_allocated_last;

#define kit_memory_probe() do { \
  if (kit_thread_allocated_bytes() != kit_thread_allocated_last) { \
    SXEL7(": Thread memory %s by %"PRIu64" to %"PRIu64" (%s:%d)",  \
          kit_thread_allocated_bytes() > kit_thread_allocated_last ? "grew" : "shrank", \
          kit_thread_allocated_bytes() > kit_thread_allocated_last ? kit_thread_allocated_bytes() - kit_thread_allocated_last  \
                                                                   : kit_thread_allocated_last - kit_thread_allocated_bytes(), \
          kit_thread_allocated_bytes(), __FILE__, __LINE__);  \
    kit_thread_allocated_last = kit_thread_allocated_bytes(); \
  } \
} while (0)

#endif

#endif
