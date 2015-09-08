#include "evas_common.h"
#include "evas_blend_private.h"

static void scale_rgba_in_to_out_clip_sample_internal(RGBA_Image *src, RGBA_Image *dst, RGBA_Draw_Context *dc, int src_region_x, int src_region_y, int src_region_w, int src_region_h, int dst_region_x, int dst_region_y, int dst_region_w, int dst_region_h);

#ifndef BUILD_SCALE_SMOOTH
#ifdef BUILD_SCALE_SAMPLE
EAPI void
evas_common_scale_rgba_in_to_out_clip_smooth_do(const Cutout_Rects *reuse,
                                                const Eina_Rectangle *clip,
                                                RGBA_Image *src, RGBA_Image *dst,
                                                RGBA_Draw_Context *dc,
                                                int src_region_x, int src_region_y,
                                                int src_region_w, int src_region_h,
                                                int dst_region_x, int dst_region_y,
                                                int dst_region_w, int dst_region_h)
{
   evas_common_scale_rgba_in_to_out_clip_sample_do(reuse, clip, src, dst, dc,
                                                   src_region_x, src_region_y,
                                                   src_region_w, src_region_h,
                                                   dst_region_x, dst_region_y,
                                                   dst_region_w, dst_region_h);
}

EAPI void
evas_common_scale_rgba_in_to_out_clip_smooth(RGBA_Image *src, RGBA_Image *dst,
                                             RGBA_Draw_Context *dc,
                                             int src_region_x, int src_region_y,
                                             int src_region_w, int src_region_h,
                                             int dst_region_x, int dst_region_y,
                                             int dst_region_w, int dst_region_h)
{
   evas_common_scale_rgba_in_to_out_clip_sample(src, dst, dc,
                                                src_region_x, src_region_y,
                                                src_region_w, src_region_h,
                                                dst_region_x, dst_region_y,
                                                dst_region_w, dst_region_h);
}
#endif
#endif

#ifdef BUILD_SCALE_SAMPLE
EAPI void
evas_common_scale_rgba_in_to_out_clip_sample(RGBA_Image *src, RGBA_Image *dst,
                                             RGBA_Draw_Context *dc,
                                             int src_region_x, int src_region_y,
                                             int src_region_w, int src_region_h,
                                             int dst_region_x, int dst_region_y,
                                             int dst_region_w, int dst_region_h)
{
   static Cutout_Rects *rects = NULL;
   Cutout_Rect  *r;
   int          c, cx, cy, cw, ch;
   int          i;
   /* handle cutouts here! */

   if ((dst_region_w <= 0) || (dst_region_h <= 0)) return;
   if (!(RECTS_INTERSECT(dst_region_x, dst_region_y, dst_region_w, dst_region_h, 0, 0, dst->cache_entry.w, dst->cache_entry.h)))
     return;
   /* no cutouts - cut right to the chase */
   if (!dc->cutout.rects)
     {
        scale_rgba_in_to_out_clip_sample_internal(src, dst, dc,
                                                  src_region_x, src_region_y,
                                                  src_region_w, src_region_h,
                                                  dst_region_x, dst_region_y,
                                                  dst_region_w, dst_region_h);
        return;
     }
   /* save out clip info */
   c = dc->clip.use; cx = dc->clip.x; cy = dc->clip.y; cw = dc->clip.w; ch = dc->clip.h;
   evas_common_draw_context_clip_clip(dc, 0, 0, dst->cache_entry.w, dst->cache_entry.h);
   evas_common_draw_context_clip_clip(dc, dst_region_x, dst_region_y, dst_region_w, dst_region_h);
   /* our clip is 0 size.. abort */
   if ((dc->clip.w <= 0) || (dc->clip.h <= 0))
     {
        dc->clip.use = c; dc->clip.x = cx; dc->clip.y = cy; dc->clip.w = cw; dc->clip.h = ch;
        return;
     }
   rects = evas_common_draw_context_apply_cutouts(dc, rects);
   for (i = 0; i < rects->active; ++i)
     {
        r = rects->rects + i;
        evas_common_draw_context_set_clip(dc, r->x, r->y, r->w, r->h);
        scale_rgba_in_to_out_clip_sample_internal(src, dst, dc,
                                                  src_region_x, src_region_y,
                                                  src_region_w, src_region_h,
                                                  dst_region_x, dst_region_y,
                                                  dst_region_w, dst_region_h);

     }
   /* restore clip info */
   dc->clip.use = c; dc->clip.x = cx; dc->clip.y = cy; dc->clip.w = cw; dc->clip.h = ch;
}

