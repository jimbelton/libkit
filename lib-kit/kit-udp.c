#include <errno.h>
#include <netinet/ip.h>
#include <string.h>
#include <mock.h>
#include <sxe-log.h>

#include "kit.h"

/**
 * Create a UDP socket and optionally set socket options for extra information
 *
 * @param domain One of AF_INET or AF_INET6
 * @param flags  0, KIT_UDP_DELAY (get delay in msec), KIT_UDP_TTLTOS (get TTL/TOS), or KIT_UDP_DELAY | KIT_UDP_TTLTOS
 */
int
kit_udp_socket(int domain, unsigned flags)
{
    int fd;
    int enabled = 1;

    SXEA1(domain == AF_INET || domain == AF_INET6, "Unexpected domain %d", domain);
    SXEE6("(domain=%s, flags=%s)", domain == AF_INET ? "AF_INET" : "AF_INET6",
          flags == 0 ? 0 : flags == KIT_UDP_DELAY ? "KIT_UDP_DELAY" : flags == KIT_UDP_TTLTOS ? "KIT_UDP_TTLTOS" : "KIT_UDP_DELAY | KIT_UDP_TTLTOS" );

    if ((fd = socket(domain, SOCK_DGRAM, 0)) >= 0) {
        if (flags & KIT_UDP_DELAY)
            SXEA1(setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, &enabled, sizeof(enabled)) >= 0, "Failed to enable UDP timestamps");

        if (flags & KIT_UDP_TTLTOS) {
            SXEA1(setsockopt(fd, IPPROTO_IP, IP_RECVTTL, &enabled, sizeof(enabled)) >= 0, "Failed to enable IP TTLs");
            SXEA1(setsockopt(fd, IPPROTO_IP, IP_RECVTOS, &enabled, sizeof(enabled)) >= 0, "Failed to enable IP TOSs");
        }
    }

    SXER6("return fd=%d", fd);
    return fd;
}

/**
 * Receive from a UDP socket, capturing the time spent in the receive queue if possible
 *
 * @param fd            UDP socket descriptor
 * @param buffer        Pointer to buffer to receive data into
 * @param buffer_len    Size of buffer
 * @param flags         Include MSG_TRUNC to have the length of the UDP message returned even if it is truncated.
 * @param address       NULL or pointer to sockaddr to store the source address
 * @param address_len   NULL or pointer to integer holding size of address structure in/size of address out
 * @param delay_in_msec NULL or pointer to an unsigned long in which to store the delay in msec. If unavailable, set to ~0UL.
 * @param ttltos        NULL or pointer to a struct in which to store the IP TTL and TOS. If unavailable, struct is unchanged.
 */
ssize_t
kit_recvfrom(int fd, void *buffer, size_t buffer_len, int flags, struct sockaddr *address, socklen_t *address_len,
             unsigned long *delay_in_msec, struct kit_udp_ttltos *ttltos)
{
    uint8_t         control[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(int)) * 2];  /* SO_TIMESTAMP + IP_TTL + IP_TOS */
    ssize_t         size;
    struct msghdr   header;
    struct iovec    io_vector[1];
    struct cmsghdr *cmsghdr;
    struct timeval  current;
    struct timeval *timestamp = NULL;
    int             rerrno, *valp;

    io_vector[0].iov_base = buffer;
    io_vector[0].iov_len  = buffer_len;

    header.msg_name       = address;   // May be NULL
    header.msg_namelen    = address_len ? *address_len : 0;
    header.msg_iov        = io_vector;
    header.msg_iovlen     = 1;
    header.msg_control    = &control;
    header.msg_controllen = sizeof(control);

    size = recvmsg(fd, &header, flags);
    rerrno = errno;

    if (delay_in_msec)
        *delay_in_msec = ~0UL;

    if (size >= 0) {
        if ((header.msg_flags & MSG_TRUNC) && !(flags & MSG_TRUNC))
            SXEL3("UDP message received from fd %d is silently truncated to %zu bytes", fd, size);

        SXEA6(!(header.msg_flags & MSG_CTRUNC), "UDP control data should never exceed %zu bytes", sizeof(control));

        for (cmsghdr = CMSG_FIRSTHDR(&header); cmsghdr != NULL; cmsghdr = CMSG_NXTHDR(&header, cmsghdr))
            switch (cmsghdr->cmsg_level) {
            case SOL_SOCKET:
                if (cmsghdr->cmsg_type != SO_TIMESTAMP)
                    SXEL3("UDP message received from fd %d control data includes an SOL_SOCKET cmsg that is not an"    /* COVERAGE EXCLUSION: Can't happen */
                          " SO_TIMESTAMP (got type %d)", fd, cmsghdr->cmsg_type);
                else if (delay_in_msec) {
                    timestamp = (struct timeval *)CMSG_DATA(cmsghdr);
                    SXEA1(gettimeofday(&current, 0) >= 0, "Kernel won't give use the timeofday");

                    if (current.tv_sec < timestamp->tv_sec
                     || (current.tv_sec == timestamp->tv_sec && current.tv_usec < timestamp->tv_usec))
                        SXEL3("UDP message received from fd %d has timestamp %lu.%06u that is after current time %lu.%06u",    /* COVERAGE EXCLUSION: Can't happen */
                              fd, timestamp->tv_sec, (unsigned)timestamp->tv_usec, current.tv_sec, (unsigned)current.tv_usec);
                    else
                        *delay_in_msec = (current.tv_sec - timestamp->tv_sec) * 1000
                                         + (current.tv_usec / 1000 - timestamp->tv_usec / 1000);
                }

                break;

            case IPPROTO_IP:
                if (ttltos) {
                    valp = (int *)CMSG_DATA(cmsghdr);
                    if (cmsghdr->cmsg_type == IP_TTL)
                        ttltos->ttl = *valp;

                    else if (cmsghdr->cmsg_type == IP_TOS)
                        ttltos->tos = *valp;

                    else
                        SXEL3("UDP message received from fd %d control data includes an IPPROTO_IP cmsg that is neither"    /* COVERAGE EXCLUSION: Can't happen */
                              " IP_TTL nor IP_TOS (got type %d)", fd, cmsghdr->cmsg_type);
                }

                break;

            default:
                SXEL3("UDP message received from fd %d control data includes an unexpected cmsg level %d", fd,    /* COVERAGE EXCLUSION: Can't happen */
                      cmsghdr->cmsg_level);
            }
    } else
        SXEL6("UDP message not received from fd %d (error=%s%s)", fd, strerror(errno), header.msg_flags & MSG_TRUNC ? " (MSG_TRUNC)" : "");

    if (address_len)
        *address_len = header.msg_namelen;

    errno = rerrno;

    return size;
}
