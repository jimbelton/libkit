#include "kit-counters.h"
#include "kit-alloc.h"

#include <pthread.h>
#include <string.h>
#include <sxe-util.h>

#define INVALID_COUNTER    0
#define COUNTER_ISVALID(c) (((c) != INVALID_COUNTER) && ((c) <= num_counters))

#define COUNTER_DYNAMIC    1
#define COUNTER_STATIC     2
#define COUNTER_USED       4

static struct combine_handler {
    kit_counter_t counter;
    unsigned long long (*handler)(int);
} combine_handlers[MAXCOUNTERS];
static kit_mibfn_t mibfns[MAXCOUNTERS];
static kit_counter_t sorted_index[MAXCOUNTERS];
static char *counter_txt[MAXCOUNTERS];
static unsigned num_counters;
static unsigned num_handlers;

static struct kit_counters wrong_counter;
static struct kit_counters dead_thread_counter;

pthread_spinlock_t counter_lock;                  /* For updating maxthreads */
static unsigned maxthreads;                       /* How many threads */
static struct kit_counters **volatile all_counters;   /* Counters per thread */
static uint8_t *counter_state;                    /* COUNTER_DYNAMIC and/or COUNTER_USED bits per thread */

/*
 * Counter structure for local thread; defaults to &wrong_counter (which
 * isn't used by kit_counters_combine()) until kit_counters_init_thread() is called
 */
static __thread struct kit_counters *volatile thread_counter = &wrong_counter;
static struct kit_counters thread0_counter;

/*
 * Maintain an array of counter indexes in sorted (by txt value) order, so that sorted text
 * output can be easily produced (counters are not necessarily registered in sorted order).
 */
static void
add_to_sorted_index(kit_counter_t counter, const char *txt)
{
    unsigned i, x;

    for (x = 0; x < (num_counters - 1) && strcmp(txt, counter_txt[sorted_index[x]]) >= 0; x++)
        ;

    for (i = num_counters - 1; i > x && i > 0; i--)
        sorted_index[i] = sorted_index[i - 1];

    sorted_index[x] = counter;
}

/*
 * Register a new counter, along with optional combine handler and mibfn.
 */
static kit_counter_t
add_counter(const char *txt, unsigned long long (*combine_handler)(int), kit_mibfn_t mibfn)
{
    kit_counter_t counter = ++num_counters;

    SXEA1(counter < MAXCOUNTERS, "Counter %d will exceed MAXCOUNTERS (%d).", counter, MAXCOUNTERS);
    SXEA1(!counter_txt[counter], "Adding counter %d with value '%s' when it already has a value '%s'.", counter, txt, counter_txt[counter]);
    SXEA1(counter_txt[counter] = kit_strdup(txt), "Failed to allocate %zu bytes for counter text.", strlen(txt));
    add_to_sorted_index(counter, txt);
    if (combine_handler) {
        combine_handlers[num_handlers].counter = counter;
        combine_handlers[num_handlers].handler = combine_handler;
        num_handlers++;
    }
    mibfns[counter] = mibfn;

    return counter;
}

unsigned
kit_num_counters(void)
{
    return num_counters;
}

bool
kit_counter_isvalid(kit_counter_t c)
{
    return COUNTER_ISVALID(c);
}

kit_counter_t
kit_sorted_index(unsigned i)
{
    return sorted_index[i];
}

const char *
kit_counter_txt(kit_counter_t c)
{
    return COUNTER_ISVALID(c) ? counter_txt[c] : NULL;
}

/* Assign and return a new counter */
kit_counter_t
kit_counter_new(const char *txt)
{
    return add_counter(txt, NULL, NULL);
}

/* Assign and return a new counter, specifying a combine handler */
kit_counter_t
kit_counter_new_with_combine_handler(const char *txt, unsigned long long (*combine_handler)(int))
{
    return add_counter(txt, combine_handler, NULL);
}

/* Assign and return a new counter, specifying a mib function */
kit_counter_t
kit_counter_new_with_mibfn(const char *txt, kit_mibfn_t mibfn)
{
    return add_counter(txt, NULL, mibfn);
}

