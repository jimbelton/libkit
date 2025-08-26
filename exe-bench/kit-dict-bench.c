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
    //struct conf_loader confloader;
    //FILE              *input;
    char              *end;
    //struct netaddr    *ips, *next;
    size_t             start_mem;
    //long               , mask;
    unsigned long      count, i;
    char               name[PATH_MAX];

    count = sizeof(corpus) / sizeof(corpus[0]);    // Default to preallocation

    while (argc > 1) {
        if (strcmp(argv[1], "-c") == 0) {
            assert(argc > 2);
            argv += 2;
            argc -= 2;
            count = strtoul(argv[0], &end, 10);
        } else {
            fprintf(stderr, "usage: kit-dict-bench [-c <initial-count>]\nerror: invalid argument '%s'\n", argv[1]);
            exit(1);
        }
    }

    for (i = 0; i < sizeof(corpus) / sizeof(corpus[0]); i++) {
        snprintf(name, sizeof(name), "match_variable_%"PRIx64, i);
        corpus[i] = strdup(name);
    }

    //start_mem = kit_allocated_bytes();
    //assert(gettimeofday(&start_time, NULL) == 0);

    //sxe_hash_use_xxh32();

    //char *last_slash = strrchr(argv[0], '/');
    //assert(last_slash);
    //assert(sizeof(corpus) >= last_slash - argv[0] + 1 + sizeof(corpus_relative_path));
    //memcpy(corpus, argv[0], last_slash - argv[0] + 1);
    //strcpy(&corpus[last_slash - argv[0] + 1], corpus_relative_path);

    //conf_loader_init(&confloader);
    //assert(conf_loader_open(&confloader, corpus, NULL, NULL, 0, CONF_LOADER_DEFAULT));
    start_mem = kit_allocated_bytes();
    assert(gettimeofday(&start_time, NULL) == 0);
    sxe_dict_init(&dict, count, 100, 2, SXE_DICT_FLAG_KEYS_NOCOPY);
    count = sizeof(corpus) / sizeof(corpus[0]);

    for (i = 0; i < count; i++) {
        assert((value = sxe_dict_add(&dict, corpus[i], 0)));
        *value = (void *)i;
    }

    printf("Construction Duration: %"PRIu64" usec\n", usec_elapsed(&start_time, NULL));
    printf("Memory Allocated: %zu bytes\n", kit_allocated_bytes() - start_mem);

    //struct networks *networks_all = networks_new(&confloader);
    //assert(networks_all);


    ///* Get the list of IP addresses, reading them from networks-all
     //*/

    //assert((input = fopen(corpus_relative_path + 3, "r")));
    //assert(fgets(buffer, sizeof(buffer), input) != NULL);        // Skip the type/version
    //assert(fgets(buffer, sizeof(buffer), input) != NULL);        // Read the count line
    //assert((end   = strchr(buffer, ' ')));
    //assert((count = strtol(end + 1, NULL, 10)) != LONG_MAX);
    //assert((ips   = malloc(count * sizeof(*ips))));
    //assert(fgets(buffer, sizeof(buffer), input) != NULL);        // Skip the section heading

    //for (next = ips, i = 0; i < count; i++) {
        //assert(fgets(buffer, sizeof(buffer), input) != NULL);
        //assert((end  = strchr(buffer, '/')));
        //assert((mask = strtol(end + 1, NULL, 10)) != LONG_MAX);

        //if (mask == 32 && strchr(buffer, '.')) {
            //assert(netaddr_from_buf(next, buffer, end - buffer, AF_INET));
            //next++;
        //}
        //else if (mask == 128 && strchr(buffer, ':')) {
            //assert(netaddr_from_buf(next, buffer, end - buffer, AF_INET6));
            //next++;
        //}
    //}

    //count = next - ips;
    //printf("Looking up %ld IPs\n", count);

    ///* Look them all up, benchmarking the time
     //*/

    //assert(gettimeofday(&start_time, NULL) == 0);

    //for (i = 0; i < count; i++)
        //assert(networks_get(networks_all, &ips[i], NULL));

    //printf("Search Duration: %"PRIu64" usec\n", usec_elapsed(&start_time, NULL));
    return 0;
}
