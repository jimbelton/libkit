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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "kit-tensor.h"
#include "kit-tensor-private.h"
#include "sxe-log.h"

bool
kit_tensor_make_begin(struct kit_tensor *tensor, const char *dim_line, size_t *num_values)
{
    unsigned consumed;

    *num_values = 0;

    if (memcmp(dim_line, "DIMS:", 5) != 0) {
        SXEL2(": Dimension line should begin with 'DIMS:', not '%.5s'", dim_line);
        return false;
    }

    for (dim_line += 5, tensor->num_dims = 0; tensor->num_dims < 2; tensor->num_dims++) {
        if (sscanf(dim_line, "%u%n", &tensor->dimension[tensor->num_dims], &consumed) != 1) {
            SXEL2(": Unsigned integer expected in dimension line not '%.10s'", dim_line);
            return false;
        }

        dim_line += consumed;

        if (*dim_line == ':')
            dim_line++;
        else if (*dim_line == '\0')
            break;
        else {
            SXEL2(": Expected ':' or EOL in dimension line after value, not '%c'", *dim_line);
            return false;
        }
    }

    tensor->num_dims++;

    if (!(tensor->value = MOCKFAIL(KIT_TENSOR_MAKE_BEGIN, NULL, kit_malloc(sizeof(*tensor->value) * kit_tensor_num_values(tensor))))) {
        SXEL2(": Failed to allocate %zu bytes for tensor values", sizeof(*tensor->value) * kit_tensor_num_values(tensor));
        return false;
    }

    return true;
}

bool
kit_tensor_make_add_values(struct kit_tensor *tensor, const char *values_line, size_t *num_values)
{
    unsigned consumed;

    while (*values_line) {
        if (*num_values >= kit_tensor_num_values(tensor)) {
            SXEL2(": Array is full but line still contains '%s'", values_line);
            return false;
        }

        if (sscanf(values_line, "%f%n", &tensor->value[*num_values], &consumed) != 1) {
            SXEL2(": Float expected in value line not '%.20s'", values_line);
            return false;
        }

        (*num_values)++;
        values_line += consumed;

        if (*values_line == ',')
            values_line++;
        else if (*values_line == '\0')
            break;
        else {
            SXEL2(": Expected ',' or EOL in line after value, not '%c'", *values_line);
            return false;
        }
    }

    return true;
}

bool
kit_tensor_make_end(const struct kit_tensor *tensor, size_t num_values)
{
    if (num_values != kit_tensor_num_values(tensor)) {
        SXEL2(": Failed to end tensor construction; got %zu values, expected %zu", kit_tensor_num_values(tensor), num_values);
        return false;
    }

    return true;
}

void
kit_tensor_fini(struct kit_tensor *a)
{
    a->num_dims = 0;
    memset(a->dimension, '\0', sizeof(a->dimension) );
    kit_free(a->value);
}

void
kit_tensor_init(struct kit_tensor *a)
{
    a->num_dims = 0;
    for (int i = 0; i < KIT_TENSOR_MAX_DIMS; i++)
        a->dimension[i] = 0;
}

void
kit_array_inner_product(const float *a, int askip, const float *b, int bskip, int len, float *c)
{
    *c = 0;
    for (int i = 0; i < len; i++)
        *c += a[i * askip] * b[i * bskip];
}

// kit_tensor_matmul function assumes that output tensor c is allocated and initialized
bool
kit_tensor_matmul(const struct kit_tensor *a, const struct kit_tensor *b, struct kit_tensor *c)
{
    bool ret = false;
    unsigned dim, index;

    SXEA1(c->sz, "Tensor size %d has no value", c->sz);
    SXEA1(c->b0, "Tensor offset b0 %d has no value", c->b0);
    SXEA1(c->b1, "Tensor offset b1 %d has no value", c->b1);

    dim = a->dimension[2];
    if ((dim != b->dimension[1]) ||
        (a->dimension[0] != b->dimension[0]) ||
        (kit_tensor_matmul_sz(a, b) != kit_tensor_sz(c)))
        goto OUT;

    c->num_dims = 2;
    c->dimension[0] = a->dimension[0];
    c->dimension[1] = a->dimension[1];
    c->dimension[2] = b->dimension[2];

    index = 0;
    for (unsigned k = 0; k < a->dimension[0]; k++)
        for (unsigned m = 0; m < a->dimension[1]; m++)
            for (unsigned n = 0; n < b->dimension[2]; n++) {
                kit_array_inner_product(a->value + k * a->dimension[1] * a->dimension[2] + m * a->dimension[2], 1, b->value + k * b->dimension[1] * b->dimension[2] + n, b->dimension[2], dim, &c->value[index]);
                index += 1;
            }

    ret = true;
OUT:
    return ret;
}

