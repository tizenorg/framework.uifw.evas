#include "evas_common.h" /* Also includes international specific stuff */
#include "evas_engine.h"
#include "evas_gl_core_private.h"

//#define TIMDBG 1
#ifdef TIMDBG
# include <sys/time.h>
# include <unistd.h>
#endif

#ifdef HAVE_DLSYM
# include <dlfcn.h>      /* dlopen,dlclose,etc */
#else
# error gl_x11 should not get compiled if dlsym is not found on the system!
#endif

#define EVAS_GL_UPDATE_TILE_SIZE 16

#define _ATOM_SET_CARD32( win, atom, p_val, cnt)                               \
      XChangeProperty(_evas_x_disp, win, atom, XA_CARDINAL, 32, PropModeReplace, \
      (unsigned char *)p_val, cnt)


/**
 * @typedef Evas_X_Window_Rotation_Transform_Hint
 * @brief Enumeration of the different transform hints of the window of Ecore_X.
 *
 * @remarks A value of @c 3 means, HINT_0 goes to HINT_180(0x3).
 *          It is same as HINT_0 goes to HINT_FLIP_H(0x1) and it goes to HINT_FLIP_V(0x2).( 0x1 + 0x2 = 0x3 )
 *
 * @remarks A value of @c 7 means, HINT_0 goes to HINT_270(0x7).
 *          It is same as HINT_0 goes to HINT_90(0x4) and it goes to HINT_FLIP_H(0x1) and HINT_FLIP_V(0x2).( 0x4 + 0x1 + 0x2 = 0x7 )
 */
typedef enum _Evas_X_Window_Rotation_Transform_Hint
{
   EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_0 = 0,
   EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_FLIP_H = 0x1,  /**< Rotate source image along the horizontal axis */
   EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_FLIP_V = 0x2,  /**< Rotate source image along the vertical axis */
   EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_180 = 0x3,       /**< Rotate source image 180 degrees clockwise */
   EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_90 = 0x4,         /**< Rotate source image 90 degrees clockwise */
   EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_270 = 0x7,        /**< Rotate source image 270 degrees clockwise */
   EVAS_X_WINDOW_PREROTATION_QUERY = 0x1111,            /**< Used to Query DDK prerotation support */
   EVAS_X_WINDOW_PREROTATION_SUPPORT = 0x2222           /**< Value set by DDK if prerotation is supported */
} Evas_X_Window_Rotation_Transform_Hint;


enum {
   MERGE_BOUNDING,
   MERGE_FULL
};

static int partial_render_debug = -1;
static int partial_rect_union_mode = -1;
static int swap_buffer_debug_mode = -1;
static int swap_buffer_debug = 0;
Display *_evas_x_disp = NULL;

enum {
   MODE_FULL,
   MODE_COPY,
   MODE_DOUBLE,
   MODE_TRIPLE,
   MODE_QUADRUPLE
};

typedef struct _Render_Engine               Render_Engine;

struct _Render_Engine
{
   Tilebuf_Rect            *rects;
   Tilebuf_Rect            *rects_prev[4];
   Eina_Inlist             *cur_rect;

   Evas_GL_X11_Window      *win;
   Evas_Engine_Info_GL_X11 *info;
   Evas                    *evas;
   Tilebuf                 *tb;
   int                      end;
   int                      mode;
   int                      w, h;
   int                      vsync;
   int                      lost_back;
   int                      prev_age;
   int                      frame_cnt;
   unsigned int             atom;

   Eina_Bool context_current : 1;
   Eina_Bool context_optimize : 1;
   Eina_Bool is_compositor : 1;
   Eina_Bool is_prerotate : 1;

   struct {
      Evas_Object_Image_Pixels_Get_Cb  get_pixels;
      void                            *get_pixels_data;
      Evas_Object                     *obj;
      int                              clip;
      int                              x, y, w, h;
   } func;
};

static int initted = 0;
static int gl_wins = 0;
static int extn_have_buffer_age = 1;
static int prev_extn_have_buffer_age = 1;
#ifdef GL_GLES
static int extn_have_y_inverted = 1;
#endif
static int evgl_initted = 0;

typedef void            (*_eng_fn) (void);
typedef _eng_fn         (*glsym_func_eng_fn) ();
typedef void            (*glsym_func_void) ();
typedef void           *(*glsym_func_void_ptr) ();
typedef int             (*glsym_func_int) ();
typedef unsigned int    (*glsym_func_uint) ();
typedef const char     *(*glsym_func_const_char_ptr) ();

#ifdef GL_GLES

#ifndef EGL_NATIVE_PIXMAP_KHR
# define EGL_NATIVE_PIXMAP_KHR 0x30b0
#endif
#ifndef EGL_BUFFER_AGE_EXT
# define EGL_BUFFER_AGE_EXT 0x313d
#endif
#ifndef EGL_Y_INVERTED_NOK
# define EGL_Y_INVERTED_NOK 0x307F
#endif

_eng_fn  (*glsym_eglGetProcAddress)            (const char *a) = NULL;
void    *(*glsym_eglCreateImage)               (EGLDisplay a, EGLContext b, EGLenum c, EGLClientBuffer d, const int *e) = NULL;
void     (*glsym_eglDestroyImage)              (EGLDisplay a, void *b) = NULL;
void     (*glsym_glEGLImageTargetTexture2DOES) (int a, void *b)  = NULL;
void          *(*glsym_eglMapImageSEC)         (void *a, void *b, int c, int d) = NULL;
unsigned int   (*glsym_eglUnmapImageSEC)       (void *a, void *b, int c) = NULL;
void           (*glsym_eglSwapBuffersWithDamage) (EGLDisplay a, void *b, const EGLint *d, EGLint c) = NULL;

#else

#ifndef GLX_BACK_BUFFER_AGE_EXT
# define GLX_BACK_BUFFER_AGE_EXT 0x20f4
#endif

typedef XID     (*glsym_func_xid) ();

_eng_fn  (*glsym_glXGetProcAddress)  (const char *a) = NULL;
void     (*glsym_glXBindTexImage)    (Display *a, GLXDrawable b, int c, int *d) = NULL;
void     (*glsym_glXReleaseTexImage) (Display *a, GLXDrawable b, int c) = NULL;
int      (*glsym_glXGetVideoSync)    (unsigned int *a) = NULL;
int      (*glsym_glXWaitVideoSync)   (int a, int b, unsigned int *c) = NULL;
XID      (*glsym_glXCreatePixmap)    (Display *a, void *b, Pixmap c, const int *d) = NULL;
void     (*glsym_glXDestroyPixmap)   (Display *a, XID b) = NULL;
void     (*glsym_glXQueryDrawable)   (Display *a, XID b, int c, unsigned int *d) = NULL;
int      (*glsym_glXSwapIntervalSGI) (int a) = NULL;
void     (*glsym_glXSwapIntervalEXT) (Display *s, GLXDrawable b, int c) = NULL;
void     (*glsym_glXReleaseBuffersMESA)   (Display *a, XID b) = NULL;
const char *(*glsym_glXQueryExtensionsString) (Display *dpy, int screen) = NULL;

#endif

#ifdef TIMDBG
static double
gettime(void)
{
   struct timeval      timev;

   gettimeofday(&timev, NULL);
   return (double)timev.tv_sec + (((double)timev.tv_usec) / 1000000);
}

static void
measure(int end, const char *name)
{
   FILE *fs;
   static unsigned long user = 0, kern = 0, user2 = 0, kern2 = 0;
   static double t = 0.0, t2 = 0.0;
   unsigned long u = 0, k = 0;

   fs = fopen("/proc/self/stat", "rb");
   if (fs) {
      fscanf(fs, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s "
             "%lu %lu %*s", &u, &k);
      fclose(fs);
   }
   if (end)
     {
        long hz;

        t2 = gettime();
        user2 = u;
        kern2 = k;
        hz = sysconf(_SC_CLK_TCK);
        fprintf(stderr, "(%8lu %8lu) k=%4lu u=%4lu == tot=%4lu@%4li in=%3.5f < %s\n",
                user, kern, kern2 - kern, user2 - user,
                (kern2 - kern) + (user2 - user), hz, t2 - t, name);
     }
   else
     {
        user = u;
        kern = k;
        t = gettime();
     }
}
#endif

static int
_check_compositor(void)
{
#define COMPOSITOR "enlightenment"
   FILE *f;
   char *slash;
   static int initialized = 0;
   static unsigned int is_compositor = 0;
   char app_name[128];

   if (initialized) return is_compositor;

   /* get the application name */
   f = fopen("/proc/self/cmdline", "r");
   if (!f)
     {
        perror(" file open error : ");
        return -1;
     }
   memset(app_name, 0x00, sizeof(app_name));
   if (fgets(app_name, 100, f) == NULL)
     {
        perror(" fgets open error : ");
        fclose(f);
        return -1;
     }
   fclose(f);
   if ((slash=strrchr(app_name, '/')) != NULL)
     {
        memmove(app_name, slash+1, strlen(slash));
     }
   initialized = 1;

   if(app_name && !strncmp(app_name, COMPOSITOR, strlen(COMPOSITOR)))
      is_compositor = 1;

#undef COMPOSITOR
   return is_compositor;
}

//----------------------------------------------------------//
// NEW_EVAS_GL Engine Functions
static void *
evgl_eng_display_get(void *data)
{
   Render_Engine *re = (Render_Engine *)data;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        return 0;
     }

#ifdef GL_GLES
   if (re->win)
      return (void*)re->win->egl_disp;
#else
   if (re->info)
      return (void*)re->info->info.display;
#endif
   else
      return NULL;
}

static void *
evgl_eng_evas_surface_get(void *data)
{
   Render_Engine *re = (Render_Engine *)data;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        return 0;
     }

#ifdef GL_GLES
   if (re->win)
      return (void*)re->win->egl_surface[re->win->offscreen];
#else
   if (re->win)
      return (void*)re->win->win;
#endif
   else
      return NULL;
}

static int
evgl_eng_make_current(void *data, void *surface, void *context, int flush)
{
   Render_Engine *re = (Render_Engine *)data;
   int ret = 0;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return 0;
     }


#ifdef GL_GLES
   EGLContext ctx = (EGLContext)context;
   EGLSurface sfc = (EGLSurface)surface;
   EGLDisplay dpy = re->win->egl_disp; //eglGetCurrentDisplay();

   if ((!context) && (!surface))
     {
        ret = eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (!ret)
          {
             int err = eglGetError();
             _evgl_error_set(err - EGL_SUCCESS);
             ERR("eglMakeCurrent() failed! Error Code=%#x", err);
             return 0;
          }
        return 1;
     }

   if ((eglGetCurrentDisplay() != dpy) ||
   	(eglGetCurrentContext() != ctx) ||
       (eglGetCurrentSurface(EGL_READ) != sfc) ||
       (eglGetCurrentSurface(EGL_DRAW) != sfc) )
     {

        //!!!! Does it need to be flushed with it's set to NULL above??
        // Flush remainder of what's in Evas' pipeline
        if (flush) eng_window_use(NULL);

        // Do a make current
        ret = eglMakeCurrent(dpy, sfc, sfc, ctx);

        if (!ret)
          {
             int err = eglGetError();
             _evgl_error_set(err - EGL_SUCCESS);
             ERR("eglMakeCurrent() failed! Error Code=%#x", err);
             return 0;
          }
     }

   return 1;
#else
   GLXContext ctx = (GLXContext)context;
   Window     sfc = (Window)surface;

   if ((!context) && (!surface))
     {
        ret = glXMakeCurrent(re->info->info.display, None, NULL);
        if (!ret)
          {
             ERR("glXMakeCurrent() failed!");
             _evgl_error_set(EVAS_GL_BAD_DISPLAY);
             return 0;
          }
        return 1;
     }


   if ((glXGetCurrentContext() != ctx))
     {
        //!!!! Does it need to be flushed with it's set to NULL above??
        // Flush remainder of what's in Evas' pipeline
        if (flush) eng_window_use(NULL);

        // Do a make current
        ret = glXMakeCurrent(re->info->info.display, sfc, ctx);

        if (!ret)
          {
             ERR("glXMakeCurrent() failed. Ret: %d! Context: %p Surface: %p", ret, (void*)ctx, (void*)sfc);
             _evgl_error_set(EVAS_GL_BAD_DISPLAY);
             return 0;
          }
     }
   return 1;
#endif
}



static void *
evgl_eng_native_window_create(void *data)
{
   Render_Engine *re = (Render_Engine *)data;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return NULL;
     }

   XSetWindowAttributes attr;
   Window win;

   attr.backing_store = NotUseful;
   attr.override_redirect = True;
   attr.border_pixel = 0;
   attr.background_pixmap = None;
   attr.bit_gravity = NorthWestGravity;
   attr.win_gravity = NorthWestGravity;
   attr.save_under = False;
   attr.do_not_propagate_mask = NoEventMask;
   attr.event_mask = 0;

   win = XCreateWindow(re->info->info.display,
                       DefaultRootWindow(re->info->info.display),
                       0, 0, 2, 2, 0,
                       CopyFromParent, InputOutput, CopyFromParent,
                       CWBackingStore | CWOverrideRedirect |
                       CWBorderPixel | CWBackPixmap |
                       CWSaveUnder | CWDontPropagate |
                       CWEventMask | CWBitGravity |
                       CWWinGravity, &attr);
   if (!win)
     {
        ERR("Creating native X window failed.");
        _evgl_error_set(EVAS_GL_BAD_DISPLAY);
        return NULL;
     }

   return (void*)win;
}

static int
evgl_eng_native_window_destroy(void *data, void *native_window)
{
   Render_Engine *re = (Render_Engine *)data;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return 0;
     }

   if (!native_window)
     {
        ERR("Inavlid native surface.");
        _evgl_error_set(EVAS_GL_BAD_NATIVE_WINDOW);
        return 0;
     }

   XDestroyWindow(re->info->info.display, (Window)native_window);

   native_window = NULL;

   return 1;
}


// Theoretically, we wouldn't need this functoin if the surfaceless context
// is supported. But, until then...
static void *
evgl_eng_window_surface_create(void *data, void *native_window)
{
   Render_Engine *re = (Render_Engine *)data;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return NULL;
     }

#ifdef GL_GLES
   EGLSurface surface = EGL_NO_SURFACE;

   // Create resource surface for EGL
   surface = eglCreateWindowSurface(re->win->egl_disp,
                                    re->win->egl_config,
                                    (EGLNativeWindowType)native_window,
                                    NULL);
   if (!surface)
     {
        int err = eglGetError();
        _evgl_error_set(err - EGL_SUCCESS);
        ERR("Creating window surface failed. Error: %#x.", err);
        return NULL;
     }

   return (void*)surface;
#else
   /*
   // We don't need to create new one for GLX
   Window surface;

   surface = re->win->win;

   return (void *)surface;
   */
   return (void*)native_window;
#endif

}

static int
evgl_eng_window_surface_destroy(void *data, void *surface)
{
   Render_Engine *re = (Render_Engine *)data;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return 0;
     }

#ifdef GL_GLES
   if (!surface)
     {
        ERR("Invalid surface.");
        _evgl_error_set(EVAS_GL_BAD_SURFACE);
        return 0;
     }

   eglDestroySurface(re->win->egl_disp, (EGLSurface)surface);
#endif

   return 1;
}

static void *
evgl_eng_context_create(void *data, void *share_ctx,
                        Evas_GL_Context_Version version)
{
   Render_Engine *re = (Render_Engine *)data;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return NULL;
     }

#ifdef GL_GLES
   EGLContext context = EGL_NO_CONTEXT;
   int context_attrs[3];

   context_attrs[0] = EGL_CONTEXT_CLIENT_VERSION;
   context_attrs[1] = (int) version;
   context_attrs[2] = EGL_NONE;

   // Share context already assumes that it's sharing with evas' context
   if (share_ctx)
     {
        context = eglCreateContext(re->win->egl_disp,
                                   re->win->egl_config,
                                   (EGLContext)share_ctx,
                                   context_attrs);
     }
   else if (version == EVAS_GL_GLES_1_X)
     {
        // This context will be used for DR only
        context = eglCreateContext(re->win->egl_disp,
                                   re->win->egl_config,
                                   NULL, // no sharing between GLES1 and GLES2
                                   context_attrs);
     }
   else
     {
        context = eglCreateContext(re->win->egl_disp,
                                   re->win->egl_config,
                                   re->win->egl_context[0], // Evas' GL Context
                                   context_attrs);
     }

   if (!context)
     {
        int err = eglGetError();
        ERR("Engine Context Creations Failed. Error: %#x.", err);
        _evgl_error_set(err - EGL_SUCCESS);
        return NULL;
     }

   return (void*)context;
#else
   GLXContext context = NULL;
   (void) version;

   // Share context already assumes that it's sharing with evas' context
   if (share_ctx)
     {
        context = glXCreateContext(re->info->info.display,
                                   re->win->visualinfo,
                                   (GLXContext)share_ctx,
                                   1);
     }
   else
     {
        context = glXCreateContext(re->info->info.display,
                                   re->win->visualinfo,
                                   re->win->context,      // Evas' GL Context
                                   1);
     }

   if (!context)
     {
        ERR("Internal Resource Context Creations Failed.");
        if(!(re->info->info.display)) _evgl_error_set(EVAS_GL_BAD_DISPLAY);
        if(!(re->win)) _evgl_error_set(EVAS_GL_BAD_NATIVE_WINDOW);
        return NULL;
     }

   return (void*)context;
#endif

}

static int
evgl_eng_context_destroy(void *data, void *context)
{
   Render_Engine *re = (Render_Engine *)data;

   if ((!re) || (!context))
     {
        ERR("Invalid Render Input Data. Engine: %p, Context: %p", data, context);
        if (!re) _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        if (!context) _evgl_error_set(EVAS_GL_BAD_CONTEXT);
        return 0;
     }

#ifdef GL_GLES
   eglDestroyContext(re->win->egl_disp, (EGLContext)context);
#else
   glXDestroyContext(re->info->info.display, (GLXContext)context);
#endif

   return 1;
}

static const char *
evgl_eng_string_get(void *data)
{
   Render_Engine *re = (Render_Engine *)data;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return NULL;
     }

#ifdef GL_GLES
   // EGL Extensions
   return eglQueryString(re->win->egl_disp, EGL_EXTENSIONS);
#else
   return glXQueryExtensionsString(re->info->info.display,
                                   re->info->info.screen);
#endif
}

static void *
evgl_eng_proc_address_get(const char *name)
{
#ifdef GL_GLES
   if (glsym_eglGetProcAddress) return glsym_eglGetProcAddress(name);
   return dlsym(RTLD_DEFAULT, name);
#else
   if (glsym_glXGetProcAddress) return glsym_glXGetProcAddress(name);
   return dlsym(RTLD_DEFAULT, name);
#endif
}

static int
evgl_eng_rotation_angle_get(void *data)
{
   Render_Engine *re = (Render_Engine *)data;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return 0;
     }

   if ((re->win) && (re->win->gl_context))
     {
        // TODO: Check that pre-rotation is not enabled (not implemented yet).
        if (_evgl_direct_enabled())
          return re->win->gl_context->rot;
        return 0;
     }
   else
     {
        ERR("Unable to retrieve rotation angle.");
        _evgl_error_set(EVAS_GL_BAD_CONTEXT);
        return 0;
     }
}

static void *
evgl_eng_native_buffer_image_create(void *data, int tex, int target, void *native)
{
#ifdef GL_GLES
   Render_Engine *re = (Render_Engine *)data;
   EGLSurface surface = EGL_NO_SURFACE;
   int native_target = 0;
   int err;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return NULL;
     }

   if (target == 1)
      native_target = (int)EGL_NATIVE_PIXMAP_KHR;
   else
     {
        ERR("Invalid native target");
        _evgl_error_set(EVAS_GL_BAD_MATCH);
        goto error;
     }

   glBindTexture(GL_TEXTURE_2D, tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

   if (glsym_eglCreateImage)
     {
        surface = glsym_eglCreateImage(re->win->egl_disp,
                                       EGL_NO_CONTEXT,
                                       (EGLenum)native_target,
                                       native,
                                       NULL);
        if (!surface)
          {
             ERR("eglCreateImage() for %p failed", native);

             if ((err = eglGetError()) != EGL_SUCCESS)
               {
                  ERR("eglCreateImage failed: %x.", err);
                  _evgl_error_set(err - EGL_SUCCESS);
               }

             goto error;
          }
     }
   else
      ERR("Try eglCreateImage on EGL with no support");

   if (glsym_glEGLImageTargetTexture2DOES)
     {
        glsym_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, surface);
        if ((err = eglGetError()) != EGL_SUCCESS)
          {
             ERR("glEGLImageTargetTexture2DOES() failed.");
             _evgl_error_set(err - EGL_SUCCESS);
             goto error;
          }
     }
   else
     {
        ERR("Try glEGLImageTargetTexture2DOES on EGL with no support");
        goto error;
     }

   glBindTexture(GL_TEXTURE_2D, 0);

   return (void *)surface;

error:
   if (surface)
      if (glsym_eglDestroyImage)
         glsym_eglDestroyImage(re->win->egl_disp, surface);
#else
   _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
