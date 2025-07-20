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

#ifdef __linux__

#include <errno.h>
#include <string.h>
#include <sxe-log.h>

#include "kit-fsevent.h"

void
kit_fsevent_init(struct kit_fsevent *me)
{
    SXEA1((me->fd = inotify_init1(IN_NONBLOCK|IN_CLOEXEC)) != -1, "Couldn't inotify_init; error '%s'", strerror(errno));
}

void
kit_fsevent_fini(struct kit_fsevent *me)
{
    close(me->fd);
    me->fd = -1;
}

int
kit_fsevent_add_watch(const struct kit_fsevent *me, const char *mon, int how)
{
    return inotify_add_watch(me->fd, mon, how);
}

void
kit_fsevent_rm_watch(const struct kit_fsevent *me, int fd)
{
    inotify_rm_watch(me->fd, fd);
}

void
kit_fsevent_iterator_init(struct kit_fsevent_iterator *me)
{
    me->pos = me->len = 0;
}

kit_fsevent_ev_t *
kit_fsevent_read(const struct kit_fsevent *me, struct kit_fsevent_iterator *iter)
{
    kit_fsevent_ev_t *ev;

    do {
        if (iter->pos >= iter->len) {
            iter->pos = 0;
            if ((iter->len = read(me->fd, iter->buf, sizeof(iter->buf))) <= 0)
                return NULL;
        }
        ev = (kit_fsevent_ev_t *)(iter->buf + iter->pos);
        if ((iter->pos += sizeof(*ev) + ev->len) > iter->len)
            return NULL;    /* COVERAGE EXCLUSION: Good kernels only give us good data */
    } while (!KIT_FSEVENT_EV_ERROR(ev) && !ev->len);

    return ev;
}

#endif