// kit_tensor_embedding function assumes that output tensor out is allocated and initialized
bool
kit_tensor_embedding(const struct kit_tensor *a, const unsigned *indices, unsigned n, struct kit_tensor *out)
{
    bool ret = false;
    unsigned wrap;

    if (a->num_dims == 2) {
        wrap = a->dimension[2];
        out->dimension[0] = 1;
        out->dimension[1] = n;
        out->dimension[2] = a->dimension[2];
    } else if (a->num_dims == 3) {
        wrap = a->dimension[1] * a->dimension[2];
        out->dimension[0] = n;
        out->dimension[1] = a->dimension[1];
        out->dimension[2] = a->dimension[2];
    } else
        goto OUT;

    out->num_dims = a->num_dims;

    SXEA1(out->sz, "Tensor size %d has no value", out->sz);
    SXEA1(out->b0, "Tensor offset b0 %d has no value", out->b0);
    SXEA1(out->b1, "Tensor offset b1 %d has no value", out->b1);

    for (unsigned i = 0; i < n; i++) {
        memcpy(&out->value[i * wrap], &a->value[wrap * indices[i]], sizeof(*a->value) * wrap);
    }

    ret = true;
OUT:
    return ret;
}

// kit_tensor_permute function assumes that output tensor b is allocated and initialized
bool
kit_tensor_permute(const struct kit_tensor *a, const unsigned dims[KIT_TENSOR_MAX_DIMS], struct kit_tensor *b)
{
    bool ret = false;
    unsigned index, idx;
    unsigned base[KIT_TENSOR_MAX_DIMS] = {0};

    if (kit_tensor_sz(a) != kit_tensor_sz(b))
        goto OUT;

    base[0] = kit_tensor_sz(a) / a->dimension[0];
    base[1] = base[0] / a->dimension[1];
    base[2] = base[1] / a->dimension[2];
    b->dimension[0] = a->dimension[dims[0]];
    b->dimension[1] = a->dimension[dims[1]];
    b->dimension[2] = a->dimension[dims[2]];

    SXEA1(b->sz, "Tensor size %d has no value", b->sz);
    SXEA1(b->b0, "Tensor offset b0 %d has no value", b->b0);
    SXEA1(b->b1, "Tensor offset b1 %d has no value", b->b1);

    index = 0;
    for (unsigned k = 0; k < b->dimension[0]; k++)
        for (unsigned m = 0; m < b->dimension[1]; m++)
            for (unsigned n = 0; n < b->dimension[2]; n++) {
                idx = base[dims[0]] * k + base[dims[1]] * m + base[dims[2]] * n;
                b->value[index] = a->value[idx];
                index++;
            }

    ret = true;
OUT:
    return ret;
}

bool
kit_tensor_transpose(const struct kit_tensor *a, unsigned i, unsigned j, struct kit_tensor *b)
{
    unsigned dims[KIT_TENSOR_MAX_DIMS] = {0, 1, 2}, tmp;

    tmp = dims[i];
    dims[i] = dims[j];
    dims[j] = tmp;
    return kit_tensor_permute(a, dims, b);
}

