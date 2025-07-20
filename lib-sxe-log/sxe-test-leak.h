/*
 * Copyright (c) 2023 Cisco Systems
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

#ifndef __SXE_TEST_LEAK__
#define __SXE_TEST_LEAK__ 1

static struct {
    unsigned num;
    void **mem;
} test_bucket;

static inline void *
test_leak(void *v)
{
    test_bucket.mem = realloc(test_bucket.mem, sizeof(*test_bucket.mem) * ++test_bucket.num);   // CONVENTION EXCLUSION: OK to use in test code
    return test_bucket.mem[test_bucket.num - 1] = v;
}

static inline void
test_plug(void)
{
    unsigned i;

    for (i = 0; i < test_bucket.num; i++)
        free(test_bucket.mem[i]);    // CONVENTION EXCLUSION: OK to use in test code
    free(test_bucket.mem);           // CONVENTION EXCLUSION: OK to use in test code
    test_bucket.mem = NULL;
    test_bucket.num = 0;
}

#endif
