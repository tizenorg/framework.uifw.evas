#include "evas_common.h"
#include "evas_private.h"

#include "Evas_Engine_Software_X11.h"
#include "evas_engine.h"

#ifdef BUILD_ENGINE_SOFTWARE_XLIB
# include "evas_xlib_outbuf.h"
# include "evas_xlib_color.h"
#endif

#ifdef BUILD_ENGINE_SOFTWARE_XCB
# include "evas_xcb_outbuf.h"
# include "evas_xcb_color.h"
# include "evas_xcb_xdefaults.h"
#endif

int _evas_engine_soft_x11_log_dom = -1;

/* function tables - filled in later (func and parent func) */
static Evas_Func func, pfunc;

#ifdef BUILD_ENGINE_SOFTWARE_XLIB
/*
struct xrdb_user
{
   time_t last_stat;
   time_t last_mtime;
   XrmDatabase db;
};
static struct xrdb_user xrdb_user = {0, 0, NULL};

static Eina_Bool
xrdb_user_query(const char *name, const char *cls, char **type, XrmValue *val)
{
   time_t last, now;

   last = xrdb_user.last_stat;
   now = time(NULL);

   xrdb_user.last_stat = now;
   if (last != now) // don't stat() more than once every second
     {
	struct stat st;
	const char *home;
	char tmp[PATH_MAX];

        if (!(home = getenv("HOME"))) 
          goto failed;

	snprintf(tmp, sizeof(tmp), "%s/.Xdefaults", home);
	if (stat(tmp, &st) != 0) goto failed;
	if (xrdb_user.last_mtime != st.st_mtime)
	  {
	     if (xrdb_user.db) XrmDestroyDatabase(xrdb_user.db);
	     xrdb_user.db = XrmGetFileDatabase(tmp);
	     if (!xrdb_user.db) goto failed;
	     xrdb_user.last_mtime = st.st_mtime;
	  }
     }

   if (!xrdb_user.db) return EINA_FALSE;
   return XrmGetResource(xrdb_user.db, name, cls, type, val);

 failed:
   if (xrdb_user.db)
     {
	XrmDestroyDatabase(xrdb_user.db);
	xrdb_user.db = NULL;
     }
   xrdb_user.last_mtime = 0;
   return EINA_FALSE;
}
*/
#endif

/* engine struct data */
typedef struct _Render_Engine Render_Engine;

struct _Render_Engine
{
   Tilebuf *tb;
   Outbuf *ob;
   Tilebuf_Rect *rects;
   Eina_Inlist *cur_rect;
   unsigned char end : 1;
/*
#ifdef BUILD_ENGINE_SOFTWARE_XLIB
   XrmDatabase xrdb;
#endif
   struct 
     {
        int dpi;
     } xr;
 */
#ifdef EVAS_FRAME_QUEUING
   Evas_Engine_Render_Mode render_mode;
#endif

   void (*outbuf_free)(Outbuf *ob);
   void (*outbuf_reconfigure)(Outbuf *ob, int w, int h, int rot, Outbuf_Depth depth);
   int (*outbuf_get_rot)(Outbuf *ob);
   RGBA_Image *(*outbuf_new_region_for_update)(Outbuf *ob, int x, int y, int w, int h, int *cx, int *cy, int *cw, int *ch);
   void (*outbuf_push_updated_region)(Outbuf *ob, RGBA_Image *update, int x, int y, int w, int h);
   void (*outbuf_free_region_for_update)(Outbuf *ob, RGBA_Image *update);
   void (*outbuf_flush)(Outbuf *ob);
   void (*outbuf_idle_flush)(Outbuf *ob);
   Eina_Bool (*outbuf_alpha_get)(Outbuf *ob);
#ifdef EVAS_FRAME_QUEUING
   void (*outbuf_set_priv)(Outbuf *ob, void *cur, void *prev);
#endif
};

/* prototypes we will use here */
static void *_best_visual_get(int backend, void *connection, int screen);
static unsigned int _best_colormap_get(int backend, void *connection, int screen);
static int _best_depth_get(int backend, void *connection, int screen);

static void *eng_info(Evas *e);
static void eng_info_free(Evas *e, void *info);
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

