#include "evas_engine.h"

static Eina_TLS _evas_gl_x11_window_key = 0;
static Eina_TLS _context_key = 0;

#ifdef GL_GLES
typedef EGLContext GLContext;
#else
#warning FIXME TLS Thread safety code not implemented for GLX!!!
// FIXME: this will only work for 1 display connection (glx land can have > 1)
typedef GLXContext GLContext;
static GLXContext rgba_context = 0;
static GLXFBConfig fbconf = 0;
static GLXFBConfig rgba_fbconf = 0;
#endif

// fixme: something is up/wrong here - dont know what tho...
//#define NEWGL 1

static XVisualInfo *_evas_gl_x11_vi = NULL;
static XVisualInfo *_evas_gl_x11_rgba_vi = NULL;
static Colormap     _evas_gl_x11_cmap = 0;
static Colormap     _evas_gl_x11_rgba_cmap = 0;

// FIXME: This should use an atomic int. But it's a useless variable...
static int win_count = 0;
static Eina_Bool initted = EINA_FALSE;

Eina_Bool
eng_init(void)
{
   if (initted)
     return EINA_TRUE;

   // FIXME: These resources are never released
   if (!eina_tls_new(&_evas_gl_x11_window_key))
     goto error;
   if (!eina_tls_new(&_context_key))
     goto error;

   eina_tls_set(_evas_gl_x11_window_key, NULL);
   eina_tls_set(_context_key, NULL);

   initted = EINA_TRUE;
   return EINA_TRUE;

error:
   ERR("Could not create TLS key!");
   return EINA_FALSE;
}

static inline Evas_GL_X11_Window *
_tls_evas_gl_x11_window_get(void)
{
   if (!initted) eng_init();
   return eina_tls_get(_evas_gl_x11_window_key);
}

static inline Eina_Bool
_tls_evas_gl_x11_window_set(Evas_GL_X11_Window *xwin)
{
   if (!initted) eng_init();
   return eina_tls_set(_evas_gl_x11_window_key, xwin);
}

static inline GLContext
_tls_context_get(void)
{
   if (!initted) eng_init();
   return eina_tls_get(_context_key);
}

static inline Eina_Bool
_tls_context_set(GLContext ctx)
{
   if (!initted) eng_init();
   return eina_tls_set(_context_key, ctx);
}

Evas_GL_X11_Window *
eng_window_new(Display *disp,
               Drawable win,
               int      screen,
               Visual  *vis,
               Colormap cmap,
               int      depth,
               int      w,
               int      h,
               int      indirect,
               int      alpha,
               int      rot,
               int      depth_bits,
               int      stencil_bits,
               int      msaa_bits,
               Drawable offscreen)
{
   Evas_GL_X11_Window *gw;
   GLContext context;

#ifdef GL_GLES
   int context_attrs[3];
   int config_attrs[40];
   EGLConfig configs[200];
   int major_version, minor_version;
   int i, num, idx;
   Eina_Bool found;
   const char *s;
//   XVisualInfo xVisual_info;
#endif
   const GLubyte *vendor, *renderer, *version;
   int blacklist = 0, val = 0;

   if (!_evas_gl_x11_vi) return NULL;

   gw = calloc(1, sizeof(Evas_GL_X11_Window));
   if (!gw) return NULL;

   win_count++;
   gw->disp = disp;
   gw->win = win;
   gw->screen = screen;
   gw->visual = vis;
   gw->colormap = cmap;
   gw->depth = depth;
   gw->alpha = alpha;
   gw->w = w;
   gw->h = h;
   gw->rot = rot;
   gw->depth_bits = depth_bits;
   gw->stencil_bits = stencil_bits;
   gw->msaa_bits = msaa_bits;
   gw->win_back = offscreen;

   if (gw->alpha && _evas_gl_x11_rgba_vi)
     gw->visualinfo = _evas_gl_x11_rgba_vi;
   else
     gw->visualinfo = _evas_gl_x11_vi;

// EGL / GLES
#ifdef GL_GLES
   gw->egl_disp = eglGetDisplay((EGLNativeDisplayType)(gw->disp));
   if (!gw->egl_disp)
     {
        ERR("eglGetDisplay() fail. code=%#x", eglGetError());
        eng_window_free(gw);
        return NULL;
     }
   if (!eglInitialize(gw->egl_disp, &major_version, &minor_version))
     {
        ERR("eglInitialize() fail. code=%#x", eglGetError());
        eng_window_free(gw);
        return NULL;
     }
   if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)
     {
        ERR("eglBindAPI() fail. code=%#x", eglGetError());
        eng_window_free(gw);
        return NULL;
     }
   /* Find matching config & visual */
