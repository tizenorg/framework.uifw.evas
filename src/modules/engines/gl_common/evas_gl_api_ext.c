
#include "evas_gl_api_ext.h"

#include <dlfcn.h>


#define MAX_EXTENSION_STRING_BUFFER 10240
static char _gl_ext_string[MAX_EXTENSION_STRING_BUFFER] = { 0 };
static char *_gles1_ext_string = NULL;

typedef void (*_getproc_fn) (void);
typedef _getproc_fn (*fp_getproc)(const char *);

#ifndef EGL_NATIVE_PIXMAP_KHR
# define EGL_NATIVE_PIXMAP_KHR 0x30b0
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Extension HEADER
/////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name)
#define _EVASGL_EXT_END()
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param) \
   ret (*gl_ext_sym_##name) param = NULL; \
   ret (*gles1_ext_sym_##name) param = NULL;
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
/////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Extension HEADER
/////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name) \
   int _gl_ext_support_##name = 0; \
   int _gles1_ext_support_##name = 0;
#define _EVASGL_EXT_END()
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param)
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
/////////////////////////////////////////////////////////////////////////////////////////////////////


// Evas extensions from EGL extensions
#ifdef GL_GLES
#define EGLDISPLAY_GET() _evgl_egl_display_get(__FUNCTION__)
static EGLDisplay
_evgl_egl_display_get(const char *function)
{
   EGLDisplay dpy = EGL_NO_DISPLAY;
   EVGL_Resource *rsc;

   if (!(rsc=_evgl_tls_resource_get()))
     {
        ERR("%s: Unable to execute GL command. Error retrieving tls", function);
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return EGL_NO_DISPLAY;
     }

   if (!rsc->current_eng)
     {
        ERR("%s: Unable to retrive Current Engine", function);
        _evgl_error_set(EVAS_GL_NOT_INITIALIZED);
        return EGL_NO_DISPLAY;
     }

   if ((evgl_engine) && (evgl_engine->funcs->display_get))
     {
        dpy = (EGLDisplay)evgl_engine->funcs->display_get(rsc->current_eng);
        return dpy;
     }
   else
     {
        ERR("%s: Invalid Engine... (Can't acccess EGL Display)\n", function);
        _evgl_error_set(EVAS_GL_BAD_DISPLAY);
        return EGL_NO_DISPLAY;
     }
}

static void *
_evgl_eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx,
                        int target, void* buffer, const int *attrib_list)
{
   int *attribs = NULL;

   /* Convert 0 terminator into a EGL_NONE terminator */
   if (attrib_list)
     {
        int cnt = 0;
        int *a;

        for (a = (int *) attrib_list; (*a) && (*a != EGL_NONE); a += 2)
          {
             /* TODO: Verify supported attributes */
             cnt += 2;
          }

        attribs = alloca(sizeof(int) * (cnt + 1));
        for (a = attribs; (*attrib_list) && (*attrib_list != EGL_NONE);
             a += 2, attrib_list += 2)
          {
             a[0] = attrib_list[0];
             a[1] = attrib_list[1];
          }
        *a = EGL_NONE;
     }

   return EXT_FUNC(eglCreateImage)(dpy, ctx, target, buffer, attribs);
}

static void *
evgl_evasglCreateImage(int target, void* buffer, const int *attrib_list)
{
   EGLDisplay dpy = EGLDISPLAY_GET();
   EGLContext ctx = EGL_NO_CONTEXT;

   if (!dpy) return NULL;

   /* EGL_NO_CONTEXT will always fail for TEXTURE_2D */
   if (target == EVAS_GL_TEXTURE_2D)
     {
        ctx = eglGetCurrentContext();
        INF("Creating EGL image based on the current context: %p", ctx);
     }

   return _evgl_eglCreateImageKHR(dpy, ctx, target, buffer, attrib_list);
}

static void *
evgl_evasglCreateImageForContext(Evas_GL *evasgl EINA_UNUSED, Evas_GL_Context *evasctx,
                                 int target, void* buffer, const int *attrib_list)
{
   EGLDisplay dpy = EGLDISPLAY_GET();
   EGLContext ctx = EGL_NO_CONTEXT;

   if (!evasgl || !dpy) return NULL;

   ctx = evgl_context_native_get(evasctx);
   return _evgl_eglCreateImageKHR(dpy, ctx, target, buffer, attrib_list);
}

static void
evgl_evasglDestroyImage(EvasGLImage image)
{
   EGLDisplay dpy = EGLDISPLAY_GET();
   if (!dpy) return;
   EXT_FUNC(eglDestroyImage)(dpy, image);
}

