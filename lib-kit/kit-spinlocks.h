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

#ifndef KIT_SPINLOCKS_H
#define KIT_SPINLOCKS_H

#include <unistd.h>
#include <pthread.h>

#if (!defined(_XOPEN_VERSION) || _XOPEN_VERSION < 700) && \
    !(defined(__FreeBSD__) || defined(__NetBSD__))

# include <errno.h>

typedef volatile int pthread_spinlock_t;

# if defined(__x86_64__) || defined(__i386) || defined(_X86_)
#  define CPU_PAUSE __asm__ ("pause")
# else
#  define CPU_PAUSE do { } while(0)
# endif

# define pthread_spin_init(L, S)                                      \ /* CONVENTION EXCLUSION: Insisting on capitalization gives no benefit */
    (*(L) = 0,                                                        \
     -((S) != PTHREAD_PROCESS_PRIVATE && (errno = EINVAL) == EINVAL))

# define pthread_spin_destroy(L) do { } while(0)                        /* CONVENTION EXCLUSION: Insisting on capitalization gives no benefit */

# define pthread_spin_lock(L)                                \          /* CONVENTION EXCLUSION: Insisting on capitalization gives no benefit */
    do {                                                     \
        if (__sync_lock_test_and_set((L), 1) != 0) {         \
            unsigned spin_count = 0U;                        \
            for (;;) {                                       \
                CPU_PAUSE;                                   \
                if (*(L) == 0U &&                            \
                    __sync_lock_test_and_set((L), 1) == 0) { \
                    break;                                   \
                }                                            \
                if (++spin_count > 100U) {                   \
                    usleep(0);                               \
                }                                            \
            }                                                \
        }                                                    \
        ANNOTATE_WRITERLOCK_ACQUIRED((const void *) (L));    \
    } while(0)

# define pthread_spin_unlock(L)                           \             /* CONVENTION EXCLUSION: Insisting on capitalization gives no benefit */
    do {                                                  \
        __sync_lock_release(L);                           \
        ANNOTATE_WRITERLOCK_RELEASED((const void *) (L)); \
    } while(0)

#endif

#endif