try_again:
   i = 0;

   context_attrs[0] = EGL_CONTEXT_CLIENT_VERSION;
   context_attrs[1] = 2;
   context_attrs[2] = EGL_NONE;

   config_attrs[i++] = EGL_SURFACE_TYPE;
   if (gw->win_back)
     config_attrs[i++] = EGL_PIXMAP_BIT;
   else
     config_attrs[i++] = EGL_WINDOW_BIT;
   config_attrs[i++] = EGL_RENDERABLE_TYPE;
   config_attrs[i++] = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES_BIT;
# if 0
// FIXME: n900 - omap3 sgx libs break here
   config_attrs[i++] = EGL_RED_SIZE;
   config_attrs[i++] = 1;
   config_attrs[i++] = EGL_GREEN_SIZE;
   config_attrs[i++] = 1;
   config_attrs[i++] = EGL_BLUE_SIZE;
   config_attrs[i++] = 1;
// FIXME: end n900 breakage
# endif

   if (gw->alpha || gw->win_back)
     {
        config_attrs[i++] = EGL_ALPHA_SIZE;
        config_attrs[i++] = 1;
     }

   // Direct Rendering Option for widget (e.g. WEBKIT)
   /*
     * Sometimes, Tizen Widget uses Evas GL with Direct Rendering.
     * This widget also runs with depth/stencil.
     * Unfortunately, Application can not know this widget uses Evas GL that runs with DR, Depth/Stencil buffer.
     * Although application does not set depth/stencil buffer size,
     * evas gl render engine should set depth/stencil buffer size with minimum.
     * This is HACK code for tizen platform.
     */

   if (depth_bits)
     {
        config_attrs[i++] = EGL_DEPTH_SIZE;
        config_attrs[i++] = depth_bits;
     }
   else
     {
        config_attrs[i++] = EGL_DEPTH_SIZE;
        config_attrs[i++] = 1;
     }

   if (stencil_bits)
     {
        config_attrs[i++] = EGL_STENCIL_SIZE;
        config_attrs[i++] = stencil_bits;
     }
   else
     {
        config_attrs[i++] = EGL_STENCIL_SIZE;
        config_attrs[i++] = 1;
     }

   if (msaa_bits)
     {
        config_attrs[i++] = EGL_SAMPLE_BUFFERS;
        config_attrs[i++] = 1;
        config_attrs[i++] = EGL_SAMPLES;
        config_attrs[i++] = msaa_bits;
     }
   config_attrs[i++] = EGL_NONE;

   num = 0;
   if ((!eglChooseConfig(gw->egl_disp, config_attrs, configs, 200, &num))
       || (num < 1))
     {
        ERR("eglChooseConfig() can't find any configs (alpha: %d, depth: %d, stencil: %d, msaa: %d, num %d)",
            alpha, depth_bits, stencil_bits, gw->msaa_bits,num);
        if ((depth_bits > 24) || (stencil_bits > 8))
          {
             WRN("Please note that your driver might not support 32-bit depth or "
                 "16-bit stencil buffers, so depth24, stencil8 are the maximum "
                 "recommended values.");
             if (depth_bits > 24) depth_bits = 24;
             if (stencil_bits > 8) stencil_bits = 8;
             DBG("Trying again with depth:%d, stencil:%d", depth_bits, stencil_bits);
             goto try_again;
          }
        else if (msaa_bits)
          {
             msaa_bits /= 2;
             DBG("Trying again with msaa_samples: %d", msaa_bits);
             goto try_again;
          }
        else if (depth_bits || stencil_bits)
          {
             depth_bits = 0;
             stencil_bits = 0;
             DBG("Trying again without any depth or stencil buffer");
             goto try_again;
          }
        free(gw);
        return NULL;
     }

   found = EINA_FALSE;
   for (i = 0; (i < num) && (!found); i++)
     {
        EGLint val = 0;
        VisualID visid = 0;
        XVisualInfo *xvi, vi_in;
        int nvi, j;

        if (!eglGetConfigAttrib(gw->egl_disp, configs[i],
                                EGL_NATIVE_VISUAL_ID, &val))
          continue;
        visid = vis;
        vi_in.screen = screen;
        vi_in.visualid = visid;
        xvi = XGetVisualInfo(disp,
                             VisualScreenMask |
                             VisualIDMask,
                             &vi_in, &nvi);
        for (j = 0; j < nvi; j++)
          {
             if (!alpha)
               {
                  if (xvi[j].depth == depth)
                    {
                       gw->egl_config = configs[i];
                       found = EINA_TRUE;
                       break;
                    }
               }
             else
               {
                  XRenderPictFormat *fmt;

                  fmt = XRenderFindVisualFormat
                        (disp, xvi[j].visual);
                  if ((fmt->direct.alphaMask > 0) &&
                      (fmt->type == PictTypeDirect))
                    {
                       gw->egl_config = configs[i];
                       found = EINA_TRUE;
                       break;
                    }
               }
          }
        if (xvi) XFree(xvi);
     }
   if (!found)
     {
        // this is a less correct fallback if the above fails to
        // find the right visuals/configs
        gw->egl_config = configs[0];
     }
   if (gw->win_back)
     {
        gw->egl_surface[0] = eglCreatePixmapSurface(gw->egl_disp, gw->egl_config,
                                                    (EGLNativePixmapType)gw->win,
                                                    NULL);
        if (gw->egl_surface[0] == EGL_NO_SURFACE)
          {
             ERR("eglCreatePixmapSurface() fail for %#x. code=%#x",
                 (unsigned int)gw->win, eglGetError());
             eng_window_free(gw);
             return NULL;
          }

        gw->egl_surface[1] = eglCreatePixmapSurface(gw->egl_disp, gw->egl_config,
                                                    (EGLNativePixmapType)gw->win_back,
                                                    NULL);
        if (gw->egl_surface[1] == EGL_NO_SURFACE)
          {
             ERR("eglCreatePixmapSurface() fail for %#x. code=%#x",
                 (unsigned int)gw->win_back, eglGetError());
             eng_window_free(gw);
             return NULL;
          }
     }
   else
     {
        gw->egl_surface[0] = eglCreateWindowSurface(gw->egl_disp, gw->egl_config,
                                                    (EGLNativeWindowType)gw->win,
                                                    NULL);
        if (gw->egl_surface[0] == EGL_NO_SURFACE)
          {
             ERR("eglCreateWindowSurface() fail for %#x. code=%#x",
                 (unsigned int)gw->win, eglGetError());
             eng_window_free(gw);
             return NULL;
          }
     }
   context = _tls_context_get();
   gw->egl_context[0] = eglCreateContext
     (gw->egl_disp, gw->egl_config, context, context_attrs);
   if (gw->egl_context[0] == EGL_NO_CONTEXT)
     {
        ERR("eglCreateContext() fail. code=%#x", eglGetError());
        eng_window_free(gw);
        return NULL;
     }
   if (context == EGL_NO_CONTEXT)
     _tls_context_set(gw->egl_context[0]);
   if (eglMakeCurrent(gw->egl_disp,
                      gw->egl_surface[0],
                      gw->egl_surface[0],
                      gw->egl_context[0]) == EGL_FALSE)
     {
        ERR("eglMakeCurrent() fail. code=%#x", eglGetError());
        eng_window_free(gw);
        return NULL;
     }
   vendor = glGetString(GL_VENDOR);
   renderer = glGetString(GL_RENDERER);
   version = glGetString(GL_VERSION);
   if (!vendor)   vendor   = (unsigned char *)"-UNKNOWN-";
   if (!renderer) renderer = (unsigned char *)"-UNKNOWN-";
   if (!version)  version  = (unsigned char *)"-UNKNOWN-";
   if (getenv("EVAS_GL_INFO"))
     {
        fprintf(stderr, "vendor: %s\n", vendor);
        fprintf(stderr, "renderer: %s\n", renderer);
        fprintf(stderr, "version: %s\n", version);
     }

   if (strstr((const char *)vendor, "Mesa Project"))
     {
        if (strstr((const char *)renderer, "Software Rasterizer"))
          blacklist = 1;
     }
   if (strstr((const char *)renderer, "softpipe"))
     blacklist = 1;
   if (strstr((const char *)renderer, "llvmpipe"))
     blacklist = 1;
   if ((blacklist) && (!getenv("EVAS_GL_NO_BLACKLIST")))
     {
        ERR("OpenGL Driver blacklisted:");
        ERR("Vendor: %s", (const char *)vendor);
        ERR("Renderer: %s", (const char *)renderer);
        ERR("Version: %s", (const char *)version);
        eng_window_free(gw);
        return NULL;
     }

   eglGetConfigAttrib(gw->egl_disp, gw->egl_config, EGL_DEPTH_SIZE, &val);
   gw->detected.depth_buffer_size = val;
   eglGetConfigAttrib(gw->egl_disp, gw->egl_config, EGL_STENCIL_SIZE, &val);
   gw->detected.stencil_buffer_size = val;
   eglGetConfigAttrib(gw->egl_disp, gw->egl_config, EGL_SAMPLES, &val);
   gw->detected.msaa = val;
