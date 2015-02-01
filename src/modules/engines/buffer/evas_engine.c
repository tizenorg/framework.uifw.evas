#include "evas_common.h"
#include "evas_private.h"
#include "evas_engine.h"
#include "Evas_Engine_Buffer.h"

/* domain for eina_log */
/* the log macros are defined in evas_common.h */
/* theirs names are EVAS_ERR, EVAS_DBG, EVAS_CRIT, EVAS_WRN and EVAS_INF */
/* although we can use the EVAS_ERROR, etc... macros it will not work
   when the -fvisibility=hidden option is passed to gcc */

int _evas_engine_buffer_log_dom = -1;

/* function tables - filled in later (func and parent func) */

static Evas_Func func, pfunc;


/* engine struct data */
typedef struct _Render_Engine Render_Engine;

struct _Render_Engine
{
   Tilebuf          *tb;
   Outbuf           *ob;
   Tilebuf_Rect     *rects;
   Eina_Inlist *cur_rect;
   Eina_Inarray      previous_rects;
   int               end : 1;
};

/* prototypes we will use here */
static void *_output_setup(int w, int h, void *dest_buffer, int dest_buffer_row_bytes, int depth_type, int use_color_key, int alpha_threshold, int color_key_r, int color_key_g, int color_key_b, void *(*new_update_region) (int x, int y, int w, int h, int *row_bytes), void (*free_update_region) (int x, int y, int w, int h, void *data), void *(*switch_buffer) (void *data, void *dest_buffer), void *switch_data);

static void *eng_info(Evas *e __UNUSED__);
static void eng_info_free(Evas *e __UNUSED__, void *info);
static int eng_setup(Evas *e, void *info);
static void eng_output_free(void *data);
static void eng_output_resize(void *data, int w, int h);
static void eng_output_tile_size_set(void *data, int w, int h);
static void eng_output_redraws_rect_add(void *data, int x, int y, int w, int h);
static void eng_output_redraws_rect_del(void *data, int x, int y, int w, int h);
static void eng_output_redraws_clear(void *data);
static void *eng_output_redraws_next_update_get(void *data, int *x, int *y, int *w, int *h, int *cx, int *cy, int *cw, int *ch);
static void eng_output_redraws_next_update_push(void *data, void *surface, int x, int y, int w, int h);
static void eng_output_flush(void *data);
static void eng_output_idle_flush(void *data);

/* internal engine routines */
static void *
_output_setup(int w,
	      int h,
	      void *dest_buffer,
	      int dest_buffer_row_bytes,
	      int depth_type,
	      int use_color_key,
	      int alpha_threshold,
	      int color_key_r,
	      int color_key_g,
	      int color_key_b,
	      void *(*new_update_region) (int x, int y, int w, int h, int *row_bytes),
	      void (*free_update_region) (int x, int y, int w, int h, void *data),
              void *(*switch_buffer) (void *data, void *dest_buffer),
              void *switch_data
	      )
{
   Render_Engine *re;

   re = calloc(1, sizeof(Render_Engine));
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

   evas_buffer_outbuf_buf_init();

     {
	Outbuf_Depth dep;
	DATA32 color_key = 0;

	dep = OUTBUF_DEPTH_BGR_24BPP_888_888;
	if      (depth_type == EVAS_ENGINE_BUFFER_DEPTH_ARGB32)
	  dep = OUTBUF_DEPTH_ARGB_32BPP_8888_8888;
	else if (depth_type == EVAS_ENGINE_BUFFER_DEPTH_RGB32)
	  dep = OUTBUF_DEPTH_RGB_32BPP_888_8888;
	else if (depth_type == EVAS_ENGINE_BUFFER_DEPTH_BGRA32)
	  dep = OUTBUF_DEPTH_BGRA_32BPP_8888_8888;
	else if (depth_type == EVAS_ENGINE_BUFFER_DEPTH_RGB24)
	  dep = OUTBUF_DEPTH_RGB_24BPP_888_888;
	else if (depth_type == EVAS_ENGINE_BUFFER_DEPTH_BGR24)
	  dep = OUTBUF_DEPTH_BGR_24BPP_888_888;
	R_VAL(&color_key) = color_key_r;
	G_VAL(&color_key) = color_key_g;
	B_VAL(&color_key) = color_key_b;
	A_VAL(&color_key) = 0;
	re->ob = evas_buffer_outbuf_buf_setup_fb(w,
						 h,
						 dep,
						 dest_buffer,
						 dest_buffer_row_bytes,
						 use_color_key,
						 color_key,
						 alpha_threshold,
						 new_update_region,
						 free_update_region,
                                                 switch_buffer,
                                                 switch_data);
     }
   re->tb = evas_common_tilebuf_new(w, h);
   evas_common_tilebuf_set_tile_size(re->tb, TILESIZE, TILESIZE);
   eina_inarray_step_set(&re->previous_rects, sizeof (Eina_Inarray), sizeof (Eina_Rectangle), 8);
   return re;
}