#ifdef BUILD_ENGINE_SOFTWARE_XLIB
static void *
_output_xlib_setup(int w, int h, int rot, Display *disp, Drawable draw,
                   Visual *vis, Colormap cmap, int depth, int debug,
                   int grayscale, int max_colors, Pixmap mask,
                   int shape_dither, int destination_alpha)
{
   Render_Engine *re;
//   int status;
//   char *type = NULL;
//   XrmValue val;

   if (!(re = calloc(1, sizeof(Render_Engine)))) return NULL;

   evas_software_xlib_x_init();
   evas_software_xlib_x_color_init();
   evas_software_xlib_outbuf_init();
/*
   re->xr.dpi = 75000; // dpy * 1000

   status = xrdb_user_query("Xft.dpi", "Xft.Dpi", &type, &val);
   if ((!status) || (!type))
     {
        if (!re->xrdb) re->xrdb = XrmGetDatabase(disp);
        if (re->xrdb)
          status = XrmGetResource(re->xrdb,
                                  "Xft.dpi", "Xft.Dpi", &type, &val);
     }

   if ((status) && (type))
     {
        if (!strcmp(type, "String"))
          {
             const char *str, *dp;
                  
             str = val.addr;
             dp = strchr(str, '.');
             if (!dp) dp = strchr(str, ',');

             if (dp)
               {
                  int subdpi, len, i;
                  char *buf;
                       
                  buf = alloca(dp - str + 1);
                  strncpy(buf, str, dp - str);
                  buf[dp - str] = 0;
                  len = strlen(dp + 1);
                  subdpi = atoi(dp + 1);

                  if (len < 3)
                    {
                       for (i = len; i < 3; i++) 
                         subdpi *= 10;
                    }
                  else if (len > 3)
                    {
                       for (i = len; i > 3; i--) 
                         subdpi /= 10;
                    }
                  re->xr.dpi = atoi(buf) * 1000;
               }
             else
               re->xr.dpi = atoi(str) * 1000;
             evas_common_font_dpi_set(re->xr.dpi / 1000);
          }
     }
 */
   re->ob = 
     evas_software_xlib_outbuf_setup_x(w, h, rot, OUTBUF_DEPTH_INHERIT, disp, 
                                       draw, vis, cmap, depth, grayscale,
                                       max_colors, mask, shape_dither,
                                       destination_alpha);
   if (!re->ob)
     {
	free(re);
	return NULL;
     }

   /* for updates return 1 big buffer, but only use portions of it, also cache
    * it and keepit around until an idle_flush */

   /* disable for now - i am hunting down why some expedite tests are slower,
    * as well as shaped stuff is broken and probable non-32bpp is broken as
    * convert funcs dont do the right thing
    *
    */
//   re->ob->onebuf = 1;

   evas_software_xlib_outbuf_debug_set(re->ob, debug);
   re->tb = evas_common_tilebuf_new(w, h);
   if (!re->tb)
     {
	evas_software_xlib_outbuf_free(re->ob);
	free(re);
	return NULL;
     }

   /* in preliminary tests 16x16 gave highest framerates */
   evas_common_tilebuf_set_tile_size(re->tb, TILESIZE, TILESIZE);
   return re;
}
#endif

#ifdef BUILD_ENGINE_SOFTWARE_XCB
static void *
_output_xcb_setup(int w, int h, int rot, xcb_connection_t *conn, 
                  xcb_screen_t *screen, xcb_drawable_t draw, 
                  xcb_visualtype_t *vis, xcb_colormap_t cmap, int depth,
                  int debug, int grayscale, int max_colors, xcb_drawable_t mask,
                  int shape_dither, int destination_alpha)
{
   Render_Engine *re;
//   int v = 0;

   if (!(re = calloc(1, sizeof(Render_Engine)))) return NULL;

   evas_software_xcb_init();
   evas_software_xcb_color_init();
   evas_software_xcb_outbuf_init();
/*
   // FIXME: re->xrdb
   _evas_xcb_xdefaults_init();
   v = _evas_xcb_xdefaults_int_get("Xft", "dpi");
   _evas_xcb_xdefaults_shutdown();
   if (v) re->xr.dpi = (v * 1000);
   else re->xr.dpi = 75000; // dpy * 1000

   evas_common_font_dpi_set(re->xr.dpi / 1000);
 */
   re->ob = 
     evas_software_xcb_outbuf_setup(w, h, rot, OUTBUF_DEPTH_INHERIT, conn, 
                                    screen, draw, vis, cmap, depth,
                                    grayscale, max_colors, mask, 
                                    shape_dither, destination_alpha);
   if (!re->ob)
     {
	free(re);
	return NULL;
     }

   /* for updates return 1 big buffer, but only use portions of it, also cache 
    * it and keepit around until an idle_flush */

   /* disable for now - i am hunting down why some expedite tests are slower,
    * as well as shaped stuff is broken and probable non-32bpp is broken as
    * convert funcs dont do the right thing
    *
    */
//   re->ob->onebuf = 1;

   evas_software_xcb_outbuf_debug_set(re->ob, debug);

   re->tb = evas_common_tilebuf_new(w, h);
   if (!re->tb)
     {
	evas_software_xcb_outbuf_free(re->ob);
	free(re);
	return NULL;
     }

   /* in preliminary tests 16x16 gave highest framerates */
   evas_common_tilebuf_set_tile_size(re->tb, TILESIZE, TILESIZE);
   return re;
}
#endif

