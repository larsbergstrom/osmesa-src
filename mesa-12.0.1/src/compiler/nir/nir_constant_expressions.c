/*
 * Copyright (C) 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jason Ekstrand (jason@jlekstrand.net)
 */

#include <math.h>
#include "main/core.h"
#include "util/rounding.h" /* for _mesa_roundeven */
#include "util/half_float.h"
#include "nir_constant_expressions.h"

/**
 * Evaluate one component of packSnorm4x8.
 */
static uint8_t
pack_snorm_1x8(float x)
{
    /* From section 8.4 of the GLSL 4.30 spec:
     *
     *    packSnorm4x8
     *    ------------
     *    The conversion for component c of v to fixed point is done as
     *    follows:
     *
     *      packSnorm4x8: round(clamp(c, -1, +1) * 127.0)
     *
     * We must first cast the float to an int, because casting a negative
     * float to a uint is undefined.
     */
   return (uint8_t) (int)
          _mesa_roundevenf(CLAMP(x, -1.0f, +1.0f) * 127.0f);
}

/**
 * Evaluate one component of packSnorm2x16.
 */
static uint16_t
pack_snorm_1x16(float x)
{
    /* From section 8.4 of the GLSL ES 3.00 spec:
     *
     *    packSnorm2x16
     *    -------------
     *    The conversion for component c of v to fixed point is done as
     *    follows:
     *
     *      packSnorm2x16: round(clamp(c, -1, +1) * 32767.0)
     *
     * We must first cast the float to an int, because casting a negative
     * float to a uint is undefined.
     */
   return (uint16_t) (int)
          _mesa_roundevenf(CLAMP(x, -1.0f, +1.0f) * 32767.0f);
}

/**
 * Evaluate one component of unpackSnorm4x8.
 */
static float
unpack_snorm_1x8(uint8_t u)
{
    /* From section 8.4 of the GLSL 4.30 spec:
     *
     *    unpackSnorm4x8
     *    --------------
     *    The conversion for unpacked fixed-point value f to floating point is
     *    done as follows:
     *
     *       unpackSnorm4x8: clamp(f / 127.0, -1, +1)
     */
   return CLAMP((int8_t) u / 127.0f, -1.0f, +1.0f);
}

/**
 * Evaluate one component of unpackSnorm2x16.
 */
static float
unpack_snorm_1x16(uint16_t u)
{
    /* From section 8.4 of the GLSL ES 3.00 spec:
     *
     *    unpackSnorm2x16
     *    ---------------
     *    The conversion for unpacked fixed-point value f to floating point is
     *    done as follows:
     *
     *       unpackSnorm2x16: clamp(f / 32767.0, -1, +1)
     */
   return CLAMP((int16_t) u / 32767.0f, -1.0f, +1.0f);
}

/**
 * Evaluate one component packUnorm4x8.
 */
static uint8_t
pack_unorm_1x8(float x)
{
    /* From section 8.4 of the GLSL 4.30 spec:
     *
     *    packUnorm4x8
     *    ------------
     *    The conversion for component c of v to fixed point is done as
     *    follows:
     *
     *       packUnorm4x8: round(clamp(c, 0, +1) * 255.0)
     */
   return (uint8_t) (int)
          _mesa_roundevenf(CLAMP(x, 0.0f, 1.0f) * 255.0f);
}

/**
 * Evaluate one component packUnorm2x16.
 */
static uint16_t
pack_unorm_1x16(float x)
{
    /* From section 8.4 of the GLSL ES 3.00 spec:
     *
     *    packUnorm2x16
     *    -------------
     *    The conversion for component c of v to fixed point is done as
     *    follows:
     *
     *       packUnorm2x16: round(clamp(c, 0, +1) * 65535.0)
     */
   return (uint16_t) (int)
          _mesa_roundevenf(CLAMP(x, 0.0f, 1.0f) * 65535.0f);
}

/**
 * Evaluate one component of unpackUnorm4x8.
 */
static float
unpack_unorm_1x8(uint8_t u)
{
    /* From section 8.4 of the GLSL 4.30 spec:
     *
     *    unpackUnorm4x8
     *    --------------
     *    The conversion for unpacked fixed-point value f to floating point is
     *    done as follows:
     *
     *       unpackUnorm4x8: f / 255.0
     */
   return (float) u / 255.0f;
}

/**
 * Evaluate one component of unpackUnorm2x16.
 */
static float
unpack_unorm_1x16(uint16_t u)
{
    /* From section 8.4 of the GLSL ES 3.00 spec:
     *
     *    unpackUnorm2x16
     *    ---------------
     *    The conversion for unpacked fixed-point value f to floating point is
     *    done as follows:
     *
     *       unpackUnorm2x16: f / 65535.0
     */
   return (float) u / 65535.0f;
}

/**
 * Evaluate one component of packHalf2x16.
 */
static uint16_t
pack_half_1x16(float x)
{
   return _mesa_float_to_half(x);
}

/**
 * Evaluate one component of unpackHalf2x16.
 */
static float
unpack_half_1x16(uint16_t u)
{
   return _mesa_half_to_float(u);
}

/* Some typed vector structures to make things like src0.y work */
typedef float float32_t;
typedef double float64_t;
typedef bool bool32_t;
struct float32_vec {
   float32_t x;
   float32_t y;
   float32_t z;
   float32_t w;
};
struct float64_vec {
   float64_t x;
   float64_t y;
   float64_t z;
   float64_t w;
};
struct int32_vec {
   int32_t x;
   int32_t y;
   int32_t z;
   int32_t w;
};
struct int64_vec {
   int64_t x;
   int64_t y;
   int64_t z;
   int64_t w;
};
struct uint32_vec {
   uint32_t x;
   uint32_t y;
   uint32_t z;
   uint32_t w;
};
struct uint64_vec {
   uint64_t x;
   uint64_t y;
   uint64_t z;
   uint64_t w;
};

struct bool32_vec {
    bool x;
    bool y;
    bool z;
    bool w;
};

