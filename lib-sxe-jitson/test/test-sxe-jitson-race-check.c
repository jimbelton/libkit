#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <tap.h>

#include "sxe-jitson.h"
#include "sxe-util.h"

static volatile unsigned  next_main;
static volatile unsigned  next_worker_1;
static volatile unsigned  next_worker_2;
static struct sxe_jitson *jitson;
static bool               test_objects = false;

#if SXE_DEBUG
static unsigned           max_iter = 99999;
#else
static unsigned           max_iter = 999999;
#endif

static void *
worker(void *next_void)
{
    unsigned           i, last = 0;
    volatile unsigned *next = next_void;

    for (i = 1; i <= max_iter; i++) {
        while (last >= next_main)
            sched_yield();

        if (test_objects)
            sxe_jitson_object_get_member(jitson, "one", 3);
        else
            sxe_jitson_array_get_element(jitson, 1);

        last  = next_main;
        *next = next_main;
    }

    return NULL;
}

int
main(int argc, char **argv)
{
    pthread_t worker1, worker2;
    unsigned  i;


    if (argc > 1)
        if (strncmp(argv[1], "-o", 2) == 0)
            test_objects = true;

    plan_tests(2);
    sxe_jitson_initialize(8, 0);

    assert(pthread_create(&worker1, NULL, &worker, SXE_CAST(void *, &next_worker_1)) == 0);
    assert(pthread_create(&worker2, NULL, &worker, SXE_CAST(void *, &next_worker_2)) == 0);

    for (i = 1; i <= max_iter; i++) {
        if (test_objects)
            assert((jitson = sxe_jitson_new("{\"one\": 1, \"two\": 2}")));
        else
            assert((jitson = sxe_jitson_new("[1, \"I'm a long string\"]")));

        next_main++;

        while (next_worker_1 < next_main)
           sched_yield();

        while (next_worker_2 < next_main)
           sched_yield();

        sxe_jitson_free(jitson);
    }

    is(pthread_join(worker1, NULL), 0, "Joined worker1");
    is(pthread_join(worker2, NULL), 0, "Joined worker2");

    sxe_jitson_finalize();
    return exit_status();
}