// GLX
#else
   context = _tls_context_get();
   if (!context)
     {
#ifdef NEWGL
        if (indirect)
          context = glXCreateNewContext(gw->disp, fbconf,
                                        GLX_RGBA_TYPE, NULL,
                                        GL_FALSE);
        else
          context = glXCreateNewContext(gw->disp, fbconf,
                                        GLX_RGBA_TYPE, NULL,
                                        GL_TRUE);
#else
        if (indirect)
          context = glXCreateContext(gw->disp, gw->visualinfo, NULL, GL_FALSE);
        else
          context = glXCreateContext(gw->disp, gw->visualinfo, NULL, GL_TRUE);
#endif
     }
#ifdef NEWGL
   if ((gw->alpha) && (!rgba_context))
     {
        if (indirect)
          rgba_context = glXCreateNewContext(gw->disp, rgba_fbconf,
                                             GLX_RGBA_TYPE, context,
                                             GL_FALSE);
        else
          rgba_context = glXCreateNewContext(gw->disp, rgba_fbconf,
                                             GLX_RGBA_TYPE, context,
                                             GL_TRUE);
     }
   if (gw->alpha)
     gw->glxwin = glXCreateWindow(gw->disp, rgba_fbconf, gw->win, NULL);
   else
     gw->glxwin = glXCreateWindow(gw->disp, fbconf, gw->win, NULL);
   if (!gw->glxwin)
     {
        eng_window_free(gw);
        return NULL;
     }

   if (gw->alpha) gw->context = rgba_context;
   else gw->context = context;