/* engine api this module provides */
static void *
eng_info(Evas *e __UNUSED__)
{
   Evas_Engine_Info_Buffer *info;
   info = calloc(1, sizeof(Evas_Engine_Info_Buffer));
   if (!info) return NULL;
   info->magic.magic = rand();
   info->render_mode = EVAS_RENDER_MODE_BLOCKING;
   return info;
}

static void
eng_info_free(Evas *e __UNUSED__, void *info)
{
   Evas_Engine_Info_Buffer *in;
   in = (Evas_Engine_Info_Buffer *)info;
   free(in);
}

static int
eng_setup(Evas *e, void *in)
{
   Render_Engine *re;
   Evas_Engine_Info_Buffer *info;

   info = (Evas_Engine_Info_Buffer *)in;
   re = _output_setup(e->output.w,
		      e->output.h,
		      info->info.dest_buffer,
		      info->info.dest_buffer_row_bytes,
		      info->info.depth_type,
		      info->info.use_color_key,
		      info->info.alpha_threshold,
		      info->info.color_key_r,
		      info->info.color_key_g,
		      info->info.color_key_b,
		      info->info.func.new_update_region,
		      info->info.func.free_update_region,
                      info->info.func.switch_buffer,
                      info->info.switch_data);
   if (e->engine.data.output)
     eng_output_free(e->engine.data.output);
   e->engine.data.output = re;
   if (!e->engine.data.output) return 0;
   if (!e->engine.data.context)
     e->engine.data.context = e->engine.func->context_new(e->engine.data.output);
   return 1;
}

static void
eng_output_free(void *data)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   evas_buffer_outbuf_buf_free(re->ob);
   evas_common_tilebuf_free(re->tb);
   if (re->rects) evas_common_tilebuf_free_render_rects(re->rects);
   free(re);

   evas_common_font_shutdown();
   evas_common_image_shutdown();
}

static void
eng_output_resize(void *data, int w, int h)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
     {
	int      depth;
	void    *dest;
	int      dest_row_bytes;
	int      alpha_level;
	DATA32   color_key;
	char     use_color_key;
	void * (*new_update_region) (int x, int y, int w, int h, int *row_bytes);
	void   (*free_update_region) (int x, int y, int w, int h, void *data);
        void * (*switch_buffer) (void *switch_data, void *dest);
        void    *switch_data;

	depth = re->ob->depth;
	dest = re->ob->dest;
	dest_row_bytes = re->ob->dest_row_bytes;
	alpha_level = re->ob->alpha_level;
	color_key = re->ob->color_key;
	use_color_key = re->ob->use_color_key;
	new_update_region = re->ob->func.new_update_region;
	free_update_region = re->ob->func.free_update_region;
        switch_buffer = re->ob->func.switch_buffer;
        switch_data = re->ob->switch_data;
	evas_buffer_outbuf_buf_free(re->ob);
	re->ob = evas_buffer_outbuf_buf_setup_fb(w,
						 h,
						 depth,
						 dest,
						 dest_row_bytes,
						 use_color_key,
						 color_key,
						 alpha_level,
						 new_update_region,
						 free_update_region,
                                                 switch_buffer,
                                                 switch_data);
     }
   evas_common_tilebuf_free(re->tb);
   re->tb = evas_common_tilebuf_new(w, h);
   if (re->tb)
     evas_common_tilebuf_set_tile_size(re->tb, TILESIZE, TILESIZE);
}

static void
eng_output_tile_size_set(void *data, int w, int h)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   evas_common_tilebuf_set_tile_size(re->tb, w, h);
}

static void
eng_output_redraws_rect_add(void *data, int x, int y, int w, int h)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   evas_common_tilebuf_add_redraw(re->tb, x, y, w, h);
}

static void
eng_output_redraws_rect_del(void *data, int x, int y, int w, int h)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   evas_common_tilebuf_del_redraw(re->tb, x, y, w, h);
}

static void
eng_output_redraws_clear(void *data)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   evas_common_tilebuf_clear(re->tb);
}

