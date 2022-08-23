#include "kit-mockfail.h"

#if defined(MAK_DEBUG) || defined(MAK_COVERAGE)
const void *kit_mockfail_failaddr;
unsigned    kit_mockfail_failfreq;
unsigned    kit_mockfail_failnum;
#endif
