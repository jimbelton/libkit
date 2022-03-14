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

const char *graphite_log_file = "graphite_log_file";

kit_counter_t COUNTER;

int
main(void)
{
    struct kit_graphitelog_thread gthr;
    pthread_t thread;
    char buf[4096];
    ssize_t read_bytes;

    plan_tests(7);

    // Initializer counters
    kit_counters_initialize(MAXCOUNTERS, 2, false);
    COUNTER = kit_counter_new("counter");
    ok(kit_counter_isvalid(COUNTER), "Created counter");
    is(kit_counter_get(COUNTER), 0, "Counter was initialized to zero");

    // Initialize the graphite log configuration, with a low json limit to ensure that the splitting code is exercised
    kit_graphitelog_update_set_options(2, INTERVAL);

    // Open the graphitelog output file
    gthr.fd = open(graphite_log_file, O_CREAT | O_NONBLOCK | O_RDWR, 0644);
    ok(gthr.fd >= 0, "Successfully created %s", graphite_log_file);

    // Create the graphitelog thread
    gthr.counter_slot = 1;
    is(pthread_create(&thread, NULL, kit_graphitelog_start_routine, &gthr), 0, "Successfully created graphitelog thread");

    // Update the counter and wait to give the graphite log time to output
    kit_counter_add(COUNTER, 5);
    usleep(1000000 * INTERVAL + 100000);

    // Terminate the graphitelog thread
    kit_graphitelog_terminate();
    pthread_join(thread, NULL);

    // Return to the beginning of the file, read it and verify the expected output is present
    lseek(gthr.fd, 0, SEEK_SET);
    ok((read_bytes = read(gthr.fd, buf, sizeof(buf))) > 0, "Read graphitelog output");
    buf[read_bytes] = '\0';
    ok(strstr(buf, "\"counter\":\"5\""), "Found expected counters in graphitelog");

    close(gthr.fd);

    ok(remove(graphite_log_file) == 0, "Successfully removed %s", graphite_log_file);

    return exit_status();
}