static void *
_best_visual_get(int backend, void *connection, int screen)
{
   if (!connection) return NULL;

#ifdef BUILD_ENGINE_SOFTWARE_XLIB
   if (backend == EVAS_ENGINE_INFO_SOFTWARE_X11_BACKEND_XLIB)
     return DefaultVisual((Display *)connection, screen);
#endif

#ifdef BUILD_ENGINE_SOFTWARE_XCB
   if (backend == EVAS_ENGINE_INFO_SOFTWARE_X11_BACKEND_XCB)
     {
        xcb_screen_iterator_t iter_screen;
        xcb_depth_iterator_t iter_depth;
        xcb_screen_t *s = NULL;

        iter_screen = 
          xcb_setup_roots_iterator(xcb_get_setup((xcb_connection_t *)connection));
        for (; iter_screen.rem; --screen, xcb_screen_next(&iter_screen))
          if (screen == 0)
            {
               s = iter_screen.data;
               break;
            }

        iter_depth = xcb_screen_allowed_depths_iterator(s);
        for (; iter_depth.rem; xcb_depth_next(&iter_depth))
          {
             xcb_visualtype_iterator_t iter_vis;

             iter_vis = xcb_depth_visuals_iterator(iter_depth.data);
             for (; iter_vis.rem; xcb_visualtype_next(&iter_vis))
               {
                  if (s->root_visual == iter_vis.data->visual_id)
                    return iter_vis.data;
               }
          }
     }
#endif

   return NULL;
}

static unsigned int
_best_colormap_get(int backend, void *connection, int screen)
{
   if (!connection) return 0;

#ifdef BUILD_ENGINE_SOFTWARE_XLIB
   if (backend == EVAS_ENGINE_INFO_SOFTWARE_X11_BACKEND_XLIB)
     return DefaultColormap((Display *)connection, screen);
#endif

#ifdef BUILD_ENGINE_SOFTWARE_XCB
   if (backend == EVAS_ENGINE_INFO_SOFTWARE_X11_BACKEND_XCB)
     {
        xcb_screen_iterator_t iter_screen;
        xcb_screen_t *s = NULL;

        iter_screen = 
          xcb_setup_roots_iterator(xcb_get_setup((xcb_connection_t *)connection));
        for (; iter_screen.rem; --screen, xcb_screen_next(&iter_screen))
          if (screen == 0)
            {
               s = iter_screen.data;
               break;
            }

        return s->default_colormap;
     }
#endif

   return 0;
}

static int
_best_depth_get(int backend, void *connection, int screen)
{
   if (!connection) return 0;

#ifdef BUILD_ENGINE_SOFTWARE_XLIB
   if (backend == EVAS_ENGINE_INFO_SOFTWARE_X11_BACKEND_XLIB)
     return DefaultDepth((Display *)connection, screen);
#endif

#ifdef BUILD_ENGINE_SOFTWARE_XCB
   if (backend == EVAS_ENGINE_INFO_SOFTWARE_X11_BACKEND_XCB)
     {
        xcb_screen_iterator_t iter_screen;
        xcb_screen_t *s = NULL;

        iter_screen = 
          xcb_setup_roots_iterator(xcb_get_setup((xcb_connection_t *)connection));
        for (; iter_screen.rem; --screen, xcb_screen_next(&iter_screen))
          if (screen == 0)
            {
               s = iter_screen.data;
               break;
            }

        return s->root_depth;
     }
#endif

   return 0;
}

