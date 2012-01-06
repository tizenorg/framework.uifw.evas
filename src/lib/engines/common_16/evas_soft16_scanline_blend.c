/** NOTE: This file is meant to be included by users **/

/** NOTE2: r, g, b parameters are 16bits, so you can pass 0 to 256 inclusive.
 **        this is due our division by 256 when multiplying the color.
 **/

/*****************************************************************************
 * Scanline processing
 *
 *    _soft16_scanline_<description>_<src>_<dst>[_<modifier>]()
 *
 ****************************************************************************/

#include "evas_soft16_point_blend.c"

/***********************************************************************
 * Regular blend operations
 */
static void
_soft16_scanline_blend_transp_solid(DATA16 *src, DATA8 *alpha, DATA16 *dst, int size)
{
   DATA16 *start, *end;

   start = dst;
   end = start + (size & ~7);

   pld(alpha, 0);
   pld(src, 0);

   /* work on 8 pixels per time, do data preload */
   while (start < end)
     {
	DATA8 alpha1, alpha2;

	alpha1 = alpha[0];
	alpha += 8;

	/* empirical tests show these give the best performance */
	pld(alpha, 8);
	pld(src, 32);

	src += 8;
	start += 8;

	alpha2 = alpha[-7];
        _soft16_pt_blend_transp_solid(start - 8, src[-8], alpha1);

	alpha1 = alpha[-6];
        _soft16_pt_blend_transp_solid(start - 7, src[-7], alpha2);

	alpha2 = alpha[-5];
	_soft16_pt_blend_transp_solid(start - 6, src[-6], alpha1);

	alpha1 = alpha[-4];
	_soft16_pt_blend_transp_solid(start - 5, src[-5], alpha2);

	alpha2 = alpha[-3];
	_soft16_pt_blend_transp_solid(start - 4, src[-4], alpha1);

	alpha1 = alpha[-2];
	_soft16_pt_blend_transp_solid(start - 3, src[-3], alpha2);

	alpha2 = alpha[-1];
	_soft16_pt_blend_transp_solid(start - 2, src[-2], alpha1);

	_soft16_pt_blend_transp_solid(start - 1, src[-1], alpha2);
     }

   /* remaining pixels (up to 7) */
   end = start + (size & 7);
   for (; start < end; start++, src++, alpha++)
      _soft16_pt_blend_transp_solid(start, *src, *alpha);
}

static inline void
_soft16_scanline_blend_solid_solid(DATA16 *src, DATA16 *dst, int size)
{
   memcpy(dst, src, size * sizeof(DATA16));
}

/***********************************************************************
 * Blend operations taking an extra alpha (fade in, out)
 */

static void
_soft16_scanline_blend_transp_solid_mul_alpha(DATA16 *src, DATA8 *alpha, DATA16 *dst, int size, const DATA8 rel_alpha)
{
   DATA16 *start, *end;

   start = dst;
   end = start + (size & ~7);

   pld(alpha, 0);
   pld(src, 0);

   while (start < end)
     {
	DATA8 alpha1, alpha2;

	alpha1 = alpha[0];
	alpha += 8;

	pld(alpha, 8);
	pld(src, 32);

	src += 8;
	start += 8;

	alpha2 = alpha[-7];
	_soft16_pt_blend_transp_solid_mul_alpha
           (start - 8, src[-8], alpha1, rel_alpha);

	alpha1 = alpha[-6];
	_soft16_pt_blend_transp_solid_mul_alpha
           (start - 7, src[-7], alpha2, rel_alpha);

	alpha2 = alpha[-5];
	_soft16_pt_blend_transp_solid_mul_alpha
           (start - 6, src[-6], alpha1, rel_alpha);

	alpha1 = alpha[-4];
	_soft16_pt_blend_transp_solid_mul_alpha
           (start - 5, src[-5], alpha2, rel_alpha);

	alpha2 = alpha[-3];
	_soft16_pt_blend_transp_solid_mul_alpha
           (start - 4, src[-4], alpha1, rel_alpha);

	alpha1 = alpha[-2];
	_soft16_pt_blend_transp_solid_mul_alpha
           (start - 3, src[-3], alpha2, rel_alpha);

	alpha2 = alpha[-1];
	_soft16_pt_blend_transp_solid_mul_alpha
           (start - 2, src[-2], alpha1, rel_alpha);

	_soft16_pt_blend_transp_solid_mul_alpha
           (start - 1, src[-1], alpha2, rel_alpha);
     }

   end = start + (size & 7);
   for (; start < end; start++, src++, alpha++)
      _soft16_pt_blend_transp_solid_mul_alpha(start, *src, *alpha, rel_alpha);
}

static void
_soft16_scanline_blend_solid_solid_mul_alpha(DATA16 *src, DATA16 *dst, int size, DATA8 rel_alpha)
{
   DATA16 *start, *end;

   start = dst;
   end = start + (size & ~7);

   pld(src, 0);

   while (start < end)
     {
	pld(src, 32);
        UNROLL8({
           _soft16_pt_blend_solid_solid_mul_alpha(start, *src, rel_alpha);
           start++;
           src++;
        });
     }

   end = start + (size & 7);
   for (; start < end; start++, src++)
     _soft16_pt_blend_solid_solid_mul_alpha(start, *src, rel_alpha);
}

/***********************************************************************
 * Blend operations with extra alpha and multiply color
 */