void
kit_counter_incr(kit_counter_t c)
{
    SXEA6(!all_counters || thread_counter != &wrong_counter, "%s(): Not initialized!", __FUNCTION__);
    if (COUNTER_ISVALID(c))
        thread_counter->val[c]++;
}

void
kit_counter_decr(kit_counter_t c)
{
    SXEA6(!all_counters || thread_counter != &wrong_counter, "%s(): Not initialized!", __FUNCTION__);
    if (COUNTER_ISVALID(c))
        thread_counter->val[c]--;
}

/* Initialize a counters structure */
void
kit_counters_initialize(unsigned slots)
{
    unsigned i;

    SXEE6("(slots=%u)", slots);

    SXEA1(!counter_state, "%s(): Already initialized (counter_state set)!", __FUNCTION__);
    SXEA1(!all_counters, "%s(): Already initialized (all_counters set)!", __FUNCTION__);
    SXEA1(thread_counter == &wrong_counter, "%s(): Already initialized (thread_counter set)!", __FUNCTION__);
    SXEA1(slots, "At least one counter slot is required");

    pthread_spin_init(&counter_lock, PTHREAD_PROCESS_PRIVATE);

    maxthreads = slots;

    SXEA1(counter_state = kit_malloc(maxthreads * sizeof(*counter_state)),
          "Failed to allocate %zu bytes for per-thread counter_state", maxthreads * sizeof(*counter_state));
    for (i = 0; i < maxthreads; i++)
        counter_state[i] = COUNTER_STATIC;

    SXEA1(all_counters = kit_malloc(maxthreads * sizeof(*all_counters)),
          "Failed to allocate %zu bytes for per-thread counter block pointers", maxthreads * sizeof(*all_counters));

    all_counters[0] = &thread0_counter;    /* calloc will use this, so we can't calloc it!! */
    kit_counters_init_thread(0);

    for (i = 1; i < maxthreads; i++)
        SXEA1(all_counters[i] = kit_calloc(maxthreads, sizeof(*all_counters[i])),
              "Failed to allocate %zu bytes for a thread counter block", maxthreads * sizeof(*all_counters[i]));

    memset(&wrong_counter, '\0', sizeof(wrong_counter));

    SXER6("return");
}

#if SXE_DEBUG
bool
kit_counters_usable(void)
{
    return !all_counters || thread_counter != &wrong_counter;
}
#endif

/* Create a combined structure of the counters for 'threadnum', or all threads if threadnm is -1 */
void
kit_counters_combine(struct kit_counters *out_counter, int threadnum)
{
    unsigned from, i, n, to;

    SXEA6(all_counters && out_counter, "Can't combine counters that aren't initialized");

#if SXE_DEBUG
    for (n = 1; n <= num_counters; n++) /* counter 0 is invalid */
        SXEA6(wrong_counter.val[n] == 0, "Oops, wrong-counter %u is not zero (got %llu)", n, wrong_counter.val[n]);
#endif

    from = threadnum == -1 ? 0 : (unsigned)threadnum;
    to = threadnum == -1 ? maxthreads : (unsigned)threadnum < maxthreads ? (unsigned)threadnum + 1 : maxthreads;

    /* Total up every field */
    pthread_spin_lock(&counter_lock);
    for (i = from; i < to; i++)                    /* main, fastlog & graphitelog threads are pos 0 */
        if (counter_state[i] & COUNTER_USED)
            for (n = 1; n <= num_counters; n++)    /* counter 0 is invalid */
                out_counter->val[n] += all_counters[i]->val[n];
    if (threadnum == -1)
        for (n = 1; n <= num_counters; n++)        /* counter 0 is invalid */
            out_counter->val[n] += dead_thread_counter.val[n];
    pthread_spin_unlock(&counter_lock);

    /* Update "special" fields that are not continuously updated */
    for (i = 0; i < num_handlers; i++)
        out_counter->val[combine_handlers[i].counter] = combine_handlers[i].handler(threadnum);
}

