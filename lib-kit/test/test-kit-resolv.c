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

#define _GNU_SOURCE       // For memrchr glibc extension
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sxe-util.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <tap.h>

#include "kit.h"

static unsigned
readpkt(uint8_t *pkt, unsigned plen, const char *txt)
{
    const char *hex = "0123456789abcdef";
    unsigned i, len, n;

    len = 0;
    while (txt && *txt) {
        if ((n = strspn(txt, hex)) != 0 && n % 2 == 0) {
            for (i = 0; i < n; i += 2) {
                SXEA1(len < plen, "Oops, readpkt() overflow");
                pkt[len++] = ((strchr(hex, txt[i]) - hex) << 4) | (strchr(hex, txt[i + 1]) - hex);
            }
            txt += n;
        } else if (*txt == '\n')
            txt = strchr(txt, ':');
        else if (*txt == '[')
            txt = strchr(txt, ']');
        else
            txt++;
    }

    return len;
}

int
main(void)
{
    struct addrinfo *info, *i4, *i6, *i;
    struct sockaddr_in6 *sin6;
    struct sockaddr_in *sin4;
    char txt[100];
    bool fail;

    plan_tests(48);

    diag("Test kit_stub_lookup()");
    {
        info = kit_stub_lookup("1:2:3::4", "domain");
        ok(info, "Lookup of 1:2:3::4 succeeds");
        is(info->ai_next, NULL, "Lookup of 1:2:3::4 yields only one answer");
        is(info->ai_family, AF_INET6, "Lookup of 1:2:3::4 yields an IPv6 answer");
        sin6 = (struct sockaddr_in6 *)info->ai_addr;
        is_eq(inet_ntop(info->ai_family, &sin6->sin6_addr, txt, sizeof(txt)), "1:2:3::4", "Lookup of 1:2:3::4 yields '1:2:3::4'");
        is(ntohs(sin6->sin6_port), 53, "Lookup of 1:2:3::4 with a 'domain' port default yields port 53");
        freeaddrinfo(info);

        info = kit_stub_lookup("[2:3:4::5]:1234", "domain");
        ok(info, "Lookup of [2:3:4::5]:1234 succeeds");
        is(info->ai_next, NULL, "Lookup of [2:3:4::5]:1234 yields only one answer");
        is(info->ai_family, AF_INET6, "Lookup of [2:3:4::5]:1234 yields an IPv6 answer");
        sin6 = (struct sockaddr_in6 *)info->ai_addr;
        is_eq(inet_ntop(info->ai_family, &sin6->sin6_addr, txt, sizeof(txt)), "2:3:4::5", "Lookup of [2:3:4::5]:1234 yields '2:3:4::5'");
        is(ntohs(sin6->sin6_port), 1234, "Lookup of [2:3:4::5]:1234 with a 'domain' port default yields port 1234");
        freeaddrinfo(info);

        info = kit_stub_lookup("1.2.3.4", "time");
        ok(info, "Lookup of 1.2.3.4 succeeds");
        is(info->ai_next, NULL, "Lookup of 1.2.3.4 yields only one answer");
        is(info->ai_family, AF_INET, "Lookup of 1.2.3.4 yields an IPv4 answer");
        sin4 = (struct sockaddr_in *)info->ai_addr;
        is_eq(inet_ntop(info->ai_family, &sin4->sin_addr, txt, sizeof(txt)), "1.2.3.4", "Lookup of 1.2.3.4 yields '1.2.3.4'");
        is(ntohs(sin4->sin_port), 37, "Lookup of 1.2.3.4 with a 'time' port default yields port 37");
        freeaddrinfo(info);

        info = kit_stub_lookup("2.3.4.5:1234", "time");
        ok(info, "Lookup of 2.3.4.5:1234 succeeds");
        is(info->ai_next, NULL, "Lookup of 2.3.4.5:1234 yields only one answer");
        is(info->ai_family, AF_INET, "Lookup of 2.3.4.5:1234 yields an IPv4 answer");
        sin4 = (struct sockaddr_in *)info->ai_addr;
        is_eq(inet_ntop(info->ai_family, &sin4->sin_addr, txt, sizeof(txt)), "2.3.4.5", "Lookup of 2.3.4.5:1234 yields '2.3.4.5'");
        is(ntohs(sin4->sin_port), 1234, "Lookup of 2.3.4.5:1234 with a 'time' port default yields port 1234");
        freeaddrinfo(info);

        info = kit_stub_lookup("resolver1.opendns.com", "42");
        for (i4 = i6 = NULL, fail = false, i = info; i; i = i->ai_next)
            if (!i6 && i->ai_family == AF_INET6)
                i6 = i;
            else if (!i4 && i->ai_family == AF_INET)
                i4 = i;
            else
                fail = true;

        ok(!fail, "Lookup of resolver1.opendns.com yields no unexpected answers");
        ok(i6, "Lookup of resolver1.opendns.com found an IPv6 address");
        sin6 = (struct sockaddr_in6 *)i6->ai_addr;
        is(ntohs(sin6->sin6_port), 42, "Lookup of resolver1.opendns.com with a '42' port default yields IPv6 port 42");

        ok(i4, "Lookup of resolver1.opendns.com found an IPv4 address");
        sin4 = (struct sockaddr_in *)i4->ai_addr;
        is(ntohs(sin4->sin_port), 42, "Lookup of resolver1.opendns.com with a '42' port default yields IPv4 port 42");
        freeaddrinfo(info);

        info = kit_stub_lookup("resolver2.opendns.com:666", "42");
        for (i4 = i6 = NULL, fail = false, i = info; i; i = i->ai_next)
            if (!i6 && i->ai_family == AF_INET6)
                i6 = i;
            else if (!i4 && i->ai_family == AF_INET)
                i4 = i;
            else
                fail = true;

        ok(!fail, "Lookup of resolver2.opendns.com:666 yields no unexpected answers");
        ok(i6, "Lookup of resolver2.opendns.com:666 found an IPv6 address");
        sin6 = (struct sockaddr_in6 *)i6->ai_addr;
        is(ntohs(sin6->sin6_port), 666, "Lookup of resolver2.opendns.com:666 with a '42' port default yields IPv6 port 666");

        ok(i4, "Lookup of resolver2.opendns.com:666 found an IPv4 address");
        sin4 = (struct sockaddr_in *)i4->ai_addr;
        is(ntohs(sin4->sin_port), 666, "Lookup of resolver2.opendns.com:666 with a '42' port default yields IPv4 port 666");
        freeaddrinfo(info);

        is(info = kit_stub_lookup("[no-closing-bracket", "domain"), NULL, "A lookup using unbalanced [] fails");
        is(info = kit_stub_lookup("", "domain"), NULL, "A lookup of an empty string fails");
    }

    diag("Test kit_ns_print()");
    {
        char *hbuf, obuf[1024], *p;
        unsigned hlen, plen;
        uint8_t pkt[1024];
        ns_msg nshandle;
        const char *hex;
        FILE *fp;

        /* opendns-edns bufsize=16384 debug=3 flags=noad,nocd,nord headers=1 port=53 qname=www.awfulhak.org qtype=A resolver=ns.tao.org.uk tcp=1 */
        hex = "8ece [DNSID] 8400 [flags/rcode] 0001 [# questions] 0002 [# answers] 0004 [# auth] 0007 [# glue]\n"
              "question #1:    03777777 [label] 08617766756c68616b [label] 036f7267 [label] 00 [label] 0001 [type] 0001 [class]\n"
              "answer #1:      c00c [comp] 0005 [type] 0001 [class] 00015180 [ttl] 0005 [rdlen] 026777c010 [rdata]\n"
              "answer #2:      c02e [comp] 0001 [type] 0001 [class] 00001c20 [ttl] 0004 [rdlen] 3244d9a8 [rdata]\n"
              "auth #1:        c010 [comp] 0002 [type] 0001 [class] 00015180 [ttl] 000f [rdlen] 026e730374616f036f726702756b00 [rdata]\n"
              "auth #2:        c010 [comp] 0002 [type] 0001 [class] 00015180 [ttl] 000d [rdlen] 036e7332066b6e69676d61c019 [rdata]\n"
              "auth #3:        c010 [comp] 0002 [type] 0001 [class] 00015180 [ttl] 0012 [rdlen] 036e733008417766756c68616b036e657400 [rdata]\n"
              "auth #4:        c010 [comp] 0002 [type] 0001 [class] 00015180 [ttl] 0006 [rdlen] 036e7333c052 [rdata]\n"
              "glue #1:        c04f [comp] 0001 [type] 0001 [class] 00000e10 [ttl] 0004 [rdlen] d40dc59c [rdata]\n"
              "glue #2:        c083 [comp] 0001 [type] 0001 [class] 00001c20 [ttl] 0004 [rdlen] 3244d9a8 [rdata]\n"
              "glue #3:        c083 [comp] 001c [type] 0001 [class] 00015180 [ttl] 0010 [rdlen] 26043d080001001dd08f57716cee06a1 [rdata]\n"
              "glue #4:        c06a [comp] 0001 [type] 0001 [class] 00003840 [ttl] 0004 [rdlen] 5102669a [rdata]\n"
              "glue #5:        c06a [comp] 001c [type] 0001 [class] 00003840 [ttl] 0010 [rdlen] 200108b000b000010000000000000001 [rdata]\n"
              "glue #6:        c0a1 [comp] 0001 [type] 0001 [class] 00000e10 [ttl] 0004 [rdlen] b2fa4c1e [rdata]\n"
              "glue #7:        00 [label] 0029 [type] 1000 [bufsize] 00000000 [flags/rcode] 0000 [rdlen]\n";
        hlen = strlen(hex);
        hbuf = alloca(hlen + 1);
        strcpy(hbuf, hex);
        plen = readpkt(pkt, sizeof(pkt), hbuf);
        ok(ns_initparse(pkt, plen, &nshandle) == 0, "www.awfulhak.org: Parsed the binary response");
        memset(obuf, '\0', sizeof(obuf));
        fp = fmemopen(obuf, sizeof(obuf) - 1, "w");
        ok(kit_ns_print(fp, nshandle, false), "www.awfulhak.org: Dumped the response");
        fflush(fp);
        fclose(fp);
        ok(strlen(obuf) < sizeof(obuf) - 2, "www.awfulhak.org: Dumped the response without overflow");
        is_eq(obuf, ";; QUESTION SECTION:\n"
                   ";www.awfulhak.org. IN A\n"
                   ";; ANSWER SECTION:\n"
                   "www.awfulhak.org.\t1D IN CNAME\tgw.awfulhak.org.\n"
                   "gw.awfulhak.org.\t2H IN A\t\t50.68.217.168\n"
                   ";; AUTHORITY SECTION:\n"
                   "awfulhak.org.\t\t1D IN NS\tns.tao.org.uk.\n"
                   "awfulhak.org.\t\t1D IN NS\tns2.knigma.org.\n"
                   "awfulhak.org.\t\t1D IN NS\tns0.Awfulhak.net.\n"
                   "awfulhak.org.\t\t1D IN NS\tns3.tao.org.uk.\n"
                   ";; ADDITIONAL SECTION:\n"
                   "ns.tao.org.uk.\t\t1H IN A\t\t212.13.197.156\n"
                   "ns0.Awfulhak.net.\t2H IN A\t\t50.68.217.168\n"
                   "ns0.Awfulhak.net.\t1D IN AAAA\t2604:3d08:1:1d:d08f:5771:6cee:6a1\n"
                   "ns2.knigma.org.\t\t4H IN A\t\t81.2.102.154\n"
                   "ns2.knigma.org.\t\t4H IN AAAA\t2001:8b0:b0:1::1\n"
                   "ns3.tao.org.uk.\t\t1H IN A\t\t178.250.76.30\n"
                   ".\t\t\t0S 4096 4096\t4096 bytes\n", "www.awfulhak.org: Response data is output correctly");

        ok(ns_initparse(pkt, plen - 1, &nshandle) != 0, "www.awfulhak.org[byte-truncated]: Cannot parse the binary response");

        p = memrchr(hbuf, '\n', hlen - 1);
        *p = '\0';
        plen = readpkt(pkt, sizeof(pkt), hbuf);
        ok(ns_initparse(pkt, plen, &nshandle) != 0, "www.awfulhak.org[RR-truncated]: Cannot parse the binary response");

        hex = "8ece [DNSID] 8400 [flags/rcode] 0001 [# questions] 0001 [# answers] 0000 [# auth] 0000 [# glue]\n"
              "question #1:    03777777 [label] 08617766756c68616b [label] 036f7267 [label] 00 [label] 0001 [type] 0001 [class]\n"
              "answer #1:      c00c [comp] 0005 [type] 0001 [class] 00015180 [ttl] 0005 [rdlen] 026777c010 [rdata]\n";
        hlen = strlen(hex);
        strcpy(hbuf, hex);
        plen = readpkt(pkt, sizeof(pkt), hbuf);
        ok(ns_initparse(pkt, plen, &nshandle) == 0, "www.awfulhak.org[short]: Parsed the binary response");
        memset(obuf, '\0', sizeof(obuf));
        fp = fmemopen(obuf, sizeof(obuf) - 1, "w");
        ok(kit_ns_print(fp, nshandle, false), "www.awfulhak.org[short]: Dumped the response");
        fflush(fp);
        fclose(fp);
        ok(strlen(obuf) < sizeof(obuf) - 2, "www.awfulhak.org[short]: Dumped the response without overflow");
        is_eq(obuf, ";; QUESTION SECTION:\n"
                   ";www.awfulhak.org. IN A\n"
                   ";; ANSWER SECTION:\n"
                   "www.awfulhak.org.\t1D IN CNAME\tgw.awfulhak.org.\n"
                   ";; AUTHORITY SECTION:\n"
                   ";; ADDITIONAL SECTION:\n", "www.awfulhak.org[short]: Response data is output correctly");

        /* Corrupt the last byte with an rdata compression ref outside of the packet */
        hex = "8ece [DNSID] 8400 [flags/rcode] 0001 [# questions] 0001 [# answers] 0000 [# auth] 0000 [# glue]\n"
              "question #1:    03777777 [label] 08617766756c68616b [label] 036f7267 [label] 00 [label] 0001 [type] 0001 [class]\n"
              "answer #1:      c00c [comp] 0005 [type] 0001 [class] 00015180 [ttl] 0005 [rdlen] 026777c0a0 [rdata]\n";
        hlen = strlen(hex);
        strcpy(hbuf, hex);
        plen = readpkt(pkt, sizeof(pkt), hbuf);
        ok(ns_initparse(pkt, plen, &nshandle) == 0, "www.awfulhak.org[corrupt-rdata]: Parsed the binary response");
        memset(obuf, '\0', sizeof(obuf));
        fp = fmemopen(obuf, sizeof(obuf) - 1, "w");
        ok(kit_ns_print(fp, nshandle, false), "www.awfulhak.org[corrupt-rdata]: Dumped the response");
        fflush(fp);
        fclose(fp);
        ok(strlen(obuf) < sizeof(obuf) - 2, "www.awfulhak.org[corrupt-rdata]: Dumped the response without overflow");
        is_eq(obuf, ";; QUESTION SECTION:\n"
                   ";www.awfulhak.org. IN A\n"
                   ";; ANSWER SECTION:\n"
                   "<INVALID>: No space left on device\n"
                   ";; AUTHORITY SECTION:\n"
                   ";; ADDITIONAL SECTION:\n", "www.awfulhak.org[corrupt-rdata]: Response data is output correctly (with <INVALID> RR displays)");

        /* Corrupt the answer name with a compression ref outside of the packet */
        hex = "8ece [DNSID] 8400 [flags/rcode] 0001 [# questions] 0001 [# answers] 0000 [# auth] 0000 [# glue]\n"
              "question #1:    03777777 [label] 08617766756c68616b [label] 036f7267 [label] 00 [label] 0001 [type] 0001 [class]\n"
              "answer #1:      c0a0 [comp] 0005 [type] 0001 [class] 00015180 [ttl] 0005 [rdlen] 026777c010 [rdata]\n";
        hlen = strlen(hex);
        strcpy(hbuf, hex);
        plen = readpkt(pkt, sizeof(pkt), hbuf);
        ok(ns_initparse(pkt, plen, &nshandle) == 0, "www.awfulhak.org[corrupt-name]: Parsed the binary response");
        memset(obuf, '\0', sizeof(obuf));
        fp = fmemopen(obuf, sizeof(obuf) - 1, "w");
        ok(!kit_ns_print(fp, nshandle, false), "www.awfulhak.org[corrupt-name]: Failed to dump the response");
        fclose(fp);
    }

    return exit_status();
}
