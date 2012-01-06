#include "evas_common_soft8.h"
#include "evas_soft8_scanline_fill.c"

static inline int
_is_empty_rectangle(const Eina_Rectangle * r)
{
   return (r->w < 1) || (r->h < 1);
}

static inline void
_soft8_rectangle_draw_solid_solid(Soft8_Image * dst, int offset, int w, int h,
                                  DATA8 gry8)
{
   DATA8 *dst_itr;
   int i;

   dst_itr = dst->pixels + offset;

   for (i = 0; i < h; i++, dst_itr += dst->stride)
      _soft8_scanline_fill_solid_solid(dst_itr, w, gry8);
}

static inline void
_soft8_rectangle_draw_transp_solid(Soft8_Image * dst, int offset, int w, int h,
                                   DATA8 gry8, DATA8 alpha)
{
   DATA8 *dst_itr;
   int i;

   dst_itr = dst->pixels + offset;
   alpha++;

   for (i = 0; i < h; i++, dst_itr += dst->stride)
      _soft8_scanline_fill_transp_solid(dst_itr, w, gry8, alpha);
}

static void
_soft8_rectangle_draw_int(Soft8_Image * dst, RGBA_Draw_Context * dc,
                          Eina_Rectangle dr)
{
   int dst_offset;

   if (_is_empty_rectangle(&dr))
      return;
   RECTS_CLIP_TO_RECT(dr.x, dr.y, dr.w, dr.h, 0, 0, dst->cache_entry.w,
                      dst->cache_entry.h);
   if (_is_empty_rectangle(&dr))
      return;

   if (dc->clip.use)
      RECTS_CLIP_TO_RECT(dr.x, dr.y, dr.w, dr.h, dc->clip.x,
                         dc->clip.y, dc->clip.w, dc->clip.h);
   if (_is_empty_rectangle(&dr))
      return;
   if (A_VAL(&dc->col.col) == 0)
      return;

   dst_offset = dr.x + (dr.y * dst->stride);

   if (!dst->cache_entry.flags.alpha)
     {
        DATA8 gry8;
        DATA8 alpha;

        alpha = A_VAL(&dc->col.col);
        gry8 = GRY_8_FROM_RGB(&dc->col.col);

        if (alpha == 0xff)
           _soft8_rectangle_draw_solid_solid(dst, dst_offset, dr.w, dr.h, gry8);
        else if (alpha > 0)
           _soft8_rectangle_draw_transp_solid
               (dst, dst_offset, dr.w, dr.h, gry8, alpha);
     }
   else
      ERR("Unsupported feature: drawing rectangle to non-opaque destination.");
}

void
evas_common_soft8_rectangle_draw(Soft8_Image * dst, RGBA_Draw_Context * dc,
                     int x, int y, int w, int h)
{
   Eina_Rectangle dr;
   Cutout_Rects *rects;
   Cutout_Rect *r;
   struct RGBA_Draw_Context_clip c_bkp;
   int i;

   /* handle cutouts here! */
   EINA_RECTANGLE_SET(&dr, x, y, w, h);

   if (_is_empty_rectangle(&dr))
      return;
   if (!
       (RECTS_INTERSECT
        (dr.x, dr.y, dr.w, dr.h, 0, 0, dst->cache_entry.w, dst->cache_entry.h)))
      return;

   /* no cutouts - cut right to the chase */
   if (!dc->cutout.rects)
     {
        _soft8_rectangle_draw_int(dst, dc, dr);
        return;
     }

   c_bkp = dc->clip;

   evas_common_draw_context_clip_clip(dc, 0, 0, dst->cache_entry.w,
                                      dst->cache_entry.h);
   evas_common_draw_context_clip_clip(dc, x, y, w, h);
   /* our clip is 0 size.. abort */
   if ((dc->clip.w <= 0) || (dc->clip.h <= 0))
     {
        dc->clip = c_bkp;
        return;
     }
   rects = evas_common_draw_context_apply_cutouts(dc);
   for (i = 0; i < rects->active; ++i)
     {
        r = rects->rects + i;
        evas_common_draw_context_set_clip(dc, r->x, r->y, r->w, r->h);
        _soft8_rectangle_draw_int(dst, dc, dr);
     }
   evas_common_draw_context_apply_clear_cutouts(rects);
   dc->clip = c_bkp;
}
