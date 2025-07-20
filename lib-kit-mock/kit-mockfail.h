#ifndef KIT_MOCKFAIL_H
#define KIT_MOCKFAIL_H

#include <errno.h>

#if !SXE_DEBUG && !SXE_COVERAGE    // In the release build, remove all mock scaffolding

#define MOCKFAIL(addr, ret, expr)     (expr)
#define MOCKERROR(addr, ret, error, expr) (expr)
#define MOCKFAIL_START_TESTS(n, addr) skip_start(1, n, "MOCKFAIL: Skipping %d test%s for release build", n, (n) == 1 ? "" : "s")
#define MOCKFAIL_SET_FREQ(n)
#define MOCKFAIL_SET_SKIP(n)
#define MOCKFAIL_END_TESTS()          skip_end

#else    // In the debug and coverage builds, mock failures can be triggered

#define MOCKFAIL(addr, ret, expr) (((addr) == kit_mockfail_failaddr && !--kit_mockfail_failnum) ? ((kit_mockfail_failnum = kit_mockfail_failfreq), ret) : expr)
#define MOCKERROR(addr, ret, error, expr) (((addr) == kit_mockfail_failaddr && !--kit_mockfail_failnum) ? ((kit_mockfail_failnum = kit_mockfail_failfreq), (errno = error ), ret) : expr)
#define MOCKFAIL_START_TESTS(n, addr) do { kit_mockfail_failaddr = (addr); kit_mockfail_failfreq = kit_mockfail_failnum = 1; } while (0)
#define MOCKFAIL_SET_FREQ(n)          do { kit_mockfail_failfreq = kit_mockfail_failnum = (n); } while (0)    // Fail every n
#define MOCKFAIL_SET_SKIP(n)          do { kit_mockfail_failnum = (n) + 1; } while (0)                        // Skip n before failing
#define MOCKFAIL_END_TESTS()          do { kit_mockfail_failaddr = NULL; } while (0)

extern const void *kit_mockfail_failaddr;
extern unsigned    kit_mockfail_failfreq;
extern unsigned    kit_mockfail_failnum;
#endif

/*
 * We can change a piece of code such as
 *
 *     if (almost_impossible_failure(args) == hard_to_make_fail) {
 *         really_hard_to_test();
 *     }
 *
 * to
 *
 *     #include "kit-mockfail.h"
 *
 *     if (MOCKFAIL(UNIQUE_ADDRESS, hard_to_make_fail, almost_impossible_failure(args)) == hard_to_make_fail) {
 *         really_hard_to_test();
 *     }
 *
 * and test with:
 *
 *     #include "kit-mockfail.h"
 *
 *     MOCKFAIL_START_TESTS(3, UNIQUE_ADDR);
 *     ok(!some_caller_of_almost_impossible_failure(), "caller fails because allmost_impossible_failure() occurred");
 *     ok(!another_caller(), "caller fails because allmost_impossible_failure() occurred");
 *     MOCKFAIL_SET_FREQ(3);
 *     ok(some_caller_of_almost_impossible_failure(), "caller succeeds because allmost_impossible_failure() only fails every third time now");
 *     MOCKFAIL_END_TESTS();
 *
 * UNIQUE_ADDR can be a global function name or the address of a global variable. It must be a global so that the test can
 * use it. If you are mocking more than one failure in a function or the function is static, you can define tags in the .h:
 *
 *     #if defined(SXE_DEBUG) || defined(SXE_COVERAGE)    // Define unique tags for mockfails
 *     #   define APPLICATION_CLONE             ((const char *)application_register_resolver + 0)
 *     #   define APPLICATION_CLONE_DOMAINLISTS ((const char *)application_register_resolver + 1)
 *     #   define APPLICATION_MOREDOMAINLISTS   ((const char *)application_register_resolver + 2)
 *     #endif
 */

#endif