#endif

   return NULL;
}

static int
evgl_eng_native_buffer_image_destroy(void *data, void *native_image)
{
   Render_Engine *re = (Render_Engine *)data;
   int err;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return 0;
     }

#ifdef GL_GLES
   if (glsym_eglDestroyImage)
     {
        glsym_eglDestroyImage(re->win->egl_disp, (EGLSurface)native_image);
        if ((err = eglGetError()) != EGL_SUCCESS)
          {
             ERR("eglDestroyImage() failed.");
             _evgl_error_set(err - EGL_SUCCESS);
          }
        native_image = NULL;
        return 1;
     }
   else
#endif
     {
        ERR("Try eglDestroyImage on EGL with no support");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return 0;
     }
}

static int
evgl_eng_texture_destroyed_cb(void *data, GLuint texid)
{
   Render_Engine *re = (Render_Engine *)data;
   void *im;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return 0;
     }

   im = eina_hash_find(re->win->gl_context->shared->native_tex_hash, &texid);
   if (!im) return 0;

   eina_hash_del(re->win->gl_context->shared->native_tex_hash, &texid, im);
   return 1;
}

static void *
evgl_eng_pbuffer_surface_create(void *data, EVGL_Surface *sfc,
                                const int *attrib_list)
{
   Render_Engine *re = (Render_Engine *)data;

   // TODO: Add support for surfaceless pbuffers (EGL_NO_TEXTURE)
   // TODO: Add support for EGL_MIPMAP_TEXTURE??? (GLX doesn't support them)

#ifdef GL_GLES
   int config_attrs[20];
   int surface_attrs[20];
   EGLSurface egl_sfc;
   EGLConfig egl_cfg;
   int num_config, i = 0;

   if (attrib_list)
     WRN("This PBuffer implementation does not support extra attributes yet");

#if 0
   // Choose framebuffer configuration
   // DISABLED FOR NOW
   if (sfc->pbuffer.color_fmt != EVAS_GL_NO_FBO)
     {
        config_attrs[i++] = EGL_RED_SIZE;
        config_attrs[i++] = 1;
        config_attrs[i++] = EGL_GREEN_SIZE;
        config_attrs[i++] = 1;
        config_attrs[i++] = EGL_BLUE_SIZE;
        config_attrs[i++] = 1;

        if (sfc->pbuffer.color_fmt == EVAS_GL_RGBA_8888)
          {
             config_attrs[i++] = EGL_ALPHA_SIZE;
             config_attrs[i++] = 1;
             //config_attrs[i++] = EGL_BIND_TO_TEXTURE_RGBA;
             //config_attrs[i++] = EGL_TRUE;
          }
        else
          {
             //config_attrs[i++] = EGL_BIND_TO_TEXTURE_RGB;
             //config_attrs[i++] = EGL_TRUE;
          }
     }

   if (sfc->depth_fmt || sfc->depth_stencil_fmt)
     {
        config_attrs[i++] = EGL_DEPTH_SIZE;
        config_attrs[i++] = 1;
     }

   if (sfc->stencil_fmt || sfc->depth_stencil_fmt)
     {
        config_attrs[i++] = EGL_STENCIL_SIZE;
        config_attrs[i++] = 1;
     }

   config_attrs[i++] = EGL_RENDERABLE_TYPE;
   config_attrs[i++] = EGL_OPENGL_ES2_BIT;
   config_attrs[i++] = EGL_SURFACE_TYPE;
   config_attrs[i++] = EGL_PBUFFER_BIT;
   config_attrs[i++] = EGL_NONE;
#else
   // It looks like eglMakeCurrent might fail if we use a different config from
   // the actual display surface. This is weird.
   i = 0;
   config_attrs[i++] = EGL_CONFIG_ID;
   config_attrs[i++] = 0;
   config_attrs[i++] = EGL_NONE;
   eglQueryContext(re->win->egl_disp, re->win->egl_context[0], EGL_CONFIG_ID, &config_attrs[1]);
#endif

   if (!eglChooseConfig(re->win->egl_disp, config_attrs, &egl_cfg, 1, &num_config)
       || (num_config < 1))
     {
        int err = eglGetError();
        _evgl_error_set(err - EGL_SUCCESS);
        ERR("eglChooseConfig failed with error %x", err);
        return NULL;
     }

   // Now, choose the config for the PBuffer
   i = 0;
   surface_attrs[i++] = EGL_WIDTH;
   surface_attrs[i++] = sfc->w;
   surface_attrs[i++] = EGL_HEIGHT;
   surface_attrs[i++] = sfc->h;
#if 0
   // Adding these parameters will trigger EGL_BAD_ATTRIBUTE because
   // the config also requires EGL_BIND_TO_TEXTURE_RGB[A]. But some drivers
   // don't support those configs (eg. nvidia)
   surface_attrs[i++] = EGL_TEXTURE_FORMAT;
   if (sfc->pbuffer.color_fmt == EVAS_GL_RGB_888)
     surface_attrs[i++] = EGL_TEXTURE_RGB;
   else
     surface_attrs[i++] = EGL_TEXTURE_RGBA;
   surface_attrs[i++] = EGL_TEXTURE_TARGET;
   surface_attrs[i++] = EGL_TEXTURE_2D;
   surface_attrs[i++] = EGL_MIPMAP_TEXTURE;
   surface_attrs[i++] = EINA_TRUE;
#endif
   surface_attrs[i++] = EGL_NONE;

   egl_sfc = eglCreatePbufferSurface(re->win->egl_disp, egl_cfg, surface_attrs);
   if (!egl_sfc)
     {
        int err = eglGetError();
        _evgl_error_set(err - EGL_SUCCESS);
        ERR("eglCreatePbufferSurface failed with error %x", err);
        return NULL;
     }

   return egl_sfc;
#else
   //FIXME: PBuffer support is not implemented yet!
#endif
}

// This function should create a surface that can be used for offscreen rendering
// with GLES 1.x, and still be bindable to a texture in Evas main GL context.
// For now, this will create an X pixmap... Ideally it should be able to create
// a bindable pbuffer surface or just an FBO if that is supported and it can
// be shared with Evas.
static void *
evgl_eng_gles1_surface_create(void *data, EVGL_Surface *evgl_sfc,
                              Evas_GL_Config *cfg, int w, int h)
{
   Render_Engine *re = (Render_Engine *)data;
   Eina_Bool alpha = EINA_FALSE;
   int colordepth;
   Pixmap px;

   if (!re || !evgl_sfc || !cfg)
     {
        _evgl_error_set(EVAS_GL_BAD_PARAMETER);
        return NULL;
     }

   if ((cfg->gles_version != EVAS_GL_GLES_1_X) || (w < 1) || (h < 1))
     {
        ERR("Inconsistent parameters, not creating any surface!");
        _evgl_error_set(EVAS_GL_BAD_PARAMETER);
        return NULL;
     }

   /* Choose appropriate pixmap depth */
   if (cfg->color_format == EVAS_GL_RGBA_8888)
     {
        alpha = EINA_TRUE;
        colordepth = 32;
     }
   else if (cfg->color_format == EVAS_GL_RGB_888)
     colordepth = 24;
   else // this could also be XDefaultDepth but this case shouldn't happen
     colordepth = 24;

   px = XCreatePixmap(re->win->disp, re->win->win, w, h, colordepth);
   if (!px)
     {
        ERR("Failed to create XPixmap!");
        _evgl_error_set(EVAS_GL_BAD_ALLOC);
        return NULL;
     }

#ifdef GL_GLES
   EGLSurface egl_sfc;
   EGLConfig egl_cfg;
   int i, num = 0, best = 0;
   EGLConfig configs[200];
   int config_attrs[40];
   Eina_Bool found = EINA_FALSE;
   int msaa = 0, depth = 0, stencil = 0;
   Visual *visual = NULL;

   /* Now we need to iterate over all EGL configurations to check the compatible
    * ones and finally check their visual ID. */

   if ((cfg->depth_bits > EVAS_GL_DEPTH_NONE) &&
       (cfg->depth_bits <= EVAS_GL_DEPTH_BIT_32))
     depth = 8 * ((int) cfg->depth_bits);

   if ((cfg->stencil_bits > EVAS_GL_STENCIL_NONE) &&
       (cfg->stencil_bits <= EVAS_GL_STENCIL_BIT_16))
     stencil = 1 << ((int) cfg->stencil_bits - 1);

   if ((cfg->multisample_bits > EVAS_GL_MULTISAMPLE_NONE) &&
       (cfg->multisample_bits <= EVAS_GL_MULTISAMPLE_HIGH))
     msaa = evgl_engine->caps.msaa_samples[(int) cfg->multisample_bits - 1];

   // The below code is similar to eng_best_visual_get() from EFL 1.12
   i = 0;
   config_attrs[i++] = EGL_SURFACE_TYPE;
   config_attrs[i++] = EGL_PIXMAP_BIT;
   config_attrs[i++] = EGL_RENDERABLE_TYPE;
   config_attrs[i++] = EGL_OPENGL_ES_BIT;
   if (alpha)
     {
        config_attrs[i++] = EGL_ALPHA_SIZE;
        config_attrs[i++] = 1; // should it be 8?
        if (evgl_engine->api_debug_mode)
          DBG("Requesting RGBA pixmap");
     }
   else
     {
        config_attrs[i++] = EGL_ALPHA_SIZE;
        config_attrs[i++] = 0;
     }
   if (depth)
     {
        config_attrs[i++] = EGL_DEPTH_SIZE;
        config_attrs[i++] = depth;
        if (evgl_engine->api_debug_mode)
          DBG("Requesting depth buffer size %d", depth);
     }
   if (stencil)
     {
        config_attrs[i++] = EGL_STENCIL_SIZE;
        config_attrs[i++] = stencil;
        if (evgl_engine->api_debug_mode)
          DBG("Requesting stencil buffer size %d", stencil);
     }
   if (msaa)
     {
        config_attrs[i++] = EGL_SAMPLE_BUFFERS;
        config_attrs[i++] = 1;
        config_attrs[i++] = EGL_SAMPLES;
        config_attrs[i++] = msaa;
        if (evgl_engine->api_debug_mode)
          DBG("Requesting MSAA buffer with %d samples", msaa);
     }
   config_attrs[i++] = EGL_NONE;
   config_attrs[i++] = 0;

   /* Note: We found some horrible problems on both Mali and Adreno drivers
    * apparently related to the use of MSAA and RGBA. Therefore the following
    * warning.
    * What happened (wild guess here) is that while a pixmap with RGBA+MSAA
    * should be sampled using MSAA, blending was not possible because MSAA
    * would maybe use the alpha bits as a meta data bit field. So, it appears
    * that MSAA and RGBA may be incompatible.
    */
   if (alpha && msaa)
     WRN("Requested RGBA and MSAA(%d) for a Pixmap buffer.\nThis configuration "
         "has been proven on various drivers to cause rendering artifacts when "
         "sampling the Pixmap to draw it on the canvas.\nIf you really want "
         "to proceed with this, you will need to disable blending.\nAt the "
         "Evas level, this means disabling alpha on the object, or setting "
         "the render_op to COPY.", msaa);

   if (!eglChooseConfig(re->win->egl_disp, config_attrs, configs, 200, &num) || !num)
     {
        int err = eglGetError();
        ERR("eglChooseConfig() can't find any configs, error: %x", err);
        _evgl_error_set(err - EGL_SUCCESS);
        XFreePixmap(re->win->disp, px);
        return NULL;
     }

   if (evgl_engine->api_debug_mode)
     DBG("Found %d potential configurations", num);

   for (i = 0; (i < num) && !found; i++)
     {
        EGLint val = 0;
        VisualID visid = 0;
        XVisualInfo *xvi, vi_in;
        XRenderPictFormat *fmt;
        int nvi = 0, j;

        if (!eglGetConfigAttrib(re->win->egl_disp, configs[i],
                                EGL_NATIVE_VISUAL_ID, &val))
          continue;

        // Find matching visuals. Only alpha & depth are really valid here.
        visid = val;
        vi_in.screen = re->win->screen;
        vi_in.visualid = visid;
        xvi = XGetVisualInfo(re->win->disp,
                             VisualScreenMask | VisualIDMask,
                             &vi_in, &nvi);
        if (xvi)
          {
             for (j = 0; (j < nvi) && !found; j++)
               {
                  if (xvi[j].depth >= colordepth)
                    {
                       if (!best) best = i;
                       if (alpha)
                         {
                            fmt = XRenderFindVisualFormat(re->win->disp, xvi[j].visual);
                            if (fmt && (fmt->direct.alphaMask))
                              found = EINA_TRUE;
                         }
                       else found = EINA_TRUE;
                    }
               }
             if (found)
               {
                  egl_cfg = configs[i];
                  visual = xvi[j].visual;
                  if (evgl_engine->api_debug_mode)
                    DBG("Found matching visual ID %d (cfg %d)", (int) visid, i);

                  XFree(xvi);
                  break;
               }
             XFree(xvi);
          }
     }

   if (!found)
     {
        // This config will probably not work, but we try anyways.
        ERR("XGetVisualInfo failed. Trying with the EGL config #%d.", best);
        if (num)
          egl_cfg = configs[best];
        else
          egl_cfg = re->win->egl_config;
     }

   egl_sfc = eglCreatePixmapSurface(re->win->egl_disp, egl_cfg, px, NULL);
   if (!egl_sfc)
     {
        int err = eglGetError();
        ERR("eglCreatePixmapSurface failed with error: %x", err);
        _evgl_error_set(err - EGL_SUCCESS);
        XFreePixmap(re->win->disp, px);
        return NULL;
     }

   evgl_sfc->gles1_indirect = EINA_TRUE;
   evgl_sfc->xpixmap = EINA_TRUE;
   evgl_sfc->gles1_sfc = egl_sfc;
   evgl_sfc->gles1_sfc_native = (void *)(intptr_t) px;
   evgl_sfc->gles1_sfc_visual = visual;
   evgl_sfc->gles1_sfc_config = egl_cfg;
   DBG("Successfully created GLES1 surface: Pixmap %p EGLSurface %p", px, egl_sfc);
   return evgl_sfc;

#else
#warning GLX support is not implemented for GLES 1.x
   CRIT("Not implemented yet! (GLX for GLES 1)");
   return NULL;
#endif
}

// This function should destroy the surface used for offscreen rendering
// with GLES 1.x.This will also destroy the X pixmap...
static int
evgl_eng_gles1_surface_destroy(void *data, EVGL_Surface *evgl_sfc)
{
   int err;
   Render_Engine *re = (Render_Engine *)data;

   if (!re)
     {
        ERR("Invalid Render Engine Data!");
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return 0;
     }

#ifdef GL_GLES
   if ((!evgl_sfc) || (!evgl_sfc->gles1_sfc))
     {
        ERR("Invalid surface.");
        _evgl_error_set(EVAS_GL_BAD_SURFACE);
        return 0;
     }

   eglDestroySurface(re->win->egl_disp, (EGLSurface)evgl_sfc->gles1_sfc);

#else
#warning GLX support is not implemented for GLES 1.x
   CRIT("Not implemented yet! (GLX for GLES 1)");
   return 0;
#endif

   if (!evgl_sfc->gles1_sfc_native)
     {
        ERR("Inconsistent parameters, not freeing XPixmap for gles1 surface!");
        _evgl_error_set(EVAS_GL_BAD_PARAMETER);
        return 0;
     }

   XFreePixmap(re->win->disp, (Pixmap)evgl_sfc->gles1_sfc_native);

   return 1;
}

static void *
evgl_eng_gles1_context_create(void *data,
                              EVGL_Context *share_ctx, EVGL_Surface *sfc)
{
   Render_Engine *re = data;
   if (!re) return NULL;

#ifdef GL_GLES
   EGLContext context = EGL_NO_CONTEXT;
   int context_attrs[3];
   EGLConfig config;

   context_attrs[0] = EGL_CONTEXT_CLIENT_VERSION;
   context_attrs[1] = 1;
   context_attrs[2] = EGL_NONE;

   if (!sfc || !sfc->gles1_sfc_config)
     {
        ERR("Surface is not set! Creating context anyways but eglMakeCurrent "
            "might very well fail with EGL_BAD_MATCH (0x3009)");
        config = re->win->egl_config;
     }
   else config = sfc->gles1_sfc_config;

   context = eglCreateContext(re->win->egl_disp, config,
                              share_ctx ? share_ctx->context : NULL,
                              context_attrs);
   if (!context)
     {
        int err = eglGetError();
        ERR("eglCreateContext failed with error 0x%x", err);
        _evgl_error_set(err - EGL_SUCCESS);
        return NULL;
     }

   DBG("Successfully created context for GLES1 indirect rendering.");
   return context;
#else
   CRIT("Support for GLES1 indirect rendering contexts is not implemented for GLX");
   return NULL;
#endif
}

static EVGL_Interface evgl_funcs =
{
   evgl_eng_display_get,
   evgl_eng_evas_surface_get,
   evgl_eng_native_window_create,
   evgl_eng_native_window_destroy,
   evgl_eng_window_surface_create,
   evgl_eng_window_surface_destroy,
   evgl_eng_context_create,
   evgl_eng_context_destroy,
   evgl_eng_make_current,
   evgl_eng_proc_address_get,
   evgl_eng_string_get,
   evgl_eng_rotation_angle_get,
   evgl_eng_native_buffer_image_create,
   evgl_eng_native_buffer_image_destroy,
   evgl_eng_texture_destroyed_cb,
   evgl_eng_pbuffer_surface_create,
   evgl_eng_gles1_surface_create,
   evgl_eng_gles1_surface_destroy,
   evgl_eng_gles1_context_create,
};

//----------------------------------------------------------//


static void
gl_symbols(void)
{
   static int done = 0;

   if (done) return;

#ifdef GL_GLES
#define FINDSYM(dst, sym, typ) \
   if (glsym_eglGetProcAddress) { \
      if (!dst) dst = (typ)glsym_eglGetProcAddress(sym); \
   } else { \
      if (!dst) dst = (typ)dlsym(RTLD_DEFAULT, sym); \
   }

   FINDSYM(glsym_eglGetProcAddress, "eglGetProcAddressKHR", glsym_func_eng_fn);
   FINDSYM(glsym_eglGetProcAddress, "eglGetProcAddressEXT", glsym_func_eng_fn);
   FINDSYM(glsym_eglGetProcAddress, "eglGetProcAddressARB", glsym_func_eng_fn);
   FINDSYM(glsym_eglGetProcAddress, "eglGetProcAddress", glsym_func_eng_fn);

   FINDSYM(glsym_eglCreateImage, "eglCreateImageKHR", glsym_func_void_ptr);
   FINDSYM(glsym_eglCreateImage, "eglCreateImageEXT", glsym_func_void_ptr);
   FINDSYM(glsym_eglCreateImage, "eglCreateImageARB", glsym_func_void_ptr);
   FINDSYM(glsym_eglCreateImage, "eglCreateImage", glsym_func_void_ptr);

   FINDSYM(glsym_eglDestroyImage, "eglDestroyImageKHR", glsym_func_void);
   FINDSYM(glsym_eglDestroyImage, "eglDestroyImageEXT", glsym_func_void);
   FINDSYM(glsym_eglDestroyImage, "eglDestroyImageARB", glsym_func_void);
   FINDSYM(glsym_eglDestroyImage, "eglDestroyImage", glsym_func_void);

   FINDSYM(glsym_glEGLImageTargetTexture2DOES, "glEGLImageTargetTexture2DOES", glsym_func_void);

   FINDSYM(glsym_eglMapImageSEC, "eglMapImageSEC", glsym_func_void_ptr);
   FINDSYM(glsym_eglUnmapImageSEC, "eglUnmapImageSEC", glsym_func_uint);

   FINDSYM(glsym_eglSwapBuffersWithDamage, "eglSwapBuffersWithDamageEXT", glsym_func_void);
   FINDSYM(glsym_eglSwapBuffersWithDamage, "eglSwapBuffersWithDamageINTEL", glsym_func_void);
   FINDSYM(glsym_eglSwapBuffersWithDamage, "eglSwapBuffersWithDamage", glsym_func_void);


#else
#define FINDSYM(dst, sym, typ) \
   if (glsym_glXGetProcAddress) { \
      if (!dst) dst = (typ)glsym_glXGetProcAddress(sym); \
   } else { \
      if (!dst) dst = (typ)dlsym(RTLD_DEFAULT, sym); \
   }

   FINDSYM(glsym_glXGetProcAddress, "glXGetProcAddressEXT", glsym_func_eng_fn);
   FINDSYM(glsym_glXGetProcAddress, "glXGetProcAddressARB", glsym_func_eng_fn);
   FINDSYM(glsym_glXGetProcAddress, "glXGetProcAddress", glsym_func_eng_fn);

   FINDSYM(glsym_glXBindTexImage, "glXBindTexImageEXT", glsym_func_void);
   FINDSYM(glsym_glXBindTexImage, "glXBindTexImageARB", glsym_func_void);
   FINDSYM(glsym_glXBindTexImage, "glXBindTexImage", glsym_func_void);

   FINDSYM(glsym_glXReleaseTexImage, "glXReleaseTexImageEXT", glsym_func_void);
   FINDSYM(glsym_glXReleaseTexImage, "glXReleaseTexImageARB", glsym_func_void);
   FINDSYM(glsym_glXReleaseTexImage, "glXReleaseTexImage", glsym_func_void);

   FINDSYM(glsym_glXGetVideoSync, "glXGetVideoSyncSGI", glsym_func_int);

   FINDSYM(glsym_glXWaitVideoSync, "glXWaitVideoSyncSGI", glsym_func_int);

   FINDSYM(glsym_glXCreatePixmap, "glXCreatePixmapEXT", glsym_func_xid);
   FINDSYM(glsym_glXCreatePixmap, "glXCreatePixmapARB", glsym_func_xid);
   FINDSYM(glsym_glXCreatePixmap, "glXCreatePixmap", glsym_func_xid);

   FINDSYM(glsym_glXDestroyPixmap, "glXDestroyPixmapEXT", glsym_func_void);
   FINDSYM(glsym_glXDestroyPixmap, "glXDestroyPixmapARB", glsym_func_void);
   FINDSYM(glsym_glXDestroyPixmap, "glXDestroyPixmap", glsym_func_void);

   FINDSYM(glsym_glXQueryDrawable, "glXQueryDrawableEXT", glsym_func_void);
   FINDSYM(glsym_glXQueryDrawable, "glXQueryDrawableARB", glsym_func_void);
   FINDSYM(glsym_glXQueryDrawable, "glXQueryDrawable", glsym_func_void);

   FINDSYM(glsym_glXSwapIntervalSGI, "glXSwapIntervalMESA", glsym_func_int);
   FINDSYM(glsym_glXSwapIntervalSGI, "glXSwapIntervalSGI", glsym_func_int);

   FINDSYM(glsym_glXSwapIntervalEXT, "glXSwapIntervalEXT", glsym_func_void);

   FINDSYM(glsym_glXReleaseBuffersMESA, "glXReleaseBuffersMESA", glsym_func_void);

   FINDSYM(glsym_glXQueryExtensionsString, "glXQueryExtensionsString", glsym_func_const_char_ptr);

#endif

   done = 1;
}