static void
_soft16_scanline_blend_transp_solid_mul_color_transp(DATA16 *src, DATA8 *alpha, DATA16 *dst, int size, DATA8 rel_alpha, DATA16 r, DATA16 g, DATA16 b)
{
   DATA16 *start, *end;

   start = dst;
   end = start + (size & ~7);

   pld(alpha, 0);
   pld(src, 0);

   while (start < end)
     {
	DATA8 alpha1, alpha2;

	alpha1 = alpha[0];
	alpha += 8;

        pld(src, 32);
        pld(start, 32);

	src += 8;
	start += 8;

	alpha2 = alpha[-7];
        _soft16_pt_blend_transp_solid_mul_color_transp
           (start - 8, src[-8], alpha1, rel_alpha, r, g, b);

	alpha1 = alpha[-6];
        _soft16_pt_blend_transp_solid_mul_color_transp
           (start - 7, src[-7], alpha2, rel_alpha, r, g, b);

	alpha2 = alpha[-5];
        _soft16_pt_blend_transp_solid_mul_color_transp
           (start - 6, src[-6], alpha1, rel_alpha, r, g, b);

	alpha1 = alpha[-4];
        _soft16_pt_blend_transp_solid_mul_color_transp
           (start - 5, src[-5], alpha2, rel_alpha, r, g, b);

	alpha2 = alpha[-3];
        _soft16_pt_blend_transp_solid_mul_color_transp
           (start - 4, src[-4], alpha1, rel_alpha, r, g, b);

	alpha1 = alpha[-2];
        _soft16_pt_blend_transp_solid_mul_color_transp
           (start - 3, src[-3], alpha2, rel_alpha, r, g, b);

	alpha2 = alpha[-1];
        _soft16_pt_blend_transp_solid_mul_color_transp
           (start - 2, src[-2], alpha1, rel_alpha, r, g, b);

        _soft16_pt_blend_transp_solid_mul_color_transp
           (start - 1, src[-1], alpha2, rel_alpha, r, g, b);
     }

   end = start + (size & 7);
   for (; start < end; start++, src++, alpha++)
      _soft16_pt_blend_transp_solid_mul_color_transp
         (start, *src, *alpha, rel_alpha, r, g, b);
}

static void
_soft16_scanline_blend_solid_solid_mul_color_transp(DATA16 *src, DATA16 *dst, int size, DATA8 rel_alpha, DATA16 r, DATA16 g, DATA16 b)
{
   DATA16 *start, *end;

   start = dst;
   end = start + (size & ~7);

   pld(src, 0);

   while (start < end)
     {
	pld(src, 32);
        UNROLL8({
           _soft16_pt_blend_solid_solid_mul_color_transp
              (start, *src, rel_alpha, r, g, b);
           start++;
           src++;
        });
     }

   end = start + (size & 7);
   for (; start < end; start++, src++)
      _soft16_pt_blend_solid_solid_mul_color_transp
         (start, *src, rel_alpha, r, g, b);
}

/***********************************************************************
 * Blend operations with extra multiply color
 */

static void
_soft16_scanline_blend_transp_solid_mul_color_solid(DATA16 *src, DATA8 *alpha, DATA16 *dst, int size, DATA16 r, DATA16 g, DATA16 b)
{
   DATA16 *start, *end;

   start = dst;
   end = start + (size & ~7);

   pld(alpha, 0);
   pld(src, 0);

   while (start < end)
     {
	DATA8 alpha1, alpha2;

	alpha1 = alpha[0];
	alpha += 8;

	pld(alpha, 8);
	pld(src, 32);

	src += 8;
	start += 8;

	alpha2 = alpha[-7];
        _soft16_pt_blend_transp_solid_mul_color_solid
           (start - 8, src[-8], alpha1, r, g, b);

	alpha1 = alpha[-6];
        _soft16_pt_blend_transp_solid_mul_color_solid
           (start - 7, src[-7], alpha2, r, g, b);

	alpha2 = alpha[-5];
        _soft16_pt_blend_transp_solid_mul_color_solid
           (start - 6, src[-6], alpha1, r, g, b);

	alpha1 = alpha[-4];
        _soft16_pt_blend_transp_solid_mul_color_solid
           (start - 5, src[-5], alpha2, r, g, b);

	alpha2 = alpha[-3];
        _soft16_pt_blend_transp_solid_mul_color_solid
           (start - 4, src[-4], alpha1, r, g, b);

	alpha1 = alpha[-2];
        _soft16_pt_blend_transp_solid_mul_color_solid
           (start - 3, src[-3], alpha2, r, g, b);

	alpha2 = alpha[-1];
        _soft16_pt_blend_transp_solid_mul_color_solid
           (start - 2, src[-2], alpha1, r, g, b);

        _soft16_pt_blend_transp_solid_mul_color_solid
           (start - 1, src[-1], alpha2, r, g, b);
     }

   end = start + (size & 7);
   for (; start < end; start++, src++, alpha++)
     _soft16_pt_blend_transp_solid_mul_color_solid
        (start, *src, *alpha, r, g, b);
}

static void
_soft16_scanline_blend_solid_solid_mul_color_solid(DATA16 *src, DATA16 *dst, int size, DATA8 r, DATA8 g, DATA8 b)
{
   DATA16 *start, *end;

   start = dst;
   end = start + (size & ~7);

   pld(src, 0);

   while (start < end)
     {
	pld(src, 32);
        UNROLL8({
           _soft16_pt_blend_solid_solid_mul_color_solid(start, *src, r, g, b);
           start++;
           src++;
        });
     }

   end = start + (size & 7);
   for (; start < end; start++, src++)
     _soft16_pt_blend_solid_solid_mul_color_solid(start, *src, r, g, b);
}
