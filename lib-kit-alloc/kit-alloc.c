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

#include <assert.h>
#include <fcntl.h>
#include <jemalloc/jemalloc.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__
#include <malloc.h>    // CONVENTION EXCLUSION: To support glibc memory growth tracking
#endif

#include "kit-alloc-private.h"
#include "kit-mockfail.h"
#include "sxe-util.h"

#ifdef __GLIBC__
#pragma GCC diagnostic ignored "-Waggregate-return"    // To allow use of glibc mallinfo
#endif

/*-
 * As of December 2012, the stock malloc implementation in linux is not
 * clever enough to handle the allocations requirements of opendnscache.
 *
 * This can be demonstrated by monitoring the following data:
 *
 *    VSZ of the process
 *        ps -axwwovsz,comm | sed -n 's, opednscache,,p'
 *
 *    # Inflight allocations & total
 *        my %stats = map { m{^"(\S+)\s(.+)"$} } `dig +short txt stats.opendns.com \@127.0.0.1`;
 *        my $inflight = $stats{'memory.calloc'} +
 *                       $stats{'memory.malloc'} -
 *                       $stats{'memory.free'};
 *        my $total = $stats{'memory.bytes'};
 *
 * On a busy resolver running with -c 800000000, you will see $inflight
 * settle at around 8,044,000 allocations and 1565 MB total.  It will
 * increase as pref files are loaded and then settle back to these numbers
 * afterwards.
 *
 * When it is settled, using jemalloc, the process VSZ starts at about
 * 2000 MB and increases to around 2450 MB after about 10 minutes.
 * It stabilizes at this value.
 *
 * Using the stock system malloc, the process VSZ starts at about
 * 2200 MB and increases to around 3219 MB in the first 10 minutes.
 * After that, on a machine with 3GB of RAM, it starts to swap and
 * takes so long to load a prefs file that there's another one there
 * before it's done.
 */

#define KIT_ALLOC_LOG(...) do { if (kit_memory_diagnostics) SXEL5(__VA_ARGS__); } while (0)

struct kit_memory_guard {    // If overflow detection is enabled, allocate on of these before the memory, and a stamp after
    size_t size;             // Size of memory allocated (rounded up)
    void  *stamp;            // A stamp value which is the address of the allocated memory
};

static_assert(sizeof(struct kit_memory_guard) == 16, "Unexpected size for kit_memory_guard; must be a power of 2");

#if SXE_DEBUG    // In a debug build of libkit, check for memory overflows by default
    static unsigned kit_memory_flags = KIT_MEMORY_CHECK_OVERFLOWS;
#else            // In the release build of libkit, overflow checking is disabled by default
    static unsigned kit_memory_flags = 0;
#endif

struct kit_memory_counters kit_memory_counters;                  // Global counter ids
size_t                     kit_memory_allocated_max  = 0;        // Tracks the high watermark ever allocated with jemalloc
int                        kit_memory_diagnostics    = 0;        // Set >0 to turn on kit memory log messages
__thread uint64_t          kit_thread_allocated_last = 0;        // Used by kit_memory_probe diagnostic macro
static bool                kit_memory_initialized    = false;    // Set once initialized
static bool                kit_memory_using_counters = false;    // Set by kit_counters_initialize

/* Global counters used until kit-counters are intialized. Not particularly thread safe.
 */
static unsigned long long count_bytes   = 0;
static unsigned long long count_calloc  = 0;
static unsigned long long count_fail    = 0;
static unsigned long long count_free    = 0;
static unsigned long long count_malloc  = 0;
static unsigned long long count_realloc = 0;

#ifdef __linux__
static int proc_statm_fd = -1;    // Open file descriptor on /proc/<pid>/statm; must be opened before calling chroot
#endif

static unsigned long long
counter_bytes_combine_handler(int threadnum)
{
    return threadnum <= 0 ? kit_allocated_bytes() : 0;
}

/**
 * Set flags that affect the behaviour of the kit memory management interface
 *
 * @param flags KIT_MEMORY_ABORT_ON_ENOMEM to abort on memory allocation failure (returns ENOMEM if unset)
 *              KIT_MEMORY_CHECK_OVERFLOWS to check for memory overflows on relloc/free and abort if found
 *
 * @note The flags must not be changed once this module has been initialized
 */