static void
gl_extn_veto(Render_Engine *re)
{
   const char *str = NULL;
#ifdef GL_GLES
   str = eglQueryString(re->win->egl_disp, EGL_EXTENSIONS);
   if (str)
     {
        const GLubyte *vendor = glGetString(GL_VENDOR);

        if (getenv("EVAS_GL_INFO"))
          printf("EGL EXTN:\n%s\n", str);

        // Disable Partial Rendering
        if (getenv("EVAS_GL_PARTIAL_DISABLE"))
          {
             extn_have_buffer_age = 0;
             glsym_eglSwapBuffersWithDamage = NULL;
          }
        // Special case for Qualcomm chipset as they don't expose the
        // extension feature through eglQueryString. Don't ask me why
        // they don't do it.
        else if (!vendor || !strstr(vendor, "Qualcomm"))
          {
             if (!strstr(str, "EGL_EXT_buffer_age"))
               {
                  extn_have_buffer_age = 0;
               }
             if (!strstr(str, "swap_buffers_with_damage"))
               {
                  glsym_eglSwapBuffersWithDamage = NULL;
               }
          }
        if (!strstr(str, "EGL_NOK_texture_from_pixmap"))
          {
             extn_have_y_inverted = 0;
          }
        else
          {
             const GLubyte *renderer;

             renderer = glGetString(GL_RENDERER);
             // XXX: workaround mesa bug!
             // looking for mesa and intel build which is known to
             // advertise the EGL_NOK_texture_from_pixmap extension
             // but not set it correctly. guessing vendor/renderer
             // strings will be like the following:
             // OpenGL vendor string: Intel Open Source Technology Center
             // OpenGL renderer string: Mesa DRI Intel(R) Sandybridge Desktop
             if (((vendor) && (strstr(vendor, "Intel"))) &&
                 ((renderer) && (strstr(renderer, "Mesa"))) &&
                 ((renderer) && (strstr(renderer, "Intel")))
                )
               extn_have_y_inverted = 0;
          }
     }
   else
     {
        if (getenv("EVAS_GL_INFO"))
          printf("NO EGL EXTN!\n");
        extn_have_buffer_age = 0;
     }
#else
   if (glXQueryExtensionsString)
     str = glsym_glXQueryExtensionsString(re->info->info.display,
                                          re->info->info.screen);

   if (str)
     {
        if (getenv("EVAS_GL_INFO"))
          printf("GLX EXTN:\n%s\n", str);
        if (!strstr(str, "_texture_from_pixmap"))
          {
             glsym_glXBindTexImage = NULL;
             glsym_glXReleaseTexImage = NULL;
          }
        if (!strstr(str, "_video_sync"))
          {
             glsym_glXGetVideoSync = NULL;
             glsym_glXWaitVideoSync = NULL;
          }
        if (!strstr(str, "GLX_EXT_buffer_age"))
          {
             extn_have_buffer_age = 0;
          }
        if (!strstr(str, "GLX_EXT_swap_control"))
          {
             glsym_glXSwapIntervalEXT = NULL;
          }
        if (!strstr(str, "GLX_SGI_swap_control"))
          {
             glsym_glXSwapIntervalSGI = NULL;
          }
        if (!strstr(str, "GLX_MESA_release_buffers"))
          {
             glsym_glXReleaseBuffersMESA = NULL;
          }
     }
   else
     {
        if (getenv("EVAS_GL_INFO"))
          printf("NO GLX EXTN!\n");
        glsym_glXBindTexImage = NULL;
        glsym_glXReleaseTexImage = NULL;
        glsym_glXGetVideoSync = NULL;
        glsym_glXWaitVideoSync = NULL;
        extn_have_buffer_age = 0;
        glsym_glXSwapIntervalEXT = NULL;
        glsym_glXSwapIntervalSGI = NULL;
        glsym_glXReleaseBuffersMESA = NULL;
     }
#endif
}

int _evas_engine_GL_X11_log_dom = -1;
/* function tables - filled in later (func and parent func) */
static Evas_Func func, pfunc;

static void *
eng_info(Evas *e)
{
   Evas_Engine_Info_GL_X11 *info;

   info = calloc(1, sizeof(Evas_Engine_Info_GL_X11));
   info->magic.magic = rand();
   info->func.best_visual_get = eng_best_visual_get;
   info->func.best_colormap_get = eng_best_colormap_get;
   info->func.best_depth_get = eng_best_depth_get;
   info->render_mode = EVAS_RENDER_MODE_BLOCKING;
   info->disable_sync_draw_done = EINA_TRUE; // TIZEN ONLY
   return info;
   e = NULL;
}

static void
eng_info_free(Evas *e __UNUSED__, void *info)
{
   Evas_Engine_Info_GL_X11 *in;
// dont free! why bother? its not worth it
//   eina_log_domain_unregister(_evas_engine_GL_X11_log_dom);
   in = (Evas_Engine_Info_GL_X11 *)info;
   free(in);
}

static int
_re_wincheck(Render_Engine *re)
{
   if (re->win->surf) return 1;
   eng_window_resurf(re->win);
   re->lost_back = 1;
   if (!re->win->surf)
     {
        ERR("GL engine can't re-create window surface!");
     }
   return 0;
}

static void
_re_winfree(Render_Engine *re)
{
   if (!re->win->surf) return;
   eng_window_unsurf(re->win, EINA_FALSE);
}

/*
 * Set CARD32 (array) property
 */
EAPI void
_evas_x_window_prop_card32_set(
      unsigned int win,
	  unsigned int atom,
      unsigned int *val,
      unsigned int num)
{
#if SIZEOF_INT == SIZEOF_LONG
   _ATOM_SET_CARD32( win, atom, val, num);
#else /* if SIZEOF_INT == SIZEOF_LONG */
   long *v2;
   unsigned int i;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);
   v2 = malloc(num * sizeof(long));
   if (!v2)
       return;

   for (i = 0; i < num; i++)
      v2[i] = val[i];
   _ATOM_SET_CARD32( win, atom, v2, num);
   free(v2);
#endif /* if SIZEOF_INT == SIZEOF_LONG */
}

/*
 * Get CARD32 (array) property
 *
 * At most len items are returned in val.
 * If the property was successfully fetched the number of items stored in
 * val is returned, otherwise -1 is returned.
 * Note: Return value 0 means that the property exists but has no elements.
 */
static int
_evas_x_window_prop_card32_get(Display *disp,
                               unsigned int win,
                               unsigned int atom,
                               unsigned int *val,
                               unsigned int len)
{
   unsigned char *prop_ret;
   Atom type_ret;
   unsigned long bytes_after, num_ret;
   int format_ret;
   unsigned int i;
   int num;

   prop_ret = NULL;
   if (XGetWindowProperty(disp, win, atom, 0, 0x7fffffff, False,
                          XA_CARDINAL, &type_ret, &format_ret, &num_ret,
                          &bytes_after, &prop_ret) != Success)
     return -1;

   if (type_ret != XA_CARDINAL || format_ret != 32)
     num = -1;
   else if (num_ret == 0 || !prop_ret)
     num = 0;
   else
     {
        if (num_ret < len)
          len = num_ret;

        for (i = 0; i < len; i++)
          val[i] = ((unsigned long *)prop_ret)[i];
        num = len;
     }

   if (prop_ret)
     XFree(prop_ret);

   return num;
}


static Eina_Bool
_evas_x_win_check_prerotation_support(Drawable win, unsigned int atom)
{
   if(!atom) return EINA_FALSE;
   unsigned int transform_hint=0;

   _evas_x_window_prop_card32_get(_evas_x_disp, win, atom, &transform_hint, 1);
   if(transform_hint == EVAS_X_WINDOW_PREROTATION_SUPPORT)
      return EINA_TRUE;

   return EINA_FALSE;
}
static void
_evas_x_win_rotation_transform_hint_set(Drawable win, int angle, unsigned int atom)
{
   if(!atom) return;

   unsigned int transform_hint; // Evas_X_Window_Rotation_Transform_Hint

   switch (angle)
   {
   case 270:
      transform_hint = EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_90;
      break;
   case 180:
      transform_hint = EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_180;
      break;
   case 90:
      transform_hint = EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_270;
      break;
   case 0:
      transform_hint = EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_0;
      break;
   case -1:
      transform_hint = EVAS_X_WINDOW_PREROTATION_QUERY;
      break;
   default:
      transform_hint = EVAS_X_WINDOW_ROTATION_TRANSFORM_HINT_0;
      break;
   }
   // atom is ECORE_X_ATOM_E_WINDOW_ROTATION_TRANSFORM_HINT
   _evas_x_window_prop_card32_set(win, atom, &transform_hint,  1);
}

static int
eng_setup(Evas *e, void *in)
{
   Render_Engine *re;
   Evas_Engine_Info_GL_X11 *info;
   const char *s;

   info = (Evas_Engine_Info_GL_X11 *)in;
   if (!e->engine.data.output)
     {
#ifdef GL_GLES
#else
        int eb, evb;

        if (!glXQueryExtension(info->info.display, &eb, &evb)) return 0;
#endif
        re = calloc(1, sizeof(Render_Engine));
        if (!re) return 0;

        if (getenv("EVAS_GL_MAKE_CURRENT_OPTIMIZE_DISABLE"))
           re->context_optimize = EINA_FALSE;
        else
           re->context_optimize = EINA_TRUE;

        if (_check_compositor())
          re->is_compositor = EINA_TRUE;
        else
          re->is_compositor = EINA_FALSE;

        re->context_current = EINA_FALSE;
        re->info = info;
        re->evas = e;
        re->w = e->output.w;
        re->h = e->output.h;

        if (getenv("EVAS_GL_WIN_PREROTATION"))
          {
             _evas_x_disp = re->info->info.display; // for pre-rotation
             re->is_prerotate = EINA_TRUE; // Set true if env is set
             _evas_x_win_rotation_transform_hint_set(re->info->info.drawable, -1, re->info->atom);
          }
        re->win = eng_window_new(re->info->info.display,
                                 re->info->info.drawable,
                                 re->info->info.screen,
                                 re->info->info.visual,
                                 re->info->info.colormap,
                                 re->info->info.depth,
                                 re->w,
                                 re->h,
                                 re->info->indirect,
                                 re->info->info.destination_alpha,
                                 re->info->info.rotation,
                                 re->info->info.drawable_back);
        re->atom = re->info->atom;

        if (re->is_prerotate)
          {
             re->is_prerotate = _evas_x_win_check_prerotation_support(re->info->info.drawable,
                                                                      re->info->atom);
             _evas_x_win_rotation_transform_hint_set(re->info->info.drawable, 0, re->info->atom);
          }

        if (!re->win)
          {
             free(re);
             return 0;
          }
        e->engine.data.output = re;
        gl_wins++;

        if (!initted)
          {
             gl_symbols();

             eng_init();
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
             gl_extn_veto(re);
             initted = 1;
          }
     }
   else
     {
        re = e->engine.data.output;
        if (re->win && _re_wincheck(re))
          {
             if ((re->info->info.display != re->win->disp) ||
                 (re->info->info.drawable != re->win->win) ||
                 (re->info->info.screen != re->win->screen) ||
                 (re->info->info.visual != re->win->visual) ||
                 (re->info->info.colormap != re->win->colormap) ||
                 (re->info->info.depth != re->win->depth) ||
                 (re->info->info.destination_alpha != re->win->alpha) ||
                 (re->info->info.drawable_back != re->win->win_back))
               {
                  re->win->gl_context->references++;
                  eng_window_free(re->win);
                  gl_wins--;

                  re->w = e->output.w;
                  re->h = e->output.h;
                  re->win = eng_window_new(re->info->info.display,
                                           re->info->info.drawable,
                                           re->info->info.screen,
                                           re->info->info.visual,
                                           re->info->info.colormap,
                                           re->info->info.depth,
                                           re->w,
                                           re->h,
                                           re->info->indirect,
                                           re->info->info.destination_alpha,
                                           re->info->info.rotation,
                                           re->info->info.drawable_back);
                  re->atom = re->info->atom;

                  eng_window_use(re->win);
                  if (re->win)
                    {
                       gl_wins++;
                       re->win->gl_context->references--;
                    }
               }
             else if ((re->win->w != e->output.w) ||
                      (re->win->h != e->output.h) ||
                      (re->info->info.rotation != re->win->rot) ||
                      (re->win->gl_context->pre_rotated)) // check for scenario when rotated back to 0
               {
                EVGL_Resource *rsc;
                Eina_Bool is_direct = EINA_FALSE;
                if((rsc = _evgl_tls_resource_get()))
                    {
                       if(rsc->current_ctx &&
                          rsc->current_ctx->current_sfc &&
                          rsc->current_ctx->current_sfc->direct_fb_opt)
                          {
                             is_direct = EINA_TRUE;
                          }
                    }
#if 0
                if((getenv("EVAS_GL_DIRECT_OVERRIDE") && getenv("EVAS_GL_DIRECT_MEM_OPT")))
                    {
                        is_direct = EINA_FALSE;
                    }
#endif
                if (re->is_prerotate && evgl_initted && is_direct)
                    {
                       _evas_x_win_rotation_transform_hint_set(re->info->info.drawable, re->info->info.rotation, re->atom);

                       // re->win's h & w are not modified
                       re->win->rot = 0;

                        /* There maybe bad frame due to mismatch of surface and
                        * window size if orientation changes in the middle of
                        * rendering pipeling, therefore recreate the surface.
                        */
                       eng_window_unsurf(re->win, EINA_TRUE);
                       eng_window_resurf(re->win);
                       re->win->gl_context->pre_rotated = EINA_TRUE;
                    }
                  else
                    {
                       re->w = e->output.w;
                       re->h = e->output.h;
                       re->win->w = e->output.w;
                       re->win->h = e->output.h;
                       re->win->rot = re->info->info.rotation;
                       eng_window_use(re->win);
                       evas_gl_common_context_resize(re->win->gl_context, re->win->w, re->win->h, re->win->rot);
                    }
               }
             else if (re->info->info.drawable_back && re->info->info.offscreen != re->win->offscreen)
               {
                  re->win->offscreen = re->info->info.offscreen;
                  eng_window_use(re->win);
               }
          }
     }
   if ((s = getenv("EVAS_GL_SWAP_MODE")))
     {
        if ((!strcasecmp(s, "full")) ||
            (!strcasecmp(s, "f")))
          re->mode = MODE_FULL;
        else if ((!strcasecmp(s, "copy")) ||
                 (!strcasecmp(s, "c")))
          re->mode = MODE_COPY;
        else if ((!strcasecmp(s, "double")) ||
                 (!strcasecmp(s, "d")) ||
                 (!strcasecmp(s, "2")))
          re->mode = MODE_DOUBLE;
        else if ((!strcasecmp(s, "triple")) ||
                 (!strcasecmp(s, "t")) ||
                 (!strcasecmp(s, "3")))
          re->mode = MODE_TRIPLE;
        else if ((!strcasecmp(s, "quadruple")) ||
                 (!strcasecmp(s, "q")) ||
                 (!strcasecmp(s, "4")))
          re->mode = MODE_QUADRUPLE;
     }
   else
     {
// in most gl implementations - egl and glx here that we care about the TEND
// to either swap or copy backbuffer and front buffer, but strictly that is
// not true. technically backbuffer content is totally undefined after a swap
// and thus you MUST re-render all of it, thus MODE_FULL
        re->mode = MODE_FULL;
// BUT... reality is that lmost every implementation copies or swaps so
// triple buffer mode can be used as it is a superset of double buffer and
// copy (though using those explicitly is more efficient). so let's play with
// triple buffer mdoe as a default and see.
//        re->mode = MODE_TRIPLE;
// XXX: note - the above seems to break on some older intel chipsets and
// drivers. it seems we CANT depend on backbuffer staying around. bugger!
        switch (info->swap_mode)
          {
           case EVAS_ENGINE_GL_X11_SWAP_MODE_FULL:
             re->mode = MODE_FULL;
             break;
           case EVAS_ENGINE_GL_X11_SWAP_MODE_COPY:
             re->mode = MODE_COPY;
             break;
           case EVAS_ENGINE_GL_X11_SWAP_MODE_DOUBLE:
             re->mode = MODE_DOUBLE;
             break;
           case EVAS_ENGINE_GL_X11_SWAP_MODE_TRIPLE:
             re->mode = MODE_TRIPLE;
             break;
           case EVAS_ENGINE_GL_X11_SWAP_MODE_QUADRUPLE:
             re->mode = MODE_QUADRUPLE;
             break;
           default:
             break;
          }
     }
   if (!re->win)
     {
        free(re);
        return 0;
     }

   if (!e->engine.data.output)
     {
        if (re->win)
          {
             eng_window_free(re->win);
             gl_wins--;
          }
        free(re);
        return 0;
     }
   if (re->tb) evas_common_tilebuf_free(re->tb);
   re->tb = evas_common_tilebuf_new(re->win->w, re->win->h);
   if (!re->tb)
     {
        if (re->win)
          {
             eng_window_free(re->win);
             gl_wins--;
          }
        free(re);
        return 0;
     }
   evas_common_tilebuf_set_tile_size(re->tb, EVAS_GL_UPDATE_TILE_SIZE, EVAS_GL_UPDATE_TILE_SIZE);
   evas_common_tilebuf_tile_strict_set(re->tb, EINA_TRUE);

   if (!e->engine.data.context)
     e->engine.data.context =
     e->engine.func->context_new(e->engine.data.output);
   eng_window_use(re->win);

   re->vsync = 0;

   return 1;
}

static void
eng_output_free(void *data)
{
   Render_Engine *re;

   re = (Render_Engine *)data;

   if (re)
     {
#if 0
#ifdef GL_GLES
        // Destroy the resource surface
        // Only required for EGL case
        if (re->surface)
           eglDestroySurface(re->win->egl_disp, re->surface);
#endif

        // Destroy the resource context
        _destroy_internal_context(re, context);
#endif
        if (re->win)
          {
             if (gl_wins == 1)
               {
                  evgl_engine_shutdown(re);
                  evgl_initted = 0;
               }
#ifdef GL_GLES
             eng_window_free(re->win);
#else
             Display *disp = re->win->disp;
             Window win = re->win->win;
             eng_window_free(re->win);
             if (glsym_glXReleaseBuffersMESA)
               glsym_glXReleaseBuffersMESA(disp, win);
#endif
             gl_wins--;
          }

        evas_common_tilebuf_free(re->tb);
        if (re->rects) evas_common_tilebuf_free_render_rects(re->rects);
        if (re->rects_prev[0]) evas_common_tilebuf_free_render_rects(re->rects_prev[0]);
        if (re->rects_prev[1]) evas_common_tilebuf_free_render_rects(re->rects_prev[1]);
        if (re->rects_prev[2]) evas_common_tilebuf_free_render_rects(re->rects_prev[2]);
        if (re->rects_prev[3]) evas_common_tilebuf_free_render_rects(re->rects_prev[3]);

        free(re);
     }
   if ((initted == 1) && (gl_wins == 0))
     {
        evas_common_image_shutdown();
        evas_common_font_shutdown();
        initted = 0;
     }
}