/* engine api this module provides */
static void *
eng_info(Evas *e __UNUSED__)
{
   Evas_Engine_Info_Software_X11 *info;

   if (!(info = calloc(1, sizeof(Evas_Engine_Info_Software_X11))))
     return NULL;

   info->magic.magic = rand();
   info->info.debug = 0;
   info->info.alloc_grayscale = 0;
   info->info.alloc_colors_max = 216;
   info->func.best_visual_get = _best_visual_get;
   info->func.best_colormap_get = _best_colormap_get;
   info->func.best_depth_get = _best_depth_get;
   info->render_mode = EVAS_RENDER_MODE_BLOCKING;
   return info;
}

static void
eng_info_free(Evas *e __UNUSED__, void *info)
{
   Evas_Engine_Info_Software_X11 *in;

   in = (Evas_Engine_Info_Software_X11 *)info;
   free(in);
}

static int
eng_setup(Evas *e, void *in)
{
   Evas_Engine_Info_Software_X11 *info;
   Render_Engine *re = NULL;

   info = (Evas_Engine_Info_Software_X11 *)in;
   if (!e->engine.data.output)
     {
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

#ifdef BUILD_ENGINE_SOFTWARE_XLIB
        if (info->info.backend == EVAS_ENGINE_INFO_SOFTWARE_X11_BACKEND_XLIB)
          {
             re = _output_xlib_setup(e->output.w, e->output.h,
                                     info->info.rotation, info->info.connection,
                                     info->info.drawable, info->info.visual,
                                     info->info.colormap,
                                     info->info.depth, info->info.debug,
                                     info->info.alloc_grayscale,
                                     info->info.alloc_colors_max,
                                     info->info.mask, info->info.shape_dither,
                                     info->info.destination_alpha);

             re->outbuf_free = evas_software_xlib_outbuf_free;
             re->outbuf_reconfigure = evas_software_xlib_outbuf_reconfigure;
             re->outbuf_get_rot = evas_software_xlib_outbuf_get_rot;
             re->outbuf_new_region_for_update = 
               evas_software_xlib_outbuf_new_region_for_update;
             re->outbuf_push_updated_region = 
               evas_software_xlib_outbuf_push_updated_region;
             re->outbuf_free_region_for_update = 
               evas_software_xlib_outbuf_free_region_for_update;
             re->outbuf_flush = evas_software_xlib_outbuf_flush;
             re->outbuf_idle_flush = evas_software_xlib_outbuf_idle_flush;
	     re->outbuf_alpha_get = evas_software_xlib_outbuf_alpha_get;
# ifdef EVAS_FRAME_QUEUING
             re->outbuf_set_priv = evas_software_xlib_outbuf_set_priv;
             re->render_mode = info->render_mode;
# endif
          }
#endif

#ifdef BUILD_ENGINE_SOFTWARE_XCB
        if (info->info.backend == EVAS_ENGINE_INFO_SOFTWARE_X11_BACKEND_XCB)
          {
             re = _output_xcb_setup(e->output.w, e->output.h,
                                    info->info.rotation, info->info.connection, 
                                    info->info.screen, info->info.drawable, 
                                    info->info.visual, info->info.colormap, 
                                    info->info.depth, info->info.debug, 
                                    info->info.alloc_grayscale,
                                    info->info.alloc_colors_max,
                                    info->info.mask, info->info.shape_dither,
                                    info->info.destination_alpha);

             re->outbuf_free = evas_software_xcb_outbuf_free;
             re->outbuf_reconfigure = evas_software_xcb_outbuf_reconfigure;
             re->outbuf_get_rot = evas_software_xcb_outbuf_rotation_get;
             re->outbuf_new_region_for_update = 
               evas_software_xcb_outbuf_new_region_for_update;
             re->outbuf_push_updated_region = 
               evas_software_xcb_outbuf_push_updated_region;
             re->outbuf_free_region_for_update = 
               evas_software_xcb_outbuf_free_region_for_update;
             re->outbuf_flush = evas_software_xcb_outbuf_flush;
             re->outbuf_idle_flush = evas_software_xcb_outbuf_idle_flush;
	     re->outbuf_alpha_get = evas_software_xcb_outbuf_alpha_get;
# ifdef EVAS_FRAME_QUEUING
             re->outbuf_set_priv = evas_software_xcb_outbuf_priv_set;
             re->render_mode = info->render_mode;
# endif
          }
#endif

        e->engine.data.output = re;
     }
   else
     {
	int ponebuf = 0;

#ifdef EVAS_FRAME_QUEUING
        evas_common_frameq_flush();
#endif
	re = e->engine.data.output;
	ponebuf = re->ob->onebuf;

#ifdef BUILD_ENGINE_SOFTWARE_XLIB
        if (info->info.backend == EVAS_ENGINE_INFO_SOFTWARE_X11_BACKEND_XLIB)
          {
             evas_software_xlib_outbuf_free(re->ob);
             re->ob = 
               evas_software_xlib_outbuf_setup_x(e->output.w, e->output.h,
                                                 info->info.rotation,
                                                 OUTBUF_DEPTH_INHERIT,
                                                 info->info.connection,
                                                 info->info.drawable,
                                                 info->info.visual,
                                                 info->info.colormap,
                                                 info->info.depth,
                                                 info->info.alloc_grayscale,
                                                 info->info.alloc_colors_max,
                                                 info->info.mask,
                                                 info->info.shape_dither,
                                                 info->info.destination_alpha);

             evas_software_xlib_outbuf_debug_set(re->ob, info->info.debug);
# ifdef EVAS_FRAME_QUEUING
             re->render_mode = info->render_mode;
# endif
          }
#endif

#ifdef BUILD_ENGINE_SOFTWARE_XCB
        if (info->info.backend == EVAS_ENGINE_INFO_SOFTWARE_X11_BACKEND_XCB)
          {
             evas_software_xcb_outbuf_free(re->ob);
             re->ob = 
               evas_software_xcb_outbuf_setup(e->output.w, e->output.h,
                                              info->info.rotation,
                                              OUTBUF_DEPTH_INHERIT,
                                              info->info.connection,
                                              info->info.screen,
                                              info->info.drawable,
                                              info->info.visual,
                                              info->info.colormap,
                                              info->info.depth,
                                              info->info.alloc_grayscale,
                                              info->info.alloc_colors_max,
                                              info->info.mask,
                                              info->info.shape_dither,
                                              info->info.destination_alpha);

             evas_software_xcb_outbuf_debug_set(re->ob, info->info.debug);
#ifdef EVAS_FRAME_QUEUING
             re->render_mode = info->render_mode;
#endif
          }
#endif
	re->ob->onebuf = ponebuf;
     }
   if (!e->engine.data.output) return 0;
   if (!e->engine.data.context) 
     {
        e->engine.data.context = 
          e->engine.func->context_new(e->engine.data.output);
     }

   re = e->engine.data.output;

   return 1;
}

