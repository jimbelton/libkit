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

#include <tap.h>
#include <string.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "kit-tensor.h"
#include "kit-tensor-private.h"
#include "sxe-log.h"

static bool
tensor_make_kxmxn(float *a, const unsigned k, const unsigned m, const unsigned n, struct kit_tensor *out)
{
    out->num_dims = k > 1 ? 3 : (m > 1 ? 2 : 1);
    out->dimension[0] = k;
    out->dimension[1] = m;
    out->dimension[2] = n;

    // Set tensor size and offsets
    out->sz = out->dimension[0] * out->dimension[1] * out->dimension[2];
    out->b0 = out->dimension[1] * out->dimension[2];
    out->b1 = out->dimension[2];

    if (!(out->value = kit_malloc(sizeof(*out->value) * k * m * n))) {
        SXEL2(": Failed to allocate %zu bytes for tensor values", sizeof(*out->value) * k * m * n);
        return false;
    }
    memcpy(out->value, a, sizeof(float) * k * m * n);
    return true;
}

int
main(void)
{
    struct kit_tensor tensor;
    size_t            num_values;

    plan_tests(279);
    kit_memory_initialize(KIT_MEMORY_ABORT_ON_ENOMEM | KIT_MEMORY_CHECK_OVERFLOWS);
    uint64_t start_allocations = kit_memory_allocations();    // Clocked the initial # of memory allocations

    diag("Happy path parsing");
    {
        num_values = -1;
        ok(kit_tensor_make_begin(&tensor, "DIMS:3:10", &num_values), "Began constructing a tensor");
        is(tensor.num_dims,      2,                                  "2 dimensional tensor");
        is(tensor.dimension[0],  3,                                  "3 rows");
        is(tensor.dimension[1], 10,                                  "10 columns");
        is(num_values,           0,                                  "In the beginning, there are no values");

        ok(kit_tensor_make_add_values(&tensor,
                                      "-0.5928198099136353,-0.7330870032310486,-0.3953676223754883,-0.3157353401184082,0.39351019263267517,"
                                      "0.015202959068119526,-0.8514717817306519,0.06482086330652237,-0.3223743736743927,0.24368461966514587,"
                                      "2.9728217124938965,-0.1696321666240692,-0.9596973657608032,-2.1359124183654785,0.5595744848251343,"
                                      "2.1181836128234863,0.30913060903549194,2.052929639816284,2.037630796432495,-0.5081333518028259,"
                                      "0.5283480882644653,-0.11088214814662933,-0.17134040594100952,0.4158063530921936,-1.7265350818634033",
                                      &num_values),
           "Added 25 values");
        ok(kit_tensor_make_add_values(&tensor,
                                      "0.09420936554670334,0.5135141611099243,-0.4182659685611725,1.1057028770446777,0.8090568780899048", &num_values),
           "Added 5 more values");
        ok(kit_tensor_make_end(&tensor, num_values), "Ended constructing a tensor");
        kit_tensor_fini(&tensor);

    }

    diag("Not so happy path parsing");
    {
        num_values = -1;
        ok(!kit_tensor_make_begin(&tensor, "XYZW:3:10", &num_values), "Wrong prefix");
        ok(!kit_tensor_make_begin(&tensor, "DIMS:A:10", &num_values), "Wrong dimension");
        ok(!kit_tensor_make_begin(&tensor, "DIMS:3c10", &num_values), "Wrong delimiter");
        ok(kit_tensor_make_begin(&tensor, "DIMS:2:2", &num_values),   "Begin constructing a tensor");
        is(num_values,           0,                                   "In the beginning, there are no values");
        is(tensor.num_dims,      2,                                   "2 dimensional tensor");
        is(tensor.dimension[0],  2,                                   "2 rows");
        is(tensor.dimension[1],  2,                                   "2 columns");
        is(num_values,           0,                                   "In the beginning, there are no values");

        ok(!kit_tensor_make_add_values(&tensor,
                                       "-0.5928198099136353,-0.7330870032310486,-0.3953676223754883,-0.3157353401184082,0.39351019263267517",
                                       &num_values),
           "Added one more value than there is room");
        kit_tensor_fini(&tensor);
        ok(kit_tensor_make_begin(&tensor, "DIMS:2:2", &num_values), "Began constructing a tensor");
        ok(!kit_tensor_make_add_values(&tensor,
                                       "abracadabra,0.5135141611099243,-0.4182659685611725,1.1057028770446777",
                                       &num_values),
           "Added wrong values");
        kit_tensor_fini(&tensor);
        ok(kit_tensor_make_begin(&tensor, "DIMS:2:2", &num_values), "Began constructing a tensor");
        ok(!kit_tensor_make_add_values(&tensor,
                                       "-0.5928198099136353+0.5135141611099243,-0.4182659685611725,1.1057028770446777",
                                       &num_values),
           "Wrong delimiter in number array");
        ok(!kit_tensor_make_end(&tensor, num_values), "Ended constructing a wrong tensor should fail");

        kit_tensor_fini(&tensor);
        MOCKFAIL_START_TESTS(1, KIT_TENSOR_MAKE_BEGIN);
        ok(!kit_tensor_make_begin(&tensor, "DIMS:3:10", &num_values),
           "Correctly defined tensor, but mocking allocation failure and returning false");
        kit_tensor_fini(&tensor);
        MOCKFAIL_END_TESTS();
    }

    diag("Let's do some math");
    {
        struct kit_tensor a, b, c, d, e;
        float w,
              x[4]   = {1.0, 2.0, 3.0, 4.0},
              m1[4]  = {0.0},
              m2[1]  = {0.0},
              m3[16] = {0.0},
              m4[8]  = {0.0};
        float y[27];

        kit_array_inner_product(x, 1, x, 1, 4, &w);
        is(w, 30, "Kit 4-dimensional array inner product calculation correct");

        tensor_make_kxmxn(x, 1, 2, 2, &a);
        tensor_make_kxmxn(x, 1, 2, 2, &b);
        tensor_make_kxmxn(m1, 1, 2, 2, &c);
        tensor_make_kxmxn(y, 3, 3, 3, &d);
        tensor_make_kxmxn(y, 2, 2, 2, &e);

        is(kit_tensor_sz(&a), 4, "1x2x2 tensor has size 4");
        is(kit_tensor_matmul_sz(&a, &b), 4, "1x2x2 matmul 1x2x2 tensor matmul results in tensor size 4");
        ok(!kit_tensor_matmul(&a, &d, &b), "1x2x2 matmul 3x3x3 tensor matmul cannot be performed");
        ok(!kit_tensor_matmul(&a, &e, &b), "1x2x2 matmul 2x2x2 tensor matmul cannot be performed");
        ok(!kit_tensor_matmul(&a, &b, &d), "1x2x2 matmul 1x2x2 tensor matmul cannot be performed with output 3x3x3");

        // We expect the following
        //     [            [            [
        //       [1, 2],      [1, 2],      [7, 10],
        //       [3, 4],      [3, 4]       [15, 22]
        //     ]            ]            ]
        //          a    @       b    =      c

        kit_tensor_matmul(&a, &b, &c);
        is(c.num_dims, 2, "2x2 matmul outputs correct dims");
        is(c.dimension[0], 1, "2x2 matmul outputs correct dimension[0]");
        is(c.dimension[1], 2, "2x2 matmul outputs correct dimension[1]");
        is(c.dimension[2], 2, "2x2 matmul outputs correct dimension[2]");
        is(c.value[0], 7.0, "2x2 matmul x11 correct");
        is(c.value[1], 10.0, "2x2 matmul x12 correct");
        is(c.value[2], 15.0, "2x2 matmul x21 correct");
        is(c.value[3], 22.0, "2x2 matmul x22 correct");
        kit_tensor_fini(&c); // kit_tensor_matmul allocates a new resulting tensor c and needs to be freed

        // Turn a into 1x4 and b into 4x1 then multiply
        a.dimension[0] = 1;
        a.dimension[1] = 1;
        a.dimension[2] = 4;
        b.dimension[0] = 1;
        b.dimension[1] = 4;
        b.dimension[2] = 1;

        tensor_make_kxmxn(m2, 1, 1, 1, &c);
        // We expect the following
        //    [1, 2, 3, 4]        [[1], [2], [3], [4]]     [[30]]
        //         a          @           b             =   c

        kit_tensor_matmul(&a, &b, &c);
        is(c.num_dims, 2, "1x4 by 4x1 matmul outputs correct dims");
        is(c.dimension[0], 1, "1x4 by 4x1 matmul outputs correct dimension[0]");
        is(c.dimension[1], 1, "1x4 by 4x1 matmul outputs correct dimension[1]");
        is(c.dimension[2], 1, "1x4 by 4x1 matmul outputs correct dimension[2]");
        is(c.value[0], 30.0, "1x4 by 4x1 matmul x11 correct");
        kit_tensor_fini(&c); // kit_tensor_matmul allocates a new resulting tensor c and needs to be freed

        // Turn a into 4x1 and b into 1x4 then multiply
        a.dimension[0] = 1;
        a.dimension[1] = 4;
        a.dimension[2] = 1;
        b.dimension[0] = 1;
        b.dimension[1] = 1;
        b.dimension[2] = 4;

        tensor_make_kxmxn(m3, 1, 4, 4, &c);
        // We expect the following
        //                                           [
        //                                              [1, 2, 3, 4],
        //                                              [2, 4, 6, 8],
        //                                              [3, 6, 9, 12],
        //                                              [4, 8, 12, 16]
        //   [[1], [2], [3], [4]]     [1, 2, 3, 4]   ]
        //              a          @      b        =       c

        kit_tensor_matmul(&a, &b, &c);
        is(c.num_dims, 2, "4x1 by 1x4 matmul outputs correct dims");
        is(c.dimension[0], 1, "4x1 by 1x4 matmul outputs correct dimension[0]");
        is(c.dimension[1], 4, "4x1 by 1x4 matmul outputs correct dimension[1]");
        is(c.dimension[2], 4, "4x1 by 1x4 matmul outputs correct dimension[2]");
        is(c.value[0], 1.0, "4x1 by 1x4 matmul x11 correct");
        is(c.value[1], 2.0, "4x1 by 1x4 matmul x12 correct");
        is(c.value[2], 3.0, "4x1 by 1x4 matmul x13 correct");
        is(c.value[3], 4.0, "4x1 by 1x4 matmul x14 correct");
        is(c.value[4], 2.0, "4x1 by 1x4 matmul x21 correct");
        is(c.value[5], 4.0, "4x1 by 1x4 matmul x22 correct");
        is(c.value[6], 6.0, "4x1 by 1x4 matmul x23 correct");
        is(c.value[7], 8.0, "4x1 by 1x4 matmul x24 correct");
        is(c.value[8], 3.0, "4x1 by 1x4 matmul x31 correct");
        is(c.value[9], 6.0, "4x1 by 1x4 matmul x32 correct");
        is(c.value[10], 9.0, "4x1 by 1x4 matmul x33 correct");
        is(c.value[11], 12.0, "4x1 by 1x4 matmul x34 correct");
        is(c.value[12], 4.0, "4x1 by 1x4 matmul x41 correct");
        is(c.value[13], 8.0, "4x1 by 1x4 matmul x42 correct");
        is(c.value[14], 12.0, "4x1 by 1x4 matmul x43 correct");
        is(c.value[15], 16.0, "4x1 by 1x4 matmul x44 correct");
        kit_tensor_fini(&c); // kit_tensor_matmul allocates a new resulting tensor c and needs to be freed

        // Turn a into 2x2x1 and b into 2x1x2 then multiply
        a.dimension[0] = 2;
        a.dimension[1] = 2;
        a.dimension[2] = 1;
        b.dimension[0] = 2;
        b.dimension[1] = 1;
        b.dimension[2] = 2;

        tensor_make_kxmxn(m4, 2, 2, 2, &c);
        // We expect the following
        // [               [             [
        //   [[1], [2]],     [[1, 2]],     [[1, 2],  [2, 4]],
        //   [[3], [4]]      [[3, 4]]      [[9, 12], [12, 16]]
        // ]               ]             ]
        //      a       @       b     =         c

        kit_tensor_matmul(&a, &b, &c);
        is(c.num_dims, 2, "2x2x1 by 2x1x2 matmul outputs correct dims");
        is(c.dimension[0], 2, "2x2x1 by 2x1x2 matmul outputs correct dimension[0]");
        is(c.dimension[1], 2, "2x2x1 by 2x1x2 matmul outputs correct dimension[1]");
        is(c.dimension[2], 2, "2x2x1 by 2x1x2 matmul outputs correct dimension[2]");
        is(c.value[0], 1.0, "2x2x1 by 2x1x2 matmul x111 correct");
        is(c.value[1], 2.0, "2x2x1 by 2x1x2 matmul x112 correct");
        is(c.value[2], 2.0, "2x2x1 by 2x1x2 matmul x121 correct");
        is(c.value[3], 4.0, "2x2x1 by 2x1x2 matmul x122 correct");
        is(c.value[4], 9.0, "2x2x1 by 2x1x2 matmul x211 correct");
        is(c.value[5], 12.0, "2x2x1 by 2x1x2 matmul x212 correct");
        is(c.value[6], 12.0, "2x2x1 by 2x1x2 matmul x221 correct");
        is(c.value[7], 16.0, "2x2x1 by 2x1x2 matmul x222 correct");

        ok(!kit_tensor_matmul(&a, &d, &c), "Dimensions do not match");
        ok(!kit_tensor_matmul(&a, &e, &c), "Dimensions do not match");

        // Turn a into 1x2x2 and b into 2x2x1 then multiply
        a.dimension[0] = 1;
        a.dimension[1] = 2;
        a.dimension[2] = 2;
        b.dimension[0] = 2;
        b.dimension[1] = 2;
        b.dimension[2] = 1;
        ok(!kit_tensor_matmul(&a, &b, &c), "Dimensions do not match");
        kit_tensor_fini(&a);
        kit_tensor_fini(&b);
        kit_tensor_fini(&c);
        kit_tensor_fini(&d);
        kit_tensor_fini(&e);
    }

    diag("Let's test a tensor lookup");
    {
        struct kit_tensor a, b, c, d;
        float x[4];
        float w[16] = {0};
        unsigned y[4] = {1, 3, 2, 0}, z[2] = {1, 0};

        // Embedding shape: [[1, 2], [3, 4]]
        x[0] = 1.0, x[1] = 2.0, x[2] = 3.0, x[3] = 4.0;
        tensor_make_kxmxn(x, 1, 2, 2, &a);

        for (int i = 0; i < 4; i++)
            x[i] = 0;
        tensor_make_kxmxn(x, 1, 2, 2, &d);

        ok(kit_tensor_embedding(&a, z, 2, &d), "Embedding lookup performed on chars: '1', '0'");
        is(d.value[0], 3.0, "Embedding lookup for char '1' (1st dim) correct");
        is(d.value[1], 4.0, "Embedding lookup for char '1' (2nd dim) correct");
        is(d.value[2], 1.0, "Embedding lookup for char '0' (1st dim) correct");
        is(d.value[3], 2.0, "Embedding lookup for char '0' (2nd dim) correct");
        is(d.num_dims, 2, "Embedding lookup results in tensor of 2-dimensions");
        is(d.dimension[0], 1, "Embedding lookup result k-dimension correct");
        is(d.dimension[1], 2, "Embedding lookup result n-dimension correct");
        is(d.dimension[2], 2, "Embedding lookup result m-dimension correct");
        kit_tensor_fini(&d);

        // Embedding shape: [[1], [2], [3], [4]]
        a.dimension[1] = 4;
        a.dimension[2] = 1;

        for (int i = 0; i < 4; i++)
            x[i] = 0;
        tensor_make_kxmxn(x, 1, 2, 2, &b);

        ok(kit_tensor_embedding(&a, y, 4, &b), "Embedding lookup performed on chars: '1', '3', '2', '0'");
        is(b.value[0], 2.0, "Embedding lookup for char '1' correct");
        is(b.value[1], 4.0, "Embedding lookup for char '3' correct");
        is(b.value[2], 3.0, "Embedding lookup for char '2' correct");
        is(b.value[3], 1.0, "Embedding lookup for char '0' correct");
        is(b.num_dims, 2, "Embedding lookup results in tensor of 2-dimensions");
        is(b.dimension[0], 1, "Embedding lookup result k-dimension correct");
        is(b.dimension[1], 4, "Embedding lookup result n-dimension correct");
        is(b.dimension[2], 1, "Embedding lookup result m-dimension correct");
        kit_tensor_fini(&a);
        kit_tensor_fini(&b);

        tensor_make_kxmxn(w, 2, 2, 4, &b);

        // Test some corner cases
        tensor_make_kxmxn(w, 1, 1, 8, &c);
        is(c.num_dims,1, "Check that number of dimensions is correctly allocated");
        ok(!kit_tensor_embedding(&c, y, 4, &b), "Embed 1 dimensional tensor should fail");
        kit_tensor_fini(&c);

        tensor_make_kxmxn(w, 1, 2, 4, &c);
        is(c.num_dims,2, "Check that number of dimensions is correctly allocated");
        ok(kit_tensor_embedding(&c, y, 4, &b), "Embed 2 dimensional tensor should succeed");
        is(b.num_dims,2, "Returning 2 dimensional tensor too");
        kit_tensor_fini(&c);


        tensor_make_kxmxn(w, 2, 2, 2, &c);
        is(c.num_dims,3, "Check that number of dimensions is correctly allocated");
        ok(kit_tensor_embedding(&c, y, 4, &b), "Embed 3 dimensional tensor should succeed");
        is(b.num_dims,3, "Returning 3 dimensional tensor too");
        kit_tensor_fini(&c);

        kit_tensor_fini(&b);
    }

    diag("Let's permute a tensor");
    {
        struct kit_tensor a,b,c;
        float x[12] = {1,2,3,4,5, 6, 7, 8, 9, 10, 11, 12};
        float y[8] = {0};
        float exp[12] = {0};
        unsigned dims[KIT_TENSOR_MAX_DIMS] = {0, 1, 2};

        tensor_make_kxmxn(x, 2, 3, 2, &a);
        tensor_make_kxmxn(x, 2, 3, 2, &b);
        tensor_make_kxmxn(y, 2, 2, 2, &c);

        // In [2]: torch.tensor([[[1,2], [3,4], [5, 6]], [[7,8],[9,10],[11,12]]]).permute(0, 1, 2)
        // Out[2]:
        // tensor([[[ 1,  2],
        //          [ 3,  4],
        //          [ 5,  6]],

        //         [[ 7,  8],
        //          [ 9, 10],
        //          [11, 12]]])
        ok(kit_tensor_permute(&a, dims, &b), "Permute tensor dimensions");

        exp[0] = 1; exp[1] = 2; exp[2] = 3; exp[3] = 4;
        exp[4] = 5; exp[5] = 6; exp[6] = 7; exp[7] = 8;
        exp[8] = 9; exp[9] = 10; exp[10] = 11; exp[11] = 12;
        for (int i = 0; i < 12; i++)
            is(b.value[i], exp[i], "Permutation index is correct");

        // In [3]: torch.tensor([[[1,2], [3,4], [5, 6]], [[7,8],[9,10],[11,12]]]).permute(2, 0, 1)
        // Out[3]:
        // tensor([[[ 1,  3,  5],
        //          [ 7,  9, 11]],

        //         [[ 2,  4,  6],
        //          [ 8, 10, 12]]])
        dims[0] = 2;
        dims[1] = 0;
        dims[2] = 1;
        ok(kit_tensor_permute(&a, dims, &b), "Permute tensor dimensions");

        exp[0] = 1; exp[1] = 3; exp[2] = 5; exp[3] = 7;
        exp[4] = 9; exp[5] = 11; exp[6] = 2; exp[7] = 4;
        exp[8] = 6; exp[9] = 8; exp[10] = 10; exp[11] = 12;
        for (int i = 0; i < 12; i++)
            is(b.value[i], exp[i], "Permutation index is correct");

        // In [4]: torch.tensor([[[1,2], [3,4], [5, 6]], [[7,8],[9,10],[11,12]]]).permute(1, 2, 0)
        // Out[4]:
        // tensor([[[ 1,  7],
        //          [ 2,  8]],

        //         [[ 3,  9],
        //          [ 4, 10]],

        //         [[ 5, 11],
        //          [ 6, 12]]])
        dims[0] = 1;
        dims[1] = 2;
        dims[2] = 0;
        ok(kit_tensor_permute(&a, dims, &b), "Permute tensor dimensions");

        exp[0] = 1; exp[1] = 7; exp[2] = 2; exp[3] = 8;
        exp[4] = 3; exp[5] = 9; exp[6] = 4; exp[7] = 10;
        exp[8] = 5; exp[9] = 11; exp[10] = 6; exp[11] = 12;
        for (int i = 0; i < 12; i++)
            is(b.value[i], exp[i], "Permutation index is correct");

        // In [5]: torch.tensor([[[1,2], [3,4], [5, 6]], [[7,8],[9,10],[11,12]]]).permute(1, 0, 2)
        // Out[5]:
        // tensor([[[ 1,  2],
        //          [ 7,  8]],

        //         [[ 3,  4],
        //          [ 9, 10]],

        //         [[ 5,  6],
        //          [11, 12]]])
        dims[0] = 1;
        dims[1] = 0;
        dims[2] = 2;
        ok(kit_tensor_permute(&a, dims, &b), "Permute tensor dimensions");

        exp[0] = 1; exp[1] = 2; exp[2] = 7; exp[3] = 8;
        exp[4] = 3; exp[5] = 4; exp[6] = 9; exp[7] = 10;
        exp[8] = 5; exp[9] = 6; exp[10] = 11; exp[11] = 12;
        for (int i = 0; i < 12; i++)
            is(b.value[i], exp[i], "Permutation index is correct");

        // In [6]: torch.tensor([[[1,2], [3,4], [5, 6]], [[7,8],[9,10],[11,12]]]).permute(0, 2, 1)
        // Out[6]:
        // tensor([[[ 1,  3,  5],
        //          [ 2,  4,  6]],

        //         [[ 7,  9, 11],
        //          [ 8, 10, 12]]])
        dims[0] = 0;
        dims[1] = 2;
        dims[2] = 1;
        ok(kit_tensor_permute(&a, dims, &b), "Permute tensor dimensions");

        exp[0] = 1; exp[1] = 3; exp[2] = 5; exp[3] = 2;
        exp[4] = 4; exp[5] = 6; exp[6] = 7; exp[7] = 9;
        exp[8] = 11; exp[9] = 8; exp[10] = 10; exp[11] = 12;
        for (int i = 0; i < 12; i++)
            is(b.value[i], exp[i], "Permutation index is correct");

        // In [7]: torch.tensor([[[1,2], [3,4], [5, 6]], [[7,8],[9,10],[11,12]]]).permute(2, 0, 1)
        // Out[7]:
        // tensor([[[ 1,  3,  5],
        //          [ 7,  9, 11]],

        //         [[ 2,  4,  6],
        //          [ 8, 10, 12]]])
        dims[0] = 2;
        dims[1] = 0;
        dims[2] = 1;
        ok(kit_tensor_permute(&a, dims, &b), "Permute tensor dimensions");

        exp[0] = 1; exp[1] = 3; exp[2] = 5; exp[3] = 7;
        exp[4] = 9; exp[5] = 11; exp[6] = 2; exp[7] = 4;
        exp[8] = 6; exp[9] = 8; exp[10] = 10; exp[11] = 12;
        for (int i = 0; i < 12; i++)
            is(b.value[i], exp[i], "Permutation index is correct");

        // Corner case failure test
        ok(!kit_tensor_permute(&a, dims, &c), "Test failure if total number of items does not match in two tensors");
        kit_tensor_fini(&a);
        kit_tensor_fini(&b);
        kit_tensor_fini(&c);
    }

    diag("Let's apply conv1d on a tensor");
    {
        struct kit_tensor a, b, c, d, e;
        float x[18] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
        float y[15] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        float z[12] = {0};
        float w[24] = {};
        float exp[12] = {178, 199, 220, 241, 412, 469, 526, 583, 646, 739, 832, 925};

        // In [115]: x
        // Out[115]:
        // tensor([[[ 1,  2,  3,  4,  5],
        //          [ 6,  7,  8,  9, 10],
        //          [11, 12, 13, 14, 15]]])

        // In [114]: conv1d.weight
        // Out[114]:
        // Parameter containing:
        // tensor([[[ 1.,  2.],
        //          [ 3.,  4.],
        //          [ 5.,  6.]],

        //         [[ 7.,  8.],
        //          [ 9., 10.],
        //          [11., 12.]],

        //         [[13., 14.],
        //          [15., 16.],
        //          [17., 18.]]], requires_grad=True)

        // In [113]: conv1d(x.float())
        // Out[113]:
        // tensor([[[177.8149, 198.8149, 219.8149, 240.8149],
        //          [411.7858, 468.7858, 525.7858, 582.7858],
        //          [645.6705, 738.6705, 831.6705, 924.6705]]],
        //        grad_fn=<ConvolutionBackward0>)

        tensor_make_kxmxn(x, 3, 3, 2, &a);
        tensor_make_kxmxn(y, 1, 3, 5, &b);
        tensor_make_kxmxn(z, 1, 3, 4, &c);
        tensor_make_kxmxn(y, 3, 5, 1, &d);
        tensor_make_kxmxn(w, 2, 3, 4, &e);

        ok(kit_tensor_conv1d(&a, &b, 1, &c), "Conv1D applied");
        for (int i = 0; i < 12; i++)
            is(c.value[i], exp[i], "Conv1D output correct");
        ok(!kit_tensor_conv1d(&b, &b, 1, &c), "Checking corner cases for tensor dimensions");
        ok(!kit_tensor_conv1d(&d, &b, 1, &c), "Checking corner cases for tensor dimensions");
        ok(!kit_tensor_conv1d(&a, &b, 1, &d), "Checking corner cases for tensor dimensions");
        ok(!kit_tensor_conv1d(&a, &b, 1, &e), "Checking corner cases for tensor dimensions");

        kit_tensor_fini(&a);
        kit_tensor_fini(&b);
        kit_tensor_fini(&c);
        kit_tensor_fini(&d);
        kit_tensor_fini(&e);
    }

    diag("Let's apply batchnorm1d_affine!");
    {
        struct kit_tensor bn, a, b;
        float x[24] = {
            1,   2,  3,  4,  5,  6,
            7,   8,  9, 10, 11, 12,
            13, 14, 15, 16, 17, 18,
            19, 20, 21, 22, 23, 24,
        };
        float y[8] = {
            1.8860, 8.2284, .3, .4,
            3.3500, 8.5524, .7, .8
        };
        float z[24] = {0};
        float exp[24] = {
            0.3073, 0.4119, 0.5165, 0.6211, 0.7257, 0.8303,
            1.6737, 1.9130, 2.1524, 2.3918, 2.6311, 2.8705,
            1.5623, 1.6669, 1.7715, 1.8761, 1.9807, 2.0853,
            4.5460, 4.7854, 5.0247, 5.2641, 5.5035, 5.7428,
        };

        // In [174]: inp
        // Out[174]:
        // tensor([[[ 1.,  2.,  3.,  4.,  5.,  6.],
        //          [ 7.,  8.,  9., 10., 11., 12.]],

        //         [[13., 14., 15., 16., 17., 18.],
        //          [19., 20., 21., 22., 23., 24.]]])

        // In [145]: m = nn.BatchNorm1d(2)

        // In [170]: m.running_mean
        // Out[170]:
        // Parameter containing:
        // tensor([1.8860, 3.3500])

        // In [171]: m.running_var
        // Out[171]:
        // Parameter containing:
        // tensor([8.2284, 8.5524])

        // In [172]: m.weight
        // Out[172]:
        // Parameter containing:
        // tensor([0.3000, 0.7000], requires_grad=True)

        // In [173]: m.bias
        // Out[173]:
        // Parameter containing:
        // tensor([0.4000, 0.8000], requires_grad=True)

        // In [176]: m(inp)
        // Out[176]:
        // tensor([[[0.3073, 0.4119, 0.5165, 0.6211, 0.7257, 0.8303],
        //          [1.6737, 1.9130, 2.1524, 2.3918, 2.6311, 2.8705]],

        //         [[1.5623, 1.6669, 1.7715, 1.8761, 1.9807, 2.0853],
        //          [4.5460, 4.7854, 5.0247, 5.2641, 5.5035, 5.7428]]],
        //        grad_fn=<NativeBatchNormBackward0>)

        tensor_make_kxmxn(x, 2, 2, 6, &a);
        tensor_make_kxmxn(y, 1, 2, 4, &bn);
        tensor_make_kxmxn(z, 2, 2, 6, &b);
        ok(kit_tensor_batchnorm1d_affine(&bn, &a, &b), "Ran batchnorm1d affine version");
        for (int i = 0; i < 12; i++)
            ok(b.value[i] - exp[i] < .001, "Computed batchnorm1d affine value correctly");
        ok(!kit_tensor_batchnorm1d_affine(&a, &a, &b), "Check corner cases for tensor dimensions");
        ok(!kit_tensor_batchnorm1d_affine(&bn, &a, &bn), "Check corner cases for tensor dimensions");

        kit_tensor_fini(&a);
        kit_tensor_fini(&bn);
        kit_tensor_fini(&b);
    }

    diag("Let's apply ReLU!");
    {
        struct kit_tensor a;
        float x[10] = {-4, -3, -2, -1, 0, 1, 2, 3, 4, 5};
        float exp[10] = {0, 0, 0, 0, 0, 1, 2, 3, 4, 5};

        is(kit_relu(-1), 0., "ReLU works on negative numbers");
        is(kit_relu(0), 0., "ReLU works on negative numbers");
        is(kit_relu(1), 1, "ReLU works on negative numbers");

        ok(tensor_make_kxmxn(x, 2, 1, 5, &a), "Make (2,1,5) tensor");
        ok(kit_tensor_relu(&a), "Applied ReLU on tensor (2, 1, 5)");
        for (int i = 0; i < 10; i++)
            is(a.value[i], exp[i], "ReLU applied over tensor (2,1,5) is correct");

        kit_tensor_fini(&a);
    }

    diag("Let's flatten a tensor!");
    {
        struct kit_tensor a;
        float x[10] = {-4,-3,-2,-1,0,1,2,3,4,5};

        ok(tensor_make_kxmxn(x, 2, 1, 5, &a), "Make (2,1,5) tensor");
        is(a.dimension[0], 2, "1st dimension is 2");
        is(a.dimension[1], 1, "2nd dimension is 1");
        is(a.dimension[2], 5, "3rd dimension is 5");
        ok(kit_tensor_flatten(&a), "Ran tensor flatten");
        is(a.dimension[0], 1, "1st dimension is 1");
        is(a.dimension[1], 1, "2nd dimension is 1");
        is(a.dimension[2], 10, "3rd dimension is 10");

        kit_tensor_fini(&a);
    }

    diag("Putting it all together");
    {
        struct kit_tensor
            runstate_embed,     // (1, 3, 4)
            runstate_permute,   // (1, 4, 6)
            runstate_conv1,     // (1, 4, 5)
            runstate_bn1,       // (1, 4, 5)
            runstate_fc,        // (1, 1, 2)
            net_embedding,      // (1, 6, 4)
            net_conv1d,         // (4, 4, 2)
            net_bn1d_affine,    // (1, 4, 4)
            net_fc_linear;      // (1, 20, 2)
        unsigned rs_input[3] = {1, 3, 5};
        float
            rs_embed[12] = {0},
            rs_permute[24] = {0},
            rs_conv1[20] = {0},
            rs_bn1[20] = {0},
            rs_fc[2] = {0},
            n_embedding[24] = {
                1, 2, 3, 4,
                2, 3, 4, 5,
                3, 4, 5, 6,
                4, 5, 6, 7,
                5, 6, 7, 8,
            },
            n_conv1d[32] = {
                1, 2,
                2, 3,
                3, 4,
                4, 5,

                5, 6,
                6, 7,
                7, 8,
                8, 9,

                9, 10,
                10, 11,
                11, 12,
                12, 13,

                13, 14,
                14, 15,
                15, 16,
                16, 17
            },
            n_bn1d_affine[16] = {
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
            },
            n_fc_linear[40] = {
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
            };

        tensor_make_kxmxn(rs_embed, 1, 3, 4, &runstate_embed);
        tensor_make_kxmxn(rs_permute, 1, 4, 3, &runstate_permute);
        tensor_make_kxmxn(rs_conv1, 1, 4, 2, &runstate_conv1);
        tensor_make_kxmxn(rs_bn1, 1, 4, 2, &runstate_bn1);
        tensor_make_kxmxn(rs_fc, 1, 1, 2, &runstate_fc);
        tensor_make_kxmxn(n_embedding, 1, 6, 4, &net_embedding);
        tensor_make_kxmxn(n_conv1d, 4, 4, 2, &net_conv1d);
        tensor_make_kxmxn(n_bn1d_affine, 1, 4, 4, &net_bn1d_affine);
        tensor_make_kxmxn(n_fc_linear, 1, 8, 2, &net_fc_linear);

        ok(kit_tensor_embedding(&net_embedding, rs_input, 3, &runstate_embed), "Take runstate input and extract embedding tensor");
        ok(kit_tensor_transpose(&runstate_embed, 1, 2, &runstate_permute), "Permute runstate embedding tensor");
        ok(kit_tensor_conv1d(&net_conv1d, &runstate_permute, 1, &runstate_conv1), "Apply conv1d on permuted embedding tensor");
        ok(kit_tensor_batchnorm1d_affine(&net_bn1d_affine, &runstate_conv1, &runstate_bn1), "Apply batchnorm1d on conv1d output tensor");
        ok(kit_tensor_relu(&runstate_bn1), "Apply ReLU on batchnorm1d output");
        ok(kit_tensor_flatten(&runstate_bn1), "Flatten ReLU output for input to final linear layer output");
        ok(kit_tensor_matmul(&runstate_bn1, &net_fc_linear, &runstate_fc), "Apply neural network linear layer to two state output");

        kit_tensor_fini(&runstate_embed);
        kit_tensor_fini(&runstate_permute);
        kit_tensor_fini(&runstate_conv1);
        kit_tensor_fini(&runstate_bn1);
        kit_tensor_fini(&runstate_fc);
        kit_tensor_fini(&net_embedding);
        kit_tensor_fini(&net_conv1d);
        kit_tensor_fini(&net_bn1d_affine);
        kit_tensor_fini(&net_fc_linear);
    }

    diag("Let's sum some tensors");
    {
        struct kit_tensor a, b, c;
        float x[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        float y[12] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        float z[12] = {0};
        float exp[12] = {5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27};

        tensor_make_kxmxn(x, 3, 2, 2, &a);
        tensor_make_kxmxn(y, 3, 2, 2, &b);
        tensor_make_kxmxn(z, 3, 2, 2, &c);

        ok(kit_tensor_sum(&a, &b, &c), "Summing of tensors of same dimensions");
        for (int i = 0; i < 12; i++)
            is(c.value[i], exp[i], "Output sum is correct");

        kit_tensor_fini(&a);
        kit_tensor_fini(&b);
        kit_tensor_fini(&c);

        tensor_make_kxmxn(z, 2, 3, 2, &b);
        ok(!kit_tensor_sum(&a, &b, &c), "Summing of tensors of mismatched dimensions fails");
        kit_tensor_fini(&b);
    }

    diag("Tensor Allocation and Initialization");
    {
        struct kit_tensor a,c;
        float x[10] = {-4, -3, -2, -1, 0, 1, 2, 3, 4, 5};

        ok(tensor_make_kxmxn(x, 2, 1, 5, &a), "Make (2,1,5) tensor");
        c.value = kit_malloc(10);
        kit_tensor_init(&c);
        is(c.dimension[0],0, "Dimension check");
        is(c.dimension[1],0, "Dimension check");
        is(c.dimension[2],0, "Dimension check");
        kit_tensor_fini(&c);
        is(c.num_dims,0, "Dimension check");
        kit_tensor_fini(&a);
    }

    diag("Tensor utils");
    {
        struct kit_tensor a;
        unsigned dims[3] = {1, 5, 1};
        float x[5] = {1., 2., 3., 4., 5.};

        ok(tensor_make_kxmxn(x, 1, 1, 5, &a), "Make (1,1,5) tensor");
        for (int i = 0; i < 5; i++)
            ok(x[i] == a.value[i], "Tensor values set correctly");

        ok(a.sz == 5, "Tensor size is set correctly");
        kit_tensor_zeros(&a);
        for (int i = 0; i < 5; i++)
            ok(a.value[i] == 0., "Tensor values set to zero correctly");
        ok(kit_tensor_dimset(&a, dims), "Change dims");
        ok(a.dimension[0] == 1 && a.dimension[1] == 5 && a.dimension[2] == 1, "Tensor dims set correctly");
        kit_tensor_fini(&a);
    }

    is(kit_memory_allocations(), start_allocations, "Tensor lookup memory allocations were freed");

    return exit_status();
}
