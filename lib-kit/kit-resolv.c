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

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sxe-log.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "kit.h"

/*-
 * Resolve the given 'qname' using the stub resolver.  The returned
 * 'struct addrinfo' is allocated by getaddrinfo(3) and must be freed
 * using freeaddrinfo();
 *
 * The returned addrinfo::ai_addr is a sockaddr that contains the given
 * 'service' as the port unless the port is specified in 'qname'.  The
 * 'service' is looked up in /etc/services.
 *
 * 'qname' is of the format addr[:port] where
 *         'port' is either the name of a service to be looked up in /etc/services
 *                or a numeric port number
 *         'addr' is a name or address.
 *                If 'addr' is a name, it is looked up as an A and AAAA record,
 *                returning the relevant number of addrinfo structures.
 *                If 'addr' is an address, it just resolves to that IPv4 or IPv6
 *                address.
 *         Note:  If an IPv6 address is specified with a port, the address part must
 *                be inclosed in [] to distinguish the port as not being part of the
 *                address itself.  i.e. '[::1]:1234', and not '::1:1234'.
 */
struct addrinfo *
kit_stub_lookup(const char *qname, const char *service)
{
    struct addrinfo hints, *ret;
    const char *dot, *port;
    char *cp;
    int err;

    SXEL5("Resolving %s", qname);
    memset(&hints, '\0', sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_CANONNAME;

    if (*qname == '[') {
        /* [x:x:x:x:x:x:x:x]:y */
        if ((port = strchr(qname + 1, ']')) == NULL) {
            fprintf(stderr, "%s: Unbalanced []\n", qname);
            return NULL;
        }
        cp = alloca(port - qname);
        memcpy(cp, qname + 1, port - qname - 1);
        cp[port - qname - 1] = '\0';
        qname = cp;
        port++;
    } else if ((port = strrchr(qname, ':')) != NULL && (dot = strrchr(qname, '.')) != NULL && dot < port) {
        /* x.x.x.x:y */
        cp = alloca(port - qname + 1);
        memcpy(cp, qname, port - qname);
        cp[port - qname] = '\0';
        qname = cp;
    } else
        /* x:x:x:x:x:x:x:x or x.x.x.x */
        port = NULL;

    if (port && *port == ':' && strlen(port + 1))
        port++;
    else
        port = service;

    if (strcmp(qname, "::1") != 0 && strcasecmp(qname, "localhost") != 0)
        hints.ai_flags |= AI_ADDRCONFIG;

    if ((err = getaddrinfo(qname, port, &hints, &ret))) {
        fprintf(stderr, "%s: %s\n", qname, gai_strerror(err));
        ret = NULL;
    }

    return ret;
}

/*
 * Output the text format of the given DNS packet to the given stream.
 *
 * The DNS packet is given as 'nshandle' which is obtained like this:
 *
 *     ns_initparse(answer, anslen, &nshandle);
 *
 * If 'shorten' is set to 'true', only the ANSWER section, without its
 * header is emitted.
 */
bool
kit_ns_print(FILE *stream, ns_msg nshandle, bool shorten)
{
    int i, records;
    char buf[1024];
    unsigned n;
    ns_rr rr;

    struct {
        ns_sect sect;
        const char *preamble;
    } show[] = {
        { ns_s_qd, ";; QUESTION SECTION:\n" },
        { ns_s_an, ";; ANSWER SECTION:\n" },
        { ns_s_ns, ";; AUTHORITY SECTION:\n" },
        { ns_s_ar, ";; ADDITIONAL SECTION:\n" }
    };

    for (n = shorten ? 1 : 0; n < (shorten ? 2 : sizeof(show) / sizeof(*show)); n++) {
        if (!shorten)
            fprintf(stream, "%s", show[n].preamble);
        records = ns_msg_count(nshandle, show[n].sect);
        for (i = 0; i < records; i++) {
            if (ns_parserr(&nshandle, show[n].sect, i, &rr) == -1)
                return false;
            if (show[n].sect == ns_s_qd)
                snprintf(buf, sizeof(buf), ";%.256s. %s %s", ns_rr_name(rr), p_class(ns_rr_class(rr)), p_type(ns_rr_type(rr)));
            else if (ns_sprintrr(&nshandle, &rr, NULL, NULL, buf, sizeof(buf)) < 0)
                snprintf(buf, sizeof(buf), "<INVALID>: %s", strerror(errno));
            fprintf(stream, "%s\n", buf);
        }
    }

    return true;
}