static void
eng_output_free(void *data)
{
   Render_Engine *re;

#ifdef BUILD_ENGINE_SOFTWARE_XLIB
// NOTE: XrmGetDatabase() result is shared per connection, do not free it.
//   if (re->xrdb) XrmDestroyDatabase(re->xrdb);
#endif

   if ((re = (Render_Engine *)data))
     {
        re->outbuf_free(re->ob);
        evas_common_tilebuf_free(re->tb);
        if (re->rects) evas_common_tilebuf_free_render_rects(re->rects);
        free(re);
     }

   evas_common_font_shutdown();
   evas_common_image_shutdown();
}

static void
eng_output_resize(void *data, int w, int h)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   re->outbuf_reconfigure(re->ob, w, h, re->outbuf_get_rot(re->ob),
                          OUTBUF_DEPTH_INHERIT);
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

   surface = 
     re->outbuf_new_region_for_update(re->ob, ux, uy, uw, uh, cx, cy, cw, ch);

   *x = ux; *y = uy; *w = uw; *h = uh;
   return surface;
}

static void
eng_output_redraws_next_update_push(void *data, void *surface, int x, int y, int w, int h)
{
   Render_Engine *re;
#ifdef EVAS_FRAME_QUEUING
   Evas_Surface *e_surface;
#endif

   re = (Render_Engine *)data;
#if defined(BUILD_PIPE_RENDER) && !defined(EVAS_FRAME_QUEUING)
   evas_common_pipe_map_begin(surface);
#endif /* BUILD_PIPE_RENDER  && !EVAS_FRAME_QUEUING*/

#ifdef EVAS_FRAME_QUEUING
   if (re->render_mode == EVAS_RENDER_MODE_NONBLOCKING)
     {
        /* create a new frame if this is the first surface of this frame */
        evas_common_frameq_prepare_frame();
        /* add surface into the frame */
        e_surface = evas_common_frameq_new_surface(surface, x, y, w, h);
        evas_common_frameq_add_surface(e_surface);
        return;
     }
#endif

   re->outbuf_push_updated_region(re->ob, surface, x, y, w, h);
   re->outbuf_free_region_for_update(re->ob, surface);
   evas_common_cpu_end_opt();
}

