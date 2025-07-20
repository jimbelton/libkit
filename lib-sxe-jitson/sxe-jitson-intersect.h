#ifndef SXE_JITSON_INTERSECT_H
#define SXE_JITSON_INTERSECT_H

#include <sxe-jitson.h>

extern unsigned sxe_jitson_oper_intersect;
extern unsigned sxe_jitson_oper_intersect_test;

#include "sxe-jitson-intersect-proto.h"

#if defined(SXE_DEBUG) || defined(SXE_COVERAGE)    // Define unique tags for mockfails
#   define SXE_JITSON_INTERSECT_OPEN        ((const char *)sxe_jitson_intersect_init + 1)
#   define SXE_JITSON_INTERSECT_ADD         ((const char *)sxe_jitson_intersect_init + 2)
#   define SXE_JITSON_INTERSECT_ADD_INDEXED ((const char *)sxe_jitson_intersect_init + 3)
#   define SXE_JITSON_INTERSECT_GET         ((const char *)sxe_jitson_intersect_init + 4)
#endif

#endif