bool
kit_tensor_conv1d(const struct kit_tensor *a, const struct kit_tensor *b, const unsigned stride, struct kit_tensor *c)
{
    bool ret = false;
    unsigned kernel = a->dimension[2], unfolds, foldloc, index;

    SXEA6(kernel, "A valid tensor is required");

    if (a->dimension[0] != b->dimension[1])
        goto OUT;

    if (a->dimension[0] != a->dimension[1])
        goto OUT;

    if (a->dimension[0] != c->dimension[1])
        goto OUT;

    unfolds = 0;
    for (int i = 0; i + kernel <= b->dimension[2]; i+= stride)
        unfolds++;

    if (unfolds != c->dimension[2])
        goto OUT;                           /* COVERAGE EXCLUSION - Not sure how to test */

    if (b->dimension[0] != c->dimension[0])
        goto OUT;

    index = 0;

    for (unsigned batch_item = 0; batch_item < b->dimension[0]; batch_item++)
        for (unsigned k = 0; k < a->dimension[0]; k++) {
            for (foldloc = 0; foldloc + a->dimension[2] <= b->dimension[2]; foldloc += stride) {
                c->value[index] = 0;
                for (unsigned m = 0; m < a->dimension[1]; m++)
                    for (unsigned n = 0; n < a->dimension[2]; n++)
                        c->value[index] += a->value[kit_dotp2(a->b0, a->b1, k, m) + n] * b->value[kit_dotp2(b->b0, b->b1, batch_item, m) + foldloc + n];
                index++;
            }
        }
    ret = true;
OUT:
    return ret;
}

bool
kit_tensor_batchnorm1d_affine(const struct kit_tensor *bn, const struct kit_tensor *x, struct kit_tensor *c)
{
    bool ret = false;
    unsigned index;

    if (!(bn->dimension[2] == 4 && bn->dimension[1] == x->dimension[1] && bn->dimension[0] == 1))
        goto OUT;

    if (!(x->dimension[0] == c->dimension[0] && x->dimension[1] == c->dimension[1] && x->dimension[2] == c->dimension[2]))
        goto OUT;

    index = 0;

    for (unsigned k = 0; k < x->dimension[0]; k++)
        for (unsigned m = 0; m < x->dimension[1]; m++)
            for (unsigned n = 0; n < x->dimension[2]; n++) {
                c->value[index] = (bn->value[kit_dotp2(bn->b0, bn->b1, 0, m) + 2] *
                                    (x->value[kit_dotp2(x->b0, x->b1, k, m) + n] - bn->value[kit_dotp2(bn->b0, bn->b1, 0, m)]) /
                                    sqrtf(bn->value[kit_dotp2(bn->b0, bn->b1, 0, m) + 1])) + bn->value[kit_dotp2(bn->b0, bn->b1, 0, m) + 3];
                index++;
            }

    ret = true;
OUT:
    return ret;
}

bool
kit_tensor_apply(struct kit_tensor *a, float (*cb)(float x))
{
    for (unsigned i = 0; i < kit_tensor_sz(a); i++)
        a->value[i] = cb(a->value[i]);

    return true;
}

float
kit_relu(float x)
{
    return x > 0 ? x : 0;
}

bool
kit_tensor_relu(struct kit_tensor *a)
{
    return kit_tensor_apply(a, kit_relu);
}

bool
kit_tensor_flatten(struct kit_tensor *a)
{
    for (unsigned i = 0; i < KIT_TENSOR_MAX_DIMS - 1; i++) {
        a->dimension[KIT_TENSOR_MAX_DIMS - 1] *= a->dimension[i];
        a->dimension[i] = 1;
    }
    return true;
}

bool
kit_tensor_sum(const struct kit_tensor *a, const struct kit_tensor *b, struct kit_tensor *c)
{
    bool ret = false;

    if (a->dimension[0] != b->dimension[0] ||
        a->dimension[1] != b->dimension[1] ||
        a->dimension[2] != b->dimension[2] ||
        b->dimension[0] != c->dimension[0] ||
        b->dimension[1] != c->dimension[1] ||
        b->dimension[2] != c->dimension[2])
        goto OUT;

    for (unsigned i = 0; i < kit_tensor_sz(a); i++)
        c->value[i] = a->value[i] + b->value[i];
    ret = true;

OUT:
    return ret;
}

void
kit_tensor_zeros(struct kit_tensor *a)
{
    memset(a->value, 0, a->sz * sizeof(*a->value));
}

bool
kit_tensor_dimset(struct kit_tensor *a, const unsigned dims[KIT_TENSOR_MAX_DIMS])
{
    a->dimension[0] = dims[0];
    a->dimension[1] = dims[1];
    a->dimension[2] = dims[2];
    a->num_dims = a->dimension[0] > 1 ? 3 : (a->dimension[1] > 1 ? 2 : 1);

    // Set tensor size and offsets
    a->sz = a->dimension[0] * a->dimension[1] * a->dimension[2];
    a->b0 = a->dimension[1] * a->dimension[2];
    a->b1 = a->dimension[2];

    return true;
}