void
kit_memory_set_flags(unsigned flags)
{
    SXEA1(!kit_memory_initialized, "%s must be called before kit_memory_initialize or kit_counters_initialize", __func__);
    SXEA1(count_calloc + count_malloc == 0, "%s must be called before any memory is allocated", __func__);
    kit_memory_flags = flags;
}

/**
 * Set the behaviour of the kit memory management interface if an enomem error occurs
 *
 * @param assert_on_enomem true to abort on memory allocation failure, false (default) to return the error
 */
void
kit_memory_set_assert_on_enomem(bool assert_on_enomem)
{
    if (assert_on_enomem)
        kit_memory_flags |= KIT_MEMORY_ABORT_ON_ENOMEM;
    else
        kit_memory_flags &= ~KIT_MEMORY_ABORT_ON_ENOMEM;
};

/**
 * Initialize the kit memory management interface; this should done once, normally on an application wide basis
 *
 * @param flags KIT_MEMORY_ABORT_ON_ENOMEM to abort on memory allocation failure (returns ENOMEM if unset)
 *              KIT_MEMORY_CHECK_OVERFLOWS to check for memory overflows on relloc/free and abort if found
 *
 * @note This function should only be used if accurate counters are not required; preferably use kit_counters_initialize
 */
void
kit_memory_initialize(unsigned flags)
{
    SXEA1(!kit_memory_initialized, "Kit memory is already initialized");

    if (flags != ~0U) {   // If not being called by kit_counters_initialize
        SXEA1(count_calloc + count_malloc == 0, "%s must be called before any memory is allocated", __func__);
        kit_memory_flags = flags;
    }

#ifdef __linux__
    char proc_statm_path[PATH_MAX];
    int  pid;

    pid = getpid();
    snprintf(proc_statm_path, sizeof(proc_statm_path), "/proc/%d/statm", pid);
    proc_statm_fd = open(proc_statm_path, O_RDONLY);
#endif

#if SXE_DEBUG
    const char *KIT_ALLOC_DIAGNOSTICS = getenv("KIT_ALLOC_DIAGNOSTICS");

    if (KIT_ALLOC_DIAGNOSTICS && KIT_ALLOC_DIAGNOSTICS[0] && KIT_ALLOC_DIAGNOSTICS[0] != '0')
        kit_memory_diagnostics = 1;
#endif

    kit_memory_initialized = true;
}

/* Called only from kit_counters_initialize
 */
void
kit_memory_initialize_counters(void)
{
    SXEA1(kit_memory_initialized, "Kit memory is not already initialized");

    kit_memory_counters.bytes   = kit_counter_reg_with_combine_handler("memory.bytes", counter_bytes_combine_handler);
    kit_memory_counters.calloc  = kit_counter_reg("memory.calloc");
    kit_memory_counters.fail    = kit_counter_reg("memory.fail");
    kit_memory_counters.free    = kit_counter_reg("memory.free");
    kit_memory_counters.malloc  = kit_counter_reg("memory.malloc");
    kit_memory_counters.realloc = kit_counter_reg("memory.realloc");

    /* Switch over to using the counters
     */
    kit_counter_add(kit_memory_counters.bytes,   count_bytes);
    kit_counter_add(kit_memory_counters.calloc,  count_calloc);
    kit_counter_add(kit_memory_counters.fail,    count_fail);
    kit_counter_add(kit_memory_counters.free,    count_free);
    kit_counter_add(kit_memory_counters.malloc,  count_malloc);
    kit_counter_add(kit_memory_counters.realloc, count_realloc);

    kit_memory_using_counters = true;
}

/**
 * Determine number of outstanding memory allocations
 *
 * @return The number of memory allocations less the number of frees
 *
 * @note This function can be called before initializing kit_counters or kit_memory (e.g. at the start of a test program)
 */
uint64_t
kit_memory_allocations(void)
{
    if (!kit_memory_using_counters)
        return count_calloc + count_malloc - count_free;

    return kit_counter_get(KIT_COUNTER_MEMORY_CALLOC) + kit_counter_get(KIT_COUNTER_MEMORY_MALLOC)
         - kit_counter_get(KIT_COUNTER_MEMORY_FREE);
}

