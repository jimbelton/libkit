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

#ifndef KIT_FSEVENT_H
#define KIT_FSEVENT_H

#ifdef __linux__

#include <stdio.h>
#include <sys/inotify.h>

#define KIT_FSEVENT_CREATE       IN_CREATE
#define KIT_FSEVENT_DELETE       IN_DELETE
#define KIT_FSEVENT_MOVED_TO     IN_MOVED_TO
#define KIT_FSEVENT_MOVED_FROM   IN_MOVED_FROM
#define KIT_FSEVENT_MODIFY       IN_MODIFY

typedef struct inotify_event    kit_fsevent_ev_t;

#define KIT_FSEVENT_WAIT_BUFSZ   (1024 * (sizeof(kit_fsevent_ev_t) + 16))
#define KIT_FSEVENT_ERRCHK       "check the fs.inotify.max_queued_events sysctl"

#define KIT_FSEVENT_EV_ERROR(ev)    ((ev)->mask & IN_Q_OVERFLOW)
#define KIT_FSEVENT_EV_FD(ev)       (ev)->wd
#define KIT_FSEVENT_EV_NAME(ev)     (ev)->name
#define KIT_FSEVENT_EV_ISDIR(ev)    ((ev)->mask & IN_ISDIR)
#define KIT_FSEVENT_EV_IS(ev, what) ((ev)->mask & (what))

#else

#include <sys/types.h>
#include <sys/event.h>

#define KIT_FSEVENT_CREATE       NOTE_WRITE
#define KIT_FSEVENT_DELETE       NOTE_WRITE
#define KIT_FSEVENT_MOVED_TO     NOTE_EXTEND
#define KIT_FSEVENT_MOVED_FROM   NOTE_EXTEND
#define KIT_FSEVENT_MODIFY       0

typedef struct kevent        kit_fsevent_ev_t;

#define KIT_FSEVENT_WAIT_BUFSZ   (1024 * sizeof(kit_fsevent_ev_t))
#define KIT_FSEVENT_ERRCHK       "no suggestions"

#define KIT_FSEVENT_EV_ERROR(ev)    ((ev)->flags & EV_ERROR)
#define KIT_FSEVENT_EV_FD(ev)       (ev)->ident
#define KIT_FSEVENT_EV_NAME(ev)     ".."
#define KIT_FSEVENT_EV_ISDIR(ev)    0
#define KIT_FSEVENT_EV_IS(ev, what) ((ev)->fflags & (what))

#endif

struct kit_fsevent {
    int fd;
};

struct kit_fsevent_iterator {
    char buf[KIT_FSEVENT_WAIT_BUFSZ];
    ssize_t pos, len;
};

#include "kit-fsevent-inotify-proto.h"

#endif
