#include "evas_common_soft8.h"
#include "evas_soft8_scanline_blend.c"

static void
_soft8_image_draw_scaled_solid_solid(Soft8_Image * src,
                                     Soft8_Image * dst,
                                     RGBA_Draw_Context * dc __UNUSED__,
                                     int dst_offset, int w, int h,
                                     int *offset_x, int *offset_y)
{
   DATA8 *dst_itr;
   int y, w_align;

   w_align = w & ~7;

   dst_itr = dst->pixels + dst_offset;
   for (y = 0; y < h; y++, dst_itr += dst->stride)
     {
        DATA8 *d, *s;
        int x;

        s = src->pixels + offset_y[y];
        pld(s, 0);
        pld(offset_x, 0);

        d = dst_itr;
        x = 0;
        while (x < w_align)
          {
             pld(s, 32);
             pld(offset_x + x, 32);

             UNROLL8(
                       {
                       _soft8_pt_blend_solid_solid(d, s[offset_x[x]]); x++; d++;}
             );
          }

        for (; x < w; x++, d++)
           _soft8_pt_blend_solid_solid(d, s[offset_x[x]]);
     }
}
static void
_soft8_image_draw_scaled_transp_solid(Soft8_Image * src,
                                      Soft8_Image * dst,
                                      RGBA_Draw_Context * dc __UNUSED__,
                                      int dst_offset, int w, int h,
                                      int *offset_x, int *offset_y)
{
   DATA8 *dst_itr;
   int y, w_align;

   w_align = w & ~7;

   dst_itr = dst->pixels + dst_offset;
   for (y = 0; y < h; y++, dst_itr += dst->stride)
     {
        DATA8 *d, *s;
        DATA8 *a;
        int x;

        s = src->pixels + offset_y[y];
        a = src->alpha + offset_y[y];
        pld(s, 0);
        pld(a, 0);
        pld(offset_x, 0);

        d = dst_itr;
        x = 0;
        while (x < w_align)
          {
             pld(s, 32);
             pld(a, 8);
             pld(offset_x + x, 32);

             UNROLL8(
                       {
                       int off_x = offset_x[x];
                       _soft8_pt_blend_transp_solid(d, s[off_x], a[off_x]);
                       x++; d++;});
          }

        for (; x < w; x++, d++)
           _soft8_pt_blend_transp_solid(d, s[offset_x[x]], a[offset_x[x]]);
     }
}

static inline void
_soft8_image_draw_scaled_no_mul(Soft8_Image * src, Soft8_Image * dst,
                                RGBA_Draw_Context * dc,
                                int dst_offset, int w, int h,
                                int *offset_x, int *offset_y)
{
   if ((src->cache_entry.flags.alpha && src->alpha) &&
       (!dst->cache_entry.flags.alpha))
      _soft8_image_draw_scaled_transp_solid
          (src, dst, dc, dst_offset, w, h, offset_x, offset_y);
   else if (!dst->cache_entry.flags.alpha)
      _soft8_image_draw_scaled_solid_solid
          (src, dst, dc, dst_offset, w, h, offset_x, offset_y);
   else
      ERR("Unsupported draw of scaled images src->cache_entry.flags.alpha=%d, "
          "dst->cache_entry.flags.alpha=%d, WITHOUT COLOR MUL",
          src->cache_entry.flags.alpha, dst->cache_entry.flags.alpha);
}

static void
_soft8_image_draw_scaled_solid_solid_mul_alpha(Soft8_Image * src,
                                               Soft8_Image * dst,
                                               RGBA_Draw_Context *
                                               dc __UNUSED__, int dst_offset,
                                               int w, int h, int *offset_x,
                                               int *offset_y, DATA8 alpha)
{
   DATA8 *dst_itr;
   int y, w_align;

   w_align = w & ~7;

   dst_itr = dst->pixels + dst_offset;
   for (y = 0; y < h; y++, dst_itr += dst->stride)
     {
        DATA8 *d, *s;
        int x;

        s = src->pixels + offset_y[y];
        pld(s, 0);
        pld(offset_x, 0);

        d = dst_itr;
        x = 0;
        while (x < w_align)
          {
             pld(s, 32);
             pld(offset_x + x, 32);

             UNROLL8(
                       {
                       _soft8_pt_blend_solid_solid_mul_alpha
                       (d, s[offset_x[x]], alpha); x++; d++;}
             );
          }

        for (; x < w; x++, d++)
           _soft8_pt_blend_solid_solid_mul_alpha(d, s[offset_x[x]], alpha);
     }
}