static void
eng_output_resize(void *data, int w, int h)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   re->win->w = w;
   re->win->h = h;
   eng_window_use(re->win);
   evas_gl_common_context_resize(re->win->gl_context, w, h, re->win->rot);
   evas_common_tilebuf_free(re->tb);
   re->tb = evas_common_tilebuf_new(w, h);
   if (re->tb)
     {
        evas_common_tilebuf_set_tile_size(re->tb, EVAS_GL_UPDATE_TILE_SIZE, EVAS_GL_UPDATE_TILE_SIZE);
        evas_common_tilebuf_tile_strict_set(re->tb, EINA_TRUE);
     }
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
   eng_window_use(re->win);
   evas_gl_common_context_resize(re->win->gl_context, re->win->w, re->win->h, re->win->rot);
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
//   INF("GL: finish update cycle!");
}

static Tilebuf_Rect *
_merge_rects(Tilebuf *tb, Tilebuf_Rect *r1, Tilebuf_Rect *r2, Tilebuf_Rect *r3, Tilebuf_Rect *r4)
{
   Tilebuf_Rect *r, *rects;
   Evas_Point p1, p2;

   if (r1)
     {
        EINA_INLIST_FOREACH(EINA_INLIST_GET(r1), r)
          {
             evas_common_tilebuf_add_redraw(tb, r->x, r->y, r->w, r->h);
          }
     }
   if (r2)
     {
        EINA_INLIST_FOREACH(EINA_INLIST_GET(r2), r)
          {
             evas_common_tilebuf_add_redraw(tb, r->x, r->y, r->w, r->h);
          }
     }
   if (r3)
     {
        EINA_INLIST_FOREACH(EINA_INLIST_GET(r3), r)
          {
             evas_common_tilebuf_add_redraw(tb, r->x, r->y, r->w, r->h);
          }
     }
   if (r4)
     {
        EINA_INLIST_FOREACH(EINA_INLIST_GET(r4), r)
          {
             evas_common_tilebuf_add_redraw(tb, r->x, r->y, r->w, r->h);
          }
     }
   rects = evas_common_tilebuf_get_render_rects(tb);

   if (partial_rect_union_mode == -1)
     {
        const char *s = getenv("EVAS_GL_PARTIAL_MERGE");
        if (s)
          {
             if ((!strcmp(s, "bounding")) ||
                 (!strcmp(s, "b")))
               partial_rect_union_mode = MERGE_BOUNDING;
             else if ((!strcmp(s, "full")) ||
                      (!strcmp(s, "f")))
               partial_rect_union_mode = MERGE_FULL;
          }
        else
          partial_rect_union_mode = MERGE_BOUNDING;
     }
   if (partial_rect_union_mode == MERGE_BOUNDING)
     {
// bounding box -> make a bounding box single region update of all regions.
// yes we could try and be smart and figure out size of regions, how far
// apart etc. etc. to try and figure out an optimal "set". this is a tradeoff
// between multiple update regions to render and total pixels to render.
        if (rects)
          {
             p1.x = rects->x; p1.y = rects->y;
             p2.x = rects->x + rects->w; p2.y = rects->y + rects->h;
             EINA_INLIST_FOREACH(EINA_INLIST_GET(rects), r)
               {
                  if (r->x < p1.x) p1.x = r->x;
                  if (r->y < p1.y) p1.y = r->y;
                  if ((r->x + r->w) > p2.x) p2.x = r->x + r->w;
                  if ((r->y + r->h) > p2.y) p2.y = r->y + r->h;
               }
             evas_common_tilebuf_free_render_rects(rects);
             rects = calloc(1, sizeof(Tilebuf_Rect));
             if (rects)
               {
                  rects->x = p1.x;
                  rects->y = p1.y;
                  rects->w = p2.x - p1.x;
                  rects->h = p2.y - p1.y;
               }
          }
     }
   evas_common_tilebuf_clear(tb);
   return rects;
}

/* vsync games - not for now though */
#define VSYNC_TO_SCREEN 1

static void _post_process_rect(Tilebuf *tb, Tilebuf_Rect *rect, int rot)
{
   int x1, x2, y1, y2;
   int x, y, w, h;

   if (rect == NULL)
      return;

   if (rot == 0 || rot == 180)
      return;

   // For 90 or 270 rotation, merged rect should be rounded up to tb->tile_size.w and tb->tile_size.h
   // AFTER rotated to 0 rotation, then rotated back to original rotation

   x = rect->x;
   y = rect->y;
   w = rect->w;
   h = rect->h;

   // STEP 1. rotate the merged rect to rotation 0
   switch (rot)
     {
      case 90:
        rect->x = y;
        rect->y = tb->outbuf_w - (x + w);
        rect->w = h;
        rect->h = w;
        break;
      case 270:
        rect->x = tb->outbuf_h - (y + h);
        rect->y = x;
        rect->w = h;
        rect->h = w;
        break;
      default:
        break;
     }

   // STEP 2. round up rects
   x1 = rect->x;
   x2 = x1 + rect->w;
   y1 = rect->y;
   y2 = y1 + rect->h;
   x1 = tb->tile_size.w * (x1 / tb->tile_size.w);
   y1 = tb->tile_size.h * (y1 / tb->tile_size.h);
   x2 = tb->tile_size.w * ((x2 + tb->tile_size.w - 1) / tb->tile_size.w);
   y2 = tb->tile_size.h * ((y2 + tb->tile_size.h - 1) / tb->tile_size.h);

   x = x1; y = y1; w = x2 - x1; h = y2 - y1;
   RECTS_CLIP_TO_RECT(x, y, w, h, 0, 0, tb->outbuf_h, tb->outbuf_w);

   // STEP 3. rotate the rect back to original rotation
   switch (rot)
     {
      case 90:
        rect->x = tb->outbuf_w - (y + h);
        rect->y = x;
        rect->w = h;
        rect->h = w;
        break;
      case 270:
        rect->x = y;
        rect->y = tb->outbuf_h - (x + w);
        rect->w = h;
        rect->h = w;
        break;
      default:
        break;
     }
}

static void *
eng_output_redraws_next_update_get(void *data, int *x, int *y, int *w, int *h, int *cx, int *cy, int *cw, int *ch)
{
   Render_Engine *re;
   Tilebuf_Rect *rect;
   Eina_Bool first_rect = EINA_FALSE;

#define CLEAR_PREV_RECTS(x) \
   do { \
      if (re->rects_prev[x]) \
        evas_common_tilebuf_free_render_rects(re->rects_prev[x]); \
      re->rects_prev[x] = NULL; \
   } while (0)

   re = (Render_Engine *)data;
   /* get the upate rect surface - return engine data as dummy */
   if (re->end)
     {
        re->end = 0;
        return NULL;
     }
   if (!re->rects)
     {
        re->rects = evas_common_tilebuf_get_render_rects(re->tb);
        if (re->rects)
          {
             if (re->info->swap_mode == EVAS_ENGINE_GL_X11_SWAP_MODE_AUTO)
               {
                  if (extn_have_buffer_age)
                    {
#ifdef GL_GLES
                       EGLint age = 0;

                       if (!eglQuerySurface(re->win->egl_disp,
                                            re->win->egl_surface[0],
                                            EGL_BUFFER_AGE_EXT, &age))
                         age = 0;
#else
                       unsigned int age = 0;

                       if (glsym_glXQueryDrawable)
                         {
                            if (re->win->glxwin)
                              glsym_glXQueryDrawable(re->win->disp,
                                                     re->win->glxwin,
                                                     GLX_BACK_BUFFER_AGE_EXT,
                                                     &age);
                            else
                              glsym_glXQueryDrawable(re->win->disp,
                                                     re->win->win,
                                                     GLX_BACK_BUFFER_AGE_EXT,
                                                     &age);
                         }
#endif
                       if (age == 1) re->mode = MODE_COPY;
                       else if (age == 2) re->mode = MODE_DOUBLE;
                       else if (age == 3) re->mode = MODE_TRIPLE;
                       else if (age == 4) re->mode = MODE_QUADRUPLE;
                       else re->mode = MODE_FULL;
                       if ((int)age != re->prev_age) re->mode = MODE_FULL;
                       re->prev_age = age;
                    }
                  else
                    {
                       re->mode = MODE_FULL;
                    }
               }

             if ( (re->lost_back) || (re->mode == MODE_FULL) )
               {
                  /* if we lost our backbuffer since the last frame redraw all */
                  re->lost_back = 0;
                  evas_common_tilebuf_add_redraw(re->tb, 0, 0, re->win->w, re->win->h);
                  evas_common_tilebuf_free_render_rects(re->rects);
                  re->rects = evas_common_tilebuf_get_render_rects(re->tb);
               }
             /* ensure we get rid of previous rect lists we dont need if mode
              * changed/is appropriate */
             evas_common_tilebuf_clear(re->tb);
             CLEAR_PREV_RECTS(3);
             re->rects_prev[3] = re->rects_prev[2];
             re->rects_prev[2] = re->rects_prev[1];
             re->rects_prev[1] = re->rects_prev[0];
             re->rects_prev[0] = re->rects;
             re->rects = NULL;
             switch (re->mode)
               {
                case MODE_FULL:
                case MODE_COPY: // no prev rects needed
                  re->rects = _merge_rects(re->tb, re->rects_prev[0], NULL, NULL, NULL);
                  break;
                case MODE_DOUBLE: // double mode - only 1 level of prev rect
                  re->rects = _merge_rects(re->tb, re->rects_prev[0], re->rects_prev[1], NULL, NULL);
                  break;
                case MODE_TRIPLE: // triple mode - 2 levels of prev rect
                  re->rects = _merge_rects(re->tb, re->rects_prev[0], re->rects_prev[1], re->rects_prev[2], NULL);
                  break;
                case MODE_QUADRUPLE: // keep all
                  re->rects = _merge_rects(re->tb, re->rects_prev[0], re->rects_prev[1], re->rects_prev[2], re->rects_prev[3]);
                  break;
                default:
                  break;
               }
             if ((extn_have_buffer_age != 0)
                 && (glsym_eglSwapBuffersWithDamage)
                 && (re->mode != MODE_FULL))
                _post_process_rect(re->tb, re->rects, re->win->rot);
             first_rect = EINA_TRUE;
          }
        evas_common_tilebuf_clear(re->tb);
        re->cur_rect = EINA_INLIST_GET(re->rects);
     }
   if (!re->cur_rect) return NULL;
   rect = (Tilebuf_Rect *)re->cur_rect;
   if (re->rects)
     {
        re->win->gl_context->preserve_bit = GL_COLOR_BUFFER_BIT0_QCOM;

        switch (re->mode)
          {
           case MODE_COPY:
           case MODE_DOUBLE:
           case MODE_TRIPLE:
           case MODE_QUADRUPLE:
             rect = (Tilebuf_Rect *)re->cur_rect;
             *x = rect->x;
             *y = rect->y;
             *w = rect->w;
             *h = rect->h;
             *cx = rect->x;
             *cy = rect->y;
             *cw = rect->w;
             *ch = rect->h;
             re->cur_rect = re->cur_rect->next;
             re->win->gl_context->master_clip.enabled = EINA_TRUE;
             re->win->gl_context->master_clip.x = rect->x;
             re->win->gl_context->master_clip.y = rect->y;
             re->win->gl_context->master_clip.w = rect->w;
             re->win->gl_context->master_clip.h = rect->h;
             break;
           case MODE_FULL:
             re->cur_rect = NULL;
             if (x) *x = 0;
             if (y) *y = 0;
             if (w) *w = re->win->w;
             if (h) *h = re->win->h;
             if (cx) *cx = 0;
             if (cy) *cy = 0;
             if (cw) *cw = re->win->w;
             if (ch) *ch = re->win->h;
             re->win->gl_context->master_clip.enabled = EINA_FALSE;
             break;
           default:
             break;
          }

        if (first_rect)
          {
#ifdef GL_GLES
             // dont need to for egl - eng_window_use() can check for other ctxt's
#else
             eng_window_use(NULL);
#endif
             eng_window_use(re->win);
             if (!_re_wincheck(re)) return NULL;

             evas_gl_common_context_flush(re->win->gl_context);
             evas_gl_common_context_newframe(re->win->gl_context);

             if (partial_render_debug == -1)
               {
                  if (getenv("EVAS_GL_PARTIAL_DEBUG")) partial_render_debug = 1;
                  else partial_render_debug = 0;
               }
             if (partial_render_debug == 1)
               {
                  if (re->is_compositor == EINA_TRUE) glClearColor(1.0, 0.0, 0.2, 1.0);
                  else glClearColor(0.2, 0.5, 1.0, 1.0);
                  glClear(GL_COLOR_BUFFER_BIT);
               }
          }
        if (!re->cur_rect)
          {
             re->end = 1;
          }
        return re->win->gl_context->def_surface;
     }
   return NULL;
}

static int safe_native = -1;

static void
eng_output_redraws_next_update_push(void *data, void *surface __UNUSED__, int x __UNUSED__, int y __UNUSED__, int w __UNUSED__, int h __UNUSED__)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   /* put back update surface.. in this case just unflag redraw */
   if (!_re_wincheck(re)) return;
   re->win->draw.drew = 1;
   evas_gl_common_context_flush(re->win->gl_context);
   if (safe_native == -1)
     {
        const char *s = getenv("EVAS_GL_SAFE_NATIVE");
        safe_native = 0;
        if (s) safe_native = atoi(s);
        else
          {
             s = (const char *)glGetString(GL_RENDERER);
             if (s)
               {
                  if (strstr(s, "PowerVR SGX 540") ||
                      strstr(s, "Mali-400 MP"))
                    safe_native = 1;
               }
          }
     }
#ifdef GL_GLES
   // this is needed to make sure all previous rendering is flushed to
   // buffers/surfaces
   // previous rendering should be done and swapped
//xx   if (!safe_native) eglWaitNative(EGL_CORE_NATIVE_ENGINE);
//   if (eglGetError() != EGL_SUCCESS)
//     {
//        printf("Error:  eglWaitNative(EGL_CORE_NATIVE_ENGINE) fail.\n");
//     }
#else
   // previous rendering should be done and swapped
//xx   if (!safe_native) glXWaitX();
#endif
}

static void
eng_output_flush(void *data)
{
   Render_Engine *re;
   static char *dname = NULL;

   re = (Render_Engine *)data;
   if (!_re_wincheck(re)) return;
   if (!re->win->draw.drew) return;

   re->context_current = EINA_FALSE;

   re->win->draw.drew = 0;
   eng_window_use(re->win);

   evas_gl_common_context_done(re->win->gl_context);

   // Save contents of the framebuffer to a file
   if (swap_buffer_debug_mode == -1)
     {
        if ((dname = getenv("EVAS_GL_SWAP_BUFFER_DEBUG_DIR")))
          {
             int stat;
             // Create a directory with 0775 permission
             stat = mkdir(dname, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
             if ((!stat) || errno == EEXIST) swap_buffer_debug_mode = 1;
          }
        else
           swap_buffer_debug_mode = 0;
     }

   if (swap_buffer_debug_mode == 1)
     {
        // Set this env var to dump files every frame
        // Or set the global var in gdb to 1|0 to turn it on and off
        if (getenv("EVAS_GL_SWAP_BUFFER_DEBUG_ALWAYS"))
           swap_buffer_debug = 1;

        if (swap_buffer_debug)
          {
             char fname[100];
             char suffix[100];
             int ret = 0;

             sprintf(fname, "%p", (void*)re->win);

             ret = evas_gl_common_buffer_dump(re->win->gl_context,
                                              (const char*)dname,
                                              (const char*)fname,
                                              re->frame_cnt,
                                              suffix);
             if (!ret) swap_buffer_debug_mode = 0;
          }
     }

#ifdef GL_GLES
   if (!re->vsync && !re->info->info.drawable_back)
     {
        if (re->info->vsync) eglSwapInterval(re->win->egl_disp, 1);
        else eglSwapInterval(re->win->egl_disp, 0);
        re->vsync = 1;
     }
   if (re->info->callback.pre_swap)
     {
        re->info->callback.pre_swap(re->info->callback.data, re->evas);
     }
   if (re->info->info.drawable_back)
     {
        glFlush();
     }
   else
     {
        // XXX: if partial swaps can be done use re->rects
        if ((extn_have_buffer_age != 0)
            && (glsym_eglSwapBuffersWithDamage)
            && (re->mode != MODE_FULL))
          {
             EGLint num = 0, *rects = NULL, i;
             Tilebuf_Rect *r;

             // if partial swaps can be done use re->rects
             EINA_INLIST_FOREACH(EINA_INLIST_GET(re->rects), r) num++;
             if (num > 0) rects = malloc(sizeof(EGLint) * 4 * num);
             if (rects)
               {
                  i = 0;
                  EINA_INLIST_FOREACH(EINA_INLIST_GET(re->rects), r)
                    {
                       int gw, gh;

                       gw = re->win->gl_context->w;
                       gh = re->win->gl_context->h;
                       switch (re->win->rot)
                         {
                          case 0:
                            rects[i + 0] = r->x;
                            rects[i + 1] = gh - (r->y + r->h);
                            rects[i + 2] = r->w;
                            rects[i + 3] = r->h;
                            break;
                          case 90:
                            rects[i + 0] = r->y;
                            rects[i + 1] = r->x;
                            rects[i + 2] = r->h;
                            rects[i + 3] = r->w;
                            break;
                          case 180:
                            rects[i + 0] = gw - (r->x + r->w);
                            rects[i + 1] = r->y;
                            rects[i + 2] = r->w;
                            rects[i + 3] = r->h;
                            break;
                          case 270:
                            rects[i + 0] = gh - (r->y + r->h);
                            rects[i + 1] = gw - (r->x + r->w);
                            rects[i + 2] = r->h;
                            rects[i + 3] = r->w;
                            break;
                          default:
                            rects[i + 0] = r->x;
                            rects[i + 1] = gh - (r->y + r->h);
                            rects[i + 2] = r->w;
                            rects[i + 3] = r->h;
                            break;
                         }
                       i += 4;
                    }

                    glsym_eglSwapBuffersWithDamage(re->win->egl_disp,
                                                   re->win->egl_surface[0],
                                                   rects, num);
                  free(rects);
               }
          }
        else
          {
             eglSwapBuffers(re->win->egl_disp, re->win->egl_surface[0]);
          }
     }
   //xx   if (!safe_native) eglWaitGL();
   if (re->info->callback.post_swap)
     {
        re->info->callback.post_swap(re->info->callback.data, re->evas);
     }
//   if (eglGetError() != EGL_SUCCESS)
//     {
//        printf("Error:  eglSwapBuffers() fail.\n");
//     }
#else
#ifdef VSYNC_TO_SCREEN
   if (re->info->vsync)
     {
        if (glsym_glXSwapIntervalEXT)
          {
             if (!re->vsync)
               {
                  if (re->info->vsync) glsym_glXSwapIntervalEXT(re->win->disp, re->win->win, 1);
                  else glsym_glXSwapIntervalEXT(re->win->disp, re->win->win, 0);
                  re->vsync = 1;
               }
          }
        else if (glsym_glXSwapIntervalSGI)
          {
             if (!re->vsync)
               {
                  if (re->info->vsync) glsym_glXSwapIntervalSGI(1);
                  else glsym_glXSwapIntervalSGI(0);
                  re->vsync = 1;
               }
          }
        else
          {
             if ((glsym_glXGetVideoSync) && (glsym_glXWaitVideoSync))
               {
                  unsigned int rc;

                  glsym_glXGetVideoSync(&rc);
                  glsym_glXWaitVideoSync(1, 0, &rc);
               }
          }
     }
#endif
   if (re->info->callback.pre_swap)
     {
        re->info->callback.pre_swap(re->info->callback.data, re->evas);
     }
   // XXX: if partial swaps can be done use re->rects
   glXSwapBuffers(re->win->disp, re->win->win);
   if (re->info->callback.post_swap)
     {
        re->info->callback.post_swap(re->info->callback.data, re->evas);
     }
#endif

   // clear out rects after swap as we may use them during swap
   if (re->rects)
     {
        evas_common_tilebuf_free_render_rects(re->rects);
        re->rects = NULL;
     }

   re->frame_cnt++;
}

static void
eng_output_idle_flush(void *data)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
}

static void
eng_output_dump(void *data)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   evas_common_image_image_all_unload();
   evas_common_font_font_all_unload();
   evas_gl_common_image_all_unload(re->win->gl_context);
   _re_winfree(re);
}

static void
eng_context_cutout_add(void *data __UNUSED__, void *context, int x, int y, int w, int h)
{
//   Render_Engine *re;
//
//   re = (Render_Engine *)data;
//   re->win->gl_context->dc = context;
   evas_common_draw_context_add_cutout(context, x, y, w, h);
}

static void
eng_context_cutout_clear(void *data __UNUSED__, void *context)
{
//   Render_Engine *re;
//
//   re = (Render_Engine *)data;
//   re->win->gl_context->dc = context;
   evas_common_draw_context_clear_cutouts(context);
}