static void
evgl_glEvasGLImageTargetTexture2D(GLenum target, EvasGLImage image)
{
   EXT_FUNC(glEGLImageTargetTexture2DOES)(target, image);
}

static void
evgl_glEvasGLImageTargetRenderbufferStorage(GLenum target, EvasGLImage image)
{
   EXT_FUNC(glEGLImageTargetRenderbufferStorageOES)(target, image);
}

static EvasGLSync
evgl_evasglCreateSync(Evas_GL *evas_gl EINA_UNUSED,
                      unsigned int type, const int *attrib_list)
{
   EGLDisplay dpy = EGLDISPLAY_GET();
   if (!dpy) return NULL;
   return EXT_FUNC(eglCreateSyncKHR)(dpy, type, attrib_list);
}

static Eina_Bool
evgl_evasglDestroySync(Evas_GL *evas_gl EINA_UNUSED, EvasGLSync sync)
{
   EGLDisplay dpy = EGLDISPLAY_GET();
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglDestroySyncKHR)(dpy, sync);
}

static int
evgl_evasglClientWaitSync(Evas_GL *evas_gl EINA_UNUSED,
                          EvasGLSync sync, int flags, EvasGLTime timeout)
{
   EGLDisplay dpy = EGLDISPLAY_GET();
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglClientWaitSyncKHR)(dpy, sync, flags, timeout);
}

static Eina_Bool
evgl_evasglSignalSync(Evas_GL *evas_gl EINA_UNUSED,
                      EvasGLSync sync, unsigned mode)
{
   EGLDisplay dpy = EGLDISPLAY_GET();
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglSignalSyncKHR)(dpy, sync, mode);
}

static Eina_Bool
evgl_evasglGetSyncAttrib(Evas_GL *evas_gl EINA_UNUSED,
                         EvasGLSync sync, int attribute, int *value)
{
   EGLDisplay dpy = EGLDISPLAY_GET();
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglGetSyncAttribKHR)(dpy, sync, attribute, value);
}

static int
evgl_evasglWaitSync(Evas_GL *evas_gl EINA_UNUSED,
                    EvasGLSync sync, int flags)
{
   EGLDisplay dpy = EGLDISPLAY_GET();
   if (!dpy) return EINA_FALSE;
   return EXT_FUNC(eglWaitSyncKHR)(dpy, sync, flags);
}


#else
#endif

// 0: not initialized, 1: GLESv2 initialized, 2: GLESv1 also initialized
static int _evgl_api_ext_status = 0;

