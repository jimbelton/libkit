#include "kit-mock.h"
#include "kit-alloc.h"

#include <string.h>
#include <sxe-util.h>
#include <jemalloc.h>

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

struct kit_memory_counters kit_memory_counters; /* global */

static unsigned long long
counter_bytes_combine_handler(int threadnum)
{
    return threadnum <= 0 ? kit_allocated_bytes() : 0;
}

void
kit_memory_counters_init(void)
{
    kit_memory_counters.bytes = kit_counter_new_with_combine_handler("memory.bytes", counter_bytes_combine_handler);
    kit_memory_counters.calloc = kit_counter_new("memory.calloc");
    kit_memory_counters.fail = kit_counter_new("memory.fail");
    kit_memory_counters.free = kit_counter_new("memory.free");
    kit_memory_counters.malloc = kit_counter_new("memory.malloc");
    kit_memory_counters.realloc = kit_counter_new("memory.realloc");
}

bool
kit_memory_counters_initialized(void)
{
    return kit_counter_isvalid(kit_memory_counters.calloc);
}

#if SXE_DEBUG
int kit_alloc_diagnostics;
#define KIT_ALLOC_LOG(...) do { if (kit_alloc_diagnostics) SXEL6(__VA_ARGS__); } while (0)
#else
#define KIT_ALLOC_LOG(...) do { } while (0)
#endif

__attribute__((malloc)) void *
KIT_ALLOC_MANGLE(kit_malloc)(size_t size KIT_ALLOC_SOURCE_PROTO)
{
    void *result = je_malloc(size);

    kit_counter_incr(KIT_COUNTER_MEMORY_MALLOC);
    if (result == NULL)
        kit_counter_incr(KIT_COUNTER_MEMORY_FAIL);    /* COVERAGE EXCLUSION: todo: mock malloc() to create failure */
    KIT_ALLOC_LOG("%s: %u: %p = kit_malloc(%zu)", file, line, result, size);

    return result;
}

__attribute__((malloc)) void *
KIT_ALLOC_MANGLE(kit_calloc)(size_t num, size_t size KIT_ALLOC_SOURCE_PROTO)
{
    void *result = je_calloc(num, size);

    kit_counter_incr(KIT_COUNTER_MEMORY_CALLOC);
    if (result == NULL)
        kit_counter_incr(KIT_COUNTER_MEMORY_FAIL);    /* COVERAGE EXCLUSION: todo: mock calloc() to create failure */
    KIT_ALLOC_LOG("%s: %u: %p = kit_calloc(%zu, %zu)", file, line, result, num, size);

    return result;
}

void
KIT_ALLOC_MANGLE(kit_free)(void *ptr KIT_ALLOC_SOURCE_PROTO)
{
    KIT_ALLOC_LOG("%s: %u: kit_free(%p)", file, line, ptr);
    if (ptr) {
        kit_counter_incr(KIT_COUNTER_MEMORY_FREE);
        SXEA6(!(((long)ptr) & 7), "ungranular free(%p)", ptr);
    }

    je_free(ptr);
}

/*-
 * Special case for realloc() since it can behave like malloc()
 * or free() !
 */
void *
KIT_ALLOC_MANGLE(kit_realloc)(void *ptr, size_t size KIT_ALLOC_SOURCE_PROTO)
{
    void *result = je_realloc(ptr, size);

    if (ptr == NULL && result != NULL)
        kit_counter_incr(KIT_COUNTER_MEMORY_MALLOC);
    else if (size == 0 && result == NULL)
        kit_counter_incr(KIT_COUNTER_MEMORY_FREE);
    else
        kit_counter_incr(KIT_COUNTER_MEMORY_REALLOC);

    if (result == NULL && (size || ptr == NULL))
        kit_counter_incr(KIT_COUNTER_MEMORY_FAIL);    /* COVERAGE EXCLUSION: todo: mock realloc() to create failure */
    KIT_ALLOC_LOG("%s: %u: %p = kit_realloc(%p, %zu)", file, line, result, ptr, size);

    return result;
}

void *
KIT_ALLOC_MANGLE(kit_reduce)(void *ptr, size_t size KIT_ALLOC_SOURCE_PROTO)
{
    void *result;

    /*-
     * We're aiming for two things here:
     * - Don't turn realloc(NULL, 0) into malloc(0)
     * - If realloc() fails, return the original
     */
    if (ptr) {
        if ((result = MOCKFAIL(KIT_REDUCE_REALLOC, NULL, je_realloc(ptr, size))) == NULL) {
            if (size == 0)
                kit_counter_incr(KIT_COUNTER_MEMORY_FREE);
            else {
                kit_counter_incr(KIT_COUNTER_MEMORY_FAIL);
                result = ptr;
            }
        }
        if (result != ptr)
            KIT_ALLOC_LOG("%s: %u: %p = kit_realloc(%p, %zu)", file, line, result, ptr, size);
    } else {
        SXEA1(!size, "Cannot kit_reduce() NULL to size %zu", size);
        result = NULL;
    }

    return result;
}

__attribute__((malloc)) char *
KIT_ALLOC_MANGLE(kit_strdup)(const char *txt KIT_ALLOC_SOURCE_PROTO)
{
    size_t len = strlen(txt);
    void *result = je_malloc(len + 1);

    kit_counter_incr(KIT_COUNTER_MEMORY_MALLOC);
    if (result == NULL)
        kit_counter_incr(KIT_COUNTER_MEMORY_FAIL);    /* COVERAGE EXCLUSION: todo: mock malloc() to create failure */
    else
        strcpy(result, txt);
    KIT_ALLOC_LOG("%s: %u: %p = kit_strdup(%p[%zu])", file, line, result, txt, len + 1);

    return result;
}

#if SXE_DEBUG
/*-
 * The debug version of jemalloc is built with --enable-debug and
 * --enable-fill.  We enable junk and redzone by default so that we
 * detect memory underflows and some overflows.
 *
 * It's worth noting that this triggers an assert():
 *     char *ptr = je_malloc(8);
 *     ptr[8] = 'x';
 *     je_free(ptr);
 *
 * But this doesn't, due to the rounding that jemalloc does:
 *     char *ptr = je_malloc(3);
 *     ptr[3] = 'x';
 *     je_free(ptr);
 */
const char *je_malloc_conf = "junk:true,redzone:true";
#endif

size_t
kit_allocated_bytes()
{
    size_t len, alloc;
    uint64_t epoch;

    epoch = 1;
    len = sizeof(epoch);
    je_mallctl("epoch", &epoch, &len, &epoch, len);

    len = sizeof(alloc);
    je_mallctl("stats.allocated", &alloc, &len, NULL, 0);

    return alloc;
}

uint64_t
kit_thread_allocated_bytes(void)
{
    static __thread uint64_t *allocatedp, *deallocatedp;
    size_t len;

    len = sizeof(allocatedp);
    if (!allocatedp) {
        je_mallctl("thread.allocatedp", &allocatedp, &len, NULL, 0);
        SXEA1(allocatedp, "Couldn't obtain thread.allocatedp, is jemalloc built with --enable-stats?");
    }

    len = sizeof(deallocatedp);
    if (!deallocatedp) {
        je_mallctl("thread.deallocatedp", &deallocatedp, &len, NULL, 0);
        SXEA1(deallocatedp, "Couldn't obtain thread.deallocatedp, is jemalloc built with --enable-stats?");
    }

    return *allocatedp - *deallocatedp;
}