static void
eng_rectangle_draw(void *data, void *context, void *surface, int x, int y, int w, int h)
{
   Render_Engine *re;

   re = (Render_Engine *)data;

   if (re->context_optimize)
     {
        if (!re->context_current)
          {
             re->context_current = EINA_TRUE;
             eng_window_use(re->win);
          }
     }
   else
      eng_window_use(re->win);

   evas_gl_common_context_target_surface_set(re->win->gl_context, surface);
   re->win->gl_context->dc = context;
   evas_gl_common_rect_draw(re->win->gl_context, x, y, w, h);
}

static void
eng_line_draw(void *data, void *context, void *surface, int x1, int y1, int x2, int y2)
{
   Render_Engine *re;

   re = (Render_Engine *)data;

   if (re->context_optimize)
     {
        if (!re->context_current)
          {
             re->context_current = EINA_TRUE;
             eng_window_use(re->win);
          }
     }
   else
      eng_window_use(re->win);

   evas_gl_common_context_target_surface_set(re->win->gl_context, surface);
   re->win->gl_context->dc = context;
   evas_gl_common_line_draw(re->win->gl_context, x1, y1, x2, y2);
}

static void *
eng_polygon_point_add(void *data, void *context __UNUSED__, void *polygon, int x, int y)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   return evas_gl_common_poly_point_add(polygon, x, y);
}

static void *
eng_polygon_points_clear(void *data, void *context __UNUSED__, void *polygon)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   return evas_gl_common_poly_points_clear(polygon);
}

static void
eng_polygon_draw(void *data, void *context, void *surface __UNUSED__, void *polygon, int x, int y)
{
   Render_Engine *re;

   re = (Render_Engine *)data;

   if (re->context_optimize)
     {
        if (!re->context_current)
          {
             re->context_current = EINA_TRUE;
             eng_window_use(re->win);
          }
     }
   else
      eng_window_use(re->win);

   evas_gl_common_context_target_surface_set(re->win->gl_context, surface);
   re->win->gl_context->dc = context;
   evas_gl_common_poly_draw(re->win->gl_context, polygon, x, y);
}

static int
eng_image_alpha_get(void *data __UNUSED__, void *image)
{
//   Render_Engine *re;
   Evas_GL_Image *im;

//   re = (Render_Engine *)data;
   if (!image) return 1;
   im = image;
   return im->alpha;
}

static int
eng_image_colorspace_get(void *data __UNUSED__, void *image)
{
//   Render_Engine *re;
   Evas_GL_Image *im;

//   re = (Render_Engine *)data;
   if (!image) return EVAS_COLORSPACE_ARGB8888;
   im = image;
   return im->cs.space;
}

static void
eng_image_mask_create(void *data __UNUSED__, void *image)
{
   Evas_GL_Image *im;

   if (!image) return;
   im = image;
   if (!im->im->image.data)
      evas_cache_image_load_data(&im->im->cache_entry);
   if (!im->tex)
      im->tex = evas_gl_common_texture_new(im->gc, im->im);
}


static void *
eng_image_alpha_set(void *data, void *image, int has_alpha)
{
   Render_Engine *re;
   Evas_GL_Image *im;

   re = (Render_Engine *)data;
   if (!image) return NULL;
   im = image;
   if (im->alpha == has_alpha) return image;
   if (im->native.data)
     {
        im->alpha = has_alpha;
        return image;
     }
   eng_window_use(re->win);
   if ((im->tex) && (im->tex->pt->dyn.img))
     {
        im->alpha = has_alpha;
        im->tex->alpha = im->alpha;
        return image;
     }
   /* FIXME: can move to gl_common */
   if (im->cs.space != EVAS_COLORSPACE_ARGB8888) return im;
   if ((has_alpha) && (im->im->cache_entry.flags.alpha)) return image;
   else if ((!has_alpha) && (!im->im->cache_entry.flags.alpha)) return image;
   if (im->references > 1)
     {
        Evas_GL_Image *im_new;

        if (!im->im->image.data)
          evas_cache_image_load_data(&im->im->cache_entry);
        evas_gl_common_image_alloc_ensure(im);
        im_new = evas_gl_common_image_new_from_copied_data
           (im->gc, im->im->cache_entry.w, im->im->cache_entry.h,
               im->im->image.data,
               eng_image_alpha_get(data, image),
               eng_image_colorspace_get(data, image));
        if (!im_new) return im;
        evas_gl_common_image_free(im);
        im = im_new;
     }
   else
     evas_gl_common_image_dirty(im, 0, 0, 0, 0);
   return evas_gl_common_image_alpha_set(im, has_alpha ? 1 : 0);
//   im->im->cache_entry.flags.alpha = has_alpha ? 1 : 0;
//   return image;
}

static void *
eng_image_border_set(void *data __UNUSED__, void *image, int l __UNUSED__, int r __UNUSED__, int t __UNUSED__, int b __UNUSED__)
{
//   Render_Engine *re;
//
//   re = (Render_Engine *)data;
   return image;
}

static void
eng_image_border_get(void *data __UNUSED__, void *image __UNUSED__, int *l __UNUSED__, int *r __UNUSED__, int *t __UNUSED__, int *b __UNUSED__)
{
//   Render_Engine *re;
//
//   re = (Render_Engine *)data;
}

static char *
eng_image_comment_get(void *data __UNUSED__, void *image, char *key __UNUSED__)
{
//   Render_Engine *re;
   Evas_GL_Image *im;

//   re = (Render_Engine *)data;
   if (!image) return NULL;
   im = image;
   if (!im->im) return NULL;
   return im->im->info.comment;
}

static char *
eng_image_format_get(void *data __UNUSED__, void *image)
{
//   Render_Engine *re;
   Evas_GL_Image *im;

//   re = (Render_Engine *)data;
   im = image;
   return NULL;
}

static void
eng_image_colorspace_set(void *data, void *image, int cspace)
{
   Render_Engine *re;
   Evas_GL_Image *im;

   re = (Render_Engine *)data;
   if (!image) return;
   im = image;
   if (im->native.data) return;
   /* FIXME: can move to gl_common */
   if (im->cs.space == cspace) return;
   eng_window_use(re->win);
   evas_gl_common_image_alloc_ensure(im);
   evas_cache_image_colorspace(&im->im->cache_entry, cspace);
   switch (cspace)
     {
      case EVAS_COLORSPACE_ARGB8888:
         if (im->cs.data)
           {
              if (!im->cs.no_free) free(im->cs.data);
              im->cs.data = NULL;
              im->cs.no_free = 0;
           }
         break;
      case EVAS_COLORSPACE_YCBCR422P601_PL:
      case EVAS_COLORSPACE_YCBCR422P709_PL:
      case EVAS_COLORSPACE_YCBCR422601_PL:
      case EVAS_COLORSPACE_YCBCR420NV12601_PL:
      case EVAS_COLORSPACE_YCBCR420TM12601_PL:
         if (im->tex) evas_gl_common_texture_free(im->tex);
         im->tex = NULL;
         if (im->cs.data)
           {
              if (!im->cs.no_free) free(im->cs.data);
           }
         if (im->im->cache_entry.h > 0)
           im->cs.data =
              calloc(1, im->im->cache_entry.h * sizeof(unsigned char *) * 2);
         else
           im->cs.data = NULL;
         im->cs.no_free = 0;
         break;
      default:
         abort();
         break;
     }
   im->cs.space = cspace;
}

/////////////////////////////////////////////////////////////////////////
//
//
typedef struct _Native Native;

struct _Native
{
   Evas_Native_Surface ns;
   Pixmap     pixmap;
   Visual    *visual;

#ifdef GL_GLES
   void        *egl_surface;
   void        *buffer;
   unsigned int recreate : 1;
#else
   void      *fbc;
   XID        glx_pixmap;
#endif
};

// FIXME: this is enabled so updates happen - but its SLOOOOOOOOOOOOOOOW
// (i am sure this is the reason)  not to mention seemingly superfluous. but
// i need to enable it for it to work on fglrx at least. havent tried nvidia.
//
// why is this the case? does anyone know? has anyone tried it on other gfx
// drivers?
//
//#define GLX_TEX_PIXMAP_RECREATE 1

static void
_native_bind_cb(void *data, void *image)
{
   Evas_GL_Image *im = image;
   Native *n = im->native.data;
   int err;

  if (n->ns.type == EVAS_NATIVE_SURFACE_X11)
    {
#ifdef GL_GLES
      if (n->egl_surface)
        {
           Render_Engine *re = data;
           if ((n->recreate) && (re->is_compositor == EINA_TRUE))
             {
                if (glsym_eglDestroyImage)
                  {
                     glsym_eglDestroyImage(re->win->egl_disp, n->egl_surface);
                     if ((err = eglGetError()) != EGL_SUCCESS)
                       {
                          ERR("eglDestroyImage() failed.");
                          _evgl_error_set(err - EGL_SUCCESS);
                       }
                  }
                else
                  ERR("Try eglDestroyImage on EGL with no support");
                if (glsym_eglCreateImage)
                  {
                     n->egl_surface = glsym_eglCreateImage(re->win->egl_disp,
                                                           EGL_NO_CONTEXT,
                                                           EGL_NATIVE_PIXMAP_KHR,
                                                           (void *)n->pixmap,
                                                           NULL);
                     if (!n->egl_surface)
                       ERR("eglCreateImage() for 0x%x failed", (unsigned int)n->pixmap);
                  }
                else
                  ERR("Try eglCreateImage on EGL with no support");
             }
           if (glsym_glEGLImageTargetTexture2DOES)
             {
                glsym_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, n->egl_surface);
                if ((err = eglGetError()) != EGL_SUCCESS)
                  {
                     ERR("glEGLImageTargetTexture2DOES() failed.");
                     _evgl_error_set(err - EGL_SUCCESS);
                  }
             }
           else
             ERR("Try glEGLImageTargetTexture2DOES on EGL with no support");
        }
#else
# ifdef GLX_BIND_TO_TEXTURE_TARGETS_EXT
      Render_Engine *re = data;

      if (glsym_glXBindTexImage)
        {
          glsym_glXBindTexImage(re->win->disp, n->glx_pixmap,
                                GLX_FRONT_LEFT_EXT, NULL);
          GLERR(__FUNCTION__, __FILE__, __LINE__, "");
        }
      else
        ERR("Try glXBindTexImage on GLX with no support");
# endif
#endif
    }
  else if (n->ns.type == EVAS_NATIVE_SURFACE_OPENGL)
    {
      glBindTexture(GL_TEXTURE_2D, n->ns.data.opengl.texture_id);
      GLERR(__FUNCTION__, __FILE__, __LINE__, "");
    }
  else if (n->ns.type == EVAS_NATIVE_SURFACE_TIZEN)
    {
#ifdef GL_GLES
#ifdef HAVE_NATIVE_BUFFER
      if (n->egl_surface)
            {
               if (n->recreate)
                 {
                    Render_Engine *re = data;
                    if (glsym_eglDestroyImage)
                      {
                         glsym_eglDestroyImage(re->win->egl_disp, n->egl_surface);
                         if ((err = eglGetError()) != EGL_SUCCESS)
                           {
                              ERR("eglDestroyImage() failed.");
                              _evgl_error_set(err - EGL_SUCCESS);
                           }
                      }
                    else
                      ERR("Try eglDestroyImage on EGL with no support");
                    if (glsym_eglCreateImage)
                      {
                         int attr[] =
                           {
                             EGL_IMAGE_PRESERVED_KHR,    EGL_TRUE,
                             EGL_NONE,
                           };
                         n->egl_surface = glsym_eglCreateImage(re->win->egl_disp,
                                                               EGL_NO_CONTEXT,
                                                               EGL_NATIVE_BUFFER_TIZEN,
                                                               (void *)n->buffer,
                                                               attr);
                         if (!n->egl_surface)
                           ERR("eglCreateImage() for 0x%x failed", (unsigned int)n->buffer);
                      }
                    else
                      ERR("Try eglCreateImage on EGL with no support");
                 }
               if (glsym_glEGLImageTargetTexture2DOES)
                 {
                    glsym_glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, n->egl_surface);
                    if ((err = eglGetError()) != EGL_SUCCESS)
                      {
                         ERR("glEGLImageTargetTexture2DOES() failed.");
                         _evgl_error_set(err - EGL_SUCCESS);
                      }
                 }
               else
                 ERR("Try glEGLImageTargetTexture2DOES on EGL with no support");
            }
#endif
#endif
    }
  else if (n->ns.type == EVAS_NATIVE_SURFACE_TBM)
    {
       if (n->egl_surface)
         {
            if (n->recreate)
              {
                 Render_Engine *re = data;
                 if (glsym_eglDestroyImage)
                   {
                      glsym_eglDestroyImage(re->win->egl_disp, n->egl_surface);
                      if ((err = eglGetError()) != EGL_SUCCESS)
                        {
                           ERR("eglDestroyImage() failed.");
                           _evgl_error_set(err - EGL_SUCCESS);
                        }
                   }
                 else
                   ERR("Try eglDestroyImage on EGL with no support");
                 if (glsym_eglCreateImage)
                   {
                      int attr[] =
                        {
                          EGL_IMAGE_PRESERVED_KHR,    EGL_TRUE,
                          EGL_NONE,
                        };
                      n->egl_surface = glsym_eglCreateImage(re->win->egl_disp,
                                                            EGL_NO_CONTEXT,
                                                            EGL_NATIVE_SURFACE_TIZEN,
                                                            (void *)n->buffer,
                                                            attr);
                      if (!n->egl_surface)
                        ERR("eglCreateImage() for %p failed", n->buffer);
                   }
                 else
                   ERR("Try eglCreateImage on EGL with no support");
              }
            if (glsym_glEGLImageTargetTexture2DOES)
              {
                 glsym_glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, n->egl_surface);
                 if ((err = eglGetError()) != EGL_SUCCESS)
                   {
                      ERR("glEGLImageTargetTexture2DOES() failed.");
                      _evgl_error_set(err - EGL_SUCCESS);
                   }
              }
            else
              ERR("Try glEGLImageTargetTexture2DOES on EGL with no support");
         }
      }
   return;
   data = NULL;
}

static void
_native_unbind_cb(void *data, void *image)
{
  Evas_GL_Image *im = image;
  Native *n = im->native.data;

  if (n->ns.type == EVAS_NATIVE_SURFACE_X11)
    {
#ifdef GL_GLES
      // nothing
#else
# ifdef GLX_BIND_TO_TEXTURE_TARGETS_EXT
      Render_Engine *re = data;

      if (glsym_glXReleaseTexImage)
        {
          glsym_glXReleaseTexImage(re->win->disp, n->glx_pixmap,
                                   GLX_FRONT_LEFT_EXT);
          GLERR(__FUNCTION__, __FILE__, __LINE__, "");
        }
      else
        ERR("Try glXReleaseTexImage on GLX with no support");
# endif
#endif
    }
  else if (n->ns.type == EVAS_NATIVE_SURFACE_OPENGL)
    {
      glBindTexture(GL_TEXTURE_2D, 0);
      GLERR(__FUNCTION__, __FILE__, __LINE__, "");
    }
  else if (n->ns.type == EVAS_NATIVE_SURFACE_TIZEN)
    {
      // nothing
    }
  else if (n->ns.type == EVAS_NATIVE_SURFACE_TBM)
    {
      // nothing
    }

   return;
   data = NULL;
}

