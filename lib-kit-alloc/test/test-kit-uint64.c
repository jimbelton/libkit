#include <tap.h>

#include "kit-unsigned.h"

int
main(void)
{
    plan_tests(13);

    is(kit_uint64_log2(1),                      0, "kit_uint64_log2(1) == 0");
    is(kit_uint64_log2(2),                      1, "kit_uint64_log2(2) == 1");
    is(kit_uint64_log2(3),                      1, "kit_uint64_log2(3) == 1");
    is(kit_uint64_log2(4),                      2, "kit_uint64_log2(4) == 2");
    is(kit_uint64_log2(128),                    7, "kit_uint64_log2(128) == 7");
    is(kit_uint64_log2(256),                    8, "kit_uint64_log2(256) == 0");
    is(kit_uint64_log2(0x10000),               16, "kit_uint64_log2(0x10000) == 16");
    is(kit_uint64_log2(0x1000000),             24, "kit_uint64_log2(0x1000000) == 24");
    is(kit_uint64_log2(0xFFFFFFFFFFFFFFFFULL), 63, "kit_uint64_log2((0xFFFFFFFFFFFFFFFFULL) == 31");

    is(kit_uint64_align(7, 9), 9,          "Align on an odd multiple");
    is(kit_uint64_align(1, 4096), 4096,    "Align on a page boundary");
    is(kit_uint64_align(8192, 4096), 8192, "Already aligned on a page boundary");
    is(kit_uint64_align(0xEFFFFFFFFFFFFFFFULL, 4096), 0xF000000000000000ULL,
       "kit_uint64_align(0xEFFFFFFFFFFFFFFFULL, 4096) == 0xF000000000000000ULL");

    return exit_status();
}