static void *
eng_output_redraws_next_update_get(void *data, int *x, int *y, int *w, int *h, int *cx, int *cy, int *cw, int *ch)
{
   Render_Engine *re;
   RGBA_Image *surface;
   Tilebuf_Rect *rect;
   int ux, uy, uw, uh;

   re = (Render_Engine *)data;
   if (re->end)
     {
	re->end = 0;
	return NULL;
     }
   if (!re->rects)
     {
	re->rects = evas_common_tilebuf_get_render_rects(re->tb);

        /* handle double buffering */
        if (re->ob->func.switch_buffer)
          {
             Eina_Rectangle *pushing;

             if (re->ob->first_frame && !re->previous_rects.len)
               {
                  evas_common_tilebuf_add_redraw(re->tb, 0, 0, re->ob->w, re->ob->h);
                  re->ob->first_frame = 0;
               }

             /* push previous frame */
             EINA_INARRAY_FOREACH(&re->previous_rects, pushing)
               evas_common_tilebuf_add_redraw(re->tb, pushing->x, pushing->y, pushing->w, pushing->h);
             eina_inarray_flush(&re->previous_rects);

             /* save current list of damage */
             EINA_INLIST_FOREACH(re->rects, rect)
               {
                  Eina_Rectangle local;

                  EINA_RECTANGLE_SET(&local, rect->x, rect->y, rect->w, rect->h);
                  eina_inarray_push(&re->previous_rects, &local);
               }

             /* and regenerate the damage list by tacking into account the damage over two frames */
             evas_common_tilebuf_free_render_rects(re->rects);
             re->rects = evas_common_tilebuf_get_render_rects(re->tb);
          }

	re->cur_rect = EINA_INLIST_GET(re->rects);
     }
   if (!re->cur_rect) return NULL;
   rect = (Tilebuf_Rect *)re->cur_rect;
   ux = rect->x; uy = rect->y; uw = rect->w; uh = rect->h;
   re->cur_rect = re->cur_rect->next;
   if (!re->cur_rect)
     {
	evas_common_tilebuf_free_render_rects(re->rects);
	re->rects = NULL;
	re->end = 1;
     }

   if ((ux + uw) > re->ob->w) uw = re->ob->w - ux;
   if ((uy + uh) > re->ob->h) uh = re->ob->h - uy;
   if ((uw <= 0) || (uh <= 0)) return NULL;
   surface = evas_buffer_outbuf_buf_new_region_for_update(re->ob,
							  ux, uy, uw, uh,
							  cx, cy, cw, ch);
   *x = ux; *y = uy; *w = uw; *h = uh;
   return surface;
}

static void
eng_output_redraws_next_update_push(void *data, void *surface, int x, int y, int w, int h)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
#ifdef BUILD_PIPE_RENDER
   evas_common_pipe_map_begin(surface);
#endif
   evas_buffer_outbuf_buf_push_updated_region(re->ob, surface, x, y, w, h);
   evas_buffer_outbuf_buf_free_region_for_update(re->ob, surface);
   evas_common_cpu_end_opt();
}

static void
eng_output_flush(void *data)
{
   Render_Engine *re = (Render_Engine *)data;
   evas_buffer_outbuf_buf_switch_buffer(re->ob);
}

static void
eng_output_idle_flush(void *data __UNUSED__)
{
}

static Eina_Bool
eng_canvas_alpha_get(void *data, void *context __UNUSED__)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   if (re->ob->priv.back_buf)
     return re->ob->priv.back_buf->cache_entry.flags.alpha;
   return EINA_TRUE;
}

/* module advertising code */
static int
module_open(Evas_Module *em)
{
   if (!em) return 0;
   /* get whatever engine module we inherit from */
   if (!_evas_module_engine_inherit(&pfunc, "software_generic")) return 0;

   _evas_engine_buffer_log_dom = eina_log_domain_register
     ("evas-buffer", EINA_COLOR_BLUE);
   if (_evas_engine_buffer_log_dom < 0)
     {
        EINA_LOG_ERR("Can not create a module log domain.");
        return 0;
     }

   /* store it for later use */
   func = pfunc;
   /* now to override methods */
#define ORD(f) EVAS_API_OVERRIDE(f, &func, eng_)
   ORD(info);
   ORD(info_free);
   ORD(setup);
   ORD(canvas_alpha_get);
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
   /* now advertise out own api */
   em->functions = (void *)(&func);
   return 1;
}

static void
module_close(Evas_Module *em __UNUSED__)
{
  eina_log_domain_unregister(_evas_engine_buffer_log_dom);
}

static Evas_Module_Api evas_modapi =
{
   EVAS_MODULE_API_VERSION,
   "buffer",
   "none",
   {
     module_open,
     module_close
   }
};

EVAS_MODULE_DEFINE(EVAS_MODULE_TYPE_ENGINE, engine, buffer);

#ifndef EVAS_STATIC_BUILD_BUFFER
EVAS_EINA_MODULE_DEFINE(engine, buffer);
#endif