static void
_soft8_image_draw_scaled_transp_solid_mul_alpha(Soft8_Image * src,
                                                Soft8_Image * dst,
                                                RGBA_Draw_Context *
                                                dc __UNUSED__, int dst_offset,
                                                int w, int h, int *offset_x,
                                                int *offset_y, DATA8 alpha)
{
   DATA8 *dst_itr;
   int y, w_align;

   w_align = w & ~7;

   dst_itr = dst->pixels + dst_offset;
   for (y = 0; y < h; y++, dst_itr += dst->stride)
     {
        DATA8 *d, *s;
        DATA8 *a;
        int x;

        s = src->pixels + offset_y[y];
        a = src->alpha + offset_y[y];
        pld(s, 0);
        pld(a, 0);
        pld(offset_x, 0);

        d = dst_itr;
        x = 0;
        while (x < w_align)
          {
             pld(s, 32);
             pld(a, 8);
             pld(offset_x + x, 32);

             UNROLL8(
                       {
                       int off_x = offset_x[x];
                       _soft8_pt_blend_transp_solid_mul_alpha
                       (d, s[off_x], a[off_x], alpha); x++; d++;});
          }

        for (; x < w; x++, d++)
           _soft8_pt_blend_transp_solid_mul_alpha
               (d, s[offset_x[x]], a[offset_x[x]], alpha);
     }
}

static inline void
_soft8_image_draw_scaled_mul_alpha(Soft8_Image * src, Soft8_Image * dst,
                                   RGBA_Draw_Context * dc,
                                   int dst_offset, int w, int h,
                                   int *offset_x, int *offset_y, DATA8 a)
{
   if ((src->cache_entry.flags.alpha && src->alpha) &&
       (!dst->cache_entry.flags.alpha))
      _soft8_image_draw_scaled_transp_solid_mul_alpha
          (src, dst, dc, dst_offset, w, h, offset_x, offset_y, a);
   else if (!dst->cache_entry.flags.alpha)
      _soft8_image_draw_scaled_solid_solid_mul_alpha
          (src, dst, dc, dst_offset, w, h, offset_x, offset_y, a);
   else
      ERR("Unsupported draw of scaled images src->cache_entry.flags.alpha=%d, "
          "dst->cache_entry.flags.alpha=%d, WITH ALPHA MUL %d",
          src->cache_entry.flags.alpha, dst->cache_entry.flags.alpha,
          A_VAL(&dc->mul.col));
}

static void
_soft8_image_draw_scaled_solid_solid_mul_color(Soft8_Image * src,
                                               Soft8_Image * dst,
                                               RGBA_Draw_Context *
                                               dc __UNUSED__, int dst_offset,
                                               int w, int h, int *offset_x,
                                               int *offset_y, DATA8 r, DATA8 g,
                                               DATA8 b, DATA8 alpha)
{
   DATA8 *dst_itr;
   int y, w_align;

   w_align = w & ~7;

   dst_itr = dst->pixels + dst_offset;

   if (alpha == 0xff)
      for (y = 0; y < h; y++, dst_itr += dst->stride)
        {
           DATA8 *d, *s;
           int x;

           s = src->pixels + offset_y[y];
           pld(s, 0);
           pld(offset_x, 0);

           d = dst_itr;
           x = 0;
           while (x < w_align)
             {
                pld(s, 32);
                pld(offset_x + x, 32);

                UNROLL8(
                          {
                          _soft8_pt_blend_solid_solid_mul_color_solid
                          (d, s[offset_x[x]], r, g, b); x++; d++;}
                );
             }

           for (; x < w; x++, d++)
              _soft8_pt_blend_solid_solid_mul_color_solid
                  (d, s[offset_x[x]], r, g, b);
        }
   else
      for (y = 0; y < h; y++, dst_itr += dst->stride)
        {
           DATA8 *d, *s;
           int x;

           s = src->pixels + offset_y[y];
           pld(s, 0);
           pld(offset_x, 0);

           d = dst_itr;
           x = 0;
           while (x < w_align)
             {
                pld(s, 32);
                pld(offset_x + x, 32);

                UNROLL8(
                          {
                          _soft8_pt_blend_solid_solid_mul_color_transp
                          (d, s[offset_x[x]], alpha, r, g, b); x++; d++;}
                );
             }

           for (; x < w; x++, d++)
              _soft8_pt_blend_solid_solid_mul_color_transp
                  (d, s[offset_x[x]], alpha, r, g, b);
        }
}