/**
 * Determine the size of memory with guard words
 *
 * @param size      The size allocated
 * @param alignment The alignment used or 0 if not allocated with memalign
 *
 * @return The amount of memory actually allocated; size if !(flags & KIT_MEMORY_CHECK_OVERFLOWS), otherwise > size
 */
size_t
kit_memory_size(size_t size, size_t alignment)
{
    if (kit_memory_flags & KIT_MEMORY_CHECK_OVERFLOWS) {
        alignment     = alignment >= sizeof(size_t) ? alignment : sizeof(size_t);
        size_t offset = sizeof(struct kit_memory_guard) >= alignment ? sizeof(struct kit_memory_guard) : alignment;
        size          = offset + size + sizeof(void *);
    }

    return size;
}

static void
guard_initialize(char **result_in_out, size_t size, size_t alignment)
{
    if (*result_in_out && kit_memory_flags & KIT_MEMORY_CHECK_OVERFLOWS) {
        ((struct kit_memory_guard *)*result_in_out)->size  = size;
        ((struct kit_memory_guard *)*result_in_out)->stamp = *result_in_out;
        *(void **)(*result_in_out + size - sizeof(void *)) = *result_in_out;

        if (alignment > sizeof(struct kit_memory_guard)) {    // Need a guard right before the address returned
            char *near_guard = *result_in_out + alignment - sizeof(struct kit_memory_guard);
            memcpy(near_guard, *result_in_out, sizeof(struct kit_memory_guard));
            *result_in_out += alignment;
        }
        else
            *result_in_out += sizeof(struct kit_memory_guard);
    }
}

/* Internal memory allocator that supports jemalloc mallocx flags
 */
static void *
memory_alloc(size_t size, size_t alignment, int flags)
{
    char  *result;

    size = kit_memory_size(size, alignment);

    if ((result = MOCKERROR(kit_malloc_diag, NULL, ENOMEM, mallocx(size, flags))))
        guard_initialize(&result, size, alignment);
    else
        SXEA1(!(kit_memory_flags & KIT_MEMORY_ABORT_ON_ENOMEM) , ": failed to allocate %zu bytes", size);

    return result;
}

static void
count_malloc_increment(bool failed)
{
    if (kit_memory_using_counters) {
        kit_counter_incr(KIT_COUNTER_MEMORY_MALLOC);

        if (failed)
            kit_counter_incr(KIT_COUNTER_MEMORY_FAIL);
    } else {
        count_malloc++;

        if (failed)
            count_fail++;
    }
}

__attribute__((malloc)) void *
kit_malloc_diag(size_t size, const char *file, int line)
{
    char *result = memory_alloc(size, 0, 0);
    count_malloc_increment(result == NULL);
    KIT_ALLOC_LOG("%s: %d: %p = kit_malloc(%zu)", file, line, result, size);
    return result;
}

__attribute__((malloc)) void *
kit_memalign_diag(size_t alignment, size_t size, const char *file, int line)
{
    int lg_align = sxe_uint64_log2(alignment);
    SXEA1(1ULL << lg_align == alignment, ": Alignment %zu is not a power of 2", alignment);

    void *result = memory_alloc(size, alignment, MALLOCX_LG_ALIGN(lg_align));
    count_malloc_increment(result == NULL);
    KIT_ALLOC_LOG("%s: %d: %p = kit_memalign(%zu,%zu)", file, line, result, alignment, size);
    return result;
}

__attribute__((malloc)) void *
kit_calloc_diag(size_t num, size_t size, const char *file, int line)
{
    size_t total_size = num * size;
    void  *result     = NULL;

    if (num && size && (total_size < num || total_size < size)) {
        SXEA1(!(kit_memory_flags & KIT_MEMORY_ABORT_ON_ENOMEM) || result, ": %zu*%zu exceeds the maximum size %zu", num, size, (size_t)~0UL);
        errno = ENOMEM;
    }
    else
        result = memory_alloc(total_size, 0, MALLOCX_ZERO);

    SXEA1(!(kit_memory_flags & KIT_MEMORY_ABORT_ON_ENOMEM) || result, ": failed to allocate %zu %zu byte objects", num, size);

    if (kit_memory_using_counters) {
        kit_counter_incr(KIT_COUNTER_MEMORY_CALLOC);

        if (result == NULL)
            kit_counter_incr(KIT_COUNTER_MEMORY_FAIL);
    } else {
        count_calloc++;

        if (result == NULL)
            count_fail++;
    }

    KIT_ALLOC_LOG("%s: %d: %p = kit_calloc(%zu, %zu)", file, line, result, num, size);
    return result;
}

