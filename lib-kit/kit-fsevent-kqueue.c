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

#ifndef __linux__

#include "kit-fsevent.h"

#include <sxe-log.h>
#include <fcntl.h>
#include <unistd.h>

void
kit_fsevent_init(struct kit_fsevent *me)
{
    SXEA1((me->fd = kqueue()) != -1, "Couldn't inotify_init()");
}

void
kit_fsevent_fini(struct kit_fsevent *me)
{
    close(me->fd);
}

int
kit_fsevent_add_watch(struct kit_fsevent *me, const char *mon, int how)
{
    kit_fsevent_ev_t ev;
    int fd, ret;

    if ((fd = open(mon, O_RDONLY)) == -1)
        return -1;

    EV_SET(&ev, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR, how, 0, NULL);
    if ((ret = kevent(me->fd, &ev, 1, NULL, 0, NULL)) == -1) {
        close(fd);
        fd = -1;
    }

    return fd;
}

void
kit_fsevent_rm_watch(struct kit_fsevent *me, int fd)
{
    kit_fsevent_ev_t ev;

    EV_SET(&ev, fd, EVFILT_VNODE, EV_DELETE, 0xffff, 0, NULL);
    kevent(me->fd, &ev, 1, NULL, 0, NULL);
}

void
kit_fsevent_iterator_init(struct kit_fsevent_iterator *me)
{
    me->pos = me->len = 0;
}

kit_fsevent_ev_t *
kit_fsevent_read(struct kit_fsevent *me, struct kit_fsevent_iterator *iter)
{
    kit_fsevent_ev_t *ev = (kit_fsevent_ev_t *)iter->buf;
    struct timespec ts;
    int ret;

    do {
        if (iter->pos >= iter->len) {
            iter->pos = 0;
            ts.tv_sec = 0;
            ts.tv_nsec = 0;
            if ((ret = kevent(me->fd, NULL, 0, ev, 1024, &ts)) <= 0)
                return NULL;
            iter->len = ret * 1024;
        }
        ev = (kit_fsevent_ev_t *)(iter->buf + iter->pos);
        if ((iter->pos += sizeof(*ev)) > iter->len)
            return NULL;
    } while (!KIT_FSEVENT_EV_ERROR(ev));

    return ev;
}

#endif