static nir_const_value
evaluate_b2f(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  bool src0 = _src[0].u32[_i] != 0;

               float32_t dst = src0 ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  bool src0 = _src[0].u32[_i] != 0;

               float32_t dst = src0 ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_b2i(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  bool src0 = _src[0].u32[_i] != 0;

               int32_t dst = src0 ? 1 : 0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  bool src0 = _src[0].u32[_i] != 0;

               int32_t dst = src0 ? 1 : 0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ball_fequal2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ball_fequal3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
               _src[0].f64[2],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
               _src[1].f64[2],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ball_fequal4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
               _src[1].f32[3],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z) && (src0.w == src1.w));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
               _src[0].f64[2],
               _src[0].f64[3],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
               _src[1].f64[2],
               _src[1].f64[3],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z) && (src0.w == src1.w));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ball_iequal2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct int32_vec src0 = {
               _src[0].i32[0],
               _src[0].i32[1],
         };

         struct int32_vec src1 = {
               _src[1].i32[0],
               _src[1].i32[1],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct int64_vec src0 = {
               _src[0].i64[0],
               _src[0].i64[1],
         };

         struct int64_vec src1 = {
               _src[1].i64[0],
               _src[1].i64[1],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ball_iequal3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct int32_vec src0 = {
               _src[0].i32[0],
               _src[0].i32[1],
               _src[0].i32[2],
         };

         struct int32_vec src1 = {
               _src[1].i32[0],
               _src[1].i32[1],
               _src[1].i32[2],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct int64_vec src0 = {
               _src[0].i64[0],
               _src[0].i64[1],
               _src[0].i64[2],
         };

         struct int64_vec src1 = {
               _src[1].i64[0],
               _src[1].i64[1],
               _src[1].i64[2],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ball_iequal4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct int32_vec src0 = {
               _src[0].i32[0],
               _src[0].i32[1],
               _src[0].i32[2],
               _src[0].i32[3],
         };

         struct int32_vec src1 = {
               _src[1].i32[0],
               _src[1].i32[1],
               _src[1].i32[2],
               _src[1].i32[3],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z) && (src0.w == src1.w));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct int64_vec src0 = {
               _src[0].i64[0],
               _src[0].i64[1],
               _src[0].i64[2],
               _src[0].i64[3],
         };

         struct int64_vec src1 = {
               _src[1].i64[0],
               _src[1].i64[1],
               _src[1].i64[2],
               _src[1].i64[3],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z) && (src0.w == src1.w));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bany_fnequal2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bany_fnequal3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
               _src[0].f64[2],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
               _src[1].f64[2],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bany_fnequal4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
               _src[1].f32[3],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z) || (src0.w != src1.w));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
               _src[0].f64[2],
               _src[0].f64[3],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
               _src[1].f64[2],
               _src[1].f64[3],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z) || (src0.w != src1.w));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bany_inequal2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct int32_vec src0 = {
               _src[0].i32[0],
               _src[0].i32[1],
         };

         struct int32_vec src1 = {
               _src[1].i32[0],
               _src[1].i32[1],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct int64_vec src0 = {
               _src[0].i64[0],
               _src[0].i64[1],
         };

         struct int64_vec src1 = {
               _src[1].i64[0],
               _src[1].i64[1],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bany_inequal3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct int32_vec src0 = {
               _src[0].i32[0],
               _src[0].i32[1],
               _src[0].i32[2],
         };

         struct int32_vec src1 = {
               _src[1].i32[0],
               _src[1].i32[1],
               _src[1].i32[2],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct int64_vec src0 = {
               _src[0].i64[0],
               _src[0].i64[1],
               _src[0].i64[2],
         };

         struct int64_vec src1 = {
               _src[1].i64[0],
               _src[1].i64[1],
               _src[1].i64[2],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bany_inequal4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct int32_vec src0 = {
               _src[0].i32[0],
               _src[0].i32[1],
               _src[0].i32[2],
               _src[0].i32[3],
         };

         struct int32_vec src1 = {
               _src[1].i32[0],
               _src[1].i32[1],
               _src[1].i32[2],
               _src[1].i32[3],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z) || (src0.w != src1.w));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }
   case 64: {
      


         struct int64_vec src0 = {
               _src[0].i64[0],
               _src[0].i64[1],
               _src[0].i64[2],
               _src[0].i64[3],
         };

         struct int64_vec src1 = {
               _src[1].i64[0],
               _src[1].i64[1],
               _src[1].i64[2],
               _src[1].i64[3],
         };

         struct bool32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z) || (src0.w != src1.w));

               _dst_val.u32[0] = dst.x ? NIR_TRUE : NIR_FALSE;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bcsel(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  bool src0 = _src[0].u32[_i] != 0;
                  uint32_t src1 =
                     _src[1].u32[_i];
                  uint32_t src2 =
                     _src[2].u32[_i];

               uint32_t dst = src0 ? src1 : src2;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  bool src0 = _src[0].u32[_i] != 0;
                  uint64_t src1 =
                     _src[1].u64[_i];
                  uint64_t src2 =
                     _src[2].u64[_i];

               uint64_t dst = src0 ? src1 : src2;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bfi(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];
                  uint32_t src2 =
                     _src[2].u32[_i];

               uint32_t dst;
               
unsigned mask = src0, insert = src1, base = src2;
if (mask == 0) {
   dst = base;
} else {
   unsigned tmp = mask;
   while (!(tmp & 1)) {
      tmp >>= 1;
      insert <<= 1;
   }
   dst = (base & ~mask) | (insert & mask);
}


               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];
                  uint32_t src2 =
                     _src[2].u32[_i];

               uint32_t dst;
               
unsigned mask = src0, insert = src1, base = src2;
if (mask == 0) {
   dst = base;
} else {
   unsigned tmp = mask;
   while (!(tmp & 1)) {
      tmp >>= 1;
      insert <<= 1;
   }
   dst = (base & ~mask) | (insert & mask);
}


               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bfm(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               uint32_t dst;
               
int bits = src0, offset = src1;
if (offset < 0 || bits < 0 || offset > 31 || bits > 31 || offset + bits > 32)
   dst = 0; /* undefined */
else
   dst = ((1u << bits) - 1) << offset;


               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               uint32_t dst;
               
int bits = src0, offset = src1;
if (offset < 0 || bits < 0 || offset > 31 || bits > 31 || offset + bits > 32)
   dst = 0; /* undefined */
else
   dst = ((1u << bits) - 1) << offset;


               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bit_count(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];

               uint32_t dst;
               
dst = 0;
for (unsigned bit = 0; bit < 32; bit++) {
   if ((src0 >> bit) & 1)
      dst++;
}


               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];

               uint32_t dst;
               
dst = 0;
for (unsigned bit = 0; bit < 32; bit++) {
   if ((src0 >> bit) & 1)
      dst++;
}


               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bitfield_insert(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                                                
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];
                  int32_t src2 =
                     _src[2].i32[_i];
                  int32_t src3 =
                     _src[3].i32[_i];

               uint32_t dst;
               
unsigned base = src0, insert = src1;
int offset = src2, bits = src3;
if (bits == 0) {
   dst = 0;
} else if (offset < 0 || bits < 0 || bits + offset > 32) {
   dst = 0;
} else {
   unsigned mask = ((1ull << bits) - 1) << offset;
   dst = (base & ~mask) | ((insert << bits) & mask);
}


               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                                                
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];
                  int32_t src2 =
                     _src[2].i32[_i];
                  int32_t src3 =
                     _src[3].i32[_i];

               uint32_t dst;
               
unsigned base = src0, insert = src1;
int offset = src2, bits = src3;
if (bits == 0) {
   dst = 0;
} else if (offset < 0 || bits < 0 || bits + offset > 32) {
   dst = 0;
} else {
   unsigned mask = ((1ull << bits) - 1) << offset;
   dst = (base & ~mask) | ((insert << bits) & mask);
}


               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_bitfield_reverse(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];

               uint32_t dst;
               
/* we're not winning any awards for speed here, but that's ok */
dst = 0;
for (unsigned bit = 0; bit < 32; bit++)
   dst |= ((src0 >> bit) & 1) << (31 - bit);


               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];

               uint32_t dst;
               