static void
count_free_increment(void)
{
    if (kit_memory_using_counters)
        kit_counter_incr(KIT_COUNTER_MEMORY_FREE);
    else
        count_free++;
}

#define KIT_MEMORY_FREE ((void *)~0UL)    // Internal size_out value to invalidate the fore guard before a free

/**
 * Check kit allocated memory for overflows
 *
 * @param ptr      Pointer returned by a kit allocation
 * @param size_out NULL, a pointer to store the size of the allocation or 0 if not bounds checking, or ~0UL (internal use)
 *
 * @return ptr if not checking overflows or a pointer to the guard structure
 *
 * @note Aborts if the guard stamps before or after the memory have been overwritten
 */
void *
kit_memory_check(void *ptr, size_t *size_out)
{
    if (!(kit_memory_flags & KIT_MEMORY_CHECK_OVERFLOWS)) {
        if (size_out && size_out != KIT_MEMORY_FREE)
            *size_out = 0;

        return ptr;
    }

    struct kit_memory_guard *guard = (struct kit_memory_guard *)ptr - 1;

    if (guard->stamp != guard) {    // This can happen if alignment > sizeof(kit_memory_guard)
        SXEA1(((uintptr_t)guard->stamp & (2 * sizeof(struct kit_memory_guard) - 1)) == 0,
              "Unaligned fore guard stamp at %p is not a multiple of %zu", guard->stamp, 2 * sizeof(struct kit_memory_guard));
        SXEA1(guard->stamp < (void *)guard, "Fore guard stamp %p must be less than unaligned guard location %p", guard->stamp,
              guard);
        guard = guard->stamp;    // There should be another guard at the beginning of the malloced area
    }

    SXEA1(guard->stamp == guard, "Fore guard stamp at %p corrupted", &guard->stamp);
    SXEA1(*(void **)((char *)guard + guard->size - sizeof(void *)) == guard, "Rear guard stamp at %p corrupted",
          (char *)guard + guard->size - sizeof(void *));

    if (size_out == KIT_MEMORY_FREE)
        guard->stamp = KIT_MEMORY_FREE;    // Invalidate the fore guard (prior to freeing the memory)
    else if (size_out)
        *size_out = guard->size - ((uintptr_t)ptr - (uintptr_t)guard) - sizeof(void *);

    return guard;
}

void
kit_free_diag(void *ptr, const char *file, int line)
{
    if (ptr) {
        KIT_ALLOC_LOG("%s: %d: kit_free(%p)", file, line, ptr);
        SXEA1(!(((long)ptr) & (sizeof(void *) - 1)), "ungranular free(%p)", ptr);
        count_free_increment();
        ptr = kit_memory_check(ptr, KIT_MEMORY_FREE);    // Get back the actual memory allocated if overflow checking
        dallocx(ptr, 0);
    }
#if SXE_DEBUG
    else if (kit_memory_diagnostics > 1)
        SXEL6("%s: %d: kit_free((nil))", file, line);
#endif
}

/*-
 * Special case for realloc() since it can behave like malloc() or free() !
 */