static void
_soft8_image_draw_scaled_transp_solid_mul_color(Soft8_Image * src,
                                                Soft8_Image * dst,
                                                RGBA_Draw_Context *
                                                dc __UNUSED__, int dst_offset,
                                                int w, int h, int *offset_x,
                                                int *offset_y, DATA8 r, DATA8 g,
                                                DATA8 b, DATA8 alpha)
{
   DATA8 *dst_itr;
   int y, w_align;

   w_align = w & ~7;

   dst_itr = dst->pixels + dst_offset;

   if (alpha == 0xff)
      for (y = 0; y < h; y++, dst_itr += dst->stride)
        {
           DATA8 *d, *s;
           DATA8 *a;
           int x;

           s = src->pixels + offset_y[y];
           a = src->alpha + offset_y[y];
           pld(s, 0);
           pld(a, 0);
           pld(offset_x, 0);

           d = dst_itr;
           x = 0;
           while (x < w_align)
             {
                pld(s, 32);
                pld(a, 8);
                pld(offset_x + x, 32);

                UNROLL8(
                          {
                          int off_x = offset_x[x];
                          _soft8_pt_blend_transp_solid_mul_color_solid
                          (d, s[off_x], a[off_x], r, g, b); x++; d++;});
             }

           for (; x < w; x++, d++)
              _soft8_pt_blend_transp_solid_mul_color_solid
                  (d, s[offset_x[x]], a[offset_x[x]], r, g, b);
        }
   else
      for (y = 0; y < h; y++, dst_itr += dst->stride)
        {
           DATA8 *d, *s;
           DATA8 *a;
           int x;

           s = src->pixels + offset_y[y];
           a = src->alpha + offset_y[y];
           pld(s, 0);
           pld(a, 0);
           pld(offset_x, 0);

           d = dst_itr;
           x = 0;
           while (x < w_align)
             {
                pld(s, 32);
                pld(a, 8);
                pld(offset_x + x, 32);

                UNROLL8(
                          {
                          int off_x = offset_x[x];
                          _soft8_pt_blend_transp_solid_mul_color_transp
                          (d, s[off_x], a[off_x], alpha, r, g, b); x++; d++;});
             }

           for (; x < w; x++, d++)
              _soft8_pt_blend_transp_solid_mul_color_transp
                  (d, s[offset_x[x]], a[offset_x[x]], alpha, r, g, b);
        }
}

static inline void
_soft8_image_draw_scaled_mul_color(Soft8_Image * src, Soft8_Image * dst,
                                   RGBA_Draw_Context * dc,
                                   int dst_offset, int w, int h,
                                   int *offset_x, int *offset_y,
                                   DATA8 r, DATA8 g, DATA8 b, DATA8 a)
{
   if ((src->cache_entry.flags.alpha && src->alpha) &&
       (!dst->cache_entry.flags.alpha))
      _soft8_image_draw_scaled_transp_solid_mul_color
          (src, dst, dc, dst_offset, w, h, offset_x, offset_y, r, g, b, a);
   else if (!dst->cache_entry.flags.alpha)
      _soft8_image_draw_scaled_solid_solid_mul_color
          (src, dst, dc, dst_offset, w, h, offset_x, offset_y, r, g, b, a);
   else
      ERR("Unsupported draw of scaled images src->cache_entry.flags.alpha=%d, "
          "dst->cache_entry.flags.alpha=%d, WITH COLOR MUL 0x%08x",
          src->cache_entry.flags.alpha, dst->cache_entry.flags.alpha,
          dc->mul.col);
}

static inline void
_soft8_image_draw_scaled_mul(Soft8_Image * src, Soft8_Image * dst,
                             RGBA_Draw_Context * dc,
                             int dst_offset, int w, int h,
                             int *offset_x, int *offset_y, DATA8 r, DATA8 g,
                             DATA8 b, DATA8 a)
{
   if ((a == r) && (a == g) && (a == b))
      _soft8_image_draw_scaled_mul_alpha
          (src, dst, dc, dst_offset, w, h, offset_x, offset_y, a);
   else
      _soft8_image_draw_scaled_mul_color
          (src, dst, dc, dst_offset, w, h, offset_x, offset_y, r, g, b, a);
}

void
evas_common_soft8_image_draw_scaled_sampled(Soft8_Image * src, Soft8_Image * dst,
                                RGBA_Draw_Context * dc,
                                const Eina_Rectangle sr,
                                const Eina_Rectangle dr,
                                const Eina_Rectangle cr)
{
   int x, y, dst_offset, *offset_x, *offset_y;
   DATA8 mul_gry8;
   DATA8 r, g, b, a;

   if (!dc->mul.use)
     {
        r = g = b = a = 0xff;
        mul_gry8 = 0xff;
     }
   else
     {
        a = A_VAL(&dc->mul.col);
        if (a == 0)
           return;

        r = R_VAL(&dc->mul.col);
        g = G_VAL(&dc->mul.col);
        b = B_VAL(&dc->mul.col);

        if (r > a)
           r = a;
        if (g > a)
           g = a;
        if (b > a)
           b = a;

        mul_gry8 = GRY_8_FROM_COMPONENTS(r, g, b);
     }

   /* pre-calculated scale tables */
   offset_x = alloca(cr.w * sizeof(*offset_x));
   for (x = 0; x < cr.w; x++)
      offset_x[x] = (((x + cr.x - dr.x) * sr.w) / dr.w) + sr.x;

   offset_y = alloca(cr.h * sizeof(*offset_y));
   for (y = 0; y < cr.h; y++)
      offset_y[y] = (((((y + cr.y - dr.y) * sr.h) / dr.h) + sr.y)
                     * src->stride);

   dst_offset = cr.x + (cr.y * dst->stride);

   if (mul_gry8 == 0xff)
      _soft8_image_draw_scaled_no_mul
          (src, dst, dc, dst_offset, cr.w, cr.h, offset_x, offset_y);
   else
      _soft8_image_draw_scaled_mul
          (src, dst, dc, dst_offset, cr.w, cr.h, offset_x, offset_y, r, g, b,
           a);
}