static void
_native_free_cb(void *data, void *image)
{
  Render_Engine *re = data;
  Evas_GL_Image *im = image;
  Native *n = im->native.data;
  uint32_t pmid, texid;
  int err;

  if (n->ns.type == EVAS_NATIVE_SURFACE_X11)
    {
      pmid = n->pixmap;
      eina_hash_del(re->win->gl_context->shared->native_pm_hash, &pmid, im);
#ifdef GL_GLES
      if (n->egl_surface)
        {
          if (glsym_eglDestroyImage)
            {
              glsym_eglDestroyImage(re->win->egl_disp,
                                    n->egl_surface);
              if ((err = eglGetError()) != EGL_SUCCESS)
                {
                  ERR("eglDestroyImage() failed.");
                  _evgl_error_set(err - EGL_SUCCESS);
                }
            }
          else
            ERR("Try eglDestroyImage on EGL with no support");
        }
#else
# ifdef GLX_BIND_TO_TEXTURE_TARGETS_EXT
      if (n->glx_pixmap)
        {
          if (im->native.loose)
            {
              if (glsym_glXReleaseTexImage)
                {
                  glsym_glXReleaseTexImage(re->win->disp, n->glx_pixmap,
                                           GLX_FRONT_LEFT_EXT);
                  GLERR(__FUNCTION__, __FILE__, __LINE__, "");
                }
              else
                ERR("Try glXReleaseTexImage on GLX with no support");
            }
          if (glsym_glXDestroyPixmap)
            {
              glsym_glXDestroyPixmap(re->win->disp, n->glx_pixmap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");
            }
          else
            ERR("Try glXDestroyPixmap on GLX with no support");
          n->glx_pixmap = 0;
        }
# endif
#endif
    }
  else if (n->ns.type == EVAS_NATIVE_SURFACE_OPENGL)
    {
      texid = n->ns.data.opengl.texture_id;
      eina_hash_del(re->win->gl_context->shared->native_tex_hash, &texid, im);
    }
#ifdef GL_GLES
  else if (n->ns.type == EVAS_NATIVE_SURFACE_TIZEN)
    {
      eina_hash_del(re->win->gl_context->shared->native_buffer_hash, &n->buffer, im);
      if (n->egl_surface)
        {
          if (glsym_eglDestroyImage)
            {
              glsym_eglDestroyImage(re->win->egl_disp,
                                    n->egl_surface);
              if ((err = eglGetError()) != EGL_SUCCESS)
                {
                  ERR("eglDestroyImage() failed.");
                  _evgl_error_set(err - EGL_SUCCESS);
                }
            }
          else
            ERR("Try eglDestroyImage on EGL with no support");
        }
    }
#endif
  else if (n->ns.type == EVAS_NATIVE_SURFACE_TBM)
    {
      eina_hash_del(re->win->gl_context->shared->native_tbm_hash, &n->buffer, im);
      if (n->egl_surface)
        {
          if (glsym_eglDestroyImage)
            {
              glsym_eglDestroyImage(re->win->egl_disp,
                                    n->egl_surface);
              if ((err = eglGetError()) != EGL_SUCCESS)
                {
                  ERR("eglDestroyImage() failed.");
                  _evgl_error_set(err - EGL_SUCCESS);
                }
            }
          else
            ERR("Try eglDestroyImage on EGL with no support");
        }
    }

  im->native.data        = NULL;
  im->native.func.data   = NULL;
  im->native.func.bind   = NULL;
  im->native.func.unbind = NULL;
  im->native.func.free   = NULL;
  free(n);
}

static void *
eng_image_native_set(void *data, void *image, void *native)
{
  Render_Engine *re = (Render_Engine *)data;
  Evas_Native_Surface *ns = native;
  Evas_GL_Image *im = image, *im2 = NULL;
  Visual *vis = NULL;
  Pixmap pm = 0;
  Native *n = NULL;
  uint32_t pmid, texid;
  unsigned int tex = 0;
  unsigned int fbo = 0;
  void *buffer = NULL;
  int err;

  if (!im)
    {
       if ((ns) && (ns->type == EVAS_NATIVE_SURFACE_OPENGL))
         {
            im = evas_gl_common_image_new_from_data(re->win->gl_context,
                                                    ns->data.opengl.w,
                                                    ns->data.opengl.h,
                                                    NULL, 1,
                                                    EVAS_COLORSPACE_ARGB8888);
         }
       else
         return NULL;
    }

  if (ns)
    {
      if (ns->type == EVAS_NATIVE_SURFACE_X11)
        {
          vis = ns->data.x11.visual;
          pm = ns->data.x11.pixmap;
          if (im->native.data)
            {
              Native *ens = im->native.data;
              if ((ens->ns.data.x11.visual == vis) &&
                  (ens->ns.data.x11.pixmap == pm))
                return im;
            }
        }
      else if (ns->type == EVAS_NATIVE_SURFACE_OPENGL)
        {
          tex = ns->data.opengl.texture_id;
          fbo = ns->data.opengl.framebuffer_id;
          if (im->native.data)
            {
              Native *ens = im->native.data;
              if ((ens->ns.data.opengl.texture_id == tex) &&
                  (ens->ns.data.opengl.framebuffer_id == fbo))
                return im;
            }
        }
#ifdef HAVE_NATIVE_BUFFER
      else if (ns->type == EVAS_NATIVE_SURFACE_TIZEN)
        {
           buffer = ns->data.tizen.buffer;
           if (im->native.data)
             {
                Native *ens = im->native.data;
                if (ens->ns.data.tizen.buffer == buffer)
                  {
                     ens->ns.data.tizen.rot = im->native.rot = ns->data.tizen.rot;
                     ens->ns.data.tizen.ratio = im->native.ratio = ns->data.tizen.ratio;
                     ens->ns.data.tizen.flip = im->native.flip = ns->data.tizen.flip;
                     return im;
                  }
             }
        }
#endif
      else if (ns->type == EVAS_NATIVE_SURFACE_TBM)
        {
           buffer = ns->data.tizen.buffer;
           if (im->native.data)
             {
                Native *ens = im->native.data;
                if (ens->ns.data.tizen.buffer == buffer)
                  {
                     ens->ns.data.tizen.rot = im->native.rot = ns->data.tizen.rot;
                     ens->ns.data.tizen.ratio = im->native.ratio = ns->data.tizen.ratio;
                     ens->ns.data.tizen.flip = im->native.flip = ns->data.tizen.flip;
                     return im;
                  }
             }
        }
    }
  if ((!ns) && (!im->native.data)) return im;

  eng_window_use(re->win);

  if (im->native.data)
    {
      if (im->native.func.free)
        im->native.func.free(im->native.func.data, im);
      evas_gl_common_image_native_disable(im);
    }

  if (!ns) return im;

  if (ns->type == EVAS_NATIVE_SURFACE_X11)
    {
      pmid = pm;
      im2 = eina_hash_find(re->win->gl_context->shared->native_pm_hash, &pmid);
      if (im2 == im) return im;
      if (im2)
        {
           n = im2->native.data;
           if (n)
             {
                evas_gl_common_image_ref(im2);
                evas_gl_common_image_free(im);
                return im2;
             }
        }
    }
  else if (ns->type == EVAS_NATIVE_SURFACE_OPENGL)
    {
       texid = tex;
       im2 = eina_hash_find(re->win->gl_context->shared->native_tex_hash, &texid);
       if (im2 == im) return im;
       if (im2)
         {
            n = im2->native.data;
            if (n)
              {
                 evas_gl_common_image_ref(im2);
                 evas_gl_common_image_free(im);
                 return im2;
              }
         }

    }
#ifdef HAVE_NATIVE_BUFFER
  else if (ns->type == EVAS_NATIVE_SURFACE_TIZEN)
    {
       im2 = eina_hash_find(re->win->gl_context->shared->native_buffer_hash, &buffer);
       if (im2 == im) return im;
       if (im2)
         {
            n = im2->native.data;
            if (n)
             {
                evas_gl_common_image_ref(im2);
                evas_gl_common_image_free(im);
                return im2;
             }
        }
    }
#endif
  else if (ns->type == EVAS_NATIVE_SURFACE_TBM)
    {
       im2 = eina_hash_find(re->win->gl_context->shared->native_tbm_hash, &buffer);
       if (im2 == im) return im;
       if (im2)
         {
            n = im2->native.data;
            if (n)
             {
                evas_gl_common_image_ref(im2);
                evas_gl_common_image_free(im);
                return im2;
             }
        }
    }
  im2 = evas_gl_common_image_new_from_data(re->win->gl_context,
                                           im->w, im->h, NULL, im->alpha,
                                           EVAS_COLORSPACE_ARGB8888);
  evas_gl_common_image_free(im);
  im = im2;
  if (!im) return NULL;
  if (ns->type == EVAS_NATIVE_SURFACE_X11)
    {
#ifdef GL_GLES
      if (native)
        {
          n = calloc(1, sizeof(Native));
          if (n)
            {
              EGLConfig egl_config;
              int config_attrs[20];
              int num_config, i = 0;
              const char *vendor;
              int yinvert = 1;

              eina_hash_add(re->win->gl_context->shared->native_pm_hash, &pmid, im);

              // assume 32bit pixmap! :)
              config_attrs[i++] = EGL_RED_SIZE;
              config_attrs[i++] = 8;
              config_attrs[i++] = EGL_GREEN_SIZE;
              config_attrs[i++] = 8;
              config_attrs[i++] = EGL_BLUE_SIZE;
              config_attrs[i++] = 8;
              config_attrs[i++] = EGL_ALPHA_SIZE;
              config_attrs[i++] = 8;
              config_attrs[i++] = EGL_DEPTH_SIZE;
              config_attrs[i++] = 0;
              config_attrs[i++] = EGL_STENCIL_SIZE;
              config_attrs[i++] = 0;
              config_attrs[i++] = EGL_RENDERABLE_TYPE;
              config_attrs[i++] = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES_BIT;
              config_attrs[i++] = EGL_SURFACE_TYPE;
              config_attrs[i++] = EGL_PIXMAP_BIT;
              config_attrs[i++] = EGL_NONE;

              if (!eglChooseConfig(re->win->egl_disp, config_attrs,
                                   &egl_config, 1, &num_config))
                {
                  err = eglGetError();
                  ERR("eglChooseConfig() failed for pixmap 0x%x, num_config = %i with error %d",
                                                         (unsigned int)pm, num_config, err);
                  _evgl_error_set(err - EGL_SUCCESS);
                }
              else
                {
                  int val;
                  if (extn_have_y_inverted &&
                      eglGetConfigAttrib(re->win->egl_disp, egl_config,
                                         EGL_Y_INVERTED_NOK, &val))
                        yinvert = val;
                }

              memcpy(&(n->ns), ns, sizeof(Evas_Native_Surface));
              n->pixmap = pm;
              n->visual = vis;
              if (glsym_eglCreateImage)
                n->egl_surface = glsym_eglCreateImage(re->win->egl_disp,
                                                      EGL_NO_CONTEXT,
                                                      EGL_NATIVE_PIXMAP_KHR,
                                                      (void *)pm,
                                                      NULL);
              else
                ERR("Try eglCreateImage on EGL with no support");
              if (!n->egl_surface)
                ERR("eglCreateImage() for 0x%x failed", (unsigned int)pm);
              vendor = (const char *)glGetString(GL_VENDOR);
              if (vendor && strstr (vendor, "Qualcomm") != NULL)
                n->recreate = 1;
              im->native.yinvert     = yinvert;
              im->native.loose       = 0;
              im->native.data        = n;
              im->native.func.data   = re;
              im->native.func.bind   = _native_bind_cb;
              im->native.func.unbind = _native_unbind_cb;
              im->native.func.free   = _native_free_cb;
              im->native.target      = GL_TEXTURE_2D;
              im->native.mipmap      = 0;
              evas_gl_common_image_native_enable(im);
            }
        }
#else
# ifdef GLX_BIND_TO_TEXTURE_TARGETS_EXT
       if (native)
         {
            int dummy;
            unsigned int w, h, depth = 32, border;
            Window wdummy;

            // fixme: round trip :(
            XGetGeometry(re->win->disp, pm, &wdummy, &dummy, &dummy,
                         &w, &h, &border, &depth);
            if (depth <= 32)
              {
                 n = calloc(1, sizeof(Native));
                 if (n)
                   {
                      int pixmap_att[20], i;
                      int config_attrs[40], num = 0;
                      int tex_format = 0, tex_target = 0, yinvert = 0, mipmap = 0;
                      unsigned int target = 0;
                      GLXFBConfig *configs;

                      i = 0;
                      config_attrs[i++] = GLX_BUFFER_SIZE;
                      config_attrs[i++] = depth;
                      if (depth == 32)
                        {
                           config_attrs[i++] = GLX_BIND_TO_TEXTURE_RGBA_EXT;
                           config_attrs[i++] = 1;
                        }
                      else
                        {
                           config_attrs[i++] = GLX_BIND_TO_TEXTURE_RGB_EXT;
                           config_attrs[i++] = 1;
                        }

#ifndef GLX_VISUAL_ID
# define GLX_VISUAL_ID 0x800b
#endif
                      config_attrs[i++] = GLX_VISUAL_ID;
                      config_attrs[i++] = XVisualIDFromVisual(vis);
#ifndef GLX_SAMPLE_BUFFERS
# define GLX_SAMPLE_BUFFERS 0x186a0
#endif
                      config_attrs[i++] = GLX_SAMPLE_BUFFERS;
                      config_attrs[i++] = 0;
                      config_attrs[i++] = GLX_DEPTH_SIZE;
                      config_attrs[i++] = 0;
                      config_attrs[i++] = GLX_STENCIL_SIZE;
                      config_attrs[i++] = 0;
                      config_attrs[i++] = GLX_AUX_BUFFERS;
                      config_attrs[i++] = 0;
                      config_attrs[i++] = GLX_STEREO;
                      config_attrs[i++] = 0;

                      config_attrs[i++] = 0;

                      configs = glXChooseFBConfig(re->win->disp,
                                                  re->win->screen,
                                                  config_attrs,
                                                  &num);
                      if (configs)
                        {
                           int j = 0, val = 0;

                           tex_format = GLX_TEXTURE_FORMAT_RGB_EXT;
                           glXGetFBConfigAttrib(re->win->disp, configs[j],
                                                GLX_ALPHA_SIZE, &val);
                           if (val > 0)
                             {
                                glXGetFBConfigAttrib(re->win->disp, configs[j],
                                                     GLX_BIND_TO_TEXTURE_RGBA_EXT, &val);
                                if (val) tex_format = GLX_TEXTURE_FORMAT_RGBA_EXT;
                             }
                           else
                             {
                                glXGetFBConfigAttrib(re->win->disp, configs[j],
                                                     GLX_BIND_TO_TEXTURE_RGB_EXT, &val);
                                if (val) tex_format = GLX_TEXTURE_FORMAT_RGB_EXT;
                             }
                           glXGetFBConfigAttrib(re->win->disp, configs[j],
                                                GLX_Y_INVERTED_EXT, &val);
                           yinvert = val;
                           glXGetFBConfigAttrib(re->win->disp, configs[j],
                                                GLX_BIND_TO_TEXTURE_TARGETS_EXT,
                                                &val);
                           tex_target = val;
                           glXGetFBConfigAttrib(re->win->disp, configs[j],
                                                GLX_BIND_TO_MIPMAP_TEXTURE_EXT, &val);
                           mipmap = val;
                           n->fbc = configs[j];
                           XFree(configs);
                        }

                      eina_hash_add(re->win->gl_context->shared->native_pm_hash, &pmid, im);
                      if ((tex_target & GLX_TEXTURE_2D_BIT_EXT))
                        target = GLX_TEXTURE_2D_EXT;
                      else if ((target & GLX_TEXTURE_RECTANGLE_BIT_EXT))
                        {
                           ERR("rect!!! (not handled)");
                           target = GLX_TEXTURE_RECTANGLE_EXT;
                        }
                      if (!target)
                        {
                           ERR("broken tex-from-pixmap");
                           if (!(tex_target & GLX_TEXTURE_2D_BIT_EXT))
                             target = GLX_TEXTURE_RECTANGLE_EXT;
                           else if (!(tex_target & GLX_TEXTURE_RECTANGLE_BIT_EXT))
                             target = GLX_TEXTURE_2D_EXT;
                        }

                      i = 0;
                      pixmap_att[i++] = GLX_TEXTURE_FORMAT_EXT;
                      pixmap_att[i++] = tex_format;
                      pixmap_att[i++] = GLX_MIPMAP_TEXTURE_EXT;
                      pixmap_att[i++] = mipmap;
                      if (target)
                        {
                           pixmap_att[i++] = GLX_TEXTURE_TARGET_EXT;
                           pixmap_att[i++] = target;
                        }
                      pixmap_att[i++] = 0;

                      memcpy(&(n->ns), ns, sizeof(Evas_Native_Surface));
                      n->pixmap = pm;
                      n->visual = vis;
                      if (glsym_glXCreatePixmap)
                        n->glx_pixmap = glsym_glXCreatePixmap(re->win->disp,
                                                              n->fbc,
                                                              n->pixmap,
                                                              pixmap_att);
                      else
                        ERR("Try glXCreatePixmap on GLX with no support");
                      if (n->glx_pixmap)
                        {
//                           printf("%p: new native texture for %x | %4i x %4i @ %2i = %p\n",
//                                  n, pm, w, h, depth, n->glx_pixmap);
                           if (!target)
                             {
                                ERR("no target :(");
                                if (glsym_glXQueryDrawable)
                                  glsym_glXQueryDrawable(re->win->disp,
                                                         n->pixmap,
                                                         GLX_TEXTURE_TARGET_EXT,
                                                         &target);
                             }
                           if (target == GLX_TEXTURE_2D_EXT)
                             {
                                im->native.target = GL_TEXTURE_2D;
                                im->native.mipmap = mipmap;
                             }
#  ifdef GL_TEXTURE_RECTANGLE_ARB
                           else if (target == GLX_TEXTURE_RECTANGLE_EXT)
                             {
                                im->native.target = GL_TEXTURE_RECTANGLE_ARB;
                                im->native.mipmap = 0;
                             }
#  endif
                           else
                             {
                                im->native.target = GL_TEXTURE_2D;
                                im->native.mipmap = 0;
                                ERR("still unknown target");
                             }
                        }
                      else
                        ERR("GLX Pixmap create fail");
                      im->native.yinvert     = yinvert;
                      im->native.loose       = re->win->detected.loose_binding;
                      im->native.data        = n;
                      im->native.func.data   = re;
                      im->native.func.bind   = _native_bind_cb;
                      im->native.func.unbind = _native_unbind_cb;
                      im->native.func.free   = _native_free_cb;

                      evas_gl_common_image_native_enable(im);
                   }
              }
         }
# endif
#endif
    }
  else if (ns->type == EVAS_NATIVE_SURFACE_OPENGL)
    {
      if (native)
        {
          n = calloc(1, sizeof(Native));
          if (n)
            {
              memcpy(&(n->ns), ns, sizeof(Evas_Native_Surface));

              eina_hash_add(re->win->gl_context->shared->native_tex_hash, &texid, im);

              n->pixmap = 0;
              n->visual = 0;
#ifdef GL_GLES
              n->egl_surface = 0;
#else
              n->fbc = 0;
              n->glx_pixmap = 0;
#endif

              im->native.yinvert     = 0;
              im->native.loose       = 0;
              im->native.data        = n;
              im->native.func.data   = re;
              im->native.func.bind   = _native_bind_cb;
              im->native.func.unbind = _native_unbind_cb;
              im->native.func.free   = _native_free_cb;
              im->native.target      = GL_TEXTURE_2D;
              im->native.mipmap      = 0;

              // FIXME: need to implement mapping sub texture regions
              // x, y, w, h for possible texture atlasing

              evas_gl_common_image_native_enable(im);
            }
        }

    }
  else if (ns->type == EVAS_NATIVE_SURFACE_TIZEN)
    {
#ifdef GL_GLES
#ifdef HAVE_NATIVE_BUFFER
       if (native)
         {
            n = calloc(1, sizeof(Native));
            if (n)
              {
                 EGLConfig egl_config;
                 int config_attrs[20];
                 int num_config, i = 0;
                 const char *vendor;

                 eina_hash_add(re->win->gl_context->shared->native_buffer_hash, &buffer, im);

                 memcpy(&(n->ns), ns, sizeof(Evas_Native_Surface));
                 n->pixmap = 0;
                 n->visual = 0;
                 n->buffer = buffer;
                 if (glsym_eglCreateImage)
                   n->egl_surface = glsym_eglCreateImage(re->win->egl_disp,
                                                         EGL_NO_CONTEXT,
                                                         EGL_NATIVE_BUFFER_TIZEN,
                                                         (void *)buffer,
                                                         NULL);
                 else
                   ERR("Try eglCreateImage on EGL with no support");
                 if (!n->egl_surface)
                   ERR("eglCreateImage() for 0x%x failed", (unsigned int)buffer);
                 vendor = (const char *)glGetString(GL_VENDOR);
                 if (vendor && strstr (vendor, "Qualcomm") != NULL)
                   n->recreate = 1;
                 im->native.yinvert     = 1;
                 im->native.loose       = 0;
                 im->native.data        = n;
                 im->native.func.data   = re;
                 im->native.func.bind   = _native_bind_cb;
                 im->native.func.unbind = _native_unbind_cb;
                 im->native.func.free   = _native_free_cb;
                 im->native.target      = GL_TEXTURE_EXTERNAL_OES;
                 im->native.mipmap      = 0;
                 im->native.rot         = ns->data.tizen.rot;
                 im->native.ratio       = ns->data.tizen.ratio;
                 im->native.flip        = ns->data.tizen.flip;
                 evas_gl_common_image_native_enable(im);
              }
         }
#endif
#endif
    }
  else if (ns->type == EVAS_NATIVE_SURFACE_TBM)
    {
       if (native)
         {
            n = calloc(1, sizeof(Native));
            if (n)
              {
                 EGLConfig egl_config;
                 int config_attrs[20];
                 int num_config, i = 0;
                 const char *vendor;

                 eina_hash_add(re->win->gl_context->shared->native_tbm_hash, &buffer, im);

                 memcpy(&(n->ns), ns, sizeof(Evas_Native_Surface));
                 n->pixmap = 0;
                 n->visual = 0;
                 n->buffer = buffer;
                 if (glsym_eglCreateImage)
                   n->egl_surface = glsym_eglCreateImage(re->win->egl_disp,
                                                         EGL_NO_CONTEXT,
                                                         EGL_NATIVE_SURFACE_TIZEN,
                                                         (void *)buffer,
                                                         NULL);
                 else
                   ERR("Try eglCreateImage on EGL with no support");
                 if (!n->egl_surface)
                   ERR("eglCreateImage() for %p failed", buffer);
                 vendor = (const char *)glGetString(GL_VENDOR);
                 if (vendor && strstr (vendor, "Qualcomm") != NULL)
                   n->recreate = 1;
                 im->native.yinvert     = 1;
                 im->native.loose       = 0;
                 im->native.data        = n;
                 im->native.func.data   = re;
                 im->native.func.bind   = _native_bind_cb;
                 im->native.func.unbind = _native_unbind_cb;
                 im->native.func.free   = _native_free_cb;
                 im->native.target      = GL_TEXTURE_EXTERNAL_OES;
                 im->native.mipmap      = 0;
                 im->native.rot         = ns->data.tizen.rot;
                 im->native.ratio       = ns->data.tizen.ratio;
                 im->native.flip        = ns->data.tizen.flip;
                 evas_gl_common_image_native_enable(im);
              }
         }
    }
  return im;
}

static void *
eng_image_native_get(void *data __UNUSED__, void *image)
{
   Evas_GL_Image *im = image;
   Native *n;
   if (!im) return NULL;
   n = im->native.data;
   if (!n) return NULL;
   return &(n->ns);
}

#if 0 // filtering disabled
static void
eng_image_draw_filtered(void *data, void *context, void *surface,
                        void *image, Evas_Filter_Info *filter)
{
   Render_Engine *re = data;

   if (!image) return;
   eng_window_use(re->win);
   evas_gl_common_context_target_surface_set(re->win->gl_context, surface);
   re->win->gl_context->dc = context;

   evas_gl_common_filter_draw(re->win->gl_context, image, filter);
}

static Filtered_Image *
eng_image_filtered_get(void *im, uint8_t *key, size_t keylen)
{
   return evas_gl_common_image_filtered_get(im, key, keylen);
}

static Filtered_Image *
eng_image_filtered_save(void *im, void *fim, uint8_t *key, size_t keylen)
{
   return evas_gl_common_image_filtered_save(im, fim, key, keylen);
}

static void
eng_image_filtered_free(void *im, Filtered_Image *fim)
{
   evas_gl_common_image_filtered_free(im, fim);
}
#endif


//
//
/////////////////////////////////////////////////////////////////////////

static void *
eng_image_load(void *data, const char *file, const char *key, int *error, Evas_Image_Load_Opts *lo)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   *error = EVAS_LOAD_ERROR_NONE;
   eng_window_use(re->win);
   return evas_gl_common_image_load(re->win->gl_context, file, key, lo, error);
}

static void *
eng_image_new_from_data(void *data, int w, int h, DATA32 *image_data, int alpha, int cspace)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   eng_window_use(re->win);
   return evas_gl_common_image_new_from_data(re->win->gl_context, w, h, image_data, alpha, cspace);
}

static void *
eng_image_new_from_copied_data(void *data, int w, int h, DATA32 *image_data, int alpha, int cspace)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   eng_window_use(re->win);
   return evas_gl_common_image_new_from_copied_data(re->win->gl_context, w, h, image_data, alpha, cspace);
}

static void
eng_image_free(void *data, void *image)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   if (!image) return;
   eng_window_use(re->win);
   evas_gl_common_image_free(image);
}

static void
eng_image_size_get(void *data __UNUSED__, void *image, int *w, int *h)
{
   if (!image)
     {
        *w = 0;
        *h = 0;
        return;
     }
   if (w) *w = ((Evas_GL_Image *)image)->w;
   if (h) *h = ((Evas_GL_Image *)image)->h;
}

static void *
eng_image_size_set(void *data, void *image, int w, int h)
{
   Render_Engine *re;
   Evas_GL_Image *im = image;
   Evas_GL_Image *im_old;

   re = (Render_Engine *)data;
   if (!im) return NULL;
   if (im->native.data)
     {
        im->w = w;
        im->h = h;
        return image;
     }
   eng_window_use(re->win);
   if ((im->tex) && (im->tex->pt->dyn.img))
     {
        evas_gl_common_texture_free(im->tex);
        im->tex = NULL;
        im->w = w;
        im->h = h;
        im->tex = evas_gl_common_texture_dynamic_new(im->gc, im);
        return image;
     }
   im_old = image;

   switch (eng_image_colorspace_get(data, image))
     {
      case EVAS_COLORSPACE_YCBCR422P601_PL:
      case EVAS_COLORSPACE_YCBCR422P709_PL:
      case EVAS_COLORSPACE_YCBCR422601_PL:
      case EVAS_COLORSPACE_YCBCR420NV12601_PL:
      case EVAS_COLORSPACE_YCBCR420TM12601_PL:
         w &= ~0x1;
         break;
     }

   evas_gl_common_image_alloc_ensure(im);
   if ((im_old) &&
       (im_old->im) &&
       ((int)im_old->im->cache_entry.w == w) &&
       ((int)im_old->im->cache_entry.h == h))
     return image;
   if (im_old)
     {
        im = evas_gl_common_image_new(re->win->gl_context, w, h,
                                      eng_image_alpha_get(data, image),
                                      eng_image_colorspace_get(data, image));
        /*
	evas_common_load_image_data_from_file(im_old->im);
	if (im_old->im->image->data)
	  {
	     evas_common_blit_rectangle(im_old->im, im->im, 0, 0, w, h, 0, 0);
	     evas_common_cpu_end_opt();
	  }
 */
        evas_gl_common_image_free(im_old);
     }
   else
     im = evas_gl_common_image_new(re->win->gl_context, w, h, 1, EVAS_COLORSPACE_ARGB8888);
   return im;
}

static void *
eng_image_dirty_region(void *data, void *image, int x, int y, int w, int h)
{
   Render_Engine *re;
   Evas_GL_Image *im = image;

   re = (Render_Engine *)data;
   if (!image) return NULL;
   if (im->native.data) return image;
   eng_window_use(re->win);
   evas_gl_common_image_dirty(image, x, y, w, h);
   return image;
}

static void *
eng_image_data_get(void *data, void *image, int to_write, DATA32 **image_data, int *err)
{
   Render_Engine *re;
   Evas_GL_Image *im;
   int error;

   re = (Render_Engine *)data;
   if (!image)
     {
        *image_data = NULL;
        if (err) *err = EVAS_LOAD_ERROR_GENERIC;
        return NULL;
     }
   im = image;
   if (im->native.data)
     {
        *image_data = NULL;
        if (err) *err = EVAS_LOAD_ERROR_NONE;
        return im;
     }

#ifdef GL_GLES
   eng_window_use(re->win);

   if ((im->tex) && (im->tex->pt) && (im->tex->pt->dyn.img) && (im->cs.space == EVAS_COLORSPACE_ARGB8888))
     {
        if (im->tex->pt->dyn.checked_out > 0)
          {
             im->tex->pt->dyn.checked_out++;
             *image_data = im->tex->pt->dyn.data;
             if (err) *err = EVAS_LOAD_ERROR_NONE;
             return im;
          }
#ifdef HAVE_NATIVE_BUFFER
        native_buffer_lock (im->tex->pt->dyn.buffer,
                            NATIVE_BUFFER_USAGE_CPU,
                            NATIVE_BUFFER_ACCESS_OPTION_READ|NATIVE_BUFFER_ACCESS_OPTION_WRITE,
                            (void **)&im->tex->pt->dyn.data);
        *image_data = im->tex->pt->dyn.data;
#else
        *image_data = im->tex->pt->dyn.data = glsym_eglMapImageSEC(re->win->egl_disp,
                                                                   im->tex->pt->dyn.img,
                                                                   EGL_MAP_GL_TEXTURE_DEVICE_CPU_SEC,
                                                                   EGL_MAP_GL_TEXTURE_OPTION_WRITE_SEC);
#endif
        if (!im->tex->pt->dyn.data)
          {
             if (err) *err = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
             GLERR(__FUNCTION__, __FILE__, __LINE__, "");
             return im;
          }
        im->tex->pt->dyn.checked_out++;

        if (err) *err = EVAS_LOAD_ERROR_NONE;
        return im;
     }
#else
   if ((im->tex) && (im->tex->pt) && (im->tex->pt->dyn.data))
     {
        *image_data = im->tex->pt->dyn.data;
        if (err) *err = EVAS_LOAD_ERROR_NONE;
        return im;
     }

   eng_window_use(re->win);
#endif

   /* Engine can be fail to create texture after cache drop like eng_image_content_hint_set function,
        so it is need to add code which check im->im's NULL value*/

   if (!im->im)
    {
       *image_data = NULL;
       if (err) *err = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
       return NULL;
    }

   error = evas_cache_image_load_data(&im->im->cache_entry);
   evas_gl_common_image_alloc_ensure(im);
   switch (im->cs.space)
     {
      case EVAS_COLORSPACE_ARGB8888:
      case EVAS_COLORSPACE_AGRY88:
      case EVAS_COLORSPACE_GRY8:
         if (to_write)
           {
              if (im->references > 1)
                {
                   Evas_GL_Image *im_new;

                   im_new = evas_gl_common_image_new_from_copied_data
                      (im->gc, im->im->cache_entry.w, im->im->cache_entry.h,
                       im->im->image.data,
                       eng_image_alpha_get(data, image),
                       eng_image_colorspace_get(data, image));
                   if (!im_new)
                     {
                        *image_data = NULL;
                        if (err) *err = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
                        return NULL;
                     }
                   evas_gl_common_image_free(im);
                   im = im_new;
                }
              else
                evas_gl_common_image_dirty(im, 0, 0, 0, 0);
           }
         *image_data = im->im->image.data;
         break;
      case EVAS_COLORSPACE_YCBCR422P601_PL:
      case EVAS_COLORSPACE_YCBCR422P709_PL:
      case EVAS_COLORSPACE_YCBCR422601_PL:
      case EVAS_COLORSPACE_YCBCR420NV12601_PL:
      case EVAS_COLORSPACE_YCBCR420TM12601_PL:
         *image_data = im->cs.data;
         break;
      case EVAS_COLORSPACE_ETC1:
      case EVAS_COLORSPACE_RGB8_ETC2:
      case EVAS_COLORSPACE_RGBA8_ETC2_EAC:
      case EVAS_COLORSPACE_ETC1_ALPHA:
         ERR("This image is encoded in ETC1 or ETC2, not returning any data");
         error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
         *image_data = NULL;
         break;
      default:
         abort();
         break;
     }
   if (err) *err = error;
   return im;
}

static void *
eng_image_data_put(void *data, void *image, DATA32 *image_data)
{
   Render_Engine *re;
   Evas_GL_Image *im, *im2;

   re = (Render_Engine *)data;
   if (!image) return NULL;
   im = image;
   if (im->native.data) return image;
   eng_window_use(re->win);
   evas_gl_common_image_alloc_ensure(im);
   if ((im->tex) && (im->tex->pt)
       && (im->tex->pt->dyn.data)
       && (im->cs.space == EVAS_COLORSPACE_ARGB8888))
     {
        if (im->tex->pt->dyn.data == image_data)
          {
             if (im->tex->pt->dyn.checked_out > 0)
               {
                 im->tex->pt->dyn.checked_out--;
#ifdef GL_GLES
                 if (im->tex->pt->dyn.checked_out == 0)
                   {
#ifdef HAVE_NATIVE_BUFFER
                      native_buffer_unlock(im->tex->pt->dyn.buffer);
#else
                      glsym_eglUnmapImageSEC(re->win->egl_disp, im->tex->pt->dyn.img, EGL_MAP_GL_TEXTURE_DEVICE_CPU_SEC);
#endif
                    }
#endif
                }

             return image;
          }
        im2 = eng_image_new_from_data(data, im->w, im->h, image_data,
                                      eng_image_alpha_get(data, image),
                                      eng_image_colorspace_get(data, image));
        if (!im2) return im;
        evas_gl_common_image_free(im);
        im = im2;
        evas_gl_common_image_dirty(im, 0, 0, 0, 0);
        return im;
     }
   switch (im->cs.space)
     {
      case EVAS_COLORSPACE_ARGB8888:
         if ((!im->im) || (image_data != im->im->image.data))
           {
              im2 = eng_image_new_from_data(data, im->w, im->h, image_data,
                                            eng_image_alpha_get(data, image),
                                            eng_image_colorspace_get(data, image));
              if (!im2) return im;
              evas_gl_common_image_free(im);
              im = im2;
           }
         break;
      case EVAS_COLORSPACE_YCBCR422P601_PL:
      case EVAS_COLORSPACE_YCBCR422P709_PL:
      case EVAS_COLORSPACE_YCBCR422601_PL:
      case EVAS_COLORSPACE_YCBCR420NV12601_PL:
      case EVAS_COLORSPACE_YCBCR420TM12601_PL:
         if (image_data != im->cs.data)
           {
              if (im->cs.data)
                {
                   if (!im->cs.no_free) free(im->cs.data);
                }
              im->cs.data = image_data;
           }
         evas_gl_common_image_dirty(im, 0, 0, 0, 0);
         break;
      default:
         abort();
         break;
     }
   return im;
}

static void
eng_image_data_preload_request(void *data __UNUSED__, void *image, const void *target)
{
   Evas_GL_Image *gim = image;
   RGBA_Image *im;

   if (!gim) return;
   if (gim->native.data) return;
   im = (RGBA_Image *)gim->im;
   if (!im) return;
   evas_cache_image_preload_data(&im->cache_entry, target);
}

static void
eng_image_data_preload_cancel(void *data __UNUSED__, void *image, const void *target)
{
   Evas_GL_Image *gim = image;
   RGBA_Image *im;

   if (!gim) return;
   if (gim->native.data) return;
   im = (RGBA_Image *)gim->im;
   if (!im) return;
   evas_cache_image_preload_cancel(&im->cache_entry, target);
}

static void
eng_image_draw(void *data, void *context, void *surface, void *image, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, int dst_w, int dst_h, int smooth)
{
   Render_Engine *re;
   re = (Render_Engine *)data;
   Evas_GL_Image *im = image;
   Evas_GL_Image *surf = surface;
   Native *n;

   if (!im) return;
   n = im->native.data;

   if ((n) && (n->ns.type == EVAS_NATIVE_SURFACE_OPENGL) &&
       (n->ns.data.opengl.framebuffer_id == 0) &&
       re->func.get_pixels)
     {
        int map_tex = 0;
        //DBG("Rendering Directly to the window: %p", data);

        re->win->gl_context->dc = context;
        if (surface != re->win->gl_context->def_surface)
          {
             map_tex = surf->tex->pt->texture;
             dst_y = re->win->gl_context->h - dst_y - dst_h;
          }

        // Call end tile if it's being used
        if ((re->win->gl_context->master_clip.enabled) &&
            (re->win->gl_context->master_clip.w > 0) &&
            (re->win->gl_context->master_clip.h > 0))
          {
             // Pass the preserve flag info the evas_gl
             evgl_direct_partial_info_set(re->win->gl_context->preserve_bit);
          }

        re->func.x = re->win->gl_context->dc->clip.x;
        re->func.y = re->win->gl_context->dc->clip.y;
        re->func.w = re->win->gl_context->dc->clip.w;
        re->func.h = re->win->gl_context->dc->clip.h;

        // Set necessary info for direct rendering
        evgl_direct_info_set(re->win->gl_context->w,
                             re->win->gl_context->h,
                             map_tex?0:re->win->gl_context->rot,
                             map_tex,
                             dst_x, dst_y, dst_w, dst_h,
                             re->win->gl_context->dc->clip.x,
                             re->win->gl_context->dc->clip.y,
                             re->win->gl_context->dc->clip.w,
                             re->win->gl_context->dc->clip.h,
                             n->ns.data.opengl.texture_id);

        // Call pixel get function
        re->func.get_pixels(re->func.get_pixels_data, re->func.obj);
        re->context_current = EINA_FALSE;

        // Call end tile if it's being used
        if ((re->win->gl_context->master_clip.enabled) &&
            (re->win->gl_context->master_clip.w > 0) &&
            (re->win->gl_context->master_clip.h > 0))
          {
             evgl_direct_partial_render_end();
             evgl_direct_partial_info_clear();
             re->win->gl_context->preserve_bit = GL_COLOR_BUFFER_BIT0_QCOM;
          }

        // Reset direct rendering info
        evgl_direct_info_clear();
     }
   else
     {
        if (re->context_optimize)
          {
             if (!re->context_current)
               {
                  re->context_current = EINA_TRUE;
                  eng_window_use(re->win);
               }
          }
        else
           eng_window_use(re->win);

        evas_gl_common_context_target_surface_set(re->win->gl_context, surface);
        re->win->gl_context->dc = context;
        evas_gl_common_image_draw(re->win->gl_context, image,
                                  src_x, src_y, src_w, src_h,
                                  dst_x, dst_y, dst_w, dst_h,
                                  smooth);
     }
}

static void
eng_image_scale_hint_set(void *data __UNUSED__, void *image, int hint)
{
   if (image) evas_gl_common_image_scale_hint_set(image, hint);
}

static int
eng_image_scale_hint_get(void *data __UNUSED__, void *image)
{
   Evas_GL_Image *gim = image;
   if (!gim) return EVAS_IMAGE_SCALE_HINT_NONE;
   return gim->scale_hint;
}

static void
eng_image_map_draw(void *data, void *context, void *surface, void *image, RGBA_Map *m, int smooth, int level)
{
   Evas_GL_Image *gim = image;
   Render_Engine *re;

   re = (Render_Engine *)data;
   if (!image) return;

   if (re->context_optimize)
     {
        if (!re->context_current)
          {
             re->context_current = EINA_TRUE;
             eng_window_use(re->win);
          }
     }
   else
      eng_window_use(re->win);

   evas_gl_common_context_target_surface_set(re->win->gl_context, surface);
   re->win->gl_context->dc = context;
   if (m->count != 4)
     {
        // FIXME: nash - you didn't fix this
        abort();
     }
   if ((m->pts[0].x == m->pts[3].x) &&
       (m->pts[1].x == m->pts[2].x) &&
       (m->pts[0].y == m->pts[1].y) &&
       (m->pts[3].y == m->pts[2].y) &&
       (m->pts[0].x <= m->pts[1].x) &&
       (m->pts[0].y <= m->pts[2].y) &&
       (m->pts[0].u == 0) &&
       (m->pts[0].v == 0) &&
       (m->pts[1].u == (gim->w << FP)) &&
       (m->pts[1].v == 0) &&
       (m->pts[2].u == (gim->w << FP)) &&
       (m->pts[2].v == (gim->h << FP)) &&
       (m->pts[3].u == 0) &&
       (m->pts[3].v == (gim->h << FP)) &&
       (m->pts[0].col == 0xffffffff) &&
       (m->pts[1].col == 0xffffffff) &&
       (m->pts[2].col == 0xffffffff) &&
       (m->pts[3].col == 0xffffffff))
     {
        int dx, dy, dw, dh;

        dx = m->pts[0].x >> FP;
        dy = m->pts[0].y >> FP;
        dw = (m->pts[2].x >> FP) - dx;
        dh = (m->pts[2].y >> FP) - dy;

        eng_image_draw(data, context, surface, image,
                       0, 0, gim->w, gim->h, dx, dy, dw, dh, smooth);
     }
   else
     {
        evas_gl_common_image_map_draw(re->win->gl_context, image, m->count, &m->pts[0],
                                      smooth, level);
     }
}

static void
eng_image_map_clean(void *data __UNUSED__, RGBA_Map *m __UNUSED__)
{
}

static void *
eng_image_map_surface_new(void *data, int w, int h, int alpha)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   eng_window_use(re->win);
   return evas_gl_common_image_surface_new(re->win->gl_context, w, h, alpha);
}

static void
eng_image_map_surface_free(void *data, void *surface)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   eng_window_use(re->win);
   evas_gl_common_image_free(surface);
}

static void
eng_image_content_hint_set(void *data, void *image, int hint)
{
   Render_Engine *re;
   re = (Render_Engine *)data;

   if (re) eng_window_use(re->win);
   if (image) evas_gl_common_image_content_hint_set(image, hint);
}

static int
eng_image_content_hint_get(void *data __UNUSED__, void *image)
{
   Evas_GL_Image *gim = image;
   if (!gim) return EVAS_IMAGE_CONTENT_HINT_NONE;
   return gim->content_hint;
}

static void
eng_image_cache_flush(void *data)
{
   Render_Engine *re;
   int tmp_size;

   re = (Render_Engine *)data;

   eng_window_use(re->win);

   tmp_size = evas_common_image_get_cache();
   evas_common_image_set_cache(0);
   evas_common_rgba_image_scalecache_flush();
   evas_gl_common_image_cache_flush(re->win->gl_context);
   evas_common_image_set_cache(tmp_size);
}

static void
eng_image_cache_set(void *data, int bytes)
{
   Render_Engine *re;

   re = (Render_Engine *)data;

   eng_window_use(re->win);

   evas_common_image_set_cache(bytes);
   evas_common_rgba_image_scalecache_size_set(bytes);
   evas_gl_common_image_cache_flush(re->win->gl_context);
}

static int
eng_image_cache_get(void *data __UNUSED__)
{
   return evas_common_image_get_cache();
}

static void
eng_image_stride_get(void *data __UNUSED__, void *image, int *stride)
{
   Evas_GL_Image *im = image;

   if ((im->tex) && (im->tex->pt->dyn.img))
     *stride = im->tex->pt->dyn.stride;
   else
     {
        switch (im->cs.space)
          {
           case EVAS_COLORSPACE_ARGB8888:
             *stride = im->w * 4;
             return;
           case EVAS_COLORSPACE_AGRY88:
             *stride = im->w * 2;
             return;
           case EVAS_COLORSPACE_GRY8:
             *stride = im->w * 1;
             return;
           case EVAS_COLORSPACE_YCBCR422P601_PL:
           case EVAS_COLORSPACE_YCBCR422P709_PL:
           case EVAS_COLORSPACE_YCBCR422601_PL:
           case EVAS_COLORSPACE_YCBCR420NV12601_PL:
           case EVAS_COLORSPACE_YCBCR420TM12601_PL:
             *stride = im->w * 1;
             return;
           default:
             ERR("Requested stride on an invalid format %d", im->cs.space);
             *stride = 0;
             return;
          }
     }
}

static void
eng_font_draw(void *data, void *context, void *surface, Evas_Font_Set *font __UNUSED__, int x, int y, int w __UNUSED__, int h __UNUSED__, int ow __UNUSED__, int oh __UNUSED__, Evas_Text_Props *intl_props)
{
   Render_Engine *re;

   re = (Render_Engine *)data;

   if (re->context_optimize)
     {
        if (!re->context_current)
          {
             re->context_current = EINA_TRUE;
             eng_window_use(re->win);
          }
     }
   else
      eng_window_use(re->win);

   evas_gl_common_context_target_surface_set(re->win->gl_context, surface);
   re->win->gl_context->dc = context;
     {
        // FIXME: put im into context so we can free it
        static RGBA_Image *im = NULL;

        if (!im)
          im = (RGBA_Image *)evas_cache_image_empty(evas_common_image_cache_get());
        im->cache_entry.w = re->win->gl_context->shared->w;
        im->cache_entry.h = re->win->gl_context->shared->h;

        evas_common_draw_context_font_ext_set(context,
                                              re->win->gl_context,
                                              evas_gl_font_texture_new,
                                              evas_gl_font_texture_free,
                                              evas_gl_font_texture_draw);
        evas_common_font_draw_prepare(intl_props);
        evas_common_font_draw(im, context, x, y, intl_props);
        evas_common_draw_context_font_ext_set(context,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL);
     }
}

static Eina_Bool
eng_canvas_alpha_get(void *data, void *info __UNUSED__)
{
   Render_Engine *re = (Render_Engine *)data;
   return re->win->alpha;
}

//--------------------------------//
// Evas GL Related Code
static void *
eng_gl_surface_create(void *data, void *config, int w, int h)
{
   Evas_GL_Config *cfg = (Evas_GL_Config *)config;
   const GLubyte *vendor = glGetString(GL_VENDOR);

   if ((cfg->options_bits & EVAS_GL_OPTIONS_DIRECT)
        && (!getenv("EVAS_GL_PARTIAL_DISABLE")))
     {
        prev_extn_have_buffer_age = extn_have_buffer_age;
        extn_have_buffer_age = 0;
     }

   return evgl_surface_create(data, cfg, 0, NULL, w, h);
}

static void *
eng_gl_pbuffer_surface_create(void *data, void *config, int w, int h, const int *attrib_list)
{
   Evas_GL_Config *cfg = (Evas_GL_Config *)config;

   return evgl_pbuffer_surface_create(data, cfg, w, h, attrib_list);
}

static int
eng_gl_surface_destroy(void *data, void *surface)
{
   EVGL_Surface  *sfc = (EVGL_Surface *)surface;
   Render_Engine *re = (Render_Engine *)data;

   if ((extn_have_buffer_age  != 1)
        && (extn_have_buffer_age != prev_extn_have_buffer_age)
        && (!getenv("EVAS_GL_PARTIAL_DISABLE")))
     {
        extn_have_buffer_age = prev_extn_have_buffer_age;
     }

   return evgl_surface_destroy(data, sfc);
}

static void *
eng_gl_context_create(void *data, void *share_context,
                      int version)
{
   EVGL_Context  *sctx = (EVGL_Context *)share_context;

   return evgl_context_create(data, sctx, version);
}

static int
eng_gl_context_destroy(void *data, void *context)
{
   EVGL_Context  *ctx = (EVGL_Context *)context;

   return evgl_context_destroy(data, ctx);
}

