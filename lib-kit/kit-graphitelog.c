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

/*
 * This module takes all defined counters (see kit-counters.c) and outputs their
 * values to a file on at a regular interval, in a format that can be sent to a
 * graphite instance for monitoring.
 */

#include <stdio.h>
#include <sxe-log.h>
#include <time.h>

#include "kit.h"
#include "kit-counters.h"
#include "kit-safe-rw.h"
#include "kit-graphitelog.h"

static __thread int      graphitelog_fd = -1;
static volatile unsigned graphitelog_json_limit;
static volatile unsigned graphitelog_interval;
static volatile unsigned graphitelog_timeout_ms = -1;
static volatile bool     timetodie;

struct kit_graphitelog_buffer {
    char buf[INT16_MAX];
    int pos;                /* position in the buffer */
    int counter;            /* the current position of element in current log. */
    time_t now;             /* the timestamp for current log iteration */
    uint8_t json_complete;  /* Flag indicating whether '}' is added to end of json */
};

static void
kit_graphitelog_complete(struct kit_graphitelog_buffer *buffer)
{
    if (!buffer->json_complete) {
        if (buffer->pos < (int)sizeof(buffer->buf))
            buffer->pos += snprintf(buffer->buf + buffer->pos, sizeof(buffer->buf) - buffer->pos, "}\n");

        if (buffer->pos >= (int)sizeof(buffer->buf))
            SXEL3("graphitelog buffer overflow - graphite data has been truncated and is invalid");    /* COVERAGE EXCLUSION: Not possible to overflow with current stats */

        buffer->json_complete = 1;
        kit_safe_write(graphitelog_fd, buffer->buf, (size_t)buffer->pos, graphitelog_timeout_ms);
    }
}

static void
kit_graphitelog_counter_callback(void *v, const char *key, const char *value)
{
    struct kit_graphitelog_buffer *buffer = v;

    /* Start a new json object */
    if (buffer->counter % graphitelog_json_limit == 0) {
        buffer->pos = snprintf(buffer->buf, sizeof(buffer->buf), "{\"log.timestamp\":\"%lu\"", (unsigned long)buffer->now);
        SXEA1(buffer->pos < (int)sizeof(buffer->buf), "Internal error: buffer is unusably small!");
        buffer->json_complete = 0;
    }

    if (buffer->pos < (int)sizeof(buffer->buf)) {
        buffer->pos += snprintf(buffer->buf + buffer->pos, sizeof(buffer->buf) - buffer->pos, ",\"%s\":\"%s\"", key, value);
        buffer->counter++;
    }

    /* If the limit is reached (or the buffer has overflowed), send the (possibly truncated) JSON object */
    if (buffer->counter % graphitelog_json_limit == 0 || buffer->pos >= (int)sizeof(buffer->buf)) {
        kit_graphitelog_complete(buffer);
    }
}

/**
 * Set or update the configurable options
 *
 * @param json_limit  Maximum number of counters in a single json line
 * @param interval    Seconds between outputting counters to the graphite log
 * @param interval    Milliseconds to wait on poll() while writing for the graphite log

 */
void
kit_graphitelog_update_set_options(unsigned json_limit, unsigned interval, unsigned timeout_ms)
{
    SXEL6("(json_limit=%u,interval=%u, timeout_ms=%u)", json_limit, interval, timeout_ms);
    graphitelog_json_limit = json_limit;
    graphitelog_interval   = interval;
    graphitelog_timeout_ms = timeout_ms;
}

/**
 * Launch the graphite logging thread
 *
 * @param arg Pointer to a struct kit_graphitelog_thread containing the log file descriptor and counter slot.
 */
void *
kit_graphitelog_start_routine(void *arg)
{
    uint64_t                             interval_ns, sleep_ns, wall_ns;
    const struct kit_graphitelog_thread *thr = arg;
    struct kit_graphitelog_buffer        buffer;
    struct timespec                      wall_time, delay_time;

    SXEL4("(): thread started");
    kit_counters_init_thread(thr->counter_slot);
    if (thr->started)
        thr->started();
    delay_time.tv_sec = 0;
    graphitelog_fd    = thr->fd;
    sleep_ns          = 0;          // Shut gcc up
    SXEL6("Graphitelog is %s", graphitelog_fd >= 0 ? "enabled" : "disabled");

    for (;;) {
        SXEA1(graphitelog_interval,                           "No configuration acquired; cannot run graphitelog thread");
        SXEA1(clock_gettime(CLOCK_REALTIME, &wall_time) == 0, "Can't get the wall clock time");
        buffer.now = wall_time.tv_sec;

        if (graphitelog_fd >= 0) {
            buffer.counter = 0;
            kit_counters_mib_text("", &buffer, kit_graphitelog_counter_callback, -1, KIT_COUNTERS_FLAG_NONE);
            kit_graphitelog_complete(&buffer);
        }

        while (!timetodie) {
            interval_ns = graphitelog_interval * 1000000000ULL;
            SXEA1(clock_gettime(CLOCK_REALTIME, &wall_time) == 0, "Can't get the wall clock time");
            wall_ns  = wall_time.tv_sec * 1000000000ULL + wall_time.tv_nsec;
            sleep_ns = interval_ns - (wall_ns + interval_ns / 2) % interval_ns;

            if (sleep_ns < 1000000000ULL)
                break;

            /* Sleep for at most 3/4 second at a time so that we wake up reasonably quickly when it's time to die
             */
            delay_time.tv_nsec = 750000000;
            nanosleep(&delay_time, NULL);
        }

        if (timetodie)
            break;

        delay_time.tv_nsec = sleep_ns;
        nanosleep(&delay_time, NULL);
    }

    SXEL4(": thread exiting");
    return NULL;
}

/**
 * Signal the graphitelog thread to gracefully terminate
 *
 * @note This initiates the termination, but it happens asynchronously. thread_join the thread to synchronize.
 */
void
kit_graphitelog_terminate(void)
{
    timetodie = true;
}