/* Set the per-thread pointer to a counter structure */
void
kit_counters_init_thread(unsigned slot)
{
    SXEA6(counter_state, "Counters not yet initialized");
    SXEA1(slot < maxthreads, "thread initialized as slot %u, but slot_count is %u", slot, maxthreads);
    SXEA1(!(counter_state[slot] & COUNTER_USED), "thread initialized as slot %u, but that slot is already in use", slot);
    thread_counter = all_counters[slot];
    counter_state[slot] |= COUNTER_USED;
}

void
kit_counters_fini_thread(unsigned slot)
{
    SXEA6(counter_state, "Counters not yet initialized");
    SXEA1(slot < maxthreads, "thread finailized at slot %u, but slot_count is %u", slot, maxthreads);
    SXEA1(counter_state[slot] & COUNTER_USED, "thread finalized at slot %u, but that slot isn't in use", slot);
    SXEA1(thread_counter == all_counters[slot], "thread finalized at wrong slot %u", slot);
    kit_counters_combine(&dead_thread_counter, slot);
    memset(thread_counter, '\0', sizeof(*thread_counter));
    thread_counter = &wrong_counter;
    counter_state[slot] &= ~COUNTER_USED;
}

unsigned long long
kit_counter_get_data(kit_counter_t c, int threadnum)
{
    unsigned long long out_counter = 0;
    unsigned from, i, to;

    SXEA6(!all_counters || thread_counter != &wrong_counter, "%s(): Not initialized!", __FUNCTION__);

    if (COUNTER_ISVALID(c)) {
        if (all_counters) {
            from = threadnum == -1 ? 0 : (unsigned)threadnum;
            to = threadnum == -1 ? maxthreads : (unsigned)threadnum < maxthreads ? (unsigned)threadnum + 1 : maxthreads;
            pthread_spin_lock(&counter_lock);
            for (i = from; i < to; i++)
                if (counter_state[i] & COUNTER_USED)
                    out_counter += all_counters[i]->val[c];
            if (threadnum == -1)
                out_counter += dead_thread_counter.val[c];
            pthread_spin_unlock(&counter_lock);
        } else
            out_counter += thread_counter->val[c];

        /* Update if we're a "special" field */
        for (i = 0; i < num_handlers; i++)
            if (combine_handlers[i].counter == c) {
                out_counter = combine_handlers[i].handler(threadnum);
                break;
            }
    }

    return out_counter;
}

void
kit_counter_add(kit_counter_t c, unsigned long long value)
{
    SXEA6(!all_counters || thread_counter != &wrong_counter, "%s(): Not initialized!", __FUNCTION__);
    if (COUNTER_ISVALID(c))
        thread_counter->val[c] += value;
}

unsigned long long
kit_counter_get(kit_counter_t c)
{
    return kit_counter_get_data(c, -1);
}

void
kit_counter_zero(kit_counter_t c)
{
    SXEA6(!all_counters || thread_counter != &wrong_counter, "%s(): Not initialized!", __FUNCTION__);
    if (COUNTER_ISVALID(c))
        thread_counter->val[c] = 0;
}

unsigned
kit_counters_init_dynamic_thread(void)
{
    unsigned slot;

    SXEA6(counter_state, "Counters not yet initialized");

    pthread_spin_lock(&counter_lock);
    for (slot = 0; slot < maxthreads; slot++)
        if (counter_state[slot] == COUNTER_DYNAMIC)
            break;
    SXEA1(slot < maxthreads, "Cannot locate a dynamic thread slot");
    thread_counter = all_counters[slot];
    counter_state[slot] |= COUNTER_USED;
    pthread_spin_unlock(&counter_lock);

    memset(thread_counter, '\0', sizeof(*thread_counter));

    return slot;
}

