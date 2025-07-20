/*
 * Copyright (c) 2024 Cisco Systems, Inc. and its affiliates
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

#include <stdbool.h>

#define KIT_TENSOR_MAX_DIMS 3

struct kit_tensor
{
    unsigned num_dims;        // 1, 2, or 3
    unsigned dimension[KIT_TENSOR_MAX_DIMS];
    float   *value;
    unsigned sz;
    unsigned b0;    // Dimension 0 offset in linearized tensor
    unsigned b1;    // Dimension 1 offset in linearized tensor
};

#include "kit-tensor-proto.h"

static inline size_t
kit_tensor_num_values(const struct kit_tensor *tensor)
{
    return (size_t)tensor->dimension[0] * (tensor->num_dims > 1 ? tensor->dimension[1] : 1)
                                        * (tensor->num_dims > 2 ? tensor->dimension[2] : 1);
    // I wonder if this calculation should be the following:
    // return (size_t)tensor->dimension[0] * tensor->dimension[1] * tensor->dimension[2];
    // the reason is that if any dimension is zero, the tensor should be "0" shaped.
}

static inline unsigned
kit_dotp2(const unsigned b0, const unsigned b1, const unsigned k0, const unsigned k1)
{
    return b0 * k0 + b1 * k1;
}

static inline unsigned
kit_tensor_matmul_sz(const struct kit_tensor *a, const struct kit_tensor *b)
{
    return a->dimension[0] * a->dimension[1] * b->dimension[2];
}

static inline unsigned
kit_tensor_sz(const struct kit_tensor *a)
{
    unsigned ret = a->sz;
    if (a->sz == 0)
        ret = a->dimension[0] * a->dimension[1] * a->dimension[2];
    return ret;
}

static inline float
kit_tensor_get_k_m_n(const struct kit_tensor *a, const unsigned k, const unsigned m, const unsigned n)
{
    return a->value[a->b0 * k + a->b1 * m + n];
}