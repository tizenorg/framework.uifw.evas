#ifndef EVAS_ENGINE_H
#define EVAS_ENGINE_H

#include "config.h"
#include "evas_common.h"
#include "evas_private.h"
#include "evas_gl_common.h"
#include "Evas.h"
#include "Evas_Engine_GL_X11.h"

#define GL_GLEXT_PROTOTYPES

#ifdef GL_GLES
# define SUPPORT_X11 1
# include <EGL/egl.h>
# include <GLES2/gl2.h>
# include <GLES2/gl2ext.h>
# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include <X11/Xutil.h>
# include <X11/extensions/Xrender.h>
# include <X11/Xresource.h> // xres - dpi
#else
# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include <X11/Xutil.h>
# include <X11/extensions/Xrender.h>
# include <X11/Xresource.h> // xres - dpi
# include <GL/gl.h>
# include <GL/glext.h>
# include <GL/glx.h>
#endif

#define EVAS_GL_NO_GL_H_CHECK 1
#include "Evas_GL.h"

extern int _evas_engine_GL_X11_log_dom ;
#ifdef ERR
# undef ERR
#endif
#define ERR(...) EINA_LOG_DOM_ERR(_evas_engine_GL_X11_log_dom, __VA_ARGS__)

#ifdef DBG
# undef DBG
#endif
#define DBG(...) EINA_LOG_DOM_DBG(_evas_engine_GL_X11_log_dom, __VA_ARGS__)

#ifdef INF
# undef INF
#endif
#define INF(...) EINA_LOG_DOM_INFO(_evas_engine_GL_X11_log_dom, __VA_ARGS__)

#ifdef WRN
# undef WRN
#endif
#define WRN(...) EINA_LOG_DOM_WARN(_evas_engine_GL_X11_log_dom, __VA_ARGS__)

#ifdef CRIT
# undef CRIT
#endif
#define CRIT(...) EINA_LOG_DOM_CRIT(_evas_engine_GL_X11_log_dom, __VA_ARGS__)

typedef struct _Evas_GL_X11_Window Evas_GL_X11_Window;

struct _Evas_GL_X11_Window
{
   Display         *disp;
   Drawable         win;
   Drawable         win_back;
   int              w, h;
   int              screen;
   XVisualInfo     *visualinfo;
   Visual          *visual;
   Colormap         colormap;
   int              depth;
   int              alpha;
   int              rot;
   int              offscreen;
   Evas_Engine_GL_Context *gl_context;
   struct {
      int           drew : 1;
   } draw;
#ifdef GL_GLES
   EGLContext       egl_context[1];
   EGLSurface       egl_surface[2];
   EGLConfig        egl_config;
   EGLDisplay       egl_disp;
#else
   GLXContext       context;
   GLXWindow        glxwin;
#endif

   struct {
      unsigned char depth_buffer_size;
      unsigned char stencil_buffer_size;
#ifndef GL_GLES
      unsigned char loose_binding : 1;
#endif
   } detected;
   unsigned char    surf : 1;
};

Evas_GL_X11_Window *eng_window_new(Display *disp, Drawable win, int screen,
                                   Visual *vis, Colormap cmap,
                                   int depth, int w, int h, int indirect,
                                   int alpha, int rot, Drawable offscreen);
void      eng_window_free(Evas_GL_X11_Window *gw);
void      eng_window_use(Evas_GL_X11_Window *gw);
void      eng_window_unsurf(Evas_GL_X11_Window *gw, Eina_Bool pre_rotation_resurf);
void      eng_window_resurf(Evas_GL_X11_Window *gw);

Visual   *eng_best_visual_get(Evas_Engine_Info_GL_X11 *einfo);
Colormap  eng_best_colormap_get(Evas_Engine_Info_GL_X11 *einfo);
int       eng_best_depth_get(Evas_Engine_Info_GL_X11 *einfo);
int       eng_native_config_check(Display *disp, void *cfg, int target, void *pixmap, int *w, int *h);
Eina_Bool eng_init(void);

#endif