void
kit_counters_prepare_dynamic_threads(unsigned count)
{
    struct kit_counters **ncounters, **ocounters;
    uint8_t *nstate, nthreads, *ostate;
    unsigned done, i;

    SXEA6(counter_state, "Counters not yet initialized");

    if (count) {
        pthread_spin_lock(&counter_lock);
        for (done = i = 0; i < maxthreads && done < count; i++)
            if (!counter_state[i]) {
                counter_state[i] = COUNTER_DYNAMIC;
                done++;
            }
        pthread_spin_unlock(&counter_lock);

        while (done < count) {
            nthreads = maxthreads + count - done;
            SXEA1(nstate = kit_malloc(nthreads * sizeof(*nstate)),
                  "Failed to allocate %zu bytes for per-thread counter_state", nthreads * sizeof(*nstate));
            SXEA1(ncounters = kit_malloc(nthreads * sizeof(*ncounters)),
                  "Failed to allocate %zu bytes for per-thread counter block pointers", nthreads * sizeof(*ncounters));
            for (i = maxthreads; i < nthreads; i++) {
                nstate[i] = COUNTER_DYNAMIC;
                SXEA1(ncounters[i] = kit_malloc(sizeof(*ncounters[i])),
                      "Failed to allocate %zu bytes for a thread counter block", sizeof(*ncounters[i]));
            }
            ocounters = NULL;
            ostate = NULL;
            pthread_spin_lock(&counter_lock);
            if (maxthreads == nthreads - count + done) {
                /* Good, nothing's changed!  There should only be one thing calling us anyway */
                memcpy(ncounters, all_counters, maxthreads * sizeof(*ncounters));
                memcpy(nstate, counter_state, maxthreads * sizeof(*nstate));
                ostate = counter_state;
                ocounters = all_counters;
                counter_state = nstate;
                all_counters = ncounters;
                done += nthreads - maxthreads;
                maxthreads = nthreads;
            }
            pthread_spin_unlock(&counter_lock);
            kit_free(ocounters);
            kit_free(ostate);
        }
    }
}

void
kit_counters_fini_dynamic_thread(unsigned slot)
{
    SXEA6(counter_state, "Counters not yet initialized");
    SXEA1(slot < maxthreads, "thread finalized as slot %u, but slot_count is %u", slot, maxthreads);
    SXEA1(counter_state[slot] == (COUNTER_USED|COUNTER_DYNAMIC), "thread finalized as slot %u, but that slot is not dynamic and in use", slot);
    kit_counters_combine(&dead_thread_counter, slot);
    thread_counter = &wrong_counter;
    counter_state[slot] = 0;
}

bool
kit_mibintree(const char *tree, const char *mib)
{
    size_t len = strlen(tree);

    return memcmp(tree, mib, len) == 0 && (len == 0 || mib[len] == '\0' || mib[len] == '.');
}

void
kit_counters_mib_text(const char *subtree, void *v, kit_counters_mib_callback_t cb, int threadnum, unsigned cflags)
{
    struct kit_counters counter_totals;
    unsigned long long val;
    const char *name;
    kit_mibfn_t mibfn;
    char buf[100];
    kit_counter_t c;
    unsigned i;

    SXEE6("(subtree=%s,v=%p,cb=%p,threadnum=%d)", subtree, v, cb, threadnum);

    memset(&counter_totals, '\0', sizeof(counter_totals));
    kit_counters_combine(&counter_totals, threadnum);

    for (i = 0; i < kit_num_counters(); i++) {    /* sorted index array starts at 0 */
        c = kit_sorted_index(i);
        SXEA6(kit_counter_isvalid(c), "Invalid counter %u at index %u", c, i);

        val = counter_totals.val[c];
        name = kit_counter_txt(c);
        mibfn = mibfns[c];

        if (mibfn) {
            /* If the mib isn't part of the tree, is the tree in part of the mib?  If so, it *may* match */
            if (kit_mibintree(subtree, name) || kit_mibintree(name, subtree))
                mibfn(c, subtree, name, v, cb, threadnum, cflags);
        } else if (kit_mibintree(subtree, name)) {
            snprintf(buf, sizeof(buf), "%llu", val);
            cb(v, name, buf);
        }
    }

    SXER6("return");
}
