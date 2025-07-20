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

#include "kit-safe-rw.h"
#include <errno.h>
#include <poll.h>

#include <sxe-log.h>

ssize_t
kit_safe_write(const int fd, const void *const buf_, size_t count, const int timeout)
{
    struct pollfd  pfd;
    const char    *buf = (const char *) buf_;
    ssize_t        written, result;
    size_t         tmp_count = count;

    pfd.fd = fd;
    pfd.events = POLLOUT;

    while (tmp_count > (size_t) 0) {
        while ((written = write(fd, buf, tmp_count)) <= (ssize_t) 0) {
            if (errno == EAGAIN) {
                if (poll(&pfd, (nfds_t) 1, timeout) == 0) {
                    errno = ETIMEDOUT;
                    goto SXE_EARLY_OUT;
                }
            } else if (errno != EINTR)
                goto SXE_EARLY_OUT;
        }
        buf += written;
        tmp_count -= written;
    }

SXE_EARLY_OUT:
    result = buf - (const char *)buf_;

    SXEL7("kit_safe_write(fd=%d, buf=%p, count=%zu, timeout=%d){} // %zd", fd, buf_, count, timeout, result);
    return result;
}

ssize_t
kit_safe_read(const int fd, void *const buf_, size_t count)
{
    unsigned char *buf = (unsigned char *) buf_;
    ssize_t        readnb;
    ssize_t        result;

    SXEE7("(fd=%d, buf=%p, count=%zu)", fd, buf, count);

    do {
        while ((readnb = read(fd, buf, count)) < 0 && errno == EINTR)
            ;
        if (readnb < 0) {
            result = readnb;
            goto SXE_EARLY_OUT;
        }
        if (readnb == 0)
            break;
        count -= readnb;
        buf += readnb;
    } while (count > 0);

    result = buf - (unsigned char *)buf_;

SXE_EARLY_OUT:
    SXER7("return %zd", result);

    return result;
}
