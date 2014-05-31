#include "evas_common.h"
#include "evas_private.h"
#include "evas_blend_private.h"

#include "language/evas_bidi_utils.h" /*defines BIDI_SUPPORT if possible */
#include "evas_font_private.h" /* for Frame-Queuing support */

#include "evas_font_ot.h"

struct _Evas_Glyph
{
   RGBA_Font_Glyph *fg;
   int x, y;
   FT_UInt idx;
};

EAPI void
evas_common_font_draw_init(void)
{
}

/*
 * BiDi handling: We receive the shaped string + other props from text_props,
 * we need to reorder it so we'll have the visual string (the way we draw)
 * and then for kerning we have to switch the order of the kerning query (as the prev
 * is on the right, and not on the left).
 */
static void
evas_common_font_draw_internal(RGBA_Image *dst, RGBA_Draw_Context *dc, int x, int y,
                               const Evas_Text_Props *text_props, RGBA_Gfx_Func func, int ext_x, int ext_y, int ext_w,
                               int ext_h, int im_w, int im_h __UNUSED__)
{
   DATA32 *im;
   Evas_Glyph *glyphs;
   unsigned int length;
   unsigned int it;

   // HAVE_EMOJI
   DATA32 *orig_color;
   orig_color = dc->col.col;
   //

   im = dst->image.data;

   if (!text_props->glyphs) return ;

   glyphs = text_props->glyphs;
   length = text_props->glyphs_length;
   for (it = 0; it < length; ++it, ++glyphs)
     {
        RGBA_Font_Glyph *fg;
        int chr_x, chr_y, w;


        // HAVE_EMOJI
        DATA32 current_color;
        RGBA_Font_Int *fi;
        //

        fg = glyphs->fg;

        // HAVE_EMOJI
        fi = text_props->font_instance;
        current_color = (DATA32)eina_hash_find(fi->emoji_hash, &(fg->index));
        if (current_color && text_props->have_emoji)
          {
             ((DATA8*)(&dc->col.col))[0] = ((DATA8*)(&current_color))[0];
             ((DATA8*)(&dc->col.col))[1] = ((DATA8*)(&current_color))[1];
             ((DATA8*)(&dc->col.col))[2] = ((DATA8*)(&current_color))[2];
          }
        //

        if ((!fg->ext_dat) && (dc->font_ext.func.gl_new))
          {
             /* extension calls */
             fg->ext_dat = dc->font_ext.func.gl_new(dc->font_ext.data, fg);
             fg->ext_dat_free = dc->font_ext.func.gl_free;
          }
        w = fg->glyph_out->bitmap.width;
        chr_x = x + glyphs->x;
        chr_y = y + glyphs->y;


        if (chr_x < (ext_x + ext_w))
          {
             if ((w > 0) && ((chr_x + w) > ext_x))
               {
                  if ((fg->ext_dat) && (dc->font_ext.func.gl_draw))
                    dc->font_ext.func.gl_draw(dc->font_ext.data, (void *)dst,
                                              dc, fg, chr_x, y - (chr_y - y));
                  else if (fg->glyph_out->rle)
                    evas_common_font_glyph_draw(fg, dc, im, im_w,
                                                chr_x, y - (chr_y - y),
                                                ext_x, ext_y, ext_w, ext_h);
               }
          }
        else
          {
             // HAVE_EMOJI(20130527) : Restore text color
             dc->col.col = orig_color;
             //
             break;
          }
        // HAVE_EMOJI(20130527) : Restore text color
        dc->col.col = orig_color;
        //
     }
}

EAPI void
evas_common_font_draw_prepare(Evas_Text_Props *text_props)
{
   RGBA_Font_Int *fi;
   RGBA_Font_Glyph *fg;
   Evas_Glyph *glyphs;
   int glyphs_length;
   int glyphs_max;
   EVAS_FONT_WALK_TEXT_INIT();

   fi = text_props->font_instance;
   if (!fi) return;

   if (!text_props->changed && text_props->generation == fi->generation && text_props->glyphs)
     return ;

   glyphs = text_props->glyphs;
   glyphs_length = 0;
   glyphs_max = text_props->glyphs_length;
   text_props->glyphs_length = 0;

   evas_common_font_int_reload(fi);

   if (fi->src->current_size != fi->size)
     {
        evas_common_font_source_reload(fi->src);
        FTLOCK();
        FT_Activate_Size(fi->ft.size);
        FTUNLOCK();
        fi->src->current_size = fi->size;
     }

   EVAS_FONT_WALK_TEXT_START()
     {
        Evas_Glyph *glyph;
        FT_UInt idx;

        if (!EVAS_FONT_WALK_IS_VISIBLE) continue;
        idx = EVAS_FONT_WALK_INDEX;

        fg = evas_common_font_int_cache_glyph_get(fi, idx);
        if (!fg) continue;
        if (!fg->glyph_out) evas_common_font_int_cache_glyph_render(fg);

	if (glyphs_length + 1 >= glyphs_max)
	  {
             Evas_Glyph *tmp;

             glyphs_max += 8;
             tmp = realloc(glyphs, glyphs_max * sizeof (Evas_Glyph));
             if (!tmp) return ;
             glyphs = tmp;
             text_props->glyphs = glyphs;
	  }

        glyph = glyphs + glyphs_length++;

        glyph->fg = fg;
        glyph->idx = idx;
        glyph->x = EVAS_FONT_WALK_PEN_X + EVAS_FONT_WALK_X_OFF + EVAS_FONT_WALK_X_BEAR;
        glyph->y = EVAS_FONT_WALK_PEN_Y + EVAS_FONT_WALK_Y_OFF + EVAS_FONT_WALK_Y_BEAR;
     }
   EVAS_FONT_WALK_TEXT_END();

   text_props->glyphs_length = glyphs_length;
   text_props->glyphs = glyphs;
   /* check if there's a request queue in fi, if so ask cserve2 to render
    * those glyphs
    */

   text_props->generation = fi->generation;
}

