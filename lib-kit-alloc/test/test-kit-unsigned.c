#include <tap.h>

#include "kit-unsigned.h"

int
main(void)
{
    plan_tests(16);

    is(kit_unsigned_log2(1),          0,        "kit_log2_unsigned(1)         == 0");
    is(kit_unsigned_log2(2),          1,        "kit_log2_unsigned(2)         == 1");
    is(kit_unsigned_log2(3),          1,        "kit_log2_unsigned(3)         == 1");
    is(kit_unsigned_log2(4),          2,        "kit_log2_unsigned(4)         == 2");
    is(kit_unsigned_log2(128),        7,        "kit_log2_unsigned(128)       == 7");
    is(kit_unsigned_log2(256),        8,        "kit_log2_unsigned(256)       == 0");
    is(kit_unsigned_log2(0x10000),   16,        "kit_log2_unsigned(0x10000)   == 16");
    is(kit_unsigned_log2(0x1000000), 24,        "kit_log2_unsigned(0x1000000) == 24");

    is(kit_unsigned_mask(1),         1,         "kit_unsigned_mask(1)         == 1");
    is(kit_unsigned_mask(2),         3,         "kit_unsigned_mask(2)         == 3");
    is(kit_unsigned_mask(3),         3,         "kit_unsigned_mask(3)         == 3");
    is(kit_unsigned_mask(255),       255,       "kit_unsigned_mask(255)       == 255");
    is(kit_unsigned_mask(256),       511,       "kit_unsigned_mask(256)       == 511");
    is(kit_unsigned_mask(511),       511,       "kit_unsigned_mask(511)       == 511");
    is(kit_unsigned_mask(0xFFFF),    0xFFFF,    "kit_unsigned_mask(0xFFFF)    == 0xFFFF");
    is(kit_unsigned_mask(0x1000000), 0x1FFFFFF, "kit_unsigned_mask(0x1000000) == 0x1FFFFFF");

    return exit_status();
}
