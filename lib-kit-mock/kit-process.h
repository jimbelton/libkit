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

#ifndef KIT_PROCESS_H
#define KIT_PROCESS_H

#ifdef __WIN32

#include <process.h>
#define kit_spawnl(mode, command, arg0, ...)  spawnl(mode, command, arg0, __VA_ARGS__)
#define kit_cwait(status, process_id, action) cwait(status, process_id, action)

#else // Emulate Windows spawn and cwait functions on UNIX



#include <unistd.h>    /* For intptr_t */

#define P_NOWAIT   1
#define WAIT_CHILD 0

#ifdef __cplusplus
extern "C" {
#endif

extern intptr_t kit_cwait(int * status, intptr_t process_id, int action);
extern intptr_t kit_spawnl(int mode, const char * command, const char * arg0, ...);

#ifdef __cplusplus
}
#endif

#endif // !_WIN32

#endif // KIT_PROCESS_H
