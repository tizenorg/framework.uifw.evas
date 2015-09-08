#include "evas_gl_private.h"

void
evas_gl_common_rect_draw(Evas_Engine_GL_Context *gc, int x, int y, int w, int h)
{
   static Cutout_Rects *rects = NULL;
   Cutout_Rect  *r;
   int          c, cx, cy, cw, ch, cr, cg, cb, ca, i;
   double mx = 0, my = 0, mw = 0, mh = 0;
   Eina_Bool mask_smooth = EINA_FALSE;
   Evas_GL_Image *mask = gc->dc->clip.mask;
   Evas_GL_Texture *mtex = mask ? mask->tex : NULL;

   if ((w <= 0) || (h <= 0)) return;
   if (!(RECTS_INTERSECT(x, y, w, h, 0, 0, gc->w, gc->h))) return;
   /* save out clip info */
   c = gc->dc->clip.use; cx = gc->dc->clip.x; cy = gc->dc->clip.y; cw = gc->dc->clip.w; ch = gc->dc->clip.h;

   ca = (gc->dc->col.col >> 24) & 0xff;
   if ((gc->dc->render_op != EVAS_RENDER_COPY) && (ca <= 0)) return;
   cr = (gc->dc->col.col >> 16) & 0xff;
   cg = (gc->dc->col.col >> 8 ) & 0xff;
   cb = (gc->dc->col.col      ) & 0xff;
   evas_common_draw_context_clip_clip(gc->dc, 0, 0, gc->shared->w, gc->shared->h);
   /* no cutouts - cut right to the chase */

   if (gc->dc->clip.use)
     {
        RECTS_CLIP_TO_RECT(x, y, w, h,
                           gc->dc->clip.x, gc->dc->clip.y,
                           gc->dc->clip.w, gc->dc->clip.h);
     }

   if (!gc->dc->cutout.rects)
     {
        if (mtex)
          {
             const double mask_x = gc->dc->clip.mask_x;
             const double mask_y = gc->dc->clip.mask_y;
             const double tmw = mtex->pt->w;
             const double tmh = mtex->pt->h;
             double scalex = 1.0;
             double scaley = 1.0;

             // canvas coords
             mx = mask_x; my = mask_y;
             if (mask->scaled.origin && mask->scaled.w && mask->scaled.h)
               {
                  mw = mask->scaled.w;
                  mh = mask->scaled.h;
                  scalex = mask->w / (double)mask->scaled.w;
                  scaley = mask->h / (double)mask->scaled.h;
                  mask_smooth = mask->scaled.smooth;
               }
             else
               {
                  mw = mask->w;
                  mh = mask->h;
               }
             RECTS_CLIP_TO_RECT(mx, my, mw, mh, x, y, w, h);
             mx -= gc->dc->clip.mask_x;
             my -= gc->dc->clip.mask_y;

             // convert to tex coords
             mx = (mtex->x / tmw) + ((mx - mask_x) * scalex / tmw);
             my = (mtex->y / tmh) + ((my - mask_y) * scaley / tmh);
             mw = mw * scalex / tmw;
             mh = mh * scaley / tmh;
          }

        evas_gl_common_context_rectangle_push(gc, x, y, w, h, cr, cg, cb, ca, mtex, mx, my, mw, mh, mask_smooth);
     }
   else
     {
        evas_common_draw_context_clip_clip(gc->dc, x, y, w, h);
        /* our clip is 0 size.. abort */
        if ((gc->dc->clip.w > 0) && (gc->dc->clip.h > 0))
          {
             rects = evas_common_draw_context_apply_cutouts(gc->dc, rects);
             for (i = 0; i < rects->active; ++i)
               {
                  r = rects->rects + i;
                  if ((r->w > 0) && (r->h > 0))
                    {
                       if (mtex)
                         {
                            const double mask_x = gc->dc->clip.mask_x;
                            const double mask_y = gc->dc->clip.mask_y;
                            const double tmw = mtex->pt->w;
                            const double tmh = mtex->pt->h;
                            double scalex = 1.0;
                            double scaley = 1.0;

                            // canvas coords
                            mx = mask_x; my = mask_y;
                            if (mask->scaled.origin && mask->scaled.w && mask->scaled.h)
                              {
                                 mw = mask->scaled.w;
                                 mh = mask->scaled.h;
                                 scalex = mask->w / (double)mask->scaled.w;
                                 scaley = mask->h / (double)mask->scaled.h;
                                 mask_smooth = mask->scaled.smooth;
                              }
                            else
                              {
                                 mw = mask->w;
                                 mh = mask->h;
                              }
                            RECTS_CLIP_TO_RECT(mx, my, mw, mh, cx, cy, cw, ch);
                            RECTS_CLIP_TO_RECT(mx, my, mw, mh, r->x, r->y, r->w, r->h);
                            mx -= gc->dc->clip.mask_x;
                            my -= gc->dc->clip.mask_y;

                            // convert to tex coords
                            mx = (mtex->x / tmw) + ((mx - mask_x) * scalex / tmw);
                            my = (mtex->y / tmh) + ((my - mask_y) * scaley / tmh);
                            mw = mw * scalex / tmw;
                            mh = mh * scaley / tmh;
                         }

                       evas_gl_common_context_rectangle_push(gc, r->x, r->y, r->w, r->h, cr, cg, cb, ca, mtex, mx, my, mw, mh, mask_smooth);
                    }
               }
          }
     }
   /* restore clip info */
   gc->dc->clip.use = c; gc->dc->clip.x = cx; gc->dc->clip.y = cy; gc->dc->clip.w = cw; gc->dc->clip.h = ch;
}