void *
kit_realloc_diag(void *ptr, size_t size, const char *file, int line)
{
    char  *result = NULL;
    void  *optr   = ptr;
    size_t osize  = size;

    if (!ptr)
        result = memory_alloc(size, 0, 0);
    else if (size) {
        ptr              = kit_memory_check(ptr, NULL);     // Get back the actual memory allocated (even if bounds checking)
        size_t alignment = (char *)optr - (char *)ptr;
        size             = kit_memory_size(size, alignment);    // Keep alignment padding consistent
        result           = MOCKERROR(kit_realloc_diag, NULL, ENOMEM, rallocx(ptr, size, 0));

        guard_initialize(&result, size, alignment);
    }
    else {
        ptr = kit_memory_check(ptr, KIT_MEMORY_FREE);    // Get back the actual memory allocated if overflow checking
        dallocx(ptr, 0);
    }

    if (result == NULL && (size || ptr == NULL)) {
        SXEA1(!(kit_memory_flags & KIT_MEMORY_ABORT_ON_ENOMEM), ": failed to reallocate object to %zu bytes", size);

        if (kit_memory_using_counters)
            kit_counter_incr(KIT_COUNTER_MEMORY_FAIL);
        else
            count_fail++;
    }

    if (ptr == NULL && result != NULL)
        count_malloc_increment(false);    // Non-failure
    else if (ptr && size == 0 && result == NULL)
        count_free_increment();
    else if (kit_memory_using_counters)
        kit_counter_incr(KIT_COUNTER_MEMORY_REALLOC);
    else
        count_realloc++;

    KIT_ALLOC_LOG("%s: %d: %p = kit_realloc(%p, %zu)", file, line, result, optr, osize);
    return result;
}

void *
kit_reduce_diag(void *ptr, size_t size, const char *file, int line)
{
    if (ptr == NULL) {
        SXEA1(!size, "Cannot kit_reduce() NULL to size %zu", size);
        return NULL;
    }

    void *result = kit_realloc_diag(ptr, size, file, line);

    if (!result && size) {    // Realloc failed (not a free)
        kit_counter_decr(KIT_COUNTER_MEMORY_REALLOC);    // Memory was not reallocated
        return ptr;
    }

    return result;
}

__attribute__((malloc)) char *
kit_strdup_diag(const char *txt, const char *file, int line)
{
    size_t len    = strlen(txt);
    void  *result = memory_alloc(len + 1, 0, 0);

    if (result)
        memcpy(result, txt, len + 1);

    count_malloc_increment(result == NULL);
    KIT_ALLOC_LOG("%s: %d: %p = kit_strdup(%p[%zu])", file, line, result, txt, len + 1);
    return result;
}

__attribute__((malloc)) char *
kit_strndup_diag(const char *txt, size_t size, const char *file, int line)
{
    size_t len    = strnlen(txt, size);
    char  *result = memory_alloc(len + 1, 0, 0);

    if (result) {
        memcpy(result, txt, len);
        result[len] = '\0';
    }

    count_malloc_increment(result == NULL);
    KIT_ALLOC_LOG("%s: %d: %p = kit_strndup(%p[%zu])", file, line, result, txt, len + 1);
    return result;
}

size_t
kit_allocated_bytes(void)
{
    static bool   kit_mib_init          = false;    // Set to true once memory mib ids are initialized
    static size_t kit_epoch_mib[1];                 // Binary mib id of the "epoch"
    static size_t kit_epoch_mib_len     = 1;        // Number of elements in the mib id
    static size_t kit_allocated_mib[2];             // Binary mib id of "stats.allocated"
    static size_t kit_allocated_mib_len = 2;        // Number of elements in the mib id
    size_t        len, alloc;
    uint64_t      epoch;

    if (!kit_mib_init) {
        // To optimize collection of je_malloc statistics, get binary MIB ids
        //
        SXEA1(!mallctlnametomib("epoch", kit_epoch_mib, &kit_epoch_mib_len)
           && !mallctlnametomib("stats.allocated", kit_allocated_mib, &kit_allocated_mib_len),
              "Failed to generated binary mib id for je_malloc's epoch or stats.allocated");
        kit_mib_init = true;
    }

    epoch = 1;
    len = sizeof(epoch);
    mallctlbymib(kit_epoch_mib, kit_epoch_mib_len, &epoch, &len, &epoch, len);

    len = sizeof(alloc);
    mallctlbymib(kit_allocated_mib, kit_allocated_mib_len, &alloc, &len, NULL, 0);

    if (alloc > kit_memory_allocated_max)
        kit_memory_allocated_max = alloc;

    return alloc;
}

