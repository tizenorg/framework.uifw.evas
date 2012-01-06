#include <assert.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <SDL/SDL.h>

#include "evas_common.h" /* Also includes international specific stuff */
#include "evas_engine.h"

int _evas_engine_soft_sdl_log_dom = -1;
/* #define DEBUG_SDL */

static Evas_Func func, pfunc;

static void*                     _sdl_output_setup	(int w, int h, int fullscreen, int noframe, int alpha, int hwsurface);

static Engine_Image_Entry       *_sdl_image_alloc       (void);
static void                      _sdl_image_delete      (Engine_Image_Entry *eim);

static int                       _sdl_image_constructor (Engine_Image_Entry*, void* data);
static void                      _sdl_image_destructor  (Engine_Image_Entry *eim);

static void                      _sdl_image_dirty_region(Engine_Image_Entry *eim, unsigned int x, unsigned int y, unsigned int w, unsigned int h);

static int                       _sdl_image_dirty       (Engine_Image_Entry *dst, const Engine_Image_Entry *src);

static int                       _sdl_image_size_set    (Engine_Image_Entry *dst, const Engine_Image_Entry *src);

static int                       _sdl_image_update_data (Engine_Image_Entry* dst, void* engine_data);

static void                      _sdl_image_load        (Engine_Image_Entry *eim, const Image_Entry* im);
static int                       _sdl_image_mem_size_get(Engine_Image_Entry *eim);

#ifdef DEBUG_SDL
static void                      _sdl_image_debug       (const char* context, Engine_Image_Entry* im);
#endif

static const Evas_Cache_Engine_Image_Func       _sdl_cache_engine_image_cb = {
  NULL /* key */,
  _sdl_image_alloc /* alloc */,
  _sdl_image_delete /* dealloc */,
  _sdl_image_constructor /* constructor */,
  _sdl_image_destructor /* destructor */,
  _sdl_image_dirty_region /* dirty_region */,
  _sdl_image_dirty /* dirty */,
  _sdl_image_size_set /* size_set */,
  _sdl_image_update_data /* update_data */,
  _sdl_image_load /* load */,
  _sdl_image_mem_size_get /* mem_size_get */,
#ifdef DEBUG_SDL  /* debug */
  _sdl_image_debug
#else
  NULL
#endif
};

#define _SDL_UPDATE_PIXELS(EIM)                                 \
  ((RGBA_Image *) EIM->cache_entry.src)->image.data = EIM->surface->pixels;

#define RMASK 0x00ff0000
#define GMASK 0x0000ff00
#define BMASK 0x000000ff
#define AMASK 0xff000000

/* SDL engine info function */
static void*
evas_engine_sdl_info		(Evas* e __UNUSED__)
{
   Evas_Engine_Info_SDL*	info;
   info = calloc(1, sizeof (Evas_Engine_Info_SDL));
   if (!info) return NULL;
   info->magic.magic = rand();
   return info;
}

static void
evas_engine_sdl_info_free	(Evas* e __UNUSED__, void* info)
{
   Evas_Engine_Info_SDL*	in;
   in = (Evas_Engine_Info_SDL*) info;
   free(in);
}

/* SDL engine output manipulation function */
static int
evas_engine_sdl_setup		(Evas* e, void* in)
{
   Evas_Engine_Info_SDL*	info = (Evas_Engine_Info_SDL*) in;

   /* if we arent set to sdl, why the hell do we get called?! */
   if (evas_output_method_get(e) != evas_render_method_lookup("software_sdl"))
      return 0;

   SDL_Init(SDL_INIT_NOPARACHUTE);

   if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
     {
	ERR("SDL_Init failed with %s", SDL_GetError());
        SDL_Quit();
        return 0;
     }

   /* lets just set up */
   e->engine.data.output = _sdl_output_setup(e->output.w, e->output.h,
					     info->info.fullscreen,
					     info->info.noframe,
                                             info->info.alpha,
                                             info->info.hwsurface);

   if (!e->engine.data.output)
      return 0;

   e->engine.func = &func;
   e->engine.data.context = e->engine.func->context_new(e->engine.data.output);

   return 1;
}