#else
   gw->context = context;
#endif

   if (!gw->context)
     {
        eng_window_free(gw);
        return NULL;
     }
   if (gw->context)
     {
        if (gw->glxwin)
          {
             if (!glXMakeContextCurrent(gw->disp, gw->glxwin, gw->glxwin,
                                        gw->context))
               {
                  printf("Error: glXMakeContextCurrent(%p, %p, %p, %p)\n", (void *)gw->disp, (void *)gw->glxwin, (void *)gw->glxwin, (void *)gw->context);
                  eng_window_free(gw);
                  return NULL;
               }
          }
        else
          {
             if (!glXMakeCurrent(gw->disp, gw->win, gw->context))
               {
                  printf("Error: glXMakeCurrent(%p, 0x%x, %p) failed\n", (void *)gw->disp, (unsigned int)gw->win, (void *)gw->context);
                  eng_window_free(gw);
                  return NULL;
               }
          }

        // FIXME: move this up to context creation

        vendor = glGetString(GL_VENDOR);
        renderer = glGetString(GL_RENDERER);
        version = glGetString(GL_VERSION);
        if (getenv("EVAS_GL_INFO"))
          {
             fprintf(stderr, "vendor: %s\n", vendor);
             fprintf(stderr, "renderer: %s\n", renderer);
             fprintf(stderr, "version: %s\n", version);
          }
        //   examples:
        // vendor: NVIDIA Corporation
        // renderer: NVIDIA Tegra
        // version: OpenGL ES 2.0
        //   or
        // vendor: Imagination Technologies
        // renderer: PowerVR SGX 540
        // version: OpenGL ES 2.0
        //   or
        // vendor: NVIDIA Corporation
        // renderer: GeForce GT 330M/PCI/SSE2
        // version: 3.3.0 NVIDIA 256.53
        //   or
        // vendor: NVIDIA Corporation
        // renderer: GeForce GT 220/PCI/SSE2
        // version: 3.2.0 NVIDIA 195.36.24
        //   or
        // vendor: NVIDIA Corporation
        // renderer: GeForce 8600 GTS/PCI/SSE2
        // version: 3.3.0 NVIDIA 260.19.36
        //   or
        // vendor: ATI Technologies Inc.
        // renderer: ATI Mobility Radeon HD 4650
        // version: 3.2.9756 Compatibility Profile Context
        //   or
        // vendor: Tungsten Graphics, Inc
        // renderer: Mesa DRI Mobile Intel® GM45 Express Chipset GEM 20100330 DEVELOPMENT x86/MMX/SSE2
        // version: 2.1 Mesa 7.9-devel
        //   or
        // vendor: Advanced Micro Devices, Inc.
        // renderer: Mesa DRI R600 (RS780 9610) 20090101  TCL DRI2
        // version: 2.1 Mesa 7.9-devel
        //   or
        // vendor: NVIDIA Corporation
        // renderer: GeForce 9600 GT/PCI/SSE2
        // version: 3.3.0 NVIDIA 260.19.29
        //   or
        // vendor: ATI Technologies Inc.
        // renderer: ATI Radeon HD 4800 Series
        // version: 3.3.10237 Compatibility Profile Context
        //   or
        // vendor: Advanced Micro Devices, Inc.
        // renderer: Mesa DRI R600 (RV770 9442) 20090101  TCL DRI2
        // version: 2.0 Mesa 7.8.2
        //   or
        // vendor: Tungsten Graphics, Inc
        // renderer: Mesa DRI Mobile Intel® GM45 Express Chipset GEM 20100330 DEVELOPMENT
        // version: 2.1 Mesa 7.9-devel
        //   or (bad - software renderer)
        // vendor: Mesa Project
        // renderer: Software Rasterizer
        // version: 2.1 Mesa 7.9-devel
        //   or (bad - software renderer)
        // vendor: VMware, Inc.
        // renderer: Gallium 0.4 on softpipe
        // version: 2.1 Mesa 7.9-devel

        if (strstr((const char *)vendor, "Mesa Project"))
          {
             if (strstr((const char *)renderer, "Software Rasterizer"))
                blacklist = 1;
          }
        if (strstr((const char *)renderer, "softpipe"))
           blacklist = 1;
        if (strstr((const char *)renderer, "llvmpipe"))
          blacklist = 1;
        if ((blacklist) && (!getenv("EVAS_GL_NO_BLACKLIST")))
          {
             fprintf(stderr,"OpenGL Driver blacklisted:");
             fprintf(stderr,"Vendor: %s", (const char *)vendor);
             fprintf(stderr,"Renderer: %s", (const char *)renderer);
             fprintf(stderr,"Version: %s", (const char *)version);
             eng_window_free(gw);
             return NULL;
          }
        if (strstr((const char *)vendor, "NVIDIA"))
          {
             if (!strstr((const char *)renderer, "NVIDIA Tegra"))
               {
                  int v1 = 0, v2 = 0, v3 = 0;

                  if (sscanf((const char *)version,
                             "%*s %*s %i.%i.%i",
                             &v1, &v2, &v3) != 3)
                    {
                       v1 = v2 = v3 = 0;
                       if (sscanf((const char *)version,
                                  "%*s %*s %i.%i",
                                  &v1, &v2) != 2)
                          v1 = 0;
                    }
                  // ALSO as of some nvidia driver version loose binding is
                  // probably not needed
                  if (v1 < 195) gw->detected.loose_binding = 1;
               }
          }
        else
          {
             // noothing yet. add more cases and options over time
          }
        glXGetConfig(gw->disp, gw->visualinfo, GLX_DEPTH_SIZE, &val);
        gw->detected.depth_buffer_size = val;
        glXGetConfig(gw->disp, gw->visualinfo, GLX_STENCIL_SIZE, &val);
        gw->detected.stencil_buffer_size = val;
        glXGetConfig(gw->disp, gw->visualinfo, GLX_SAMPLES, &val);
        gw->detected.msaa = val;
     }