static int
eng_gl_make_current(void *data, void *surface, void *context)
{
   Render_Engine *re  = (Render_Engine *)data;
   EVGL_Surface  *sfc = (EVGL_Surface *)surface;
   EVGL_Context  *ctx = (EVGL_Context *)context;

   EVGL_Resource *rsc = _evgl_tls_resource_get();

   if ((sfc) && (ctx) && (rsc) && (rsc->id == evgl_engine->main_tid))
     {
        if ((re->win->gl_context->havestuff) ||
            (re->win->gl_context->master_clip.used))
          {
             eng_window_use(re->win);
             evas_gl_common_context_flush(re->win->gl_context);
             if (re->win->gl_context->master_clip.used)
                evas_gl_common_context_done(re->win->gl_context);
          }
     }

   return evgl_make_current(data, sfc, ctx);
}

static void *
eng_gl_current_context_get(void *data EINA_UNUSED)
{
   EVGL_Context *ctx;

   ctx = _evgl_current_context_get();
   if (!ctx)
     return NULL;

#ifdef GL_GLES
   if (eglGetCurrentContext() == (ctx->context))
     return ctx;
   else
     return NULL;
#else
   if (glXGetCurrentContext() == (ctx->context))
     return ctx;
   else
     return NULL;
#endif
}

static void *
eng_gl_current_surface_get(void *data EINA_UNUSED)
{
   EVGL_Context *ctx;

   ctx = _evgl_current_context_get();
   if (!ctx)
     return NULL;

   // Note: We could verify with a call to eglGetCurrentSurface

   return ctx->current_sfc;
}

static int
eng_gl_rotation_angle_get(void *data)
{
   if (!evgl_engine->funcs->rotation_angle_get) return 0;
   return evgl_engine->funcs->rotation_angle_get(data);
}

static const char *
eng_gl_string_query(void *data __UNUSED__, int name)
{
   return evgl_string_query(name);
}

static void *
eng_gl_proc_address_get(void *data EINA_UNUSED, Evas_GL_Ext *ext, const char *name)
{
   void *func = NULL;
   int i = 0;

   // Evas GL extensions
   while (ext[i].name != NULL)
     {
        if (!strcmp(ext[i].name, name))
           return ext[i].func;
        ++i;
     }

   // Allowed GL extensions
   if (!evgl_safe_extension_get(name, &func))
     {
        DBG("The extension '%s' is not safe to use with Evas GL or is not "
            "supported on this platform.", name);
        return NULL;
     }

   if (func)
     return func;

   if (evgl_funcs.proc_address_get)
     return evgl_funcs.proc_address_get(name);

   return NULL;
}

static int
eng_gl_native_surface_get(void *data __UNUSED__, void *surface, void *native_surface)
{
   EVGL_Surface  *sfc = (EVGL_Surface *)surface;
   Evas_Native_Surface *ns = (Evas_Native_Surface *)native_surface;

   return evgl_native_surface_get(sfc, ns);
}

static void *
eng_gl_api_get(void *data __UNUSED__, int version)
{
   return evgl_api_get(version);
}

static void
eng_gl_direct_override_get(void *data __UNUSED__, int *override, int *force_off)
{
   evgl_direct_override_get(override, force_off);
}

static Eina_Bool
eng_gl_surface_direct_renderable_get(void *data, Evas_Native_Surface *ns, Eina_Bool *direct_override)
{
   Render_Engine *re = data;
   Eina_Bool direct_render, client_side_rotation;

   if (!re || !ns) return EINA_FALSE;
   if (!evgl_native_surface_direct_opts_get(ns, &direct_render, &client_side_rotation, direct_override))
     return EINA_FALSE;

   if (!direct_render)
     return EINA_FALSE;

   if ((re->win->gl_context->rot != 0) && (!client_side_rotation))
     return EINA_FALSE;

   return EINA_TRUE;
}

static void
eng_gl_get_pixels_set(void *data, void *get_pixels, void *get_pixels_data, void *obj)
{
   Render_Engine *re = (Render_Engine *)data;

   re->func.get_pixels = get_pixels;
   re->func.get_pixels_data = get_pixels_data;
   re->func.obj = (Evas_Object*)obj;

   if (re->func.get_pixels)
      re->func.clip = 1;
   else
      re->func.clip = 0;
}



static Eina_Bool
eng_gl_surface_lock(void *data, void *surface)
{
   Render_Engine *re = data;
   Evas_GL_Image *im = surface;

   if (!im->tex || !im->tex->pt)
     {
        ERR("Can not lock image that is not a surface!");
        return EINA_FALSE;
     }

   evas_gl_common_context_flush(im->gc);
   im->locked = EINA_TRUE;
   return EINA_TRUE;
}

static Eina_Bool
eng_gl_surface_unlock(void *data, void *surface)
{
   Render_Engine *re = data;
   Evas_GL_Image *im = surface;

   im->locked = EINA_FALSE;
   return EINA_TRUE;
}

static Eina_Bool
eng_gl_surface_read_pixels(void *data, void *surface,
                           int x, int y, int w, int h,
                           Evas_Colorspace cspace, void *pixels)
{
   Render_Engine *re = data;
   Evas_GL_Image *im = surface;

   EINA_SAFETY_ON_NULL_RETURN_VAL(pixels, EINA_FALSE);

   if (!im->locked)
     {
        // For now, this is useless, but let's force clients to lock :)
        ERR("The surface must be locked before reading its pixels!");
        return EINA_FALSE;
     }

   if (cspace != EVAS_COLORSPACE_ARGB8888)
     {
        ERR("Conversion to colorspace %d is not supported!", (int) cspace);
        return EINA_FALSE;
     }

#ifdef GL_GLES
# ifndef GL_FRAMEBUFFER
#  define GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
# endif
#else
# ifndef GL_FRAMEBUFFER
#  define GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
# endif
#endif

   /* Since this is an FBO, the pixels are already in the right Y order.
    * But some devices don't support GL_BGRA, so we still need to convert.
    */

   glsym_glBindFramebuffer(GL_FRAMEBUFFER, im->tex->pt->fb);
   if (im->tex->pt->format == GL_BGRA)
     glReadPixels(x, y, w, h, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
   else
     {
        DATA32 *ptr = pixels;
        int k;

        glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        for (k = w * h; k; --k)
          {
             const DATA32 v = *ptr;
             *ptr++ = (v & 0xFF00FF00)
                   | ((v & 0x00FF0000) >> 16)
                   | ((v & 0x000000FF) << 16);
          }
     }
   glsym_glBindFramebuffer(GL_FRAMEBUFFER, 0);

   evas_common_convert_argb_premul(pixels, w * h);

   return EINA_TRUE;
}

static int
eng_gl_error_get(void *data __UNUSED__)
{
   int err;

   if ((err = _evgl_error_get()) != EVAS_GL_SUCCESS) goto end;

#ifdef GL_GLES
   err = eglGetError() - EGL_SUCCESS;
#else
   Render_Engine *re = data;

   if(!(re->win))
     {
        err = EVAS_GL_BAD_DISPLAY;
        goto end;
     }

   if(!(re->info))
     {
        err = EVAS_GL_BAD_SURFACE;
     }
#endif

end:
    _evgl_error_set(EVAS_GL_SUCCESS);
   return err;
}

static Eina_Bool
eng_gl_surface_query(void *data, void *surface, int attr, void *value)
{
   Render_Engine *re  = (Render_Engine *)data;
   EVGL_Surface  *sfc = (EVGL_Surface *)surface;

#ifdef GL_GLES
   if (sfc->pbuffer.is_pbuffer)
     {
        // This is a real EGL surface, let's just call EGL directly
        int val;
        Eina_Bool ok;

        ok = eglQuerySurface(re->win->egl_disp, sfc->pbuffer.native_surface, attr, &val);
        if (!ok) return EINA_FALSE;
        switch (attr)
          {
           case EVAS_GL_TEXTURE_FORMAT:
             if (val == EGL_TEXTURE_RGB)
               *((int *) value) = EVAS_GL_RGB_888;
             else if (val == EGL_TEXTURE_RGBA)
               *((int *) value) = EVAS_GL_RGBA_8888;
             else // if (val == EGL_NO_TEXTURE)
               *((int *) value) = EVAS_GL_NO_FBO;
             break;
           case EVAS_GL_TEXTURE_TARGET:
             if (val == EGL_TEXTURE_2D)
               *((int *) value) = val;
             else
               *((int *) value) = 0;
             break;
           default:
             *((int *) value) = val;
             break;
          }
        return EINA_TRUE;
     }
   else
     {
        // Since this is a fake surface (shared with evas), we must filter the
        // queries...
        switch (attr)
          {
           // TODO: Add support for whole config get
           /*
           case EVAS_GL_CONFIG_ID:
             *((int *) value) = sfc->cfg_index;
             return EINA_TRUE;
             */
           case EVAS_GL_WIDTH:
             *((int *) value) = sfc->w;
             return EINA_TRUE;
           case EVAS_GL_HEIGHT:
             *((int *) value) = sfc->h;
             return EINA_TRUE;
           case EVAS_GL_TEXTURE_FORMAT:
             // FIXME: Check the possible color formats
             if (sfc->color_buf)
               {
                  if ((sfc->color_fmt == GL_RGBA) || (sfc->color_fmt == GL_BGRA))
                    {
                       *((Evas_GL_Color_Format *) value) = EVAS_GL_RGBA_8888;
                       return EINA_TRUE;
                    }
                  else if (sfc->color_fmt == GL_RGB)
                    {
                       *((Evas_GL_Color_Format *) value) = EVAS_GL_RGB_888;
                       return EINA_TRUE;
                    }
               }
             *((Evas_GL_Color_Format *) value) = EVAS_GL_NO_FBO;
             return EINA_TRUE;
           case EVAS_GL_TEXTURE_TARGET:
             if (sfc->color_buf)
               *((int *) value) = EVAS_GL_TEXTURE_2D;
             else
               *((int *) value) = 0;
             return EINA_TRUE;
           // TODO: Add support for this:
           /*
           case EVAS_GL_MULTISAMPLE_RESOLVE:
             *((int *) value) = sfc->msaa_samples;
             return EINA_TRUE;
             */
           // TODO: Add support for mipmaps
           /*
           case EVAS_GL_MIPMAP_TEXTURE:
           case EVAS_GL_MIPMAP_LEVEL:
             return eglQuerySurface(re->win->egl_disp, re->win->egl_surface[0],
                                    attr, (int *) value);
             */
           default: break;
          }
        _evgl_error_set(EVAS_GL_BAD_ATTRIBUTE);
        return EINA_FALSE;
     }
#else
   ERR("GLX support for surface_query is not implemented!");
   return EINA_FALSE;
#endif
}

//--------------------------------//

static int
eng_image_load_error_get(void *data __UNUSED__, void *image)
{
   Evas_GL_Image *im;

   if (!image) return EVAS_LOAD_ERROR_NONE;
   im = image;
   return im->im->cache_entry.load_error;
}

static Eina_Bool
eng_image_animated_get(void *data __UNUSED__, void *image)
{
   Evas_GL_Image *gim = image;
   Image_Entry *im;

   if (!gim) return EINA_FALSE;
   im = (Image_Entry *)gim->im;
   if (!im) return EINA_FALSE;

   return im->flags.animated;
}

static int
eng_image_animated_frame_count_get(void *data __UNUSED__, void *image)
{
   Evas_GL_Image *gim = image;
   Image_Entry *im;

   if (!gim) return -1;
   im = (Image_Entry *)gim->im;
   if (!im) return -1;

   if (!im->flags.animated) return -1;
   return im->frame_count;
}

static Evas_Image_Animated_Loop_Hint
eng_image_animated_loop_type_get(void *data __UNUSED__, void *image)
{
   Evas_GL_Image *gim = image;
   Image_Entry *im;

   if (!gim) return EVAS_IMAGE_ANIMATED_HINT_NONE;
   im = (Image_Entry *)gim->im;
   if (!im) return EVAS_IMAGE_ANIMATED_HINT_NONE;

   if (!im->flags.animated) return EVAS_IMAGE_ANIMATED_HINT_NONE;
   return im->loop_hint;
}

static int
eng_image_animated_loop_count_get(void *data __UNUSED__, void *image)
{
   Evas_GL_Image *gim = image;
   Image_Entry *im;

   if (!gim) return -1;
   im = (Image_Entry *)gim->im;
   if (!im) return -1;

   if (!im->flags.animated) return -1;
   return im->loop_count;
}

static double
eng_image_animated_frame_duration_get(void *data __UNUSED__, void *image, int start_frame, int frame_num)
{
   Evas_GL_Image *gim = image;
   Image_Entry *im;

   if (!gim) return -1;
   im = (Image_Entry *)gim->im;
   if (!im) return -1;

   if (!im->flags.animated) return -1;
   return evas_common_load_rgba_image_frame_duration_from_file(im, start_frame, frame_num);
}

static Eina_Bool
eng_image_animated_frame_set(void *data __UNUSED__, void *image, int frame_index)
{
   Evas_GL_Image *gim = image;
   Image_Entry *im;

   if (!gim) return EINA_FALSE;
   im = (Image_Entry *)gim->im;
   if (!im) return EINA_FALSE;

   if (!im->flags.animated) return EINA_FALSE;
   if (im->cur_frame == frame_index) return EINA_FALSE;

   im->cur_frame = frame_index;
   return EINA_TRUE;
}

static Eina_Bool
eng_image_can_region_get(void *data __UNUSED__, void *image)
{
   Evas_GL_Image *gim = image;
   Image_Entry *im;
   if (!gim) return EINA_FALSE;
   im = (Image_Entry *)gim->im;
   if (!im) return EINA_FALSE;
   return ((Evas_Image_Load_Func*) im->info.loader)->do_region;
}


static void
eng_image_max_size_get(void *data, int *maxw, int *maxh)
{
   Render_Engine *re = (Render_Engine *)data;
   if (maxw) *maxw = re->win->gl_context->shared->info.max_texture_size;
   if (maxh) *maxh = re->win->gl_context->shared->info.max_texture_size;
}

static void
eng_context_flush(void *data)
{
   Render_Engine *re;
   re = (Render_Engine *)data;

   if ((re->win->gl_context->havestuff) ||
     (re->win->gl_context->master_clip.used))
   {
      eng_window_use(re->win);
      evas_gl_common_context_flush(re->win->gl_context);
      if (re->win->gl_context->master_clip.used)
         evas_gl_common_context_done(re->win->gl_context);
   }
}

static int
eng_gl_ext_buffer_age_get(void *data)
{
   Render_Engine *re;
   re = (Render_Engine *)(data);

   if (!re)
     {
        ERR("No valid engine set yet.");
        return 0;
     }

   return re->prev_age;
}


static int
eng_gl_ext_update_region_get(void *data, int *x, int *y, int *w, int *h)
{
   Render_Engine *re;
   re = (Render_Engine *)(data);

   if (!re)
     {
        ERR("No valid engine set yet.");
        return 0;
     }

   if (re->func.clip)
     {
        *x = re->func.x;
        *y = re->func.y;
        *w = re->func.w;
        *h = re->func.h;

        ERR(" Update Region: [%d %d %d %d]", *x, *y, *w, *h);
        return 1;
     }
   else
      return 0;
}

static void *
eng_gl_ext_surface_from_native_create(void *data, void *config, int target, void *native)
{
   int w = 0, h = 0;
   Render_Engine *re = (Render_Engine *)data;
   Evas_GL_Config *cfg = (Evas_GL_Config *)config;
   const GLubyte *vendor = glGetString(GL_VENDOR);

   // Check the validity of the target and retrive width, height
   if (!eng_native_config_check(re->info->info.display, cfg, target, native, &w, &h))
     {
        ERR("Error retrieving pixmap infomration native depth");
        _evgl_error_set(EVAS_GL_BAD_NATIVE_PIXMAP);
        return NULL;
     }

   if ((cfg->options_bits & EVAS_GL_OPTIONS_DIRECT)
        && (!getenv("EVAS_GL_PARTIAL_DISABLE")))
     {
        prev_extn_have_buffer_age = extn_have_buffer_age;
        extn_have_buffer_age = 0;
     }

   return evgl_surface_create(data, cfg, target, native, w, h);
}

static int
eng_gl_ext_surface_is_texture(void *data, void *surface)
{
   Render_Engine *re = (Render_Engine *)data;
   EVGL_Surface  *sfc = (EVGL_Surface *)surface;

   if (!re)
     {
        ERR("No valid engine set yet.");
        return 0;
     }

   return evgl_surface_is_texture(data, sfc);
}

static void
eng_get_pixels_render_post(void *data)
{
   Render_Engine *re;

   re = (Render_Engine *)data;
   re->context_current = EINA_FALSE;
}

static void
eng_gl_engine_init(void *data)
{
   Render_Engine *re;

   re = (Render_Engine *)data;

   if (!evgl_initted)
     {
        eng_window_use(re->win);
        evgl_engine_init(re, &evgl_funcs);
        evgl_initted = 1;
     }
}

static int
module_open(Evas_Module *em)
{
   static Eina_Bool xrm_inited = EINA_FALSE;
   if (!xrm_inited)
     {
        xrm_inited = EINA_TRUE;
        XrmInitialize();
     }

   if (!em) return 0;
   if (!evas_gl_common_module_open()) return 0;
   /* get whatever engine module we inherit from */
   if (!_evas_module_engine_inherit(&pfunc, "software_generic")) return 0;
   if (_evas_engine_GL_X11_log_dom < 0)
     _evas_engine_GL_X11_log_dom = eina_log_domain_register
       ("evas-gl_x11", EVAS_DEFAULT_LOG_COLOR);
   if (_evas_engine_GL_X11_log_dom < 0)
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
   ORD(context_cutout_add);
   ORD(context_cutout_clear);
   ORD(output_flush);
   ORD(output_idle_flush);
   ORD(output_dump);
   ORD(rectangle_draw);
   ORD(line_draw);
   ORD(polygon_point_add);
   ORD(polygon_points_clear);
   ORD(polygon_draw);

   ORD(image_load);
   ORD(image_new_from_data);
   ORD(image_new_from_copied_data);
   ORD(image_free);
   ORD(image_size_get);
   ORD(image_size_set);
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
   ORD(image_comment_get);
   ORD(image_format_get);
   ORD(image_colorspace_set);
   ORD(image_colorspace_get);
   ORD(image_can_region_get);
   ORD(image_mask_create);
   ORD(image_native_set);
   ORD(image_native_get);
#if 0 // filtering disabled
   ORD(image_draw_filtered);
   ORD(image_filtered_get);
   ORD(image_filtered_save);
   ORD(image_filtered_free);
#endif

   ORD(font_draw);

   ORD(image_scale_hint_set);
   ORD(image_scale_hint_get);
   ORD(image_stride_get);

   ORD(image_map_draw);
   ORD(image_map_surface_new);
   ORD(image_map_surface_free);
   ORD(image_map_clean);

   ORD(image_content_hint_set);
   ORD(image_content_hint_get);

   ORD(image_cache_flush);
   ORD(image_cache_set);
   ORD(image_cache_get);

   ORD(gl_surface_create);
   ORD(gl_pbuffer_surface_create);
   ORD(gl_surface_destroy);
   ORD(gl_context_create);
   ORD(gl_context_destroy);
   ORD(gl_make_current);
   ORD(gl_string_query);
   ORD(gl_proc_address_get);
   ORD(gl_native_surface_get);
   ORD(gl_api_get);
   ORD(gl_direct_override_get);
   ORD(gl_surface_direct_renderable_get);
   ORD(gl_get_pixels_set);
   ORD(gl_surface_lock);
   ORD(gl_surface_read_pixels);
   ORD(gl_surface_unlock);
   ORD(gl_error_get);
   ORD(gl_surface_query);
   ORD(gl_current_context_get);
   ORD(gl_current_surface_get);
   ORD(gl_rotation_angle_get);

   ORD(image_load_error_get);

   /* now advertise out own api */
   ORD(image_animated_get);
   ORD(image_animated_frame_count_get);
   ORD(image_animated_loop_type_get);
   ORD(image_animated_loop_count_get);
   ORD(image_animated_frame_duration_get);
   ORD(image_animated_frame_set);

   ORD(image_max_size_get);

   ORD(context_flush);

   ORD(gl_ext_buffer_age_get);
   ORD(gl_ext_update_region_get);
   ORD(gl_ext_surface_from_native_create);
   ORD(gl_ext_surface_is_texture);

   ORD(get_pixels_render_post);

   ORD(gl_engine_init);

   /* now advertise out own api */
   em->functions = (void *)(&func);
   return 1;
}

static void
module_close(Evas_Module *em __UNUSED__)
{
    eina_log_domain_unregister(_evas_engine_GL_X11_log_dom);
    evas_gl_common_module_close();
}

static Evas_Module_Api evas_modapi =
{
   EVAS_MODULE_API_VERSION,
   "gl_x11",
   "none",
   {
     module_open,
     module_close
   }
};

EVAS_MODULE_DEFINE(EVAS_MODULE_TYPE_ENGINE, engine, gl_x11);

#ifndef EVAS_STATIC_BUILD_GL_XLIB
EVAS_EINA_MODULE_DEFINE(engine, gl_x11);
#endif

/* vim:set ts=8 sw=3 sts=3 expandtab cino=>5n-2f0^-2{2(0W1st0 :*/
