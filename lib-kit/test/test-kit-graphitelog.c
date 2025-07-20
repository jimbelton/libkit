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

#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <tap.h>

#include "kit.h"
#include "kit-counters.h"
#include "kit-graphitelog.h"


#define INTERVAL 2

static const char *graphite_log_file = "graphite_log_file";
static kit_counter_t COUNTER;
static bool thread_started;

static void
started(void)
{
    thread_started = true;
}

int
main(void)
{
    struct kit_graphitelog_thread gthr;
    unsigned long                 lastval, val;
    const char                   *eor, *p;
    ssize_t                       read_bytes;
    pthread_t                     thread;
    unsigned                      i, rec;
    char                          buf[4096];

    plan_tests(11);

    // Initializer counters
    kit_counters_initialize(KIT_COUNTERS_MAX, 2, false);
    COUNTER = kit_counter_reg("counter");
    ok(kit_counter_isvalid(COUNTER), "Created counter");
    is(kit_counter_get(COUNTER), 0, "Counter was initialized to zero");

    // Initialize the graphite log with a ridiculous interval so that nothing will ever get logged
    kit_graphitelog_update_set_options(2, ~0U, 1000);

    // Open the graphitelog output file, emptying it if it exists because it might be left from a previous run
    gthr.fd = open(graphite_log_file, O_CREAT | O_NONBLOCK | O_RDWR | O_TRUNC, 0644);
    ok(gthr.fd >= 0, "Successfully created %s", graphite_log_file);

    // Create the graphitelog thread
    gthr.counter_slot = 1;
    gthr.started = started;
    is(pthread_create(&thread, NULL, kit_graphitelog_start_routine, &gthr), 0, "Successfully created graphitelog thread");
    for (i = 0; i < 10 && !thread_started; i++)
        usleep(1000);
    is(thread_started, true, "The graphitelog thread has started");

    // Reinitialize the graphite log configuration, with a low json limit to ensure that the splitting code is exercised
    kit_graphitelog_update_set_options(2, INTERVAL, 1000);

    // Update the counter and wait to give the graphite log time to output
    kit_counter_add(COUNTER, 5);
    usleep(1000000 * INTERVAL * 3 + 1000000);

    // Terminate the graphitelog thread
    kit_graphitelog_terminate();
    pthread_join(thread, NULL);

    // Return to the beginning of the file, read it and verify the expected output is present
    lseek(gthr.fd, 0, SEEK_SET);
    ok((read_bytes = read(gthr.fd, buf, sizeof(buf))) > 0, "Read graphitelog output");
    buf[read_bytes] = '\0';
    ok(strstr(buf, "\"counter\":\"5\""), "Found expected counters in graphitelog");

#define TSDATA "\"log.timestamp\":\""
    diag("Checking timestamps, but skipping the first (immediate) and last (terminated) records");

    for (lastval = 0, p = buf, rec = 0; p && *p; p = eor) {
        if (*p == '{') {
            if ((eor = strchr(p, '}')) != NULL)
                eor += strspn(eor + 1, "\r\n") + 1;

            p = strstr(p, TSDATA);
            val = p && eor && p < eor ? kit_strtoul(p + sizeof(TSDATA) - 1, NULL, 10) : 0;

            if (!val) {
                fail("Record %u: Invalid Record", ++rec);
            } else if (val != lastval) {
                if (lastval) {
                    if (++rec > 1) {
                        if (rec < 4)
                            is(lastval % INTERVAL, INTERVAL / 2, "Record %u: timestamp %lu is at a half-interval", rec, lastval);
                        else if (lastval % INTERVAL != INTERVAL / 2)
                            fail("Record %u: timestamp %lu is not at a half-interval", rec, lastval);
                    }
                    else
                        pass("Record %u: timestamp %lu could be anything (first record)", rec, lastval);
                }

                lastval = val;
            }

        } else {
            fail("Record %u: Invalid Record", rec);
            break;
        }
    }

    pass("Record %u: timestamp %lu could be anything (last record)", ++rec, lastval);
    close(gthr.fd);
    return exit_status();
}
