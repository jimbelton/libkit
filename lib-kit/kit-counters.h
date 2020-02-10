#ifndef KIT_COUNTERS_H
#define KIT_COUNTERS_H

#define MAXCOUNTERS             400
#define COUNTER_FLAG_NONE      0x00
#define COUNTER_FLAG_SUMMARIZE 0x01

#include <inttypes.h>
#include <stdbool.h>

struct kit_counters {
    unsigned long long val[MAXCOUNTERS];
};

typedef unsigned kit_counter_t;
typedef void (*kit_counters_mib_callback_t)(void *, const char *, const char *);
typedef void (*kit_mibfn_t)(kit_counter_t, const char *subtree, const char *mib, void *v, kit_counters_mib_callback_t cb, int threadnum, unsigned cflags);

#include "kit-counters-proto.h"

#endif
