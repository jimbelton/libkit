/* Copyright (c) 2010 Sophos Group.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __SXE_MOCK_H__
#define __SXE_MOCK_H__

#ifdef __SXE_TEST_MEMORY_H__
#   ifndef SXE_MOCK_NO_CALLOC
#       error "sxe-test-memory.h is incompatible with mock.h; define SXE_MOCK_NO_CALLOC before to mix them"
#   endif
#endif

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#   include <direct.h>
#else
#   include <netdb.h>
#   include <syslog.h>

#   if defined(__APPLE__) || defined(__FreeBSD__)
#       include <sys/uio.h>
#   else
#       include <sys/sendfile.h>
#   endif
#endif

#include "kit-port.h"

/* CONVENTION EXCLUSION: system functions mocked using #define */

#ifdef MOCK

#define MOCK_SET_HOOK(func, test) kit_mock_ ## func = (test)
#define MOCK_SKIP_START(numtests)
#define MOCK_SKIP_END

#ifdef _WIN32
#   define MOCK_SOCKET         SOCKET
#   define MOCK_SOCKET_VOID    char
#   define MOCK_SOCKET_SSIZE_T int

#else  /* UNIX */
#   define MOCK_SOCKET         int
#   define MOCK_SOCKET_VOID    void
#   define MOCK_SOCKET_SSIZE_T size_t

#   ifdef __APPLE__
#       define __timezone_ptr_t  void *
#   elif defined(__FreeBSD__)
#       define __timezone_ptr_t  struct timezone *
#   elif __GNUC__ >= 9
        typedef void *__restrict __timezone_ptr_t;
#   endif
#endif

/* External definitions of the mock function table
 *  - __stdcall signifies that Windows implements this function in an OS API, not in the C runtime
 */
extern MOCK_SOCKET (__stdcall * kit_mock_accept)      (MOCK_SOCKET, struct sockaddr *, socklen_t *);
extern MOCK_SOCKET (__stdcall * kit_mock_accept4)     (MOCK_SOCKET, struct sockaddr *, socklen_t *, int);
extern int         (__stdcall * kit_mock_bind)        (MOCK_SOCKET, const struct sockaddr *, socklen_t);
extern int         (__stdcall * kit_mock_getaddrinfo) (const char *, const char *, const struct addrinfo *, struct addrinfo **);
extern void *      (          * kit_mock_calloc)      (size_t, size_t);
extern int         (          * kit_mock_close)       (int);
extern int         (__stdcall * kit_mock_connect)     (MOCK_SOCKET, const struct sockaddr *, socklen_t);
extern FILE *      (          * kit_mock_fopen)       (const char * file, const char * mode);
extern int         (          * kit_mock_fputs)       (const char * string, FILE * file);
extern int         (__stdcall * kit_mock_getsockopt)  (MOCK_SOCKET, int, int, MOCK_SOCKET_VOID *, socklen_t * __restrict);
extern int         (          * kit_mock_gettimeofday)(struct timeval * __restrict, __timezone_ptr_t);
extern int         (__stdcall * kit_mock_listen)      (MOCK_SOCKET, int);
extern off_t       (          * kit_mock_lseek)       (int fd, off_t offset, int whence);
extern void *      (          * kit_mock_malloc)      (size_t);
extern ssize_t     (__stdcall * kit_mock_recvfrom)    (MOCK_SOCKET, MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int,
                                                       struct sockaddr *, socklen_t *);
extern ssize_t     (__stdcall * kit_mock_send)        (MOCK_SOCKET, const MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int);
extern ssize_t     (__stdcall * kit_mock_sendto)      (MOCK_SOCKET, const MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int,
                                                       const struct sockaddr *, socklen_t);
extern MOCK_SOCKET (__stdcall * kit_mock_socket)      (int, int, int);
extern ssize_t     (          * kit_mock_recv)        (int, void *, size_t, int);
extern ssize_t     (          * kit_mock_write)       (int, const void *, size_t);
extern ssize_t     (          * kit_mock_readv)       (int, const struct iovec *, int);
extern ssize_t     (          * kit_mock_writev)      (int, const struct iovec *, int);
extern int         (          * kit_mock_clock_gettime)(clockid_t clk_id, struct timespec *tp);

#ifdef _WIN32
extern DWORD       (__stdcall * kit_mock_timeGetTime) (void);
extern int         (          * kit_mock_mkdir)       (const char * pathname);
#else /* UNIX */
#   if defined(__APPLE__)
extern int         (__stdcall * kit_mock_sendfile)    (int, int, off_t, off_t *, struct sf_hdtr *, int);
#   elif defined(__FreeBSD__)
extern int         (__stdcall * kit_mock_sendfile)    (int, int, off_t, size_t, struct sf_hdtr *, off_t *, int);
#   else /* not __APPLE__ and not __FreeBSD__ */
extern ssize_t     (__stdcall * kit_mock_sendfile)    (int, int, off_t *, size_t);
#   endif