Eina_Bool
evgl_api_ext_init(void *getproc, const char *glueexts)
{
   const char *glexts;

   fp_getproc gp = (fp_getproc)getproc;
   int _curext_supported = 0;

   memset(_gl_ext_string, 0x00, MAX_EXTENSION_STRING_BUFFER);

   // GLES Extensions
   glexts = (const char*)glGetString(GL_EXTENSIONS);
   if (!glexts)
     {
        ERR("glGetString returned NULL! Something is very wrong...");
        return EINA_FALSE;
     }

   /*
   // GLUE Extensions
#ifdef GL_GLES
getproc = &eglGetProcAddress;
glueexts = eglQueryString(re->win->egl_disp, EGL_EXTENSIONS);
#else
getproc = &glXGetProcAddress;
glueexts = glXQueryExtensionsString(re->info->info.display,
re->info->info.screen);
#endif
    */

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Extension HEADER
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#define GETPROCADDR(sym) \
   (((!(*drvfunc)) && (gp)) ? (__typeof__((*drvfunc)))gp(sym) : (__typeof__((*drvfunc)))dlsym(RTLD_DEFAULT, sym))

#define _EVASGL_EXT_BEGIN(name) \
     { \
        int *ext_support = &_gl_ext_support_##name; \
        *ext_support = 0;

#define _EVASGL_EXT_END() \
     }

#define _EVASGL_EXT_CHECK_SUPPORT(name) \
   (strstr(glexts, name) != NULL || strstr(glueexts, name) != NULL)

#define _EVASGL_EXT_DISCARD_SUPPORT() \
   *ext_support = 0;

#define _EVASGL_EXT_DRVNAME(name) \
   if (_EVASGL_EXT_CHECK_SUPPORT(#name)) *ext_support = 1;

#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param) \
     { \
        ret (**drvfunc)param = &gl_ext_sym_##name;

#define _EVASGL_EXT_FUNCTION_END() \
        if ((*drvfunc) == NULL) _EVASGL_EXT_DISCARD_SUPPORT(); \
     }

#define _EVASGL_EXT_FUNCTION_DRVFUNC(name) \
   if ((*drvfunc) == NULL) *drvfunc = name;

// This adds all the function names to the "safe" list but only one pointer
// will be stored in the hash table.
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name) \
   if ((*drvfunc) == NULL) \
     { \
        *drvfunc = GETPROCADDR(name); \
        evgl_safe_extension_add(name, (void *) (*drvfunc)); \
     } \
   else evgl_safe_extension_add(name, NULL);

#ifdef _EVASGL_EXT_FUNCTION_WHITELIST
# undef _EVASGL_EXT_FUNCTION_WHITELIST
#endif
#define _EVASGL_EXT_FUNCTION_WHITELIST(name) evgl_safe_extension_add(name, NULL);
//#define _EVASGL_EXT_VERIFY

#include "evas_gl_api_ext_def.h"

//#undef _EVASGL_EXT_VERIFY
#undef _EVASGL_EXT_FUNCTION_WHITELIST
#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR

#undef GETPROCADDR
   /////////////////////////////////////////////////////////////////////////////////////////////////////



	_gl_ext_string[0] = 0x00; //NULL;

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Extension HEADER
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_BEGIN(name) \
     if (_gl_ext_support_##name != 0) { strcat(_gl_ext_string, #name" "); _curext_supported = 1; } \
     else _curext_supported = 0;

#define _EVASGL_EXT_END()
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_DRVNAME(name) if (_curext_supported) strcat(_gl_ext_string, #name" ");
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param)
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
   /////////////////////////////////////////////////////////////////////////////////////////////////////




   _evgl_api_ext_status = 1;
   return EINA_TRUE;
}

void
evgl_api_ext_get(Evas_GL_API *gl_funcs)
{
   if (_evgl_api_ext_status < 1)
     {
        ERR("EVGL extension is not yet initialized.");
        return;
     }

#define ORD(f) EVAS_API_OVERRIDE(f, gl_funcs, gl_ext_sym_)

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Extension HEADER
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name) \
   if (_gl_ext_support_##name != 0) \
     {
#define _EVASGL_EXT_END() \
     }
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param) \
   ORD(name);
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN(ret, name, param)
#define _EVASGL_EXT_FUNCTION_PRIVATE_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#undef _EVASGL_EXT_WHITELIST_ONLY
#define _EVASGL_EXT_WHITELIST_ONLY 0

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_WHITELIST_ONLY
#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN
#undef _EVASGL_EXT_FUNCTION_PRIVATE_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#undef ORD

}

Eina_Bool
_evgl_api_gles1_ext_init(void)
{
   if (_evgl_api_ext_status >= 2)
     return EINA_TRUE;

#ifdef GL_GLES
   int _curext_supported = 0;
   Evas_GL_API *gles1_funcs;
   const char *gles1_exts;

   /* Note from the EGL documentation:
    *   Function pointers returned by eglGetProcAddress are independent of the
    *   display and the currently bound context and may be used by any context
    *   which supports the extension.
    * So, we don't need to check that GLESv1 is current.
    */

   gles1_funcs = _evgl_api_gles1_internal_get();
   if (!gles1_funcs || !gles1_funcs->glGetString)
     {
        ERR("Could not get address of glGetString in GLESv1 library!");
        return EINA_FALSE;
     }

   gles1_exts = (const char *) gles1_funcs->glGetString(GL_EXTENSIONS);
   if (!gles1_exts)
     {
        ERR("GLESv1:glGetString(GL_EXTENSIONS) returned NULL!");
        return EINA_FALSE;
     }

   if (!_gles1_ext_string)
     {
        _gles1_ext_string = calloc(MAX_EXTENSION_STRING_BUFFER, 1);
        if (!_gles1_ext_string) return EINA_FALSE;
     }

   _gles1_ext_string[0] = '\0';

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Scanning supported extensions, sets the variables
   /////////////////////////////////////////////////////////////////////////////////////////////////////

   // Preparing all the magic macros
#define GETPROCADDR(sym) \
   ((__typeof__((*drvfunc))) (eglGetProcAddress(sym)))

#define _EVASGL_EXT_BEGIN(name) \
   { \
      int *ext_support = &_gles1_ext_support_##name; \
      *ext_support = 0;

#define _EVASGL_EXT_END() \
   }

#define _EVASGL_EXT_CHECK_SUPPORT(name) \
   (strstr(gles1_exts, name) != NULL)

#define _EVASGL_EXT_DISCARD_SUPPORT() \
   *ext_support = 0;

#define _EVASGL_EXT_DRVNAME(name) \
   if (_EVASGL_EXT_CHECK_SUPPORT(#name)) *ext_support = 1;

#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param) \
     { \
        ret (**drvfunc)param = &gles1_ext_sym_##name;

#define _EVASGL_EXT_FUNCTION_END() \
        if ((*drvfunc) == NULL) _EVASGL_EXT_DISCARD_SUPPORT(); \
     }

#define _EVASGL_EXT_FUNCTION_DRVFUNC(name) \
   if ((*drvfunc) == NULL) *drvfunc = name;

#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name) \
   if ((*drvfunc) == NULL) \
     { \
        *drvfunc = GETPROCADDR(name); \
        evgl_safe_extension_add(name, (void *) (*drvfunc)); \
     } \
   else evgl_safe_extension_add(name, NULL);

#ifdef _EVASGL_EXT_FUNCTION_WHITELIST
# undef _EVASGL_EXT_FUNCTION_WHITELIST
#endif
#define _EVASGL_EXT_FUNCTION_WHITELIST(name) evgl_safe_extension_add(name, NULL);

#define _EVASGL_EXT_GLES1_ONLY 1

   // Okay, now we are ready to scan.
#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_GLES1_ONLY
#undef _EVASGL_EXT_FUNCTION_WHITELIST
#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
#undef GETPROCADDR

#define _EVASGL_EXT_BEGIN(name) \
     _curext_supported = (_gles1_ext_support_##name != 0);


   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Scanning again to add to the gles1 ext string list
   /////////////////////////////////////////////////////////////////////////////////////////////////////

#define _EVASGL_EXT_END()
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_DRVNAME(name) if (_curext_supported) strcat(_gles1_ext_string, #name" ");
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param)
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR

   if (evgl_engine->api_debug_mode)
     DBG("GLES1: List of supported extensions:\n%s", _gles1_ext_string);

   // Both GLES versions have been initialized!
   _evgl_api_ext_status = 2;
   return EINA_TRUE;
#else
   ERR("GLESv1 support is not implemented for GLX");
   return EINA_FALSE;
#endif
}

void
evgl_api_gles1_ext_get(Evas_GL_API *gl_funcs)
{
   if (_evgl_api_ext_status < 1)
     {
        ERR("EVGL extension is not yet initialized.");
        return;
     }

   if (_evgl_api_ext_status < 2)
     {
        DBG("Initializing GLESv1 extensions...");
        if (!_evgl_api_gles1_ext_init())
          {
             ERR("GLESv1 extensions initialization failed");
             return;
          }
     }

#define ORD(f) EVAS_API_OVERRIDE(f, gl_funcs, gles1_ext_sym_)

   /////////////////////////////////////////////////////////////////////////////////////////////////////
   // Extension HEADER
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#define _EVASGL_EXT_CHECK_SUPPORT(name)
#define _EVASGL_EXT_DISCARD_SUPPORT()
#define _EVASGL_EXT_BEGIN(name) \
   if (_gles1_ext_support_##name != 0) \
     {
#define _EVASGL_EXT_END() \
     }
#define _EVASGL_EXT_DRVNAME(name)
#define _EVASGL_EXT_FUNCTION_BEGIN(ret, name, param) \
   ORD(name);
#define _EVASGL_EXT_FUNCTION_END()
#define _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN(ret, name, param)
#define _EVASGL_EXT_FUNCTION_PRIVATE_END()
#define _EVASGL_EXT_FUNCTION_DRVFUNC(name)
#define _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR(name)

#undef _EVASGL_EXT_WHITELIST_ONLY
#define _EVASGL_EXT_WHITELIST_ONLY 0

#include "evas_gl_api_ext_def.h"

#undef _EVASGL_EXT_CHECK_SUPPORT
#undef _EVASGL_EXT_DISCARD_SUPPORT
#undef _EVASGL_EXT_BEGIN
#undef _EVASGL_EXT_END
#undef _EVASGL_EXT_DRVNAME
#undef _EVASGL_EXT_FUNCTION_BEGIN
#undef _EVASGL_EXT_FUNCTION_END
#undef _EVASGL_EXT_FUNCTION_PRIVATE_BEGIN
#undef _EVASGL_EXT_FUNCTION_PRIVATE_END
#undef _EVASGL_EXT_FUNCTION_DRVFUNC
#undef _EVASGL_EXT_FUNCTION_DRVFUNC_PROCADDR
   /////////////////////////////////////////////////////////////////////////////////////////////////////
#undef ORD

}

const char *
evgl_api_ext_string_get()
{
   if (_evgl_api_ext_status < 1)
     {
        ERR("EVGL extension is not yet initialized.");
        return NULL;
     }

   return _gl_ext_string;
}