#ifdef EVAS_FRAME_QUEUING
static void *
eng_image_map_surface_new(void *data , int w, int h, int alpha)
{
   void *surface;
   DATA32 *pixels;
   Render_Engine *re;
   Evas_Surface *e_surface;

   re = (Render_Engine *)data;

   surface = 
     evas_cache_image_copied_data(evas_common_image_cache_get(), w, h, NULL, 
                                  alpha, EVAS_COLORSPACE_ARGB8888);
   pixels = evas_cache_image_pixels(surface);

   if (re->render_mode == EVAS_RENDER_MODE_NONBLOCKING)
     {
        /* create a new frame if this is the first surface of this frame */
        evas_common_frameq_prepare_frame();

        /* add surface into the frame */
        e_surface = evas_common_frameq_new_surface(surface, 0, 0, w, h);

        /* this surface is not going to be pushed to screen */
        e_surface->dontpush = 1;
        evas_common_frameq_add_surface(e_surface);
     }
   return surface;
}

static void
eng_output_frameq_redraws_next_update_push(void *data, void *surface, int x, int y, int w, int h)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   re->outbuf_push_updated_region(re->ob, surface, x, y, w, h);
   re->outbuf_free_region_for_update(re->ob, surface);
   evas_common_cpu_end_opt();
}

static void
eng_output_frameq_flush(void *data)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   re->outbuf_flush(re->ob);
}

static void
eng_output_frameq_set_priv(void *data, void *cur, void *prev)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   re->outbuf_set_priv(re->ob, cur, prev);
}
#endif

static void
eng_output_flush(void *data)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
#ifdef EVAS_FRAME_QUEUING
   if (re->render_mode == EVAS_RENDER_MODE_NONBLOCKING)
     {
        evas_common_frameq_set_frame_data(data,
                                          eng_output_frameq_redraws_next_update_push,
                                          eng_output_frameq_flush,
                                          eng_output_frameq_set_priv);
        evas_common_frameq_ready_frame();
        evas_common_frameq_begin();
     } 
   else
#endif
     re->outbuf_flush(re->ob);
}

static void
eng_output_idle_flush(void *data)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   re->outbuf_idle_flush(re->ob);
}

static Eina_Bool
eng_canvas_alpha_get(void *data, void *context __UNUSED__)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   return (re->ob->priv.destination_alpha) || (re->outbuf_alpha_get(re->ob));
}


/* module advertising code */
static int
module_open(Evas_Module *em)
{
#ifdef BUILD_ENGINE_SOFTWARE_XLIB
   static Eina_Bool xrm_inited = EINA_FALSE;

   if (!xrm_inited)
     {
	xrm_inited = EINA_TRUE;
	XrmInitialize();
     }
#endif

   if (!em) return 0;

   /* get whatever engine module we inherit from */
   if (!_evas_module_engine_inherit(&pfunc, "software_generic")) return 0;

   _evas_engine_soft_x11_log_dom = 
     eina_log_domain_register("evas-software_x11", EVAS_DEFAULT_LOG_COLOR);

   if (_evas_engine_soft_x11_log_dom < 0)
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
#ifdef EVAS_FRAME_QUEUING
   ORD(image_map_surface_new);
#endif

   /* now advertise out own api */
   em->functions = (void *)(&func);
   return 1;
}

static void
module_close(Evas_Module *em __UNUSED__)
{
  eina_log_domain_unregister(_evas_engine_soft_x11_log_dom);
#ifdef BUILD_ENGINE_SOFTWARE_XLIB
/*   
  if (xrdb_user.db)
    {
       XrmDestroyDatabase(xrdb_user.db);
       xrdb_user.last_stat = 0;
       xrdb_user.last_mtime = 0;
       xrdb_user.db = NULL;
    }
 */
#endif
}

static Evas_Module_Api evas_modapi =
{
   EVAS_MODULE_API_VERSION, "software_x11", "none",
   {
     module_open,
     module_close
   }
};

EVAS_MODULE_DEFINE(EVAS_MODULE_TYPE_ENGINE, engine, software_x11);

#ifndef EVAS_STATIC_BUILD_SOFTWARE_X11
EVAS_EINA_MODULE_DEFINE(engine, software_x11);
#endif