uint64_t
kit_thread_allocated_bytes(void)
{
    static __thread uint64_t *allocatedp, *deallocatedp;
    size_t len;

    if (!allocatedp) {
        len = sizeof(allocatedp);
        mallctl("thread.allocatedp", &allocatedp, &len, NULL, 0);
        SXEA1(allocatedp, "Couldn't obtain thread.allocatedp, is jemalloc built with --enable-stats?");
    }

    if (!deallocatedp) {
        len = sizeof(deallocatedp);
        mallctl("thread.deallocatedp", &deallocatedp, &len, NULL, 0);
        SXEA1(deallocatedp, "Couldn't obtain thread.deallocatedp, is jemalloc built with --enable-stats?");
    }

    return *allocatedp - *deallocatedp;
}

bool
kit_memory_log_growth(__printflike(1, 2) int (*printer)(const char *format, ...))
{
    static size_t jemalloc_allocated_max = 0;
    size_t        jemalloc_allocated_cur = kit_allocated_bytes();
    bool          growth                 = false;

    if (jemalloc_allocated_cur > jemalloc_allocated_max) {
        (*printer)("Maximum memory allocated via jemalloc %zu (previous maximum %zu)\n", jemalloc_allocated_cur,
                   jemalloc_allocated_max);
        jemalloc_allocated_max = jemalloc_allocated_cur;
        growth                 = true;
    }

#ifdef __GLIBC__
    static size_t    glibc_allocated_max = 0;    // High watermark of glibc bytes allocated
#if __GLIBC_MINOR__ >= 33
    struct mallinfo2 glibc_mallinfo = mallinfo2();
#else
    struct mallinfo  glibc_mallinfo = mallinfo();
#endif

    if ((size_t)glibc_mallinfo.uordblks > glibc_allocated_max) {
        /* COVERAGE EXCLUSION - This code can't be reached as long as malloc is correctly revectored to jemalloc */
        (*printer)("Maximum memory allocated via glibc %zu (previous maximum %zu)\n",    /* COVERAGE EXCLUSION - Can't happen */
                   (size_t)glibc_mallinfo.uordblks, glibc_allocated_max);
        glibc_allocated_max = (size_t)glibc_mallinfo.uordblks;                           /* COVERAGE EXCLUSION - Can't happen */
        growth              = true;                                                      /* COVERAGE EXCLUSION - Can't happen */
    }
#endif

#ifdef __linux__
    static size_t rss_max = 0;    // High watermark of RSS pages allocated
    long long     rss_cur;
    char          buf[256];       // Buffer for /proc/<pid>/statm contents (e.g. "14214701 8277571 1403 269 0 14138966 0")
    ssize_t       len;
    const char   *rss_str;
    char         *end_ptr;

    // If the /proc/<pid>/statm is open, seek to start and read contents
    if (proc_statm_fd >= 0 && lseek(proc_statm_fd, 0, SEEK_SET) != (off_t)-1
     && (len = read(proc_statm_fd, buf, sizeof(buf) - 1)) >= 0) {
        buf[len] = '\0';

        if ((rss_str = strchr(buf, ' '))) {    // Find the RSS number in the string
            rss_str++;

            // Try to convert to a number and, if a new maximum, log it
            if ((rss_cur = strtoll(rss_str, &end_ptr, 10)) > 0 && *end_ptr == ' ' && (size_t)rss_cur > rss_max) {
                (*printer)("Maximum memory allocated in RSS pages %zu (previous maximum %zu)\n", (size_t)rss_cur, rss_max);
                rss_max = (size_t)rss_cur;
                growth  = true;
            }
        }
    }
#endif

    return growth;
}

struct printer_visitor {
    int                      written;
    __printflike(1, 2) int (*printer)(const char *format, ...);
};

static void
memory_stats_line(void *visitor_void, const char *line)
{
    struct printer_visitor *visitor = visitor_void;

    if (visitor->written < 0)
        return;    /* COVERAGE EXCLUSION - Test printer failure case */

    int written = (*visitor->printer)("%s", line);

    if (written >= 0)
        visitor->written += written;
    else
        visitor->written  = written;    /* COVERAGE EXCLUSION - Test printer failure case */
}

bool
kit_memory_log_stats(__printflike(1, 2) int (*printer)(const char *format, ...), const char *options)
{
    struct printer_visitor visitor;

    visitor.written = 0;
    visitor.printer = printer;
    malloc_stats_print(memory_stats_line, &visitor, options ?: "gblxe");
    return visitor.written > 0;
}