EAPI void
evas_common_font_draw(RGBA_Image *dst, RGBA_Draw_Context *dc, int x, int y, const Evas_Text_Props *text_props)
{
   static Cutout_Rects *rects = NULL;
   int ext_x, ext_y, ext_w, ext_h;
   int im_w, im_h;
   RGBA_Gfx_Func func;
   Cutout_Rect  *r;
   int c, cx, cy, cw, ch;
   int i;

   im_w = dst->cache_entry.w;
   im_h = dst->cache_entry.h;

   ext_x = 0; ext_y = 0; ext_w = im_w; ext_h = im_h;
   if (dc->clip.use)
     {
	ext_x = dc->clip.x;
	ext_y = dc->clip.y;
	ext_w = dc->clip.w;
	ext_h = dc->clip.h;
	if (ext_x < 0)
	  {
	     ext_w += ext_x;
	     ext_x = 0;
	  }
	if (ext_y < 0)
	  {
	     ext_h += ext_y;
	     ext_y = 0;
	  }
	if ((ext_x + ext_w) > im_w)
	  ext_w = im_w - ext_x;
	if ((ext_y + ext_h) > im_h)
	  ext_h = im_h - ext_y;
     }
   if (ext_w <= 0) return;
   if (ext_h <= 0) return;

//   evas_common_font_size_use(fn);
   func = evas_common_gfx_func_composite_mask_color_span_get(dc->col.col, dst, 1, dc->render_op);

   if (!dc->cutout.rects)
     {
        evas_common_font_draw_internal(dst, dc, x, y, text_props,
                                       func, ext_x, ext_y, ext_w, ext_h,
                                       im_w, im_h);
     }
   else
     {
        c = dc->clip.use; cx = dc->clip.x; cy = dc->clip.y; cw = dc->clip.w; ch = dc->clip.h;
        evas_common_draw_context_clip_clip(dc, 0, 0, dst->cache_entry.w, dst->cache_entry.h);
        /* our clip is 0 size.. abort */
        if ((dc->clip.w > 0) && (dc->clip.h > 0))
          {
             rects = evas_common_draw_context_apply_cutouts(dc, rects);
             for (i = 0; i < rects->active; ++i)
               {
                  r = rects->rects + i;
                  evas_common_draw_context_set_clip(dc, r->x, r->y, r->w, r->h);
                  evas_common_font_draw_internal(dst, dc, x, y, text_props,
                                                 func, r->x, r->y, r->w, r->h,
                                                 im_w, im_h);
               }
          }
        dc->clip.use = c; dc->clip.x = cx; dc->clip.y = cy; dc->clip.w = cw; dc->clip.h = ch;
     }
}

EAPI void
evas_common_font_draw_do(const Cutout_Rects *reuse, const Eina_Rectangle *clip, RGBA_Gfx_Func func,
                         RGBA_Image *dst, RGBA_Draw_Context *dc,
                         int x, int y, const Evas_Text_Props *text_props)
{
   Eina_Rectangle area;
   Cutout_Rect *r;
   int i;
   int im_w, im_h;

   im_w = dst->cache_entry.w;
   im_h = dst->cache_entry.h;

   if (!reuse)
     {
        evas_common_draw_context_clip_clip(dc,
                                           clip->x, clip->y,
                                           clip->w, clip->h);
        evas_common_font_draw_internal(dst, dc, x, y, text_props,
                                       func,
                                       dc->clip.x, dc->clip.y,
                                       dc->clip.w, dc->clip.h,
                                       im_w, im_h);
        return ;
     }

   for (i = 0; i < reuse->active; ++i)
     {
        r = reuse->rects + i;

        EINA_RECTANGLE_SET(&area, r->x, r->y, r->w - 1, r->h - 1);
        if (!eina_rectangle_intersection(&area, clip)) continue ;
        evas_common_draw_context_set_clip(dc, area.x, area.y, area.w, area.h);
        evas_common_font_draw_internal(dst, dc, x, y, text_props,
                                       func, area.x, area.y, area.w, area.h,
                                       im_w, im_h);
     }
}

EAPI Eina_Bool
evas_common_font_draw_prepare_cutout(Cutout_Rects *reuse, RGBA_Image *dst, RGBA_Draw_Context *dc, RGBA_Gfx_Func *func)
{
   int im_w, im_h;

   im_w = dst->cache_entry.w;
   im_h = dst->cache_entry.h;

   *func = evas_common_gfx_func_composite_mask_color_span_get(dc->col.col, dst, 1, dc->render_op);

   evas_common_draw_context_clip_clip(dc, 0, 0, im_w, im_h);
   if (dc->clip.w <= 0) return EINA_FALSE;
   if (dc->clip.h <= 0) return EINA_FALSE;

   if (dc->cutout.rects)
     {
        reuse = evas_common_draw_context_apply_cutouts(dc, reuse);
     }

   return EINA_TRUE;
}