static void
evas_engine_sdl_output_free	(void *data)
{
   Render_Engine*		re = data;

   if (re->tb)
     evas_common_tilebuf_free(re->tb);
   if (re->rects)
      evas_common_tilebuf_free_render_rects(re->rects);
   if (re->rgba_engine_image)
     evas_cache_engine_image_drop(&re->rgba_engine_image->cache_entry);
   if (re->cache)
     evas_cache_engine_image_shutdown(re->cache);

   if (re->update_rects)
     free(re->update_rects);
   free(re);

   evas_common_font_shutdown();
   evas_common_image_shutdown();

   SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void
evas_engine_sdl_output_resize	(void *data, int w, int h)
{
   /* FIXME */
   Render_Engine        *re = data;
   SDL_Surface          *surface;

   if (w == re->tb->outbuf_w && h == re->tb->outbuf_h)
     return;

   /* Destroy the current screen */
   evas_cache_engine_image_drop(&re->rgba_engine_image->cache_entry);

   /* Rebuil tilebuf */
   evas_common_tilebuf_free(re->tb);
   re->tb = evas_common_tilebuf_new(w, h);
   if (re->tb)
      evas_common_tilebuf_set_tile_size(re->tb, TILESIZE, TILESIZE);

   /* Build the new screen */
   surface = SDL_SetVideoMode(w, h, 32,
                              (re->flags.hwsurface ? SDL_HWSURFACE : SDL_SWSURFACE)
                              | (re->flags.fullscreen ? SDL_FULLSCREEN : 0)
                              | (re->flags.noframe ? SDL_NOFRAME : 0)
                              | (re->flags.alpha ? SDL_SRCALPHA : 0));

   if (!surface)
     {
	ERR("Unable to change the resolution to : %ix%i", w, h);
	exit(-1);
     }
   re->rgba_engine_image = (SDL_Engine_Image_Entry *) evas_cache_engine_image_engine(re->cache, surface);
   if (!re->rgba_engine_image)
     {
	ERR("RGBA_Image allocation from SDL failed");
	exit(-1);
     }

   SDL_FillRect(surface, NULL, 0);
}

static void
evas_engine_sdl_output_tile_size_set	(void *data, int w, int h)
{
   Render_Engine*			re = (Render_Engine*) data;

   evas_common_tilebuf_set_tile_size(re->tb, w, h);
}

static void
evas_engine_sdl_output_redraws_rect_add	(void *data, int x, int y, int w, int h)
{
   Render_Engine*			re = (Render_Engine*) data;

   evas_common_tilebuf_add_redraw(re->tb, x, y, w, h);
}

static void
evas_engine_sdl_output_redraws_rect_del	(void *data, int x, int y, int w, int h)
{
   Render_Engine*			re = (Render_Engine*) data;

   evas_common_tilebuf_del_redraw(re->tb, x, y, w, h);
}

static void
evas_engine_sdl_output_redraws_clear	(void *data)
{
   Render_Engine*			re = (Render_Engine*) data;

   evas_common_tilebuf_clear(re->tb);
}

static void*
evas_engine_sdl_output_redraws_next_update_get	(void *data,
						 int *x, int *y, int *w, int *h,
						 int *cx, int *cy, int *cw, int *ch)
{
   Render_Engine        *re = data;
   Tilebuf_Rect         *tb_rect;
   SDL_Rect              rect;

   if (re->flags.end)
     {
	re->flags.end = 0;
	return NULL;
     }
   if (!re->rects)
     {
	re->rects = evas_common_tilebuf_get_render_rects(re->tb);
	re->cur_rect = EINA_INLIST_GET(re->rects);
     }
   if (!re->cur_rect)
     {
        if (re->rects) evas_common_tilebuf_free_render_rects(re->rects);
        re->rects = NULL;
        return NULL;
     }

   tb_rect = (Tilebuf_Rect*) re->cur_rect;
   *cx = *x = tb_rect->x;
   *cy = *y = tb_rect->y;
   *cw = *w = tb_rect->w;
   *ch = *h = tb_rect->h;
   re->cur_rect = re->cur_rect->next;
   if (!re->cur_rect)
     {
	evas_common_tilebuf_free_render_rects(re->rects);
	re->rects = NULL;
	re->flags.end = 1;
     }

   rect.x = *x;
   rect.y = *y;
   rect.w = *w;
   rect.h = *h;

   /* Return the "fake" surface so it is passed to the drawing routines. */
   return re->rgba_engine_image;
}

static void
evas_engine_sdl_output_redraws_next_update_push	(void *data, void *surface __UNUSED__,
						 int x, int y, int w, int h)
{
   Render_Engine				*re = (Render_Engine *) data;

   if (re->update_rects_count + 1 > re->update_rects_limit)
     {
	re->update_rects_limit += 8;
	re->update_rects = realloc(re->update_rects, sizeof (SDL_Rect) * re->update_rects_limit);
     }

   re->update_rects[re->update_rects_count].x = x;
   re->update_rects[re->update_rects_count].y = y;
   re->update_rects[re->update_rects_count].w = w;
   re->update_rects[re->update_rects_count].h = h;

   ++re->update_rects_count;

   evas_common_cpu_end_opt();
}

static void
_sdl_image_dirty_region(Engine_Image_Entry *eim, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
   SDL_Engine_Image_Entry       *dst;
   RGBA_Image *im;

   dst = (SDL_Engine_Image_Entry *) eim;

   SDL_UpdateRect(dst->surface, x, y, w, h);

   im = (RGBA_Image *)eim->src;
   im->flags |= RGBA_IMAGE_IS_DIRTY;
}

static void
evas_engine_sdl_output_flush(void *data)
{
   Render_Engine        *re = (Render_Engine *) data;

   if (re->update_rects_count > 0)
     SDL_UpdateRects(re->rgba_engine_image->surface, re->update_rects_count, re->update_rects);

   re->update_rects_count = 0;
}


static void
evas_engine_sdl_output_idle_flush(void *data)
{
   (void) data;
}

/*
 * Image objects
 */

static void*
evas_engine_sdl_image_load(void *data, const char *file, const char *key, int *error, Evas_Image_Load_Opts *lo)
{
   Render_Engine*	re = (Render_Engine*) data;;

   *error = 0;
   return evas_cache_engine_image_request(re->cache, file, key, lo, NULL, error);
}

static int
evas_engine_sdl_image_alpha_get(void *data __UNUSED__, void *image)
{
   SDL_Engine_Image_Entry       *eim = image;
   RGBA_Image                   *im;

   if (!eim) return 1;
   im = (RGBA_Image *) eim->cache_entry.src;
   switch (eim->cache_entry.src->space)
     {
     case EVAS_COLORSPACE_ARGB8888:
        if (im->cache_entry.flags.alpha) return 1;
     default:
        break;
     }
   return 0;
}

static void
evas_engine_sdl_image_size_get(void *data __UNUSED__, void *image, int *w, int *h)
{
   SDL_Engine_Image_Entry       *eim;

   eim = image;
   if (w) *w = eim->cache_entry.src->w;
   if (h) *h = eim->cache_entry.src->h;
}

static int
evas_engine_sdl_image_colorspace_get(void *data __UNUSED__, void *image)
{
   SDL_Engine_Image_Entry       *eim = image;

   if (!eim) return EVAS_COLORSPACE_ARGB8888;
   return eim->cache_entry.src->space;
}

static void
evas_engine_sdl_image_colorspace_set(void *data __UNUSED__, void *image, int cspace)
{
   SDL_Engine_Image_Entry       *eim = image;

   if (!eim) return;
   if (eim->cache_entry.src->space == cspace) return;

   evas_cache_engine_image_colorspace(&eim->cache_entry, cspace, NULL);
}

static void*
evas_engine_sdl_image_new_from_copied_data(void *data,
					   int w, int h,
					   DATA32* image_data,
                                           int alpha, int cspace)
{
   Render_Engine	*re = (Render_Engine*) data;

   return evas_cache_engine_image_copied_data(re->cache, w, h, image_data, alpha, cspace, NULL);
}

static void*
evas_engine_sdl_image_new_from_data(void *data, int w, int h, DATA32* image_data, int alpha, int cspace)
{
   Render_Engine	*re = (Render_Engine*) data;

   return evas_cache_engine_image_data(re->cache, w, h, image_data, alpha, cspace, NULL);
}

static void
evas_engine_sdl_image_free(void *data, void *image)
{
   SDL_Engine_Image_Entry       *eim = image;

   (void) data;

   evas_cache_engine_image_drop(&eim->cache_entry);
}

static void*
evas_engine_sdl_image_size_set(void *data, void *image, int w, int h)
{
   SDL_Engine_Image_Entry       *eim = image;

   (void) data;

   return evas_cache_engine_image_size_set(&eim->cache_entry, w, h);
}

static void*
evas_engine_sdl_image_dirty_region(void *data,
				   void *image,
				   int x, int y, int w, int h)
{
   SDL_Engine_Image_Entry       *eim = image;

   (void) data;

   return evas_cache_engine_image_dirty(&eim->cache_entry, x, y, w, h);
}

static void*
evas_engine_sdl_image_data_get(void *data, void *image,
			       int to_write, DATA32** image_data, int *err)
{
   SDL_Engine_Image_Entry       *eim = image;
   RGBA_Image                   *im;

   (void) data;

   if (!eim)
     {
        *image_data = NULL;
        if (err) *err = EVAS_LOAD_ERROR_GENERIC;
        return NULL;
     }
   im = (RGBA_Image *) eim->cache_entry.src;

   switch (eim->cache_entry.src->space)
     {
     case EVAS_COLORSPACE_ARGB8888:
        if (to_write)
          eim = (SDL_Engine_Image_Entry *) evas_cache_engine_image_dirty(&eim->cache_entry, 0, 0, eim->cache_entry.src->w, eim->cache_entry.src->h);

        evas_cache_engine_image_load_data(&eim->cache_entry);
        *image_data = im->image.data;
        break;
     case EVAS_COLORSPACE_YCBCR422P709_PL:
     case EVAS_COLORSPACE_YCBCR422P601_PL:
     case EVAS_COLORSPACE_YCBCR422601_PL:
        *image_data = im->cs.data;
        break;
     default:
        abort();
        break;
     }
   if (err) *err = EVAS_LOAD_ERROR_NONE;
   return eim;
}

static void*
evas_engine_sdl_image_data_put(void *data, void *image, DATA32* image_data)
{
   SDL_Engine_Image_Entry       *eim = image;
   Render_Engine                *re = data;
   RGBA_Image                   *im;

   if (!eim) return NULL;
   im = (RGBA_Image*) eim->cache_entry.src;

   switch (eim->cache_entry.src->space)
     {
     case EVAS_COLORSPACE_ARGB8888:
        if (image_data != im->image.data)
          {
             evas_cache_engine_image_drop(&eim->cache_entry);
             eim = (SDL_Engine_Image_Entry *) evas_cache_engine_image_data(re->cache,
                                                                           eim->cache_entry.w, eim->cache_entry.h,
                                                                           image_data,
                                                                           func.image_alpha_get(data, eim),
                                                                           func.image_colorspace_get(data, eim),
                                                                           NULL);
          }
        break;
     case EVAS_COLORSPACE_YCBCR422P601_PL:
     case EVAS_COLORSPACE_YCBCR422P709_PL:
     case EVAS_COLORSPACE_YCBCR422601_PL:
        if (image_data != im->cs.data)
          {
             if (im->cs.data)
               if (!im->cs.no_free)
                 free(im->cs.data);
             im->cs.data = image_data;
             evas_common_image_colorspace_dirty(im);
          }
        break;
     default:
        abort();
        break;
     }
   return eim;
}

static void
evas_engine_sdl_image_data_preload_request(void *data __UNUSED__, void *image, const void *target)
{
   SDL_Engine_Image_Entry       *eim = image;
   RGBA_Image                   *im;

   if (!eim) return ;
   im = (RGBA_Image*) eim->cache_entry.src;
   if (!im) return ;
   evas_cache_image_preload_data(&im->cache_entry, target);
}

static void
evas_engine_sdl_image_data_preload_cancel(void *data __UNUSED__, void *image, const void *target)
{
   SDL_Engine_Image_Entry       *eim = image;
   RGBA_Image                   *im;

   if (!eim) return ;
   im = (RGBA_Image*) eim->cache_entry.src;
   if (!im) return ;
   evas_cache_image_preload_cancel(&im->cache_entry, target);
}

static void*
evas_engine_sdl_image_alpha_set(void *data, void *image, int has_alpha)
{
   SDL_Engine_Image_Entry       *eim = image;
   RGBA_Image                   *im;

   (void) data;

   if (!eim) return NULL;

   im = (RGBA_Image *) eim->cache_entry.src;

   if (eim->cache_entry.src->space != EVAS_COLORSPACE_ARGB8888)
     {
        im->cache_entry.flags.alpha = 0;
        return eim;
     }

   eim = (SDL_Engine_Image_Entry *) evas_cache_engine_image_dirty(&eim->cache_entry, 0, 0, eim->cache_entry.w, eim->cache_entry.h);

   /* FIXME: update SDL_Surface flags */
   im->cache_entry.flags.alpha = has_alpha ? 1 : 0;
   return eim;
}

static void*
evas_engine_sdl_image_border_set(void *data __UNUSED__, void *image, int l __UNUSED__, int r __UNUSED__, int t __UNUSED__, int b __UNUSED__)
{
   return image;
}

static void
evas_engine_sdl_image_border_get(void *data __UNUSED__, void *image __UNUSED__, int *l __UNUSED__, int *r __UNUSED__, int *t __UNUSED__, int *b __UNUSED__)
{
   /* FIXME: need to know what evas expect from this call */
}

static void
evas_engine_sdl_image_draw(void *data, void *context, void *surface, void *image,
			   int src_region_x, int src_region_y, int src_region_w, int src_region_h,
			   int dst_region_x, int dst_region_y, int dst_region_w, int dst_region_h,
			   int smooth)
{
   SDL_Engine_Image_Entry       *eim = image;
   SDL_Engine_Image_Entry       *dst = surface;
   RGBA_Draw_Context            *dc = (RGBA_Draw_Context*) context;
   int                           mustlock_im = 0;
   int                           mustlock_dst = 0;

   (void) data;

   if (eim->cache_entry.src->space == EVAS_COLORSPACE_ARGB8888)
     evas_cache_engine_image_load_data(&eim->cache_entry);

   /* Fallback to software method */
   if (SDL_MUSTLOCK(dst->surface))
     {
        mustlock_dst = 1;
	SDL_LockSurface(dst->surface);
	_SDL_UPDATE_PIXELS(dst);
     }

   if (eim->surface && SDL_MUSTLOCK(eim->surface))
     {
        mustlock_im = 1;
	SDL_LockSurface(eim->surface);
	_SDL_UPDATE_PIXELS(eim);
     }

   evas_common_image_colorspace_normalize((RGBA_Image *) eim->cache_entry.src);

   if (smooth)
     evas_common_scale_rgba_in_to_out_clip_smooth((RGBA_Image *) eim->cache_entry.src,
                                                  (RGBA_Image *) dst->cache_entry.src,
                                                  dc,
                                                  src_region_x, src_region_y, src_region_w, src_region_h,
                                                  dst_region_x, dst_region_y, dst_region_w, dst_region_h);
   else
     evas_common_scale_rgba_in_to_out_clip_sample((RGBA_Image *) eim->cache_entry.src,
                                                  (RGBA_Image *) dst->cache_entry.src,
                                                  dc,
                                                  src_region_x, src_region_y, src_region_w, src_region_h,
                                                  dst_region_x, dst_region_y, dst_region_w, dst_region_h);
   evas_common_cpu_end_opt ();

   if (mustlock_im)
     SDL_UnlockSurface(eim->surface);

   if (mustlock_dst)
     SDL_UnlockSurface(dst->surface);
}

static void
evas_engine_sdl_image_map_draw(void *data __UNUSED__, void *context, void *surface, void *image, int npoints, RGBA_Map_Point *p, int smooth, int level)
{
   SDL_Engine_Image_Entry *eim = image;
   SDL_Engine_Image_Entry *dst = surface;
   int mustlock_im = 0;
   int mustlock_dst = 0;

   if (!eim || !dst) return;

   if (SDL_MUSTLOCK(dst->surface))
     {
        mustlock_dst = 1;
	SDL_LockSurface(dst->surface);
	_SDL_UPDATE_PIXELS(dst);
     }

   if (eim->surface && SDL_MUSTLOCK(eim->surface))
     {
        mustlock_im = 1;
	SDL_LockSurface(eim->surface);
	_SDL_UPDATE_PIXELS(eim);
     }

   evas_common_map_rgba((RGBA_Image*) eim->cache_entry.src,
			 (RGBA_Image*) dst->cache_entry.src, context, npoints, p, smooth, level);
   evas_common_cpu_end_opt();

   if (mustlock_im)
     SDL_UnlockSurface(eim->surface);

   if (mustlock_dst)
     SDL_UnlockSurface(dst->surface);
}

static void *
evas_engine_sdl_image_map_surface_new(void *data, int w, int h, int alpha)
{
   Render_Engine *re = (Render_Engine*) data;
   void *surface;

   surface = evas_cache_engine_image_copied_data(re->cache,
						 w, h, NULL, alpha,
						 EVAS_COLORSPACE_ARGB8888,
						 NULL);
   return surface;
}

static void
evas_engine_sdl_image_map_surface_free(void *data __UNUSED__, void *surface)
{
   evas_cache_engine_image_drop(surface);
}

static void
evas_engine_sdl_image_scale_hint_set(void *data __UNUSED__, void *image, int hint)
{
   SDL_Engine_Image_Entry *eim;

   if (!image) return ;
   eim = image;
   eim->cache_entry.src->scale_hint = hint;
}

static int
evas_engine_sdl_image_scale_hint_get(void *data __UNUSED__, void *image)
{
   SDL_Engine_Image_Entry *eim;

   if (!image) return EVAS_IMAGE_SCALE_HINT_NONE;
   eim = image;
   return eim->cache_entry.src->scale_hint;
}

static void
evas_engine_sdl_image_cache_flush(void *data)
{
   Render_Engine        *re = (Render_Engine*) data;
   int                   size;

   size = evas_cache_engine_image_get(re->cache);
   evas_cache_engine_image_set(re->cache, 0);
   evas_cache_engine_image_set(re->cache, size);
}

static void
evas_engine_sdl_image_cache_set(void *data, int bytes)
{
   Render_Engine        *re = (Render_Engine*) data;

   evas_cache_engine_image_set(re->cache, bytes);
}

static int
evas_engine_sdl_image_cache_get(void *data)
{
   Render_Engine        *re = (Render_Engine*) data;

   return evas_cache_engine_image_get(re->cache);
}

static char*
evas_engine_sdl_image_comment_get(void *data __UNUSED__, void *image, char *key __UNUSED__)
{
   SDL_Engine_Image_Entry       *eim = image;
   RGBA_Image                   *im;

   if (!eim) return NULL;
   im = (RGBA_Image *) eim->cache_entry.src;

   return im->info.comment;
}

static char*
evas_engine_sdl_image_format_get(void *data __UNUSED__, void *image __UNUSED__)
{
   /* FIXME: need to know what evas expect from this call */
   return NULL;
}

static void
evas_engine_sdl_font_draw(void *data __UNUSED__, void *context, void *surface, void *font, int x, int y, int w __UNUSED__, int h __UNUSED__, int ow __UNUSED__, int oh __UNUSED__, const Evas_Text_Props *intl_props)
{
   SDL_Engine_Image_Entry       *eim = surface;
   int                           mustlock_im = 0;

   if (eim->surface && SDL_MUSTLOCK(eim->surface))
     {
        mustlock_im = 1;
	SDL_LockSurface(eim->surface);
	_SDL_UPDATE_PIXELS(eim);
     }

   evas_common_font_draw((RGBA_Image *) eim->cache_entry.src, context, font, x, y, intl_props);
   evas_common_cpu_end_opt();

   if (mustlock_im)
     SDL_UnlockSurface(eim->surface);
}

static void
evas_engine_sdl_line_draw(void *data __UNUSED__, void *context, void *surface, int x1, int y1, int x2, int y2)
{
   SDL_Engine_Image_Entry       *eim = surface;
   int                           mustlock_im = 0;

   if (eim->surface && SDL_MUSTLOCK(eim->surface))
     {
        mustlock_im = 1;
	SDL_LockSurface(eim->surface);
	_SDL_UPDATE_PIXELS(eim);
     }

   evas_common_line_draw((RGBA_Image *) eim->cache_entry.src, context, x1, y1, x2, y2);
   evas_common_cpu_end_opt();

   if (mustlock_im)
     SDL_UnlockSurface(eim->surface);
}

static void
evas_engine_sdl_rectangle_draw(void *data __UNUSED__, void *context, void *surface, int x, int y, int w, int h)
{
   SDL_Engine_Image_Entry       *eim = surface;
#if ENGINE_SDL_PRIMITIVE
   RGBA_Draw_Context            *dc = context;
#endif
   int                           mustlock_im = 0;

#if ENGINE_SDL_PRIMITIVE
   if (A_VAL(&dc->col.col) != 0x00)
     {
        if (A_VAL(&dc->col.col) != 0xFF)
          {
#endif
             if (eim->surface && SDL_MUSTLOCK(eim->surface))
               {
                  mustlock_im = 1;
                  SDL_LockSurface(eim->surface);
                  _SDL_UPDATE_PIXELS(eim);
               }

             evas_common_rectangle_draw((RGBA_Image *) eim->cache_entry.src, context, x, y, w, h);
             evas_common_cpu_end_opt();

             if (mustlock_im)
               SDL_UnlockSurface(eim->surface);
#if ENGINE_SDL_PRIMITIVE
          }
        else
          {
             SDL_Rect        dstrect;

             if (dc->clip.use)
               {
                  SDL_Rect   cliprect;

                  cliprect.x = dc->clip.x;
                  cliprect.y = dc->clip.y;
                  cliprect.w = dc->clip.w;
                  cliprect.h = dc->clip.h;

                  SDL_SetClipRect(eim->surface, &cliprect);
               }

             dstrect.x = x;
             dstrect.y = y;
             dstrect.w = w;
             dstrect.h = h;

             SDL_FillRect(eim->surface, &dstrect, SDL_MapRGBA(eim->surface->format, R_VAL(&dc->col.col), G_VAL(&dc->col.col), B_VAL(&dc->col.col), 0xFF));

             if (dc->clip.use)
               SDL_SetClipRect(eim->surface, NULL);
          }
     }
#endif
}

static void
evas_engine_sdl_polygon_draw(void *data __UNUSED__, void *context, void *surface, void *polygon, int x, int y)
{
   SDL_Engine_Image_Entry       *eim = surface;
   int                           mustlock_im = 0;

   if (eim->surface && SDL_MUSTLOCK(eim->surface))
     {
        mustlock_im = 1;
	SDL_LockSurface(eim->surface);
	_SDL_UPDATE_PIXELS(eim);
     }

   evas_common_polygon_draw((RGBA_Image *) eim->cache_entry.src, context, polygon, x, y);
   evas_common_cpu_end_opt();

   if (mustlock_im)
     SDL_UnlockSurface(eim->surface);
}

static int module_open(Evas_Module *em)
{
   if (!em) return 0;
   /* get whatever engine module we inherit from */
   if (!_evas_module_engine_inherit(&pfunc, "software_generic")) return 0;
   _evas_engine_soft_sdl_log_dom = eina_log_domain_register
     ("evas-software_sdl", EVAS_DEFAULT_LOG_COLOR);
   if (_evas_engine_soft_sdl_log_dom < 0)
     {
        EINA_LOG_ERR("Can not create a module log domain.");
        return 0;
     }
   /* store it for later use */
   func = pfunc;
   /* now to override methods */
#define ORD(f) EVAS_API_OVERRIDE(f, &func, evas_engine_sdl_)
   ORD(info);
   ORD(info_free);
   ORD(setup);
   ORD(output_free);
   ORD(output_resize);
   ORD(output_tile_size_set);
   ORD(output_redraws_rect_add);
   ORD(output_redraws_rect_del);
   ORD(output_redraws_clear);
   ORD(output_redraws_next_update_get);
   ORD(output_redraws_next_update_push);
   ORD(output_flush);
   ORD(output_idle_flush);
   ORD(image_load);
   ORD(image_new_from_data);
   ORD(image_new_from_copied_data);
   ORD(image_colorspace_set);
   ORD(image_colorspace_get);
   ORD(image_free);
   ORD(image_size_set);
   ORD(image_size_get);
   ORD(image_dirty_region);
   ORD(image_data_get);
   ORD(image_data_put);
   ORD(image_data_preload_request);
   ORD(image_data_preload_cancel);
   ORD(image_alpha_set);
   ORD(image_alpha_get);
   ORD(image_border_set);
   ORD(image_border_get);
   ORD(image_draw);
   ORD(image_map_draw);
   ORD(image_map_surface_new);
   ORD(image_map_surface_free);
   ORD(image_comment_get);
   ORD(image_format_get);
   ORD(image_cache_flush);
   ORD(image_cache_set);
   ORD(image_cache_get);
   ORD(font_draw);
   ORD(line_draw);
   ORD(rectangle_draw);
   ORD(polygon_draw);

   ORD(image_scale_hint_set);
   ORD(image_scale_hint_get);

   /* now advertise out own api */
   em->functions = (void *)(&func);
   return 1;
}

static void module_close(Evas_Module *em __UNUSED__)
{
  eina_log_domain_unregister(_evas_engine_soft_sdl_log_dom);
}

static Evas_Module_Api evas_modapi =
{
  EVAS_MODULE_API_VERSION,
  "software_sdl",
  "none",
  {
    module_open,
    module_close
  }
};

EVAS_MODULE_DEFINE(EVAS_MODULE_TYPE_ENGINE, engine, software_sdl);

#ifndef EVAS_STATIC_BUILD_SOFTWARE_SDL
EVAS_EINA_MODULE_DEFINE(engine, software_sdl);
#endif

/* Private routines. */

static void*
_sdl_output_setup		(int w, int h, int fullscreen, int noframe, int alpha, int hwsurface)
{
   Render_Engine		*re = calloc(1, sizeof(Render_Engine));
   SDL_Surface                  *surface;

   if (!re)
     return NULL;

   /* if we haven't initialized - init (automatic abort if already done) */
   evas_common_cpu_init();
   evas_common_blend_init();
   evas_common_image_init();
   evas_common_convert_init();
   evas_common_scale_init();
   evas_common_rectangle_init();
   evas_common_polygon_init();
   evas_common_line_init();
   evas_common_font_init();
   evas_common_draw_init();
   evas_common_tilebuf_init();

   if (w <= 0) w = 640;
   if (h <= 0) h = 480;

   re->cache = evas_cache_engine_image_init(&_sdl_cache_engine_image_cb, evas_common_image_cache_get());
   if (!re->cache)
     {
        ERR("Evas_Cache_Engine_Image allocation failed!");
        free (re);
        return NULL;
     }

   re->tb = evas_common_tilebuf_new(w, h);
   /* in preliminary tests 16x16 gave highest framerates */
   evas_common_tilebuf_set_tile_size(re->tb, TILESIZE, TILESIZE);
   surface = SDL_SetVideoMode(w, h, 32,
                              (hwsurface ? SDL_HWSURFACE : SDL_SWSURFACE)
                              | (fullscreen ? SDL_FULLSCREEN : 0)
                              | (noframe ? SDL_NOFRAME : 0)
                              | (alpha ? SDL_SRCALPHA : 0));

   if (!surface)
     {
        ERR("SDL_SetVideoMode [ %i x %i x 32 ] failed.", w, h);
        evas_cache_engine_image_shutdown(re->cache);
        free (re);
        return NULL;
     }

   SDL_SetAlpha(surface, SDL_SRCALPHA | SDL_RLEACCEL, 0);

   /* We create a "fake" RGBA_Image which points to the SDL surface. Each access
    * to that surface is wrapped in Lock / Unlock calls whenever the data is
    * manipulated directly. */
   re->rgba_engine_image = (SDL_Engine_Image_Entry *) evas_cache_engine_image_engine(re->cache, surface);
   if (!re->rgba_engine_image)
     {
	CRIT("RGBA_Image allocation from SDL failed");
        evas_cache_engine_image_shutdown(re->cache);
        free (re);
        return NULL;
     }

   SDL_FillRect(surface, NULL, 0);

   re->flags.alpha = alpha;
   re->flags.hwsurface = hwsurface;
   re->flags.fullscreen = fullscreen;
   re->flags.noframe = noframe;
   return re;
}

static Engine_Image_Entry*
_sdl_image_alloc(void)
{
   SDL_Engine_Image_Entry       *new;

   new = calloc(1, sizeof (SDL_Engine_Image_Entry));

   return (Engine_Image_Entry *) new;
}

static void
_sdl_image_delete(Engine_Image_Entry *eim)
{
   free(eim);
}

static int
_sdl_image_constructor(Engine_Image_Entry *ie, void *data __UNUSED__)
{
   SDL_Surface                  *sdl = NULL;
   SDL_Engine_Image_Entry       *eim = (SDL_Engine_Image_Entry *) ie;
   RGBA_Image                   *im;

   im = (RGBA_Image *) ie->src;

   if (im)
     {
        evas_cache_image_load_data(&im->cache_entry);

        if (im->image.data)
          {
             /* FIXME: Take care of CSPACE */
             sdl = SDL_CreateRGBSurfaceFrom(im->image.data,
                                            ie->w, ie->h,
                                            32, ie->w * 4,
                                            RMASK, GMASK, BMASK, AMASK);
             eim->surface = sdl;
             eim->flags.engine_surface = 0;
          }
     }

   return EVAS_LOAD_ERROR_NONE;
}

static void
_sdl_image_destructor(Engine_Image_Entry *eie)
{
   SDL_Engine_Image_Entry       *seie = (SDL_Engine_Image_Entry *) eie;

   if (seie->surface && !seie->flags.engine_surface)
     SDL_FreeSurface(seie->surface);
   seie->surface = NULL;
}

static int
_sdl_image_dirty(Engine_Image_Entry *dst, const Engine_Image_Entry *src __UNUSED__)
{
   SDL_Engine_Image_Entry       *eim = (SDL_Engine_Image_Entry *) dst;
   SDL_Surface                  *sdl = NULL;
   RGBA_Image                   *im;

   im = (RGBA_Image *) dst->src;

   /* FIXME: Take care of CSPACE */
   sdl = SDL_CreateRGBSurfaceFrom(im->image.data,
                                  dst->w, dst->h,
                                  32, dst->w * 4,
                                  0xff0000, 0xff00, 0xff, 0xff000000);
   eim->surface = sdl;
   eim->flags.engine_surface = 0;

   return 0;
}

static int
_sdl_image_update_data(Engine_Image_Entry *dst, void* engine_data)
{
   SDL_Engine_Image_Entry       *eim = (SDL_Engine_Image_Entry *) dst;
   SDL_Surface                  *sdl = NULL;
   RGBA_Image                   *im;

   im = (RGBA_Image *) dst->src;

   if (engine_data)
     {
        sdl = engine_data;

        if (im)
          {
             im->image.data = sdl->pixels;
             im->image.no_free = 1;
             im->cache_entry.flags.alpha = 0;
             dst->src->w = sdl->w;
             dst->src->h = sdl->h;
          }
        dst->w = sdl->w;
        dst->h = sdl->h;
     }
   else
     {
        /* FIXME: Take care of CSPACE */
        SDL_FreeSurface(eim->surface);
        sdl = SDL_CreateRGBSurfaceFrom(im->image.data,
                                       dst->w, dst->h,
                                       32, dst->w * 4,
                                       RMASK, GMASK, BMASK, AMASK);
     }

   eim->surface = sdl;

   return 0;
}

static int
_sdl_image_size_set(Engine_Image_Entry *dst, const Engine_Image_Entry *src __UNUSED__)
{
   SDL_Engine_Image_Entry       *eim = (SDL_Engine_Image_Entry *) dst;
   SDL_Surface                  *sdl;
   RGBA_Image                   *im;

   im = (RGBA_Image *) dst->src;

   /* FIXME: handle im == NULL */
   sdl = SDL_CreateRGBSurfaceFrom(im->image.data,
                                  dst->w, dst->h,
                                  32, dst->w * 4,
                                  RMASK, GMASK, BMASK, AMASK);

   eim->surface = sdl;

   return 0;
}

static void
_sdl_image_load(Engine_Image_Entry *eim, const Image_Entry *ie_im)
{
   SDL_Engine_Image_Entry       *load = (SDL_Engine_Image_Entry *) eim;
   SDL_Surface                  *sdl;

   if (!load->surface)
     {
        RGBA_Image      *im;

        im = (RGBA_Image *) ie_im;

        sdl = SDL_CreateRGBSurfaceFrom(im->image.data,
                                       eim->w, eim->h,
                                       32, eim->w * 4,
                                       RMASK, GMASK, BMASK, AMASK);
        load->surface = sdl;
     }
}

static int
_sdl_image_mem_size_get(Engine_Image_Entry *eim)
{
   SDL_Engine_Image_Entry       *seie = (SDL_Engine_Image_Entry *) eim;
   int                           size = 0;

   /* FIXME: Count surface size. */
   if (seie->surface)
     size = sizeof (SDL_Surface) + sizeof (SDL_PixelFormat);

   return size;
}

#ifdef DEBUG_SDL
static void
_sdl_image_debug(const char* context, Engine_Image_Entry* eie)
{
   SDL_Engine_Image_Entry       *eim = (SDL_Engine_Image_Entry *) eie;

   DBG("*** %s image (%p) ***", context, eim);
   if (eim)
     {
        DBG("W: %i, H: %i, R: %i", eim->cache_entry.w, eim->cache_entry.h, eim->cache_entry.references);
        if (eim->cache_entry.src)
          DBG("Pixels: %p, SDL Surface: %p",((RGBA_Image*) eim->cache_entry.src)->image.data, eim->surface);
        if (eim->surface)
          DBG("Surface->pixels: %p", eim->surface->pixels);
	DBG("Key: %s", eim->cache_entry.cache_key);
        DBG("Reference: %i", eim->cache_entry.references);
     }
   DBG("*** ***");
}
#endif
