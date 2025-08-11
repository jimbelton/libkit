#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/time.h>    // Hack: Include before sxe-hash.h because <kit/kit-timestamp.h> does include <sys/time.h>
//#include <sxe-hash.h>
//#include <zlib.h>

//#include "conf-loader.h"
//#include "netsock.h"
//#include "networks.h"
#include "kit-alloc.h"
#include "sxe-dict.h"

//static char corpus_relative_path[] = "../../corpra/networks-all";
//static char buffer[65536];    // Some lines are > 8192, so use a really big buffer
static char *corpus[10000000];

// Should move this to a generic "bench.c" once we have more than one benchmark
static uint64_t
usec_elapsed(struct timeval *start, struct timeval *end)
{
    struct timeval end_time;

    if (end == NULL) {
        end = &end_time;
        assert(gettimeofday(end, NULL) == 0);
    }

    return (uint64_t)(end->tv_sec - start->tv_sec) * 1000000 + end->tv_usec - start->tv_usec;
}

int
main(int argc, char **argv)
{
    struct sxe_dict    dict;
    const void       **value;
    struct timeval     start_time;
    char              *end;
    size_t             start_mem;
    unsigned long      count, i;
    char               name[PATH_MAX];
    bool               use_hashes = false;

    count = sizeof(corpus) / sizeof(corpus[0]);    // Default to preallocation

    while (argc > 1) {
        if (strcmp(argv[1], "-c") == 0) {
            assert(argc > 2);
            argv += 1;
            argc -= 1;
            count = strtoul(argv[0], &end, 10);
        }
        else if (strcmp(argv[1], "-h") == 0) {
            assert(argc > 1);
            use_hashes = true;
        } 
        else {
            fprintf(stderr, "usage: kit-dict-bench [-c <initial-count>] [-h]\nerror: invalid argument '%s'\n", argv[1]);
            exit(1);
        }

        argv += 1;
        argc -= 1;
    }

    for (i = 0; i < sizeof(corpus) / sizeof(corpus[0]); i++) {
        snprintf(name, sizeof(name), "match_variable_%"PRIx64, i);
        corpus[i] = strdup(name);
    }

    start_mem = kit_allocated_bytes();
    assert(gettimeofday(&start_time, NULL) == 0);
    sxe_dict_init(&dict, count, 100, 2, use_hashes ? SXE_DICT_FLAG_KEYS_HASHED : SXE_DICT_FLAG_KEYS_NOCOPY);
    count = sizeof(corpus) / sizeof(corpus[0]);

    for (i = 0; i < count; i++) {
        assert((value = sxe_dict_add(&dict, corpus[i], 0)));
        *value = (void *)i;
    }

    printf("Construction Duration: %"PRIu64" usec\n", usec_elapsed(&start_time, NULL));
    printf("Memory Allocated: %zu bytes\n", kit_allocated_bytes() - start_mem);

    /* Look all entries up, benchmarking the time
     */
    assert(gettimeofday(&start_time, NULL) == 0);

    for (i = 0; i < count; i++)
        assert(sxe_dict_find(&dict, corpus[i], 0) == (void *)i);

    printf("Search Duration: %"PRIu64" usec\n", usec_elapsed(&start_time, NULL));
    return 0;
}