EAPI void
evas_common_scale_rgba_in_to_out_clip_sample_do(const Cutout_Rects *reuse,
                                                const Eina_Rectangle *clip,
                                                RGBA_Image *src, RGBA_Image *dst,
                                                RGBA_Draw_Context *dc,
                                                int src_region_x, int src_region_y,
                                                int src_region_w, int src_region_h,
                                                int dst_region_x, int dst_region_y,
                                                int dst_region_w, int dst_region_h)
{
   Eina_Rectangle area;
   Cutout_Rect *r;
   int i;

   if (!reuse)
     {
        evas_common_draw_context_clip_clip(dc, clip->x, clip->y, clip->w, clip->h);
        scale_rgba_in_to_out_clip_sample_internal(src, dst, dc,
                                                  src_region_x, src_region_y,
                                                  src_region_w, src_region_h,
                                                  dst_region_x, dst_region_y,
                                                  dst_region_w, dst_region_h);
        return;
     }

   for (i = 0; i < reuse->active; ++i)
     {
        r = reuse->rects + i;

        EINA_RECTANGLE_SET(&area, r->x, r->y, r->w, r->h);
        if (!eina_rectangle_intersection(&area, clip)) continue ;
        evas_common_draw_context_set_clip(dc, area.x, area.y, area.w, area.h);
        scale_rgba_in_to_out_clip_sample_internal(src, dst, dc,
                                                  src_region_x, src_region_y,
                                                  src_region_w, src_region_h,
                                                  dst_region_x, dst_region_y,
                                                  dst_region_w, dst_region_h);
     }
}