#endif

   gw->gl_context = evas_gl_common_context_new();
   if (!gw->gl_context)
     {
        eng_window_free(gw);
        return NULL;
     }
#ifdef GL_GLES
   gw->gl_context->egldisp = gw->egl_disp;
#endif
   eng_window_use(gw);
   evas_gl_common_context_resize(gw->gl_context, w, h, rot);
   gw->surf = 1;
   return gw;
   indirect = 0;
}

void
eng_window_free(Evas_GL_X11_Window *gw)
{
   Evas_GL_X11_Window *xwin;
   GLContext context;
   int ref = 0;

   win_count--;
   eng_window_use(gw);

   context = _tls_context_get();
   xwin = _tls_evas_gl_x11_window_get();

   if (gw == xwin) _tls_evas_gl_x11_window_set(NULL);
   if (gw->gl_context)
     {
        ref = gw->gl_context->references - 1;
        evas_gl_common_context_free(gw->gl_context);
     }
#ifdef GL_GLES
   eglMakeCurrent(gw->egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   if (gw->egl_surface[0] != EGL_NO_SURFACE)
      eglDestroySurface(gw->egl_disp, gw->egl_surface[0]);
   if (gw->egl_surface[1] != EGL_NO_SURFACE)
      eglDestroySurface(gw->egl_disp, gw->egl_surface[1]);
   if (gw->egl_context[0] != context)
     eglDestroyContext(gw->egl_disp, gw->egl_context[0]);
   if (ref == 0)
     {
        if (context) eglDestroyContext(gw->egl_disp, context);
        eglTerminate(gw->egl_disp);
        eglReleaseThread();
        _tls_context_set(EGL_NO_CONTEXT);
     }
#else
   if (gw->glxwin) glXDestroyWindow(gw->disp, gw->glxwin);
   if (ref == 0)
     {
        if (context) glXDestroyContext(gw->disp, context);
        if (rgba_context) glXDestroyContext(gw->disp, rgba_context);
        _tls_context_set(0);
        rgba_context = 0;
        fbconf = 0;
        rgba_fbconf = 0;
     }
#endif
   free(gw);
}

void
eng_window_use(Evas_GL_X11_Window *gw)
{
   Eina_Bool force_use = EINA_FALSE;
   Evas_GL_X11_Window *xwin;

   xwin = _tls_evas_gl_x11_window_get();

#ifdef GL_GLES
   if (xwin)
     {
        if ((eglGetCurrentDisplay() !=
             xwin->egl_disp) ||
        	(eglGetCurrentContext() !=
             xwin->egl_context[0]) ||
            (eglGetCurrentSurface(EGL_READ) !=
                xwin->egl_surface[xwin->offscreen]) ||
            (eglGetCurrentSurface(EGL_DRAW) !=
                xwin->egl_surface[xwin->offscreen]))
           force_use = EINA_TRUE;
     }
#else
   if (xwin)
     {
        if (glXGetCurrentContext() != xwin->context)
           force_use = EINA_TRUE;
     }
#endif
   if ((xwin != gw) || (force_use))
     {
        if (xwin)
          {
             evas_gl_common_context_use(xwin->gl_context);
             evas_gl_common_context_flush(xwin->gl_context);
             xwin->gl_context->pipe[0].shader.surface = NULL;
          }
        _tls_evas_gl_x11_window_set(gw);
        if (gw)
          {
// EGL / GLES
#ifdef GL_GLES
           if (gw->egl_surface[gw->offscreen] != EGL_NO_SURFACE)
             {
                if (eglMakeCurrent(gw->egl_disp,
                                   gw->egl_surface[gw->offscreen],
                                   gw->egl_surface[gw->offscreen],
                                   gw->egl_context[0]) == EGL_FALSE)
                  {
                     ERR("eglMakeCurrent() failed!");
                  }
             }
// GLX
#else
           if (gw->glxwin)
             {
               if (!glXMakeContextCurrent(gw->disp, gw->glxwin, gw->glxwin,
                                          gw->context))
                 {
                   ERR("glXMakeContextCurrent(%p, %p, %p, %p)", (void *)gw->disp, (void *)gw->glxwin, (void *)gw->glxwin, (void *)gw->context);
                 }
             }
           else
             {
               if (!glXMakeCurrent(gw->disp, gw->win, gw->context))
                 {
                   ERR("glXMakeCurrent(%p, 0x%x, %p) failed", gw->disp, (unsigned int)gw->win, (void *)gw->context);
                 }
             }
#endif
          }
     }
   if (gw) evas_gl_common_context_use(gw->gl_context);
}

void
eng_window_unsurf(Evas_GL_X11_Window *gw, Eina_Bool pre_rotation_resurf)
{
   if (!gw->surf) return;
   if (!getenv("EVAS_GL_WIN_RESURF") && !pre_rotation_resurf) return;
   if (getenv("EVAS_GL_INFO"))
      printf("unsurf %p\n", gw);

#ifdef GL_GLES
   Evas_GL_X11_Window *xwin;

   xwin = _tls_evas_gl_x11_window_get();
   if (xwin)
      evas_gl_common_context_flush(xwin->gl_context);
   if (xwin == gw || pre_rotation_resurf)
     {
        eglMakeCurrent(gw->egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (gw->egl_surface[0] != EGL_NO_SURFACE)
           eglDestroySurface(gw->egl_disp, gw->egl_surface[0]);
        gw->egl_surface[0] = EGL_NO_SURFACE;
        if (gw->egl_surface[1] != EGL_NO_SURFACE)
           eglDestroySurface(gw->egl_disp, gw->egl_surface[1]);
        gw->egl_surface[1] = EGL_NO_SURFACE;
        _tls_evas_gl_x11_window_set(NULL);
     }
#else
   if (gw->glxwin)
      {
         glXDestroyWindow(gw->disp, gw->glxwin);
      }
   else
     {
     }
#endif
   gw->surf = 0;
}

void
eng_window_resurf(Evas_GL_X11_Window *gw)
{
   if (gw->surf) return;
   if (getenv("EVAS_GL_INFO"))
      printf("resurf %p\n", gw);
#ifdef GL_GLES
   if (gw->win_back)
     {
        gw->egl_surface[0] = eglCreatePixmapSurface(gw->egl_disp, gw->egl_config,
                                                        (EGLNativePixmapType)gw->win,
                                                        NULL);
        if (gw->egl_surface[0] == EGL_NO_SURFACE)
          {
             ERR("eglCreatePixmapSurface() fail for %#x. code=%#x",
                 (unsigned int)gw->win, eglGetError());
             return;
          }
        ERR("%x = eglCreatePixmapSurface(pixmap %x)", gw->egl_surface[0], gw->win);

        gw->egl_surface[1] = eglCreatePixmapSurface(gw->egl_disp, gw->egl_config,
                                                        (EGLNativePixmapType)gw->win_back,
                                                        NULL);
        if (gw->egl_surface[1] == EGL_NO_SURFACE)
          {
             ERR("eglCreatePixmapSurface() fail for %#x. code=%#x",
                 (unsigned int)gw->win_back, eglGetError());
             return;
          }
        ERR("%x = eglCreatePixmapSurface(pixmap %x)", gw->egl_surface[1], gw->win_back);
     }
   else
     {
        gw->egl_surface[0] = eglCreateWindowSurface(gw->egl_disp, gw->egl_config,
                                                        (EGLNativeWindowType)gw->win,
                                                        NULL);
        if (gw->egl_surface[0] == EGL_NO_SURFACE)
          {
             ERR("eglCreateWindowSurface() fail for %#x. code=%#x",
                 (unsigned int)gw->win, eglGetError());
             return;
          }
     }
   if (eglMakeCurrent(gw->egl_disp,
                      gw->egl_surface[gw->offscreen],
                      gw->egl_surface[gw->offscreen],
                      gw->egl_context[0]) == EGL_FALSE)
     {
        ERR("eglMakeCurrent() failed!");
     }
#else
#ifdef NEWGL
   if (gw->alpha)
     gw->glxwin = glXCreateWindow(gw->disp, rgba_fbconf, gw->win, NULL);
   else
     gw->glxwin = glXCreateWindow(gw->disp, fbconf, gw->win, NULL);
   if (!glXMakeContextCurrent(gw->disp, gw->glxwin, gw->glxwin,
                              gw->context))
     {
        ERR("glXMakeContextCurrent(%p, %p, %p, %p)", (void *)gw->disp, (void *)gw->glxwin, (void *)gw->glxwin, (void *)gw->context);
     }
#else
   if (!glXMakeCurrent(gw->disp, gw->win, gw->context))
     {
        ERR("glXMakeCurrent(%p, 0x%x, %p) failed", (void *)gw->disp, (unsigned int)gw->win, (void *)gw->context);
     }
#endif
#endif
   gw->surf = 1;
}

Visual *
eng_best_visual_get(Evas_Engine_Info_GL_X11 *einfo)
{
   if (!einfo) return NULL;
   if (!einfo->info.display) return NULL;
   if (!_evas_gl_x11_vi)
     {
        int alpha;

// EGL / GLES
#ifdef GL_GLES
        for (alpha = 0; alpha < 2; alpha++)
          {
             int depth = DefaultDepth(einfo->info.display,
                                      einfo->info.screen);
             if (alpha)
               {
                  XVisualInfo *xvi, vi_in;
                  int nvi, i;
                  XRenderPictFormat *fmt;

                  vi_in.screen = einfo->info.screen;
                  vi_in.depth = 32;
                  vi_in.class = TrueColor;
                  xvi = XGetVisualInfo(einfo->info.display,
                                       VisualScreenMask | VisualDepthMask |
                                       VisualClassMask,
                                       &vi_in, &nvi);
                  if (xvi)
                    {
                       for (i = 0; i < nvi; i++)
                         {
                            fmt = XRenderFindVisualFormat(einfo->info.display,
                                                          xvi[i].visual);
                            if ((fmt->type == PictTypeDirect) &&
                                (fmt->direct.alphaMask))
                              {
                                 _evas_gl_x11_rgba_vi =
                                   calloc(1, sizeof(XVisualInfo));
                                 if (_evas_gl_x11_rgba_vi)
                                   memcpy(_evas_gl_x11_rgba_vi,
                                          &(xvi[i]), sizeof(XVisualInfo));
                                 break;
                              }
                         }
                       XFree (xvi);
                    }
               }
             else
               {
                  _evas_gl_x11_vi = calloc(1, sizeof(XVisualInfo));
                  XMatchVisualInfo(einfo->info.display,
                                   einfo->info.screen, depth, TrueColor,
                                   _evas_gl_x11_vi);
               }
          }
// GLX
#else
        for (alpha = 0; alpha < 2; alpha++)
          {
             int config_attrs[40];
             GLXFBConfig *configs = NULL, config = 0;
             int i, num;

             i = 0;
             config_attrs[i++] = GLX_DRAWABLE_TYPE;
             config_attrs[i++] = GLX_WINDOW_BIT;
             config_attrs[i++] = GLX_DOUBLEBUFFER;
             config_attrs[i++] = 1;
             config_attrs[i++] = GLX_RED_SIZE;
             config_attrs[i++] = 1;
             config_attrs[i++] = GLX_GREEN_SIZE;
             config_attrs[i++] = 1;
             config_attrs[i++] = GLX_BLUE_SIZE;
             config_attrs[i++] = 1;
             if (alpha)
               {
                  config_attrs[i++] = GLX_RENDER_TYPE;
                  config_attrs[i++] = GLX_RGBA_BIT;
                  config_attrs[i++] = GLX_ALPHA_SIZE;
                  config_attrs[i++] = 1;
               }
             else
               {
                  config_attrs[i++] = GLX_ALPHA_SIZE;
                  config_attrs[i++] = 0;
               }
             config_attrs[i++] = GLX_DEPTH_SIZE;
             config_attrs[i++] = 0;
             config_attrs[i++] = GLX_STENCIL_SIZE;
             config_attrs[i++] = 0;
             config_attrs[i++] = GLX_AUX_BUFFERS;
             config_attrs[i++] = 0;
             config_attrs[i++] = GLX_STEREO;
             config_attrs[i++] = 0;
             config_attrs[i++] = GLX_TRANSPARENT_TYPE;
             config_attrs[i++] = GLX_NONE;//GLX_NONE;//GLX_TRANSPARENT_INDEX//GLX_TRANSPARENT_RGB;
             config_attrs[i++] = 0;

             configs = glXChooseFBConfig(einfo->info.display,
                                         einfo->info.screen,
                                         config_attrs, &num);
             if ((!configs) || (num < 1))
               {
                  ERR("glXChooseFBConfig returned no configs");
                  return NULL;
               }
             for (i = 0; i < num; i++)
               {
                  XVisualInfo *visinfo;
                  XRenderPictFormat *format = NULL;

                  visinfo = glXGetVisualFromFBConfig(einfo->info.display,
                                                     configs[i]);
                  if (!visinfo) continue;
                  if (!alpha)
                    {
                       config = configs[i];
                       _evas_gl_x11_vi = malloc(sizeof(XVisualInfo));
                       memcpy(_evas_gl_x11_vi, visinfo, sizeof(XVisualInfo));
                       fbconf = config;
                       XFree(visinfo);
                       break;
                    }
                  else
                    {
                       format = XRenderFindVisualFormat
                         (einfo->info.display, visinfo->visual);
                       if (!format)
                         {
                            XFree(visinfo);
                            continue;
                         }
                       if (format->direct.alphaMask > 0)
                         {
                            config = configs[i];
                            _evas_gl_x11_rgba_vi = malloc(sizeof(XVisualInfo));
                            memcpy(_evas_gl_x11_rgba_vi, visinfo, sizeof(XVisualInfo));
                            rgba_fbconf = config;
                            XFree(visinfo);
                            break;
                         }
                    }
                  XFree(visinfo);
               }
          }
#endif
     }
   if (!_evas_gl_x11_vi) return NULL;
   if (einfo->info.destination_alpha)
     {
// EGL / GLES
#ifdef GL_GLES
        if (_evas_gl_x11_rgba_vi) return _evas_gl_x11_rgba_vi->visual;
#else
//# ifdef NEWGL
        if (_evas_gl_x11_rgba_vi) return _evas_gl_x11_rgba_vi->visual;
//# endif
#endif
     }
   return _evas_gl_x11_vi->visual;
}

Colormap
eng_best_colormap_get(Evas_Engine_Info_GL_X11 *einfo)
{
   if (!einfo) return 0;
   if (!einfo->info.display) return 0;
   if (!_evas_gl_x11_vi) eng_best_visual_get(einfo);
   if (!_evas_gl_x11_vi) return 0;
   if (einfo->info.destination_alpha)
     {
        if (!_evas_gl_x11_rgba_cmap)
          _evas_gl_x11_rgba_cmap =
          XCreateColormap(einfo->info.display,
                          RootWindow(einfo->info.display,
                                     einfo->info.screen),
                          _evas_gl_x11_rgba_vi->visual,
                          0);
        return _evas_gl_x11_rgba_cmap;
     }
   if (!_evas_gl_x11_cmap)
     _evas_gl_x11_cmap =
     XCreateColormap(einfo->info.display,
                     RootWindow(einfo->info.display,
                                einfo->info.screen),
                     _evas_gl_x11_vi->visual,
                     0);
   return _evas_gl_x11_cmap;
}

int
eng_best_depth_get(Evas_Engine_Info_GL_X11 *einfo)
{
   if (!einfo) return 0;
   if (!einfo->info.display) return 0;
   if (!_evas_gl_x11_vi) eng_best_visual_get(einfo);
   if (!_evas_gl_x11_vi) return 0;
   if (einfo->info.destination_alpha)
     {
        if (_evas_gl_x11_rgba_vi) return _evas_gl_x11_rgba_vi->depth;
     }
   return _evas_gl_x11_vi->depth;
}

int
eng_native_config_check(Display *disp, void *config , int target, void *pixmap, int *w, int *h)
{
   Window root;
   int x, y;
   unsigned int border;
   unsigned int depth;
   int ret = 0;
   Evas_GL_Config *cfg = config;

   // Check target
   if (target!=1) // EGL_NATIVE_PIXMAP_KHR
     {
        ERR("Invalid native target");
        return 0;
     }

   // Retrive pixmap information
   ret = XGetGeometry(disp, (Drawable)pixmap, &root, &x, &y, w, h, &border, &depth);
   if (!ret)
     {
        ERR("Unable to retrieve pixmap information");
        return 0;
     }

   // Check the validity of the depth of the target
   if ( ((cfg->color_format==EVAS_GL_RGB_888)   && (depth != 24)) ||
        ((cfg->color_format==EVAS_GL_RGBA_8888) && (depth != 32)) )
     {
        ERR("Unsupported native depth");
        return 0;
     }

   DBG("Pixmap Width [%d] Height [%d] Depth [%d]", *w, *h, depth);
   return 1;
}