extern int         (          * kit_mock_mkdir)       (const char * pathname, mode_t mode);
extern void        (          * kit_mock_openlog)     (const char * ident, int option, int facility);
extern void        (          * kit_mock_syslog)      (int priority, const char *format, ...);
#endif

#ifndef MOCK_IMPL

#define accept(fd, addr, len)                    (*kit_mock_accept)      ((fd), (addr), (len))
#define accept4(fd, addr, len, flags)            (*kit_mock_accept4)     ((fd), (addr), (len), (flags))
#define bind(fd, addr, len)                      (*kit_mock_bind)        ((fd), (addr), (len))
#define getaddrinfo(node, service, hints, res)   (*kit_mock_getaddrinfo) ((node), (service), (hints), (res))

#ifndef SXE_MOCK_NO_CALLOC
#   define calloc(num, size)                     (*kit_mock_calloc)      ((num), (size))
#endif

#define close(fd)                                (*kit_mock_close)       (fd)
#define connect(fd, addr, len)                   (*kit_mock_connect)     ((fd), (addr), (len))
#define fopen(file, mode)                        (*kit_mock_fopen)       ((file), (mode))
#define fputs(string, file)                      (*kit_mock_fputs)       ((string), (file))
#define getsockopt(fd, lev, oname, oval, olen)   (*kit_mock_getsockopt)  ((fd), (lev), (oname), (oval), (olen))
#define gettimeofday(tm,tz)                      (*kit_mock_gettimeofday)((tm), (tz))
#define listen(fd, backlog)                      (*kit_mock_listen)      ((fd), (backlog))
#define lseek(fd, offset, whence)                (*kit_mock_lseek)       ((fd), (offset), (whence))
#define malloc(size)                             (*kit_mock_malloc)      (size)
#define send(fd, buf, len, flags)                (*kit_mock_send)        ((fd), (buf), (len), (flags))

#ifdef WINDOWS_NT
#define timeGetTime()                            (*kit_mock_timeGetTime) ()
#define mkdir(pathname)                          (*kit_mock_mkdir)       (pathname)
#else
#if defined(__APPLE__)
#define sendfile(in_fd, out_fd, off, len, hdr, _r) (*kit_mock_sendfile)  ((in_fd), (out_fd), (off), (len), (hdr), (_r))
#elif defined(__FreeBSD__)
#define sendfile(in_fd, out_fd, off, nbytes, hdr, sbytes, flags) (*kit_mock_sendfile)  ((in_fd), (out_fd), (off), (nbytes), (hdr), (sbytes), (flags))
#else
#define sendfile(out_fd, in_fd, offset, count)   (*kit_mock_sendfile)    ((out_fd), (in_fd), (offset), (count))
#endif
#define mkdir(pathname, mode)                    (*kit_mock_mkdir)       ((pathname), (mode))
#define openlog(ident, option, facility)         (*kit_mock_openlog)     ((ident), (option), (facility))
#define syslog(priority, ...)                    (*kit_mock_syslog)      ((priority), __VA_ARGS__)
#endif
#define sendto(fd, buf, len, flags, to, tolen)   (*kit_mock_sendto)      ((fd), (buf), (len), (flags), (to), (tolen))
#define socket(dom, typ, pro)                    (*kit_mock_socket)      ((dom), (typ), (pro))
#define recv(fd, buf, len, flags)                (*kit_mock_recv)        ((fd), (buf), (len), (flags))
#define recvfrom(fd, buf, len, flags, to, tolen) (*kit_mock_recvfrom)    ((fd), (buf), (len), (flags), (to), (tolen))
#define write(fd, buf, len)                      (*kit_mock_write)       ((fd), (buf), (len))
#define readv(fd, iov, iovcnt)                   (*kit_mock_readv)       ((fd), (iov), (iovcnt))
#define writev(fd, iov, iovcnt)                  (*kit_mock_writev)      ((fd), (iov), (iovcnt))
#define clock_gettime(clk_id, tp)                (*kit_mock_clock_gettime)((clk_id), (tp))

#endif /* !MOCK_IMPL     */

#else  /* !defined(MOCK) */

#define MOCK_SET_HOOK(func, test)  ((void)test)
#define MOCK_SKIP_START(num_tests) skip_start(1, (num_tests), "- this test requires mock functions")
#define MOCK_SKIP_END              skip_end

#endif /* !defined(MOCK) */

/* The following mock was removed because the function that it mocks cannot be linked statically with the debian version of glibc.
 */
#ifdef DYNAMIC_LINKING_REQUIRED
#define gethostbyname(name)                      (*kit_mock_gethostbyname)((name))
extern struct hostent * (__stdcall * kit_mock_gethostbyname)(const char *);
#endif

#endif /* __SXE_MOCK_H__ */