static void
scale_rgba_in_to_out_clip_sample_internal(RGBA_Image *src, RGBA_Image *dst,
                                          RGBA_Draw_Context *dc,
                                          int src_region_x, int src_region_y,
                                          int src_region_w, int src_region_h,
                                          int dst_region_x, int dst_region_y,
                                          int dst_region_w, int dst_region_h)
{
   int      x, y;
   int     *lin_ptr;
   int      offset;
   DATA32  *buf, *dptr;
   DATA32 **row_ptr;
   DATA32  *ptr, *dst_ptr, *src_data, *dst_data;
   int      dst_clip_x, dst_clip_y, dst_clip_w, dst_clip_h;
   int      m_clip_x = 0, m_clip_y = 0, m_clip_w = 0, m_clip_h = 0, mdx = 0, mdy = 0;
   int      src_w, src_h, dst_w, dst_h;
   RGBA_Gfx_Func func, func2 = NULL;
   RGBA_Image *maskobj = NULL;
   DATA8   *mask = NULL;

   if (!(RECTS_INTERSECT(dst_region_x, dst_region_y, dst_region_w, dst_region_h, 0, 0, dst->cache_entry.w, dst->cache_entry.h)))
     return;
   if (!(RECTS_INTERSECT(src_region_x, src_region_y, src_region_w, src_region_h, 0, 0, src->cache_entry.w, src->cache_entry.h)))
     return;

   src_w = src->cache_entry.w;
   src_h = src->cache_entry.h;
   dst_w = dst->cache_entry.w;
   dst_h = dst->cache_entry.h;

   src_data = src->image.data;
   dst_data = dst->image.data;

   if (dc->clip.use)
     {
        dst_clip_x = dc->clip.x;
        dst_clip_y = dc->clip.y;
        dst_clip_w = dc->clip.w;
        dst_clip_h = dc->clip.h;
        if (dst_clip_x < 0)
          {
             dst_clip_w += dst_clip_x;
             dst_clip_x = 0;
          }
        if (dst_clip_y < 0)
          {
             dst_clip_h += dst_clip_y;
             dst_clip_y = 0;
          }
        if ((dst_clip_x + dst_clip_w) > dst_w)
          dst_clip_w = dst_w - dst_clip_x;
        if ((dst_clip_y + dst_clip_h) > dst_h)
          dst_clip_h = dst_h - dst_clip_y;
     }
   else
     {
        dst_clip_x = 0;
        dst_clip_y = 0;
        dst_clip_w = dst_w;
        dst_clip_h = dst_h;
     }

   if (dst_clip_x < dst_region_x)
     {
        dst_clip_w += dst_clip_x - dst_region_x;
        dst_clip_x = dst_region_x;
     }
   if ((dst_clip_x + dst_clip_w) > (dst_region_x + dst_region_w))
     dst_clip_w = dst_region_x + dst_region_w - dst_clip_x;
   if (dst_clip_y < dst_region_y)
     {
        dst_clip_h += dst_clip_y - dst_region_y;
        dst_clip_y = dst_region_y;
     }
   if ((dst_clip_y + dst_clip_h) > (dst_region_y + dst_region_h))
     dst_clip_h = dst_region_y + dst_region_h - dst_clip_y;

   if ((src_region_w <= 0) || (src_region_h <= 0) ||
       (dst_region_w <= 0) || (dst_region_h <= 0) ||
       (dst_clip_w <= 0) || (dst_clip_h <= 0))
     return;

   /* sanitise x */
   if (src_region_x < 0)
     {
        dst_region_x -= (src_region_x * dst_region_w) / src_region_w;
        dst_region_w += (src_region_x * dst_region_w) / src_region_w;
        src_region_w += src_region_x;
        src_region_x = 0;
     }
   if (src_region_x >= src_w) return;
   if ((src_region_x + src_region_w) > src_w)
     {
        dst_region_w = (dst_region_w * (src_w - src_region_x)) / (src_region_w);
        src_region_w = src_w - src_region_x;
     }
   if (dst_region_w <= 0) return;
   if (src_region_w <= 0) return;
   if (dst_clip_x < 0)
     {
        dst_clip_w += dst_clip_x;
        dst_clip_x = 0;
     }
   if (dst_clip_w <= 0) return;
   if (dst_clip_x >= dst_w) return;
   if (dst_clip_x < dst_region_x)
     {
        dst_clip_w += (dst_clip_x - dst_region_x);
        dst_clip_x = dst_region_x;
     }
   if ((dst_clip_x + dst_clip_w) > dst_w)
     {
        dst_clip_w = dst_w - dst_clip_x;
     }
   if (dst_clip_w <= 0) return;

   /* sanitise y */
   if (src_region_y < 0)
     {
        dst_region_y -= (src_region_y * dst_region_h) / src_region_h;
        dst_region_h += (src_region_y * dst_region_h) / src_region_h;
        src_region_h += src_region_y;
        src_region_y = 0;
     }
   if (src_region_y >= src_h) return;
   if ((src_region_y + src_region_h) > src_h)
     {
        dst_region_h = (dst_region_h * (src_h - src_region_y)) / (src_region_h);
        src_region_h = src_h - src_region_y;
     }
   if (dst_region_h <= 0) return;
   if (src_region_h <= 0) return;
   if (dst_clip_y < 0)
     {
        dst_clip_h += dst_clip_y;
        dst_clip_y = 0;
     }
   if (dst_clip_h <= 0) return;
   if (dst_clip_y >= dst_h) return;
   if (dst_clip_y < dst_region_y)
     {
        dst_clip_h += (dst_clip_y - dst_region_y);
        dst_clip_y = dst_region_y;
     }
   if ((dst_clip_y + dst_clip_h) > dst_h)
     {
        dst_clip_h = dst_h - dst_clip_y;
     }
   if (dst_clip_h <= 0) return;

   /* figure out dst jump */
   //dst_jump = dst_w - dst_clip_w;

   /* figure out dest start ptr */
   dst_ptr = dst_data + dst_clip_x + (dst_clip_y * dst_w);

   if (!dc->clip.mask)
     {
        if (dc->mul.use)
          func = evas_common_gfx_func_composite_pixel_color_span_get(src, dc->mul.col, dst, dst_clip_w, dc->render_op);
        else
          func = evas_common_gfx_func_composite_pixel_span_get(src, dst, dst_clip_w, dc->render_op);
     }
   else
     {
        func = evas_common_gfx_func_composite_pixel_mask_span_get(src, dst, dst_clip_w, dc->render_op);
        if (dc->mul.use)
          func2 = evas_common_gfx_func_composite_pixel_color_span_get(src, dc->mul.col, dst, dst_clip_w, EVAS_RENDER_COPY);
     }

   if ((dst_region_w == src_region_w) && (dst_region_h == src_region_h))
     {
#ifdef HAVE_PIXMAN
# ifdef PIXMAN_IMAGE_SCALE_SAMPLE        
        if ((src->pixman.im) && (dst->pixman.im) && (!dc->clip.mask) &&
            ((!dc->mul.use) ||
             ((dc->mul.use) && (dc->mul.col == 0xffffffff))) &&
            ((dc->render_op == _EVAS_RENDER_COPY) ||
             (dc->render_op == _EVAS_RENDER_BLEND)))
          {
             pixman_op_t op = PIXMAN_OP_OVER; // _EVAS_RENDER_BLEND
             if ((dc->render_op == _EVAS_RENDER_COPY) || (!src->cache_entry.flags.alpha))
               op = PIXMAN_OP_SRC;

             pixman_image_composite(op,
                                    src->pixman.im, NULL,
                                    dst->pixman.im,
                                    (dst_clip_x - dst_region_x) + src_region_x,
                                    (dst_clip_y - dst_region_y) + src_region_y,
                                    0, 0,
                                    dst_clip_x, dst_clip_y,
                                    dst_clip_w, dst_clip_h);
          }
        // NOTE: Removed old masking code with pixman
        else
# endif
#endif
          {
             ptr = src_data + ((dst_clip_y - dst_region_y + src_region_y) * src_w) + (dst_clip_x - dst_region_x) + src_region_x;

             /* image masking */
             if (dc->clip.mask)
               {
                  RGBA_Image *im = dc->clip.mask;
                  DATA8 *mbegin = im->image.data8;
                  DATA8 *mend = mbegin + (im->cache_entry.w * im->cache_entry.h);

                  if (dc->mul.use)
                    buf = alloca(dst_clip_w * sizeof(DATA32));

                  for (y = 0; y < dst_clip_h; y++)
                    {
                       mask = im->image.data8
                          + ((dst_clip_y - dc->clip.mask_y + y) * im->cache_entry.w)
                          + (dst_clip_x - dc->clip.mask_x);

                       /* FIXME!!! Quick workaround crashes */
                       if ((mask < mbegin) || ((mask + dst_clip_w) > mend))
                         {
                            ptr += src_w;
                            dst_ptr += dst_w;
                            continue;
                         }

                       /* * blend here [clip_w *] ptr -> dst_ptr * */
                       if (dc->mul.use)
                         {
                            func2(ptr, NULL, dc->mul.col, buf, dst_clip_w);
                            func(buf, mask, 0, dst_ptr, dst_clip_w);
                         }
                       else
                         func(ptr, mask, 0, dst_ptr, dst_clip_w);

                       ptr += src_w;
                       dst_ptr += dst_w;
                    }
               }
             else
               {
                  for (y = 0; y < dst_clip_h; y++)
                    {
                       /* * blend here [clip_w *] ptr -> dst_ptr * */
                       func(ptr, NULL, dc->mul.col, dst_ptr, dst_clip_w);

                       ptr += src_w;
                       dst_ptr += dst_w;
                    }
               }
          }
     }
   else
     {
        /* allocate scale lookup tables */
        lin_ptr = alloca(dst_clip_w * sizeof(int));
        row_ptr = alloca(dst_clip_h * sizeof(DATA32 *));

        /* fill scale tables */
        for (x = 0; x < dst_clip_w; x++)
          lin_ptr[x] = (((x + dst_clip_x - dst_region_x) * src_region_w) / dst_region_w) + src_region_x;
        for (y = 0; y < dst_clip_h; y++)
          row_ptr[y] = src_data + (((((y + dst_clip_y - dst_region_y) * src_region_h) / dst_region_h)
                                    + src_region_y) * src_w);

        /* scale to dst */
        dptr = dst_ptr;
        offset = dst_clip_y - dst_region_y;

#ifdef DIRECT_SCALE
        if ((!src->cache_entry.flags.alpha) &&
            (!dst->cache_entry.flags.alpha) &&
            (!dc->mul.use) &&
            (!dc->clip.mask))
          {
             for (y = 0; y < dst_clip_h; y++)
               {
                  dst_ptr = dptr;
                  for (x = 0; x < dst_clip_w; x++)
                    {
                       ptr = row_ptr[y] + lin_ptr[x];
                       *dst_ptr = *ptr;
                       dst_ptr++;
                    }

                  dptr += dst_w;
               }
          }
        else
#endif
          {
             /* a scanline buffer */
             buf = alloca(dst_clip_w * sizeof(DATA32));

             /* image masking */
             if (dc->clip.mask)
               {
                  RGBA_Image *im = dc->clip.mask;
                  DATA8 *mbegin = im->image.data8;
                  DATA8 *mend = mbegin + (im->cache_entry.w * im->cache_entry.h);

                  for (y = 0; y < dst_clip_h; y++)
                    {
                       dst_ptr = buf;
                       mask = im->image.data8
                          + ((dst_clip_y - dc->clip.mask_y + y) * im->cache_entry.w)
                          + (dst_clip_x - dc->clip.mask_x);

                       /* FIXME!!! Quick workaround crashes */
                       if ((mask < mbegin) || ((mask + dst_clip_w) > mend))
                         {
                            dptr += dst_w;
                            continue;
                         }

                       for (x = 0; x < dst_clip_w; x++)
                         {
                            ptr = row_ptr[y] + lin_ptr[x];
                            *dst_ptr = *ptr;
                            dst_ptr++;
                         }

                       /* * blend here [clip_w *] buf -> dptr * */
                       if (dc->mul.use) func2(buf, NULL, dc->mul.col, buf, dst_clip_w);
                       func(buf, mask, 0, dptr, dst_clip_w);

                       dptr += dst_w;
                    }
               }
             else
               {
                  for (y = 0; y < dst_clip_h; y++)
                    {
                       dst_ptr = buf;
                       for (x = 0; x < dst_clip_w; x++)
                         {
                            ptr = row_ptr[y] + lin_ptr[x];
                            *dst_ptr = *ptr;
                            dst_ptr++;
                         }

                       /* * blend here [clip_w *] buf -> dptr * */
                       func(buf, NULL, dc->mul.col, dptr, dst_clip_w);

                       dptr += dst_w;
                    }
               }
          }
     }
}
#else
#ifdef BUILD_SCALE_SMOOTH
EAPI void
evas_common_scale_rgba_in_to_out_clip_sample(RGBA_Image *src, RGBA_Image *dst,
                                             RGBA_Draw_Context *dc,
                                             int src_region_x, int src_region_y,
                                             int src_region_w, int src_region_h,
                                             int dst_region_x, int dst_region_y,
                                             int dst_region_w, int dst_region_h)
{
   evas_common_scale_rgba_in_to_out_clip_smooth(src, dst, dc,
                                                src_region_x, src_region_y,
                                                src_region_w, src_region_h,
                                                dst_region_x, dst_region_y,
                                                dst_region_w, dst_region_h);
}

EAPI void
evas_common_scale_rgba_in_to_out_clip_sample_do(const Cutout_Rects *reuse,
                                                const Eina_Rectangle *clip,
                                                RGBA_Image *src, RGBA_Image *dst,
                                                RGBA_Draw_Context *dc,
                                                int src_region_x, int src_region_y,
                                                int src_region_w, int src_region_h,
                                                int dst_region_x, int dst_region_y,
                                                int dst_region_w, int dst_region_h)
{
   evas_common_scale_rgba_in_to_out_clip_smooth_do(reuse, clip, src, dst, dc,
                                                   src_region_x, src_region_y,
                                                   src_region_w, src_region_h,
                                                   dst_region_x, dst_region_y,
                                                   dst_region_w, dst_region_h);
}
#endif
#endif
