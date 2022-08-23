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

#define _GNU_SOURCE
#include "kit-mock.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#define __cdecl    // __cdecl is the default on Windows, and the one and only on other platforms
#define MOCK_DEF(type, scope, function, parameters) type (__ ## scope * kit_mock_ ## function) parameters = function

/* Declarations of the mock function table.   For Windows, CRT functions have __cdecl, OS (e.g. WinSock) APIs have __stdcall
 *       Return Type   CRT/OS   Function      Parameter Types
 */
MOCK_DEF(MOCK_SOCKET, stdcall, accept,       (MOCK_SOCKET, struct sockaddr *, socklen_t *));
MOCK_DEF(MOCK_SOCKET, stdcall, accept4,      (MOCK_SOCKET, struct sockaddr *, socklen_t *, int));
MOCK_DEF(int,         stdcall, bind,         (MOCK_SOCKET, const struct sockaddr *, socklen_t));
MOCK_DEF(int,         stdcall, getaddrinfo,  (const char *, const char *, const struct addrinfo *, struct addrinfo **));
MOCK_DEF(void *,      cdecl,   calloc,       (size_t, size_t));    // CONVENTION EXCLUSION: Allow mocking libc calloc
MOCK_DEF(int,         cdecl,   close,        (int));
MOCK_DEF(int,         stdcall, connect,      (MOCK_SOCKET, const struct sockaddr *, socklen_t));
MOCK_DEF(FILE *,      cdecl,   fopen,        (const char * file, const char * mode));
MOCK_DEF(int,         cdecl,   fputs,        (const char * string, FILE * file));
MOCK_DEF(int,         stdcall, getsockopt,   (MOCK_SOCKET, int, int, MOCK_SOCKET_VOID *, socklen_t * __restrict));
MOCK_DEF(int,         cdecl,   gettimeofday, (struct timeval * __restrict tm, __timezone_ptr_t __restrict tz));
MOCK_DEF(off_t,       cdecl,   lseek,        (int fd, off_t offset, int whence));
MOCK_DEF(int,         stdcall, listen,       (MOCK_SOCKET, int));
MOCK_DEF(void *,      cdecl,   malloc,       (size_t));            // CONVENTION EXCLUSION: Allow mocking libc malloc
MOCK_DEF(ssize_t,     stdcall, recvfrom,     (MOCK_SOCKET, MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int, struct sockaddr *, socklen_t *));
MOCK_DEF(ssize_t,     stdcall, send,         (MOCK_SOCKET, const MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int));
MOCK_DEF(ssize_t,     stdcall, sendto,       (MOCK_SOCKET, const MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int, const struct sockaddr *, socklen_t));
MOCK_DEF(MOCK_SOCKET, stdcall, socket,       (int, int, int));
MOCK_DEF(ssize_t,     cdecl,   recv,         (int, void *, size_t, int));
MOCK_DEF(ssize_t,     cdecl,   write,        (int, const void *, size_t));
MOCK_DEF(ssize_t,     cdecl,   readv,        (int, const struct iovec *, int));
MOCK_DEF(ssize_t,     cdecl,   writev,       (int, const struct iovec *, int));
MOCK_DEF(int,         cdecl,   clock_gettime,(clockid_t clk_id, struct timespec *tp));

#ifdef _WIN32
MOCK_DEF(DWORD,       stdcall, timeGetTime,  (void));
MOCK_DEF(int,         cdecl,   mkdir,        (const char * pathname));
#else
# if defined(__APPLE__)
MOCK_DEF(int,         stdcall, sendfile,     (int, int, off_t, off_t *, struct sf_hdtr *, int));
# elif defined(__FreeBSD__)
MOCK_DEF(int,         stdcall, sendfile,     (int, int, off_t, size_t, struct sf_hdtr *, off_t *, int));
# else
MOCK_DEF(ssize_t, stdcall, sendfile,     (int, int, off_t *, size_t));
# endif
MOCK_DEF(int,         cdecl,   mkdir,        (const char * pathname, mode_t mode));
MOCK_DEF(void,        cdecl,   openlog,      (const char * ident, int option, int facility));
MOCK_DEF(void,        cdecl,   syslog,       (int priority, const char * format, ...));
#endif

/* The following mock was removed because the function that it mocks cannot be linked statically with the debian version of glibc.
 * If you need to mock this function, put it in a separate file so that only the program that uses it requires dynamic linking.
 */
#ifdef DYNAMIC_LINKING_REQUIRED
MOCK_DEF(struct hostent *, stdcall, gethostbyname, (const char *));
#endif