/* we're not winning any awards for speed here, but that's ok */
dst = 0;
for (unsigned bit = 0; bit < 32; bit++)
   dst |= ((src0 >> bit) & 1) << (31 - bit);


               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_d2b(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               bool32_t dst = src0 != 0.0;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               bool32_t dst = src0 != 0.0;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_d2f(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float32_t dst = src0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float32_t dst = src0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_d2i(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               int32_t dst = src0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               int32_t dst = src0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_d2u(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               uint32_t dst = src0;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               uint32_t dst = src0;

               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_extract_i16(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = (int16_t)(src0 >> (src1 * 16));

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = (int16_t)(src0 >> (src1 * 16));

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_extract_i8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = (int8_t)(src0 >> (src1 * 8));

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = (int8_t)(src0 >> (src1 * 8));

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_extract_u16(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = (uint16_t)(src0 >> (src1 * 16));

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = (uint16_t)(src0 >> (src1 * 16));

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_extract_u8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = (uint8_t)(src0 >> (src1 * 8));

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = (uint8_t)(src0 >> (src1 * 8));

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_f2b(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               bool32_t dst = src0 != 0.0f;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               bool32_t dst = src0 != 0.0f;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_f2d(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float64_t dst = src0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float64_t dst = src0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_f2i(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               int32_t dst = src0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               int32_t dst = src0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_f2u(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               uint32_t dst = src0;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               uint32_t dst = src0;

               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fabs(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? fabs(src0) : fabsf(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? fabs(src0) : fabsf(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fadd(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = src0 + src1;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               float64_t dst = src0 + src1;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fall_equal2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fall_equal3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fall_equal4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
               _src[1].f32[3],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z) && (src0.w == src1.w)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
               _src[1].f32[3],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x == src1.x) && (src0.y == src1.y) && (src0.z == src1.z) && (src0.w == src1.w)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fand(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = ((src0 != 0.0f) && (src1 != 0.0f)) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = ((src0 != 0.0f) && (src1 != 0.0f)) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fany_nequal2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fany_nequal3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fany_nequal4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
               _src[1].f32[3],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z) || (src0.w != src1.w)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
               _src[1].f32[3],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x != src1.x) || (src0.y != src1.y) || (src0.z != src1.z) || (src0.w != src1.w)) ? 1.0f : 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fceil(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? ceil(src0) : ceilf(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? ceil(src0) : ceilf(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fcos(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? cos(src0) : cosf(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? cos(src0) : cosf(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fcsel(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];
                  float32_t src2 =
                     _src[2].f32[_i];

               float32_t dst = (src0 != 0.0f) ? src1 : src2;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];
                  float32_t src2 =
                     _src[2].f32[_i];

               float32_t dst = (src0 != 0.0f) ? src1 : src2;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fddx(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float32_t dst = 0.0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float64_t dst = 0.0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fddx_coarse(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float32_t dst = 0.0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float64_t dst = 0.0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fddx_fine(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float32_t dst = 0.0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float64_t dst = 0.0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fddy(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float32_t dst = 0.0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float64_t dst = 0.0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fddy_coarse(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float32_t dst = 0.0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float64_t dst = 0.0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fddy_fine(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float32_t dst = 0.0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  
               float64_t dst = 0.0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fdiv(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = src0 / src1;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               float64_t dst = src0 / src1;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fdot2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y));

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
         };

         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y));

               _dst_val.f64[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fdot3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y) + (src0.z * src1.z));

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
               _src[0].f64[2],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
               _src[1].f64[2],
         };

         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y) + (src0.z * src1.z));

               _dst_val.f64[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fdot4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
               _src[1].f32[3],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y) + (src0.z * src1.z) + (src0.w * src1.w));

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
               _src[0].f64[2],
               _src[0].f64[3],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
               _src[1].f64[2],
               _src[1].f64[3],
         };

         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y) + (src0.z * src1.z) + (src0.w * src1.w));

               _dst_val.f64[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fdot_replicated2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y));

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
         };

         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y));

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;
               _dst_val.f64[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fdot_replicated3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y) + (src0.z * src1.z));

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
               _src[0].f64[2],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
               _src[1].f64[2],
         };

         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y) + (src0.z * src1.z));

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;
               _dst_val.f64[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fdot_replicated4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
               _src[1].f32[3],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y) + (src0.z * src1.z) + (src0.w * src1.w));

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
               _src[0].f64[2],
               _src[0].f64[3],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
               _src[1].f64[2],
               _src[1].f64[3],
         };

         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = ((src0.x * src1.x) + (src0.y * src1.y) + (src0.z * src1.z) + (src0.w * src1.w));

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;
               _dst_val.f64[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fdph(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
               _src[1].f32[3],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = src0.x * src1.x + src0.y * src1.y + src0.z * src1.z + src1.w;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
               _src[0].f64[2],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
               _src[1].f64[2],
               _src[1].f64[3],
         };

         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = src0.x * src1.x + src0.y * src1.y + src0.z * src1.z + src1.w;

               _dst_val.f64[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fdph_replicated(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
               _src[1].f32[1],
               _src[1].f32[2],
               _src[1].f32[3],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = src0.x * src1.x + src0.y * src1.y + src0.z * src1.z + src1.w;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }
   case 64: {
      


         struct float64_vec src0 = {
               _src[0].f64[0],
               _src[0].f64[1],
               _src[0].f64[2],
         };

         struct float64_vec src1 = {
               _src[1].f64[0],
               _src[1].f64[1],
               _src[1].f64[2],
               _src[1].f64[3],
         };

         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = src0.x * src1.x + src0.y * src1.y + src0.z * src1.z + src1.w;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;
               _dst_val.f64[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_feq(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               bool32_t dst = src0 == src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               bool32_t dst = src0 == src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fexp2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = exp2f(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = exp2f(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ffloor(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? floor(src0) : floorf(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? floor(src0) : floorf(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ffma(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];
                  float32_t src2 =
                     _src[2].f32[_i];

               float32_t dst = src0 * src1 + src2;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];
                  float64_t src2 =
                     _src[2].f64[_i];

               float64_t dst = src0 * src1 + src2;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ffract(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = src0 - (bit_size == 64 ? floor(src0) : floorf(src0));

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = src0 - (bit_size == 64 ? floor(src0) : floorf(src0));

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fge(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               bool32_t dst = src0 >= src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               bool32_t dst = src0 >= src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_find_lsb(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               int32_t dst;
               
dst = -1;
for (unsigned bit = 0; bit < 32; bit++) {
   if ((src0 >> bit) & 1) {
      dst = bit;
      break;
   }
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               int32_t dst;
               
dst = -1;
for (unsigned bit = 0; bit < 32; bit++) {
   if ((src0 >> bit) & 1) {
      dst = bit;
      break;
   }
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_flog2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = log2f(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = log2f(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_flrp(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];
                  float32_t src2 =
                     _src[2].f32[_i];

               float32_t dst = src0 * (1 - src2) + src1 * src2;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];
                  float64_t src2 =
                     _src[2].f64[_i];

               float64_t dst = src0 * (1 - src2) + src1 * src2;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_flt(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               bool32_t dst = src0 < src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               bool32_t dst = src0 < src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fmax(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = fmaxf(src0, src1);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               float64_t dst = fmaxf(src0, src1);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fmin(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = fminf(src0, src1);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               float64_t dst = fminf(src0, src1);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fmod(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = src0 - src1 * floorf(src0 / src1);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               float64_t dst = src0 - src1 * floorf(src0 / src1);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fmov(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = src0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = src0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fmul(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = src0 * src1;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               float64_t dst = src0 * src1;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fne(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               bool32_t dst = src0 != src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               bool32_t dst = src0 != src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fneg(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = -src0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = -src0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise1_1(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise1_2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise1_3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise1_4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise2_1(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise2_2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise2_3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise2_4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise3_1(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise3_2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise3_3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise3_4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise4_1(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;
               _dst_val.f64[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise4_2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;
               _dst_val.f64[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise4_3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;
               _dst_val.f64[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnoise4_4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }
   case 64: {
      

            
         struct float64_vec dst;

            dst.x = dst.y = dst.z = dst.w = 0.0f;

               _dst_val.f64[0] = dst.x;
               _dst_val.f64[1] = dst.y;
               _dst_val.f64[2] = dst.z;
               _dst_val.f64[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fnot(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? ((src0 == 0.0) ? 1.0 : 0.0f) : ((src0 == 0.0f) ? 1.0f : 0.0f);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? ((src0 == 0.0) ? 1.0 : 0.0f) : ((src0 == 0.0f) ? 1.0f : 0.0f);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_for(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = ((src0 != 0.0f) || (src1 != 0.0f)) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = ((src0 != 0.0f) || (src1 != 0.0f)) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fpow(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = bit_size == 64 ? powf(src0, src1) : pow(src0, src1);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               float64_t dst = bit_size == 64 ? powf(src0, src1) : pow(src0, src1);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fquantize2f16(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = (fabs(src0) < ldexpf(1.0, -14)) ? copysignf(0.0f, src0) : _mesa_half_to_float(_mesa_float_to_half(src0));

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = (fabs(src0) < ldexpf(1.0, -14)) ? copysignf(0.0f, src0) : _mesa_half_to_float(_mesa_float_to_half(src0));

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_frcp(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? 1.0 / src0 : 1.0f / src0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? 1.0 / src0 : 1.0f / src0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_frem(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = src0 - src1 * truncf(src0 / src1);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               float64_t dst = src0 - src1 * truncf(src0 / src1);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fround_even(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? _mesa_roundeven(src0) : _mesa_roundevenf(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? _mesa_roundeven(src0) : _mesa_roundevenf(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_frsq(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? 1.0 / sqrt(src0) : 1.0f / sqrtf(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? 1.0 / sqrt(src0) : 1.0f / sqrtf(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fsat(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? ((src0 > 1.0) ? 1.0 : ((src0 <= 0.0) ? 0.0 : src0)) : ((src0 > 1.0f) ? 1.0f : ((src0 <= 0.0f) ? 0.0f : src0));

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? ((src0 > 1.0) ? 1.0 : ((src0 <= 0.0) ? 0.0 : src0)) : ((src0 > 1.0f) ? 1.0f : ((src0 <= 0.0f) ? 0.0f : src0));

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fsign(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? ((src0 == 0.0) ? 0.0 : ((src0 > 0.0) ? 1.0 : -1.0)) : ((src0 == 0.0f) ? 0.0f : ((src0 > 0.0f) ? 1.0f : -1.0f));

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? ((src0 == 0.0) ? 0.0 : ((src0 > 0.0) ? 1.0 : -1.0)) : ((src0 == 0.0f) ? 0.0f : ((src0 > 0.0f) ? 1.0f : -1.0f));

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fsin(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? sin(src0) : sinf(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? sin(src0) : sinf(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fsqrt(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? sqrt(src0) : sqrtf(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? sqrt(src0) : sqrtf(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fsub(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = src0 - src1;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  float64_t src1 =
                     _src[1].f64[_i];

               float64_t dst = src0 - src1;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ftrunc(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];

               float32_t dst = bit_size == 64 ? trunc(src0) : truncf(src0);

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];

               float64_t dst = bit_size == 64 ? trunc(src0) : truncf(src0);

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_fxor(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = (src0 != 0.0f && src1 == 0.0f) || (src0 == 0.0f && src1 != 0.0f) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = (src0 != 0.0f && src1 == 0.0f) || (src0 == 0.0f && src1 != 0.0f) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_i2b(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               bool32_t dst = src0 != 0;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               bool32_t dst = src0 != 0;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_i2d(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               float64_t dst = src0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               float64_t dst = src0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_i2f(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               float32_t dst = src0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               float32_t dst = src0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_iabs(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               int32_t dst = (src0 < 0) ? -src0 : src0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];

               int64_t dst = (src0 < 0) ? -src0 : src0;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_iadd(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = src0 + src1;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = src0 + src1;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_iand(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = src0 & src1;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = src0 & src1;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ibfe(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];
                  int32_t src2 =
                     _src[2].i32[_i];

               int32_t dst;
               
int base = src0;
int offset = src1, bits = src2;
if (bits == 0) {
   dst = 0;
} else if (bits < 0 || offset < 0) {
   dst = 0; /* undefined */
} else if (offset + bits < 32) {
   dst = (base << (32 - bits - offset)) >> (32 - bits);
} else {
   dst = base >> offset;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];
                  int32_t src2 =
                     _src[2].i32[_i];

               int32_t dst;
               
int base = src0;
int offset = src1, bits = src2;
if (bits == 0) {
   dst = 0;
} else if (bits < 0 || offset < 0) {
   dst = 0; /* undefined */
} else if (offset + bits < 32) {
   dst = (base << (32 - bits - offset)) >> (32 - bits);
} else {
   dst = base >> offset;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ibitfield_extract(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];
                  int32_t src2 =
                     _src[2].i32[_i];

               int32_t dst;
               
int base = src0;
int offset = src1, bits = src2;
if (bits == 0) {
   dst = 0;
} else if (offset < 0 || bits < 0 || offset + bits > 32) {
   dst = 0;
} else {
   dst = (base << (32 - offset - bits)) >> offset; /* use sign-extending shift */
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];
                  int32_t src2 =
                     _src[2].i32[_i];

               int32_t dst;
               
int base = src0;
int offset = src1, bits = src2;
if (bits == 0) {
   dst = 0;
} else if (offset < 0 || bits < 0 || offset + bits > 32) {
   dst = 0;
} else {
   dst = (base << (32 - offset - bits)) >> offset; /* use sign-extending shift */
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_idiv(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = src0 / src1;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = src0 / src1;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ieq(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               bool32_t dst = src0 == src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               bool32_t dst = src0 == src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ifind_msb(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               int32_t dst;
               
dst = -1;
for (int bit = 31; bit >= 0; bit--) {
   /* If src0 < 0, we're looking for the first 0 bit.
    * if src0 >= 0, we're looking for the first 1 bit.
    */
   if ((((src0 >> bit) & 1) && (src0 >= 0)) ||
      (!((src0 >> bit) & 1) && (src0 < 0))) {
      dst = bit;
      break;
   }
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               int32_t dst;
               
dst = -1;
for (int bit = 31; bit >= 0; bit--) {
   /* If src0 < 0, we're looking for the first 0 bit.
    * if src0 >= 0, we're looking for the first 1 bit.
    */
   if ((((src0 >> bit) & 1) && (src0 >= 0)) ||
      (!((src0 >> bit) & 1) && (src0 < 0))) {
      dst = bit;
      break;
   }
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ige(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               bool32_t dst = src0 >= src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               bool32_t dst = src0 >= src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ilt(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               bool32_t dst = src0 < src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               bool32_t dst = src0 < src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_imax(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = src1 > src0 ? src1 : src0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = src1 > src0 ? src1 : src0;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_imin(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = src1 > src0 ? src0 : src1;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = src1 > src0 ? src0 : src1;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_imod(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = src1 == 0 ? 0 : ((src0 % src1 == 0 || (src0 >= 0) == (src1 >= 0)) ?                 src0 % src1 : src0 % src1 + src1);

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = src1 == 0 ? 0 : ((src0 % src1 == 0 || (src0 >= 0) == (src1 >= 0)) ?                 src0 % src1 : src0 % src1 + src1);

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_imov(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               int32_t dst = src0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];

               int64_t dst = src0;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_imul(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = src0 * src1;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = src0 * src1;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_imul_high(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = (int32_t)(((int64_t) src0 * (int64_t) src1) >> 32);

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = (int32_t)(((int64_t) src0 * (int64_t) src1) >> 32);

               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ine(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               bool32_t dst = src0 != src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               bool32_t dst = src0 != src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ineg(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               int32_t dst = -src0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];

               int64_t dst = -src0;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_inot(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               int32_t dst = ~src0;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];

               int64_t dst = ~src0;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ior(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = src0 | src1;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = src0 | src1;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_irem(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = src1 == 0 ? 0 : src0 % src1;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = src1 == 0 ? 0 : src0 % src1;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ishl(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = src0 << src1;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = src0 << src1;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ishr(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = src0 >> src1;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = src0 >> src1;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_isign(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];

               int32_t dst = (src0 == 0) ? 0 : ((src0 > 0) ? 1 : -1);

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];

               int64_t dst = (src0 == 0) ? 0 : ((src0 > 0) ? 1 : -1);

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_isub(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst = src0 - src1;

               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int64_t src0 =
                     _src[0].i64[_i];
                  int64_t src1 =
                     _src[1].i64[_i];

               int64_t dst = src0 - src1;

               _dst_val.i64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ixor(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = src0 ^ src1;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = src0 ^ src1;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ldexp(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               float32_t dst;
               
dst = (bit_size == 64) ? ldexp(src0, src1) : ldexpf(src0, src1);
/* flush denormals to zero. */
if (!isnormal(dst))
   dst = copysignf(0.0f, src0);


               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float64_t src0 =
                     _src[0].f64[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               float64_t dst;
               
dst = (bit_size == 64) ? ldexp(src0, src1) : ldexpf(src0, src1);
/* flush denormals to zero. */
if (!isnormal(dst))
   dst = copysignf(0.0f, src0);


               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_pack_double_2x32(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
               _src[0].u32[1],
         };

         struct uint64_vec dst;

            dst.x = src0.x | ((uint64_t)src0.y << 32);

               _dst_val.u64[0] = dst.x;

      break;
   }
   case 64: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
               _src[0].u32[1],
         };

         struct uint64_vec dst;

            dst.x = src0.x | ((uint64_t)src0.y << 32);

               _dst_val.u64[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_pack_double_2x32_split(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint64_t dst = src0 | ((uint64_t)src1 << 32);

               _dst_val.u64[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint64_t dst = src0 | ((uint64_t)src1 << 32);

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_pack_half_2x16(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct uint32_vec dst;

            
dst.x = (uint32_t) pack_half_1x16(src0.x);
dst.x |= ((uint32_t) pack_half_1x16(src0.y)) << 16;


               _dst_val.u32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct uint32_vec dst;

            
dst.x = (uint32_t) pack_half_1x16(src0.x);
dst.x |= ((uint32_t) pack_half_1x16(src0.y)) << 16;


               _dst_val.u32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_pack_half_2x16_split(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
         };

         struct uint32_vec dst;

            dst.x = dst.y = dst.z = dst.w = pack_half_1x16(src0.x) | (pack_half_1x16(src1.x) << 16);

               _dst_val.u32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
         };

         struct float32_vec src1 = {
               _src[1].f32[0],
         };

         struct uint32_vec dst;

            dst.x = dst.y = dst.z = dst.w = pack_half_1x16(src0.x) | (pack_half_1x16(src1.x) << 16);

               _dst_val.u32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_pack_snorm_2x16(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct uint32_vec dst;

            
dst.x = (uint32_t) pack_snorm_1x16(src0.x);
dst.x |= ((uint32_t) pack_snorm_1x16(src0.y)) << 16;


               _dst_val.u32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct uint32_vec dst;

            
dst.x = (uint32_t) pack_snorm_1x16(src0.x);
dst.x |= ((uint32_t) pack_snorm_1x16(src0.y)) << 16;


               _dst_val.u32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_pack_snorm_4x8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct uint32_vec dst;

            
dst.x = (uint32_t) pack_snorm_1x8(src0.x);
dst.x |= ((uint32_t) pack_snorm_1x8(src0.y)) << 8;
dst.x |= ((uint32_t) pack_snorm_1x8(src0.z)) << 16;
dst.x |= ((uint32_t) pack_snorm_1x8(src0.w)) << 24;


               _dst_val.u32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct uint32_vec dst;

            
dst.x = (uint32_t) pack_snorm_1x8(src0.x);
dst.x |= ((uint32_t) pack_snorm_1x8(src0.y)) << 8;
dst.x |= ((uint32_t) pack_snorm_1x8(src0.z)) << 16;
dst.x |= ((uint32_t) pack_snorm_1x8(src0.w)) << 24;


               _dst_val.u32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_pack_unorm_2x16(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct uint32_vec dst;

            
dst.x = (uint32_t) pack_unorm_1x16(src0.x);
dst.x |= ((uint32_t) pack_unorm_1x16(src0.y)) << 16;


               _dst_val.u32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
         };

         struct uint32_vec dst;

            
dst.x = (uint32_t) pack_unorm_1x16(src0.x);
dst.x |= ((uint32_t) pack_unorm_1x16(src0.y)) << 16;


               _dst_val.u32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_pack_unorm_4x8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct uint32_vec dst;

            
dst.x = (uint32_t) pack_unorm_1x8(src0.x);
dst.x |= ((uint32_t) pack_unorm_1x8(src0.y)) << 8;
dst.x |= ((uint32_t) pack_unorm_1x8(src0.z)) << 16;
dst.x |= ((uint32_t) pack_unorm_1x8(src0.w)) << 24;


               _dst_val.u32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct float32_vec src0 = {
               _src[0].f32[0],
               _src[0].f32[1],
               _src[0].f32[2],
               _src[0].f32[3],
         };

         struct uint32_vec dst;

            
dst.x = (uint32_t) pack_unorm_1x8(src0.x);
dst.x |= ((uint32_t) pack_unorm_1x8(src0.y)) << 8;
dst.x |= ((uint32_t) pack_unorm_1x8(src0.z)) << 16;
dst.x |= ((uint32_t) pack_unorm_1x8(src0.w)) << 24;


               _dst_val.u32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_pack_uvec2_to_uint(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
               _src[0].u32[1],
         };

         struct uint32_vec dst;

            
dst.x = (src0.x & 0xffff) | (src0.y >> 16);


               _dst_val.u32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
               _src[0].u32[1],
         };

         struct uint32_vec dst;

            
dst.x = (src0.x & 0xffff) | (src0.y >> 16);


               _dst_val.u32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_pack_uvec4_to_uint(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
               _src[0].u32[1],
               _src[0].u32[2],
               _src[0].u32[3],
         };

         struct uint32_vec dst;

            
dst.x = (src0.x <<  0) |
        (src0.y <<  8) |
        (src0.z << 16) |
        (src0.w << 24);


               _dst_val.u32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
               _src[0].u32[1],
               _src[0].u32[2],
               _src[0].u32[3],
         };

         struct uint32_vec dst;

            
dst.x = (src0.x <<  0) |
        (src0.y <<  8) |
        (src0.z << 16) |
        (src0.w << 24);


               _dst_val.u32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_seq(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = (src0 == src1) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = (src0 == src1) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_sge(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = (src0 >= src1) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = (src0 >= src1) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_slt(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = (src0 < src1) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = (src0 < src1) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_sne(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = (src0 != src1) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  float32_t src0 =
                     _src[0].f32[_i];
                  float32_t src1 =
                     _src[1].f32[_i];

               float32_t dst = (src0 != src1) ? 1.0f : 0.0f;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_u2d(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];

               float64_t dst = src0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];

               float64_t dst = src0;

               _dst_val.f64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_u2f(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];

               float32_t dst = src0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];

               float32_t dst = src0;

               _dst_val.f32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_uadd_carry(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = src0 + src1 < src0;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = src0 + src1 < src0;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ubfe(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];
                  int32_t src2 =
                     _src[2].i32[_i];

               uint32_t dst;
               
unsigned base = src0;
int offset = src1, bits = src2;
if (bits == 0) {
   dst = 0;
} else if (bits < 0 || offset < 0) {
   dst = 0; /* undefined */
} else if (offset + bits < 32) {
   dst = (base << (32 - bits - offset)) >> (32 - bits);
} else {
   dst = base >> offset;
}


               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];
                  int32_t src2 =
                     _src[2].i32[_i];

               uint32_t dst;
               
unsigned base = src0;
int offset = src1, bits = src2;
if (bits == 0) {
   dst = 0;
} else if (bits < 0 || offset < 0) {
   dst = 0; /* undefined */
} else if (offset + bits < 32) {
   dst = (base << (32 - bits - offset)) >> (32 - bits);
} else {
   dst = base >> offset;
}


               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ubitfield_extract(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];
                  int32_t src2 =
                     _src[2].i32[_i];

               uint32_t dst;
               
unsigned base = src0;
int offset = src1, bits = src2;
if (bits == 0) {
   dst = 0;
} else if (bits < 0 || offset < 0 || offset + bits > 32) {
   dst = 0; /* undefined per the spec */
} else {
   dst = (base >> offset) & ((1ull << bits) - 1);
}


               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                                    
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];
                  int32_t src2 =
                     _src[2].i32[_i];

               uint32_t dst;
               
unsigned base = src0;
int offset = src1, bits = src2;
if (bits == 0) {
   dst = 0;
} else if (bits < 0 || offset < 0 || offset + bits > 32) {
   dst = 0; /* undefined per the spec */
} else {
   dst = (base >> offset) & ((1ull << bits) - 1);
}


               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_udiv(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = src0 / src1;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = src0 / src1;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ufind_msb(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];

               int32_t dst;
               
dst = -1;
for (int bit = 31; bit > 0; bit--) {
   if ((src0 >> bit) & 1) {
      dst = bit;
      break;
   }
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];

               int32_t dst;
               
dst = -1;
for (int bit = 31; bit > 0; bit--) {
   if ((src0 >> bit) & 1) {
      dst = bit;
      break;
   }
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_uge(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               bool32_t dst = src0 >= src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               bool32_t dst = src0 >= src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ult(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               bool32_t dst = src0 < src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               bool32_t dst = src0 < src1;

               _dst_val.u32[_i] = dst ? NIR_TRUE : NIR_FALSE;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_umax(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = src1 > src0 ? src1 : src0;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = src1 > src0 ? src1 : src0;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_umax_4x8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst;
               
dst = 0;
for (int i = 0; i < 32; i += 8) {
   dst |= MAX2((src0 >> i) & 0xff, (src1 >> i) & 0xff) << i;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst;
               
dst = 0;
for (int i = 0; i < 32; i += 8) {
   dst |= MAX2((src0 >> i) & 0xff, (src1 >> i) & 0xff) << i;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_umin(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = src1 > src0 ? src0 : src1;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = src1 > src0 ? src0 : src1;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_umin_4x8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst;
               
dst = 0;
for (int i = 0; i < 32; i += 8) {
   dst |= MIN2((src0 >> i) & 0xff, (src1 >> i) & 0xff) << i;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst;
               
dst = 0;
for (int i = 0; i < 32; i += 8) {
   dst |= MIN2((src0 >> i) & 0xff, (src1 >> i) & 0xff) << i;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_umod(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = src1 == 0 ? 0 : src0 % src1;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = src1 == 0 ? 0 : src0 % src1;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_umul_high(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = (uint32_t)(((uint64_t) src0 * (uint64_t) src1) >> 32);

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = (uint32_t)(((uint64_t) src0 * (uint64_t) src1) >> 32);

               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_umul_unorm_4x8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst;
               
dst = 0;
for (int i = 0; i < 32; i += 8) {
   int src0_chan = (src0 >> i) & 0xff;
   int src1_chan = (src1 >> i) & 0xff;
   dst |= ((src0_chan * src1_chan) / 255) << i;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst;
               
dst = 0;
for (int i = 0; i < 32; i += 8) {
   int src0_chan = (src0 >> i) & 0xff;
   int src1_chan = (src1 >> i) & 0xff;
   dst |= ((src0_chan * src1_chan) / 255) << i;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_unpack_double_2x32(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint64_vec src0 = {
               _src[0].u64[0],
         };

         struct uint32_vec dst;

            dst.x = src0.x; dst.y = src0.x >> 32;

               _dst_val.u32[0] = dst.x;
               _dst_val.u32[1] = dst.y;

      break;
   }
   case 64: {
      


         struct uint64_vec src0 = {
               _src[0].u64[0],
         };

         struct uint32_vec dst;

            dst.x = src0.x; dst.y = src0.x >> 32;

               _dst_val.u32[0] = dst.x;
               _dst_val.u32[1] = dst.y;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_unpack_double_2x32_split_x(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];

               uint32_t dst = src0;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];

               uint32_t dst = src0;

               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_unpack_double_2x32_split_y(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];

               uint32_t dst = src0 >> 32;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

            
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];

               uint32_t dst = src0 >> 32;

               _dst_val.u32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_unpack_half_2x16(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            
dst.x = unpack_half_1x16((uint16_t)(src0.x & 0xffff));
dst.y = unpack_half_1x16((uint16_t)(src0.x << 16));


               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;

      break;
   }
   case 64: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            
dst.x = unpack_half_1x16((uint16_t)(src0.x & 0xffff));
dst.y = unpack_half_1x16((uint16_t)(src0.x << 16));


               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_unpack_half_2x16_split_x(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = unpack_half_1x16((uint16_t)(src0.x & 0xffff));

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = unpack_half_1x16((uint16_t)(src0.x & 0xffff));

               _dst_val.f32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_unpack_half_2x16_split_y(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = unpack_half_1x16((uint16_t)(src0.x >> 16));

               _dst_val.f32[0] = dst.x;

      break;
   }
   case 64: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            dst.x = dst.y = dst.z = dst.w = unpack_half_1x16((uint16_t)(src0.x >> 16));

               _dst_val.f32[0] = dst.x;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_unpack_snorm_2x16(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            
dst.x = unpack_snorm_1x16((uint16_t)(src0.x & 0xffff));
dst.y = unpack_snorm_1x16((uint16_t)(src0.x << 16));


               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;

      break;
   }
   case 64: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            
dst.x = unpack_snorm_1x16((uint16_t)(src0.x & 0xffff));
dst.y = unpack_snorm_1x16((uint16_t)(src0.x << 16));


               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_unpack_snorm_4x8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            
dst.x = unpack_snorm_1x8((uint8_t)(src0.x & 0xff));
dst.y = unpack_snorm_1x8((uint8_t)((src0.x >> 8) & 0xff));
dst.z = unpack_snorm_1x8((uint8_t)((src0.x >> 16) & 0xff));
dst.w = unpack_snorm_1x8((uint8_t)(src0.x >> 24));


               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }
   case 64: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            
dst.x = unpack_snorm_1x8((uint8_t)(src0.x & 0xff));
dst.y = unpack_snorm_1x8((uint8_t)((src0.x >> 8) & 0xff));
dst.z = unpack_snorm_1x8((uint8_t)((src0.x >> 16) & 0xff));
dst.w = unpack_snorm_1x8((uint8_t)(src0.x >> 24));


               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_unpack_unorm_2x16(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            
dst.x = unpack_unorm_1x16((uint16_t)(src0.x & 0xffff));
dst.y = unpack_unorm_1x16((uint16_t)(src0.x << 16));


               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;

      break;
   }
   case 64: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            
dst.x = unpack_unorm_1x16((uint16_t)(src0.x & 0xffff));
dst.y = unpack_unorm_1x16((uint16_t)(src0.x << 16));


               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_unpack_unorm_4x8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            
dst.x = unpack_unorm_1x8((uint8_t)(src0.x & 0xff));
dst.y = unpack_unorm_1x8((uint8_t)((src0.x >> 8) & 0xff));
dst.z = unpack_unorm_1x8((uint8_t)((src0.x >> 16) & 0xff));
dst.w = unpack_unorm_1x8((uint8_t)(src0.x >> 24));


               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }
   case 64: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct float32_vec dst;

            
dst.x = unpack_unorm_1x8((uint8_t)(src0.x & 0xff));
dst.y = unpack_unorm_1x8((uint8_t)((src0.x >> 8) & 0xff));
dst.z = unpack_unorm_1x8((uint8_t)((src0.x >> 16) & 0xff));
dst.w = unpack_unorm_1x8((uint8_t)(src0.x >> 24));


               _dst_val.f32[0] = dst.x;
               _dst_val.f32[1] = dst.y;
               _dst_val.f32[2] = dst.z;
               _dst_val.f32[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_usadd_4x8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst;
               
dst = 0;
for (int i = 0; i < 32; i += 8) {
   dst |= MIN2(((src0 >> i) & 0xff) + ((src1 >> i) & 0xff), 0xff) << i;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst;
               
dst = 0;
for (int i = 0; i < 32; i += 8) {
   dst |= MIN2(((src0 >> i) & 0xff) + ((src1 >> i) & 0xff), 0xff) << i;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ushr(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = src0 >> src1;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = src0 >> src1;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_ussub_4x8(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst;
               
dst = 0;
for (int i = 0; i < 32; i += 8) {
   int src0_chan = (src0 >> i) & 0xff;
   int src1_chan = (src1 >> i) & 0xff;
   if (src0_chan > src1_chan)
      dst |= (src0_chan - src1_chan) << i;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  int32_t src0 =
                     _src[0].i32[_i];
                  int32_t src1 =
                     _src[1].i32[_i];

               int32_t dst;
               
dst = 0;
for (int i = 0; i < 32; i += 8) {
   int src0_chan = (src0 >> i) & 0xff;
   int src1_chan = (src1 >> i) & 0xff;
   if (src0_chan > src1_chan)
      dst |= (src0_chan - src1_chan) << i;
}


               _dst_val.i32[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_usub_borrow(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint32_t src0 =
                     _src[0].u32[_i];
                  uint32_t src1 =
                     _src[1].u32[_i];

               uint32_t dst = src0 < src1;

               _dst_val.u32[_i] = dst;
         }

      break;
   }
   case 64: {
      

                        
         for (unsigned _i = 0; _i < num_components; _i++) {
                  uint64_t src0 =
                     _src[0].u64[_i];
                  uint64_t src1 =
                     _src[1].u64[_i];

               uint64_t dst = src0 < src1;

               _dst_val.u64[_i] = dst;
         }

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_vec2(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct uint32_vec src1 = {
               _src[1].u32[0],
         };

         struct uint32_vec dst;

            
dst.x = src0.x;
dst.y = src1.x;


               _dst_val.u32[0] = dst.x;
               _dst_val.u32[1] = dst.y;

      break;
   }
   case 64: {
      


         struct uint64_vec src0 = {
               _src[0].u64[0],
         };

         struct uint64_vec src1 = {
               _src[1].u64[0],
         };

         struct uint64_vec dst;

            
dst.x = src0.x;
dst.y = src1.x;


               _dst_val.u64[0] = dst.x;
               _dst_val.u64[1] = dst.y;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_vec3(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct uint32_vec src1 = {
               _src[1].u32[0],
         };

         struct uint32_vec src2 = {
               _src[2].u32[0],
         };

         struct uint32_vec dst;

            
dst.x = src0.x;
dst.y = src1.x;
dst.z = src2.x;


               _dst_val.u32[0] = dst.x;
               _dst_val.u32[1] = dst.y;
               _dst_val.u32[2] = dst.z;

      break;
   }
   case 64: {
      


         struct uint64_vec src0 = {
               _src[0].u64[0],
         };

         struct uint64_vec src1 = {
               _src[1].u64[0],
         };

         struct uint64_vec src2 = {
               _src[2].u64[0],
         };

         struct uint64_vec dst;

            
dst.x = src0.x;
dst.y = src1.x;
dst.z = src2.x;


               _dst_val.u64[0] = dst.x;
               _dst_val.u64[1] = dst.y;
               _dst_val.u64[2] = dst.z;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}
static nir_const_value
evaluate_vec4(unsigned num_components, unsigned bit_size,
                 nir_const_value *_src)
{
   nir_const_value _dst_val = { {0, } };

   switch (bit_size) {
   case 32: {
      


         struct uint32_vec src0 = {
               _src[0].u32[0],
         };

         struct uint32_vec src1 = {
               _src[1].u32[0],
         };

         struct uint32_vec src2 = {
               _src[2].u32[0],
         };

         struct uint32_vec src3 = {
               _src[3].u32[0],
         };

         struct uint32_vec dst;

            
dst.x = src0.x;
dst.y = src1.x;
dst.z = src2.x;
dst.w = src3.x;


               _dst_val.u32[0] = dst.x;
               _dst_val.u32[1] = dst.y;
               _dst_val.u32[2] = dst.z;
               _dst_val.u32[3] = dst.w;

      break;
   }
   case 64: {
      


         struct uint64_vec src0 = {
               _src[0].u64[0],
         };

         struct uint64_vec src1 = {
               _src[1].u64[0],
         };

         struct uint64_vec src2 = {
               _src[2].u64[0],
         };

         struct uint64_vec src3 = {
               _src[3].u64[0],
         };

         struct uint64_vec dst;

            
dst.x = src0.x;
dst.y = src1.x;
dst.z = src2.x;
dst.w = src3.x;


               _dst_val.u64[0] = dst.x;
               _dst_val.u64[1] = dst.y;
               _dst_val.u64[2] = dst.z;
               _dst_val.u64[3] = dst.w;

      break;
   }

   default:
      unreachable("unknown bit width");
   }

   return _dst_val;
}

nir_const_value
nir_eval_const_opcode(nir_op op, unsigned num_components,
                      unsigned bit_width, nir_const_value *src)
{
   switch (op) {
   case nir_op_b2f: {
      return evaluate_b2f(num_components, bit_width, src);
      break;
   }
   case nir_op_b2i: {
      return evaluate_b2i(num_components, bit_width, src);
      break;
   }
   case nir_op_ball_fequal2: {
      return evaluate_ball_fequal2(num_components, bit_width, src);
      break;
   }
   case nir_op_ball_fequal3: {
      return evaluate_ball_fequal3(num_components, bit_width, src);
      break;
   }
   case nir_op_ball_fequal4: {
      return evaluate_ball_fequal4(num_components, bit_width, src);
      break;
   }
   case nir_op_ball_iequal2: {
      return evaluate_ball_iequal2(num_components, bit_width, src);
      break;
   }
   case nir_op_ball_iequal3: {
      return evaluate_ball_iequal3(num_components, bit_width, src);
      break;
   }
   case nir_op_ball_iequal4: {
      return evaluate_ball_iequal4(num_components, bit_width, src);
      break;
   }
   case nir_op_bany_fnequal2: {
      return evaluate_bany_fnequal2(num_components, bit_width, src);
      break;
   }
   case nir_op_bany_fnequal3: {
      return evaluate_bany_fnequal3(num_components, bit_width, src);
      break;
   }
   case nir_op_bany_fnequal4: {
      return evaluate_bany_fnequal4(num_components, bit_width, src);
      break;
   }
   case nir_op_bany_inequal2: {
      return evaluate_bany_inequal2(num_components, bit_width, src);
      break;
   }
   case nir_op_bany_inequal3: {
      return evaluate_bany_inequal3(num_components, bit_width, src);
      break;
   }
   case nir_op_bany_inequal4: {
      return evaluate_bany_inequal4(num_components, bit_width, src);
      break;
   }
   case nir_op_bcsel: {
      return evaluate_bcsel(num_components, bit_width, src);
      break;
   }
   case nir_op_bfi: {
      return evaluate_bfi(num_components, bit_width, src);
      break;
   }
   case nir_op_bfm: {
      return evaluate_bfm(num_components, bit_width, src);
      break;
   }
   case nir_op_bit_count: {
      return evaluate_bit_count(num_components, bit_width, src);
      break;
   }
   case nir_op_bitfield_insert: {
      return evaluate_bitfield_insert(num_components, bit_width, src);
      break;
   }
   case nir_op_bitfield_reverse: {
      return evaluate_bitfield_reverse(num_components, bit_width, src);
      break;
   }
   case nir_op_d2b: {
      return evaluate_d2b(num_components, bit_width, src);
      break;
   }
   case nir_op_d2f: {
      return evaluate_d2f(num_components, bit_width, src);
      break;
   }
   case nir_op_d2i: {
      return evaluate_d2i(num_components, bit_width, src);
      break;
   }
   case nir_op_d2u: {
      return evaluate_d2u(num_components, bit_width, src);
      break;
   }
   case nir_op_extract_i16: {
      return evaluate_extract_i16(num_components, bit_width, src);
      break;
   }
   case nir_op_extract_i8: {
      return evaluate_extract_i8(num_components, bit_width, src);
      break;
   }
   case nir_op_extract_u16: {
      return evaluate_extract_u16(num_components, bit_width, src);
      break;
   }
   case nir_op_extract_u8: {
      return evaluate_extract_u8(num_components, bit_width, src);
      break;
   }
   case nir_op_f2b: {
      return evaluate_f2b(num_components, bit_width, src);
      break;
   }
   case nir_op_f2d: {
      return evaluate_f2d(num_components, bit_width, src);
      break;
   }
   case nir_op_f2i: {
      return evaluate_f2i(num_components, bit_width, src);
      break;
   }
   case nir_op_f2u: {
      return evaluate_f2u(num_components, bit_width, src);
      break;
   }
   case nir_op_fabs: {
      return evaluate_fabs(num_components, bit_width, src);
      break;
   }
   case nir_op_fadd: {
      return evaluate_fadd(num_components, bit_width, src);
      break;
   }
   case nir_op_fall_equal2: {
      return evaluate_fall_equal2(num_components, bit_width, src);
      break;
   }
   case nir_op_fall_equal3: {
      return evaluate_fall_equal3(num_components, bit_width, src);
      break;
   }
   case nir_op_fall_equal4: {
      return evaluate_fall_equal4(num_components, bit_width, src);
      break;
   }
   case nir_op_fand: {
      return evaluate_fand(num_components, bit_width, src);
      break;
   }
   case nir_op_fany_nequal2: {
      return evaluate_fany_nequal2(num_components, bit_width, src);
      break;
   }
   case nir_op_fany_nequal3: {
      return evaluate_fany_nequal3(num_components, bit_width, src);
      break;
   }
   case nir_op_fany_nequal4: {
      return evaluate_fany_nequal4(num_components, bit_width, src);
      break;
   }
   case nir_op_fceil: {
      return evaluate_fceil(num_components, bit_width, src);
      break;
   }
   case nir_op_fcos: {
      return evaluate_fcos(num_components, bit_width, src);
      break;
   }
   case nir_op_fcsel: {
      return evaluate_fcsel(num_components, bit_width, src);
      break;
   }
   case nir_op_fddx: {
      return evaluate_fddx(num_components, bit_width, src);
      break;
   }
   case nir_op_fddx_coarse: {
      return evaluate_fddx_coarse(num_components, bit_width, src);
      break;
   }
   case nir_op_fddx_fine: {
      return evaluate_fddx_fine(num_components, bit_width, src);
      break;
   }
   case nir_op_fddy: {
      return evaluate_fddy(num_components, bit_width, src);
      break;
   }
   case nir_op_fddy_coarse: {
      return evaluate_fddy_coarse(num_components, bit_width, src);
      break;
   }
   case nir_op_fddy_fine: {
      return evaluate_fddy_fine(num_components, bit_width, src);
      break;
   }
   case nir_op_fdiv: {
      return evaluate_fdiv(num_components, bit_width, src);
      break;
   }
   case nir_op_fdot2: {
      return evaluate_fdot2(num_components, bit_width, src);
      break;
   }
   case nir_op_fdot3: {
      return evaluate_fdot3(num_components, bit_width, src);
      break;
   }
   case nir_op_fdot4: {
      return evaluate_fdot4(num_components, bit_width, src);
      break;
   }
   case nir_op_fdot_replicated2: {
      return evaluate_fdot_replicated2(num_components, bit_width, src);
      break;
   }
   case nir_op_fdot_replicated3: {
      return evaluate_fdot_replicated3(num_components, bit_width, src);
      break;
   }
   case nir_op_fdot_replicated4: {
      return evaluate_fdot_replicated4(num_components, bit_width, src);
      break;
   }
   case nir_op_fdph: {
      return evaluate_fdph(num_components, bit_width, src);
      break;
   }
   case nir_op_fdph_replicated: {
      return evaluate_fdph_replicated(num_components, bit_width, src);
      break;
   }
   case nir_op_feq: {
      return evaluate_feq(num_components, bit_width, src);
      break;
   }
   case nir_op_fexp2: {
      return evaluate_fexp2(num_components, bit_width, src);
      break;
   }
   case nir_op_ffloor: {
      return evaluate_ffloor(num_components, bit_width, src);
      break;
   }
   case nir_op_ffma: {
      return evaluate_ffma(num_components, bit_width, src);
      break;
   }
   case nir_op_ffract: {
      return evaluate_ffract(num_components, bit_width, src);
      break;
   }
   case nir_op_fge: {
      return evaluate_fge(num_components, bit_width, src);
      break;
   }
   case nir_op_find_lsb: {
      return evaluate_find_lsb(num_components, bit_width, src);
      break;
   }
   case nir_op_flog2: {
      return evaluate_flog2(num_components, bit_width, src);
      break;
   }
   case nir_op_flrp: {
      return evaluate_flrp(num_components, bit_width, src);
      break;
   }
   case nir_op_flt: {
      return evaluate_flt(num_components, bit_width, src);
      break;
   }
   case nir_op_fmax: {
      return evaluate_fmax(num_components, bit_width, src);
      break;
   }
   case nir_op_fmin: {
      return evaluate_fmin(num_components, bit_width, src);
      break;
   }
   case nir_op_fmod: {
      return evaluate_fmod(num_components, bit_width, src);
      break;
   }
   case nir_op_fmov: {
      return evaluate_fmov(num_components, bit_width, src);
      break;
   }
   case nir_op_fmul: {
      return evaluate_fmul(num_components, bit_width, src);
      break;
   }
   case nir_op_fne: {
      return evaluate_fne(num_components, bit_width, src);
      break;
   }
   case nir_op_fneg: {
      return evaluate_fneg(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise1_1: {
      return evaluate_fnoise1_1(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise1_2: {
      return evaluate_fnoise1_2(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise1_3: {
      return evaluate_fnoise1_3(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise1_4: {
      return evaluate_fnoise1_4(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise2_1: {
      return evaluate_fnoise2_1(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise2_2: {
      return evaluate_fnoise2_2(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise2_3: {
      return evaluate_fnoise2_3(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise2_4: {
      return evaluate_fnoise2_4(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise3_1: {
      return evaluate_fnoise3_1(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise3_2: {
      return evaluate_fnoise3_2(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise3_3: {
      return evaluate_fnoise3_3(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise3_4: {
      return evaluate_fnoise3_4(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise4_1: {
      return evaluate_fnoise4_1(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise4_2: {
      return evaluate_fnoise4_2(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise4_3: {
      return evaluate_fnoise4_3(num_components, bit_width, src);
      break;
   }
   case nir_op_fnoise4_4: {
      return evaluate_fnoise4_4(num_components, bit_width, src);
      break;
   }
   case nir_op_fnot: {
      return evaluate_fnot(num_components, bit_width, src);
      break;
   }
   case nir_op_for: {
      return evaluate_for(num_components, bit_width, src);
      break;
   }
   case nir_op_fpow: {
      return evaluate_fpow(num_components, bit_width, src);
      break;
   }
   case nir_op_fquantize2f16: {
      return evaluate_fquantize2f16(num_components, bit_width, src);
      break;
   }
   case nir_op_frcp: {
      return evaluate_frcp(num_components, bit_width, src);
      break;
   }
   case nir_op_frem: {
      return evaluate_frem(num_components, bit_width, src);
      break;
   }
   case nir_op_fround_even: {
      return evaluate_fround_even(num_components, bit_width, src);
      break;
   }
   case nir_op_frsq: {
      return evaluate_frsq(num_components, bit_width, src);
      break;
   }
   case nir_op_fsat: {
      return evaluate_fsat(num_components, bit_width, src);
      break;
   }
   case nir_op_fsign: {
      return evaluate_fsign(num_components, bit_width, src);
      break;
   }
   case nir_op_fsin: {
      return evaluate_fsin(num_components, bit_width, src);
      break;
   }
   case nir_op_fsqrt: {
      return evaluate_fsqrt(num_components, bit_width, src);
      break;
   }
   case nir_op_fsub: {
      return evaluate_fsub(num_components, bit_width, src);
      break;
   }
   case nir_op_ftrunc: {
      return evaluate_ftrunc(num_components, bit_width, src);
      break;
   }
   case nir_op_fxor: {
      return evaluate_fxor(num_components, bit_width, src);
      break;
   }
   case nir_op_i2b: {
      return evaluate_i2b(num_components, bit_width, src);
      break;
   }
   case nir_op_i2d: {
      return evaluate_i2d(num_components, bit_width, src);
      break;
   }
   case nir_op_i2f: {
      return evaluate_i2f(num_components, bit_width, src);
      break;
   }
   case nir_op_iabs: {
      return evaluate_iabs(num_components, bit_width, src);
      break;
   }
   case nir_op_iadd: {
      return evaluate_iadd(num_components, bit_width, src);
      break;
   }
   case nir_op_iand: {
      return evaluate_iand(num_components, bit_width, src);
      break;
   }
   case nir_op_ibfe: {
      return evaluate_ibfe(num_components, bit_width, src);
      break;
   }
   case nir_op_ibitfield_extract: {
      return evaluate_ibitfield_extract(num_components, bit_width, src);
      break;
   }
   case nir_op_idiv: {
      return evaluate_idiv(num_components, bit_width, src);
      break;
   }
   case nir_op_ieq: {
      return evaluate_ieq(num_components, bit_width, src);
      break;
   }
   case nir_op_ifind_msb: {
      return evaluate_ifind_msb(num_components, bit_width, src);
      break;
   }
   case nir_op_ige: {
      return evaluate_ige(num_components, bit_width, src);
      break;
   }
   case nir_op_ilt: {
      return evaluate_ilt(num_components, bit_width, src);
      break;
   }
   case nir_op_imax: {
      return evaluate_imax(num_components, bit_width, src);
      break;
   }
   case nir_op_imin: {
      return evaluate_imin(num_components, bit_width, src);
      break;
   }
   case nir_op_imod: {
      return evaluate_imod(num_components, bit_width, src);
      break;
   }
   case nir_op_imov: {
      return evaluate_imov(num_components, bit_width, src);
      break;
   }
   case nir_op_imul: {
      return evaluate_imul(num_components, bit_width, src);
      break;
   }
   case nir_op_imul_high: {
      return evaluate_imul_high(num_components, bit_width, src);
      break;
   }
   case nir_op_ine: {
      return evaluate_ine(num_components, bit_width, src);
      break;
   }
   case nir_op_ineg: {
      return evaluate_ineg(num_components, bit_width, src);
      break;
   }
   case nir_op_inot: {
      return evaluate_inot(num_components, bit_width, src);
      break;
   }
   case nir_op_ior: {
      return evaluate_ior(num_components, bit_width, src);
      break;
   }
   case nir_op_irem: {
      return evaluate_irem(num_components, bit_width, src);
      break;
   }
   case nir_op_ishl: {
      return evaluate_ishl(num_components, bit_width, src);
      break;
   }
   case nir_op_ishr: {
      return evaluate_ishr(num_components, bit_width, src);
      break;
   }
   case nir_op_isign: {
      return evaluate_isign(num_components, bit_width, src);
      break;
   }
   case nir_op_isub: {
      return evaluate_isub(num_components, bit_width, src);
      break;
   }
   case nir_op_ixor: {
      return evaluate_ixor(num_components, bit_width, src);
      break;
   }
   case nir_op_ldexp: {
      return evaluate_ldexp(num_components, bit_width, src);
      break;
   }
   case nir_op_pack_double_2x32: {
      return evaluate_pack_double_2x32(num_components, bit_width, src);
      break;
   }
   case nir_op_pack_double_2x32_split: {
      return evaluate_pack_double_2x32_split(num_components, bit_width, src);
      break;
   }
   case nir_op_pack_half_2x16: {
      return evaluate_pack_half_2x16(num_components, bit_width, src);
      break;
   }
   case nir_op_pack_half_2x16_split: {
      return evaluate_pack_half_2x16_split(num_components, bit_width, src);
      break;
   }
   case nir_op_pack_snorm_2x16: {
      return evaluate_pack_snorm_2x16(num_components, bit_width, src);
      break;
   }
   case nir_op_pack_snorm_4x8: {
      return evaluate_pack_snorm_4x8(num_components, bit_width, src);
      break;
   }
   case nir_op_pack_unorm_2x16: {
      return evaluate_pack_unorm_2x16(num_components, bit_width, src);
      break;
   }
   case nir_op_pack_unorm_4x8: {
      return evaluate_pack_unorm_4x8(num_components, bit_width, src);
      break;
   }
   case nir_op_pack_uvec2_to_uint: {
      return evaluate_pack_uvec2_to_uint(num_components, bit_width, src);
      break;
   }
   case nir_op_pack_uvec4_to_uint: {
      return evaluate_pack_uvec4_to_uint(num_components, bit_width, src);
      break;
   }
   case nir_op_seq: {
      return evaluate_seq(num_components, bit_width, src);
      break;
   }
   case nir_op_sge: {
      return evaluate_sge(num_components, bit_width, src);
      break;
   }
   case nir_op_slt: {
      return evaluate_slt(num_components, bit_width, src);
      break;
   }
   case nir_op_sne: {
      return evaluate_sne(num_components, bit_width, src);
      break;
   }
   case nir_op_u2d: {
      return evaluate_u2d(num_components, bit_width, src);
      break;
   }
   case nir_op_u2f: {
      return evaluate_u2f(num_components, bit_width, src);
      break;
   }
   case nir_op_uadd_carry: {
      return evaluate_uadd_carry(num_components, bit_width, src);
      break;
   }
   case nir_op_ubfe: {
      return evaluate_ubfe(num_components, bit_width, src);
      break;
   }
   case nir_op_ubitfield_extract: {
      return evaluate_ubitfield_extract(num_components, bit_width, src);
      break;
   }
   case nir_op_udiv: {
      return evaluate_udiv(num_components, bit_width, src);
      break;
   }
   case nir_op_ufind_msb: {
      return evaluate_ufind_msb(num_components, bit_width, src);
      break;
   }
   case nir_op_uge: {
      return evaluate_uge(num_components, bit_width, src);
      break;
   }
   case nir_op_ult: {
      return evaluate_ult(num_components, bit_width, src);
      break;
   }
   case nir_op_umax: {
      return evaluate_umax(num_components, bit_width, src);
      break;
   }
   case nir_op_umax_4x8: {
      return evaluate_umax_4x8(num_components, bit_width, src);
      break;
   }
   case nir_op_umin: {
      return evaluate_umin(num_components, bit_width, src);
      break;
   }
   case nir_op_umin_4x8: {
      return evaluate_umin_4x8(num_components, bit_width, src);
      break;
   }
   case nir_op_umod: {
      return evaluate_umod(num_components, bit_width, src);
      break;
   }
   case nir_op_umul_high: {
      return evaluate_umul_high(num_components, bit_width, src);
      break;
   }
   case nir_op_umul_unorm_4x8: {
      return evaluate_umul_unorm_4x8(num_components, bit_width, src);
      break;
   }
   case nir_op_unpack_double_2x32: {
      return evaluate_unpack_double_2x32(num_components, bit_width, src);
      break;
   }
   case nir_op_unpack_double_2x32_split_x: {
      return evaluate_unpack_double_2x32_split_x(num_components, bit_width, src);
      break;
   }
   case nir_op_unpack_double_2x32_split_y: {
      return evaluate_unpack_double_2x32_split_y(num_components, bit_width, src);
      break;
   }
   case nir_op_unpack_half_2x16: {
      return evaluate_unpack_half_2x16(num_components, bit_width, src);
      break;
   }
   case nir_op_unpack_half_2x16_split_x: {
      return evaluate_unpack_half_2x16_split_x(num_components, bit_width, src);
      break;
   }
   case nir_op_unpack_half_2x16_split_y: {
      return evaluate_unpack_half_2x16_split_y(num_components, bit_width, src);
      break;
   }
   case nir_op_unpack_snorm_2x16: {
      return evaluate_unpack_snorm_2x16(num_components, bit_width, src);
      break;
   }
   case nir_op_unpack_snorm_4x8: {
      return evaluate_unpack_snorm_4x8(num_components, bit_width, src);
      break;
   }
   case nir_op_unpack_unorm_2x16: {
      return evaluate_unpack_unorm_2x16(num_components, bit_width, src);
      break;
   }
   case nir_op_unpack_unorm_4x8: {
      return evaluate_unpack_unorm_4x8(num_components, bit_width, src);
      break;
   }
   case nir_op_usadd_4x8: {
      return evaluate_usadd_4x8(num_components, bit_width, src);
      break;
   }
   case nir_op_ushr: {
      return evaluate_ushr(num_components, bit_width, src);
      break;
   }
   case nir_op_ussub_4x8: {
      return evaluate_ussub_4x8(num_components, bit_width, src);
      break;
   }
   case nir_op_usub_borrow: {
      return evaluate_usub_borrow(num_components, bit_width, src);
      break;
   }
   case nir_op_vec2: {
      return evaluate_vec2(num_components, bit_width, src);
      break;
   }
   case nir_op_vec3: {
      return evaluate_vec3(num_components, bit_width, src);
      break;
   }
   case nir_op_vec4: {
      return evaluate_vec4(num_components, bit_width, src);
      break;
   }
   default:
      unreachable("shouldn't get here");
   }
}
