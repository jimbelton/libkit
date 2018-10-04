#ifndef KIT_ALLOC_H
#define KIT_ALLOC_H

#include "kit-counters.h"

#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>

struct kit_memory_counters {
    kit_counter_t bytes;
    kit_counter_t calloc;
    kit_counter_t fail;
    kit_counter_t free;
    kit_counter_t malloc;
    kit_counter_t realloc;
};

extern struct kit_memory_counters kit_memory_counters;

#define KIT_COUNTER_MEMORY_BYTES     (kit_memory_counters.bytes)
#define KIT_COUNTER_MEMORY_CALLOC    (kit_memory_counters.calloc)
#define KIT_COUNTER_MEMORY_FAIL      (kit_memory_counters.fail)
#define KIT_COUNTER_MEMORY_FREE      (kit_memory_counters.free)
#define KIT_COUNTER_MEMORY_MALLOC    (kit_memory_counters.malloc)
#define KIT_COUNTER_MEMORY_REALLOC   (kit_memory_counters.realloc)

#if SXE_DEBUG
/* Use bin/kit-alloc-analyze to parse kit_alloc_diagnostics lines */
extern int kit_alloc_diagnostics;
#define KIT_ALLOC_SET_LOG(n) do { kit_alloc_diagnostics = (n); } while (0)
#define KIT_ALLOC_SOURCE_PROTO , const char *file, unsigned line
#define KIT_ALLOC_MANGLE(name) name ## _diag

#define kit_malloc(size) kit_malloc_diag(size, __FILE__, __LINE__)                /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_reduce(ptr, size) kit_reduce_diag(ptr, size, __FILE__, __LINE__)      /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_strdup(txt) kit_strdup_diag(txt, __FILE__, __LINE__)                  /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_calloc(num, size) kit_calloc_diag(num, size, __FILE__, __LINE__)      /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_realloc(ptr, size) kit_realloc_diag(ptr, size, __FILE__, __LINE__)    /* CONVENTION EXCLUSION: these are supposed to look like functions */
#define kit_free(ptr) kit_free_diag(ptr, __FILE__, __LINE__)                      /* CONVENTION EXCLUSION: these are supposed to look like functions */

#else
#define KIT_ALLOC_SET_LOG(n) do { } while (0)
#define KIT_ALLOC_SOURCE_PROTO
#define KIT_ALLOC_MANGLE(name) name
#define KIT_ALLOC_SUFFIX
#endif

extern void kit_memory_counters_init(void);
extern bool kit_memory_counters_initialized(void);
extern size_t kit_allocated_bytes(void);
extern uint64_t kit_thread_allocated_bytes(void);
extern __attribute__((malloc)) void *KIT_ALLOC_MANGLE(kit_malloc)(size_t size KIT_ALLOC_SOURCE_PROTO);
extern void *KIT_ALLOC_MANGLE(kit_reduce)(void *ptr, size_t size KIT_ALLOC_SOURCE_PROTO);
extern __attribute__((malloc)) char *KIT_ALLOC_MANGLE(kit_strdup)(const char *txt KIT_ALLOC_SOURCE_PROTO);
extern __attribute__((malloc)) void *KIT_ALLOC_MANGLE(kit_calloc)(size_t num, size_t size KIT_ALLOC_SOURCE_PROTO);
extern void *KIT_ALLOC_MANGLE(kit_realloc)(void *ptr, size_t size KIT_ALLOC_SOURCE_PROTO);
extern void KIT_ALLOC_MANGLE(kit_free)(void *ptr KIT_ALLOC_SOURCE_PROTO);

#endif