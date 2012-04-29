#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "evas_gl_core.h"


#define FLAG_BIT_0      0x01
#define FLAG_BIT_1      0x02
#define FLAG_BIT_2      0x04
#define FLAG_BIT_3      0x08
#define FLAG_BIT_4      0x10
#define FLAG_BIT_5      0x20
#define FLAG_BIT_6      0x40
#define FLAG_BIT_7      0x80

//#define EVAS_GL_DEBUG 1
//#define EVAS_GL_PROFILE_LOG

#define EVAS_GL_DEBUG_ASSERT(x)  \
   do { \
        if (!(x)) { ERR("Assert(%s) failed..", #x); exit(0); } \
   } while (0)

#ifdef EVAS_GL_DEBUG
   #define COMPARE_OPT_SKIP  (1) ||
   #define THREAD_CHECK_DEBUG() thread_check()
#else
   #define COMPARE_OPT_SKIP
   #define THREAD_CHECK_DEBUG()
#endif


typedef struct _Call_Log
{
   int count;
   struct timeval start;
   struct timeval end;
   struct timeval total;
} Call_Log;

static Call_Log mc_log;
static int frame_count = 0;


#ifdef EVAS_GL_PROFILE_LOG
#  define EVAS_GL_INIT_LOG(a) \
   do { \
        (a).count = 0; \
        (a).total.tv_sec = 0; \
        (a).total.tv_usec = 0; \
   } while(0)

#  define EVAS_GL_START_LOG(a) \
   do { \
        if (gettimeofday(&(a.start), NULL) != 0) \
        fprintf(stderr, "Error Getting Time of Day \n"); \
   } while(0)\


#  define EVAS_GL_END_LOG(a) \
   do { \
        gettimeofday(&(a.end), NULL); \
        a.count++; \
        a.total.tv_sec = (a.end.tv_sec - a.start.tv_sec); \
        a.total.tv_usec = (a.end.tv_usec - a.start.tv_usec); \
        print_log("GLProfile", &a); \
   } while(0)
#else
#  define EVAS_GL_INIT_LOG(a)

#  define EVAS_GL_START_LOG(a)

#  define EVAS_GL_END_LOG(a)
#endif


static void
print_log(const char * name, Call_Log *log)
{
    static double total_time = 0;
    static int total_count = 0;
    double seconds = log->total.tv_sec + log->total.tv_usec / 1000000.0;
    total_count++;
    total_time += seconds;

    //if (!(log->count % 100))
    if (!(total_count % 100))
      {
         fprintf(stderr, "\t[%s] Count: %d in %6.3f seconds : %d total count %6.3f total second ---- FRAMECOUNT: %d\n",
                 name, log->count, seconds, total_count, total_time, frame_count);
      }

    EVAS_GL_INIT_LOG(*log);
}

#define EVAS_GL_PRINT_LOG(a, b) print_log(#a, &b)



//------------------------------------------------------//
typedef _eng_fn                (*glsym_func_eng_fn) ();
typedef void                   (*glsym_func_void) ();
typedef XID                    (*glsym_func_xid) ();
typedef XVisualInfo           *(*glsym_func_xvisinfo_ptr) ();
typedef Bool                   (*glsym_func_bool) ();
typedef unsigned int           (*glsym_func_uint) ();
typedef int                    (*glsym_func_int) ();
typedef unsigned char          (*glsym_func_uchar) ();
typedef unsigned char         *(*glsym_func_uchar_ptr) ();
typedef const unsigned char   *(*glsym_func_const_uchar_ptr) ();
typedef char const            *(*glsym_func_char_const_ptr) ();

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
typedef EGLDisplay             (*glsym_func_egldpy) ();
typedef EGLContext             (*glsym_func_eglctx) ();
typedef EGLSurface             (*glsym_func_eglsfc) ();
typedef EGLBoolean             (*glsym_func_eglbool) ();
#else
typedef GLXContext             (*glsym_func_glxctx) ();
typedef GLXDrawable            (*glsym_func_glxdraw) ();
typedef GLXFBConfig           *(*glsym_func_glxfbcfg_ptr) ();
#endif
//------------------------------------------------------//

static void                *gl_lib_handle;

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
static void                *egl_lib_handle;
static EGLDisplay           global_dpy         = EGL_DEFAULT_DISPLAY;
static EGLSurface           current_surf       = EGL_NO_SURFACE;
static EGLContext           global_ctx         = NULL;
#else
static Display             *global_dpy         = NULL;
static GLXDrawable          current_surf       = None;
static GLXContext           global_ctx         = NULL;
#endif
static int                  global_ctx_initted = 0;
static EvasGlueContext      current_ctx        = NULL;
static EvasGlueContext      real_current_ctx   = NULL;
static int                  ctx_ref_count      = 0;
static int                  current_tid        = 0;


static int                  log_opt            = 0;


#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)

//------------------------------------------------------//
// EGL APIs... Only ones that are being used.
//--------//
_eng_fn      (*GL(eglGetProcAddress))            (const char *procname) = NULL;

//--------//
// Standard EGL APIs
EGLint       (*GL(eglGetError))                  (void) = NULL;
EGLDisplay   (*GL(eglGetDisplay))                (EGLNativeDisplayType display_id) = NULL;
EGLBoolean   (*GL(eglInitialize))                (EGLDisplay dpy, EGLint* major, EGLint* minor) = NULL;
EGLBoolean   (*GL(eglTerminate))                 (EGLDisplay dpy) = NULL;
EGLBoolean   (*GL(eglChooseConfig))              (EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, EGLint config_size, EGLint* num_config) = NULL;
EGLSurface   (*GL(eglCreateWindowSurface)  )     (EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint* attrib_list) = NULL;
EGLSurface   (*GL(eglCreatePixmapSurface)  )     (EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint* attrib_list) = NULL;
EGLBoolean   (*GL(eglDestroySurface))            (EGLDisplay dpy, EGLSurface surface) = NULL;
EGLBoolean   (*GL(eglBindAPI))                   (EGLenum api) = NULL;
EGLBoolean   (*GL(eglWaitClient))                (void) = NULL;
EGLBoolean   (*GL(eglSurfaceAttrib))             (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value) = NULL;
void         (*GL(eglBindTexImage))              (EGLDisplay dpy, EGLSurface surface, EGLint buffer) = NULL;
EGLBoolean   (*GL(eglReleaseTexImage))           (EGLDisplay dpy, EGLSurface surface, EGLint buffer) = NULL;
EGLBoolean   (*GL(eglSwapInterval))              (EGLDisplay dpy, EGLint interval) = NULL;
EGLContext   (*GL(eglCreateContext))             (EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list) = NULL;
EGLBoolean   (*GL(eglDestroyContext))            (EGLDisplay dpy, EGLContext ctx) = NULL;
EGLBoolean   (*GL(eglMakeCurrent))               (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) = NULL;
EGLContext   (*GL(eglGetCurrentContext))         (void) = NULL;
EGLSurface   (*GL(eglGetCurrentSurface))         (EGLint readdraw) = NULL;
EGLDisplay   (*GL(eglGetCurrentDisplay))         (void) = NULL;
EGLBoolean   (*GL(eglWaitGL))                    (void) = NULL;
EGLBoolean   (*GL(eglWaitNative))                (EGLint engine) = NULL;
EGLBoolean   (*GL(eglSwapBuffers))               (EGLDisplay dpy, EGLSurface surface) = NULL;
EGLBoolean   (*GL(eglCopyBuffers))               (EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target) = NULL;
char const  *(*GL(eglQueryString))               (EGLDisplay dpy, EGLint name) = NULL;

// Extensions
void         *(*GL(eglCreateImage))               (void *a, void *b, GLenum c, void *d, const int *e) = NULL;
unsigned int  (*GL(eglDestroyImage))              (void *a, void *b) = NULL;
void          (*GL(glEGLImageTargetTexture2DOES)) (int a, void *b) = NULL;
void          (*GL(glEGLImageTargetRenderbufferStorageOES)) (int a, void *b) = NULL;
void         *(*GL(eglMapImageSEC))               (void *a, void *b) = NULL;
unsigned int  (*GL(eglUnmapImageSEC))             (void *a, void *b) = NULL;
unsigned int  (*GL(eglGetImageAttribSEC))         (void *a, void *b, int c, int *d) = NULL;


//--------//

//------------------------------------------------------//
// Internal EGL APIs... Only ones that are being used.
//--------//
static _eng_fn      (*_sym_eglGetProcAddress)            (const char* procname) = NULL;
// Standard EGL funtions
static EGLint       (*_sym_eglGetError)                  (void) = NULL;
static EGLDisplay   (*_sym_eglGetDisplay)                (EGLNativeDisplayType display_id) = NULL;
static EGLBoolean   (*_sym_eglInitialize)                (EGLDisplay dpy, EGLint* major, EGLint* minor) = NULL;
static EGLBoolean   (*_sym_eglTerminate)                 (EGLDisplay dpy) = NULL;
static EGLBoolean   (*_sym_eglChooseConfig)              (EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, EGLint config_size, EGLint* num_config) = NULL;
static EGLSurface   (*_sym_eglCreateWindowSurface  )     (EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint* attrib_list) = NULL;
static EGLSurface   (*_sym_eglCreatePixmapSurface  )     (EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint* attrib_list) = NULL;
static EGLBoolean   (*_sym_eglDestroySurface)            (EGLDisplay dpy, EGLSurface surface) = NULL;
static EGLBoolean   (*_sym_eglBindAPI)                   (EGLenum api) = NULL;
static EGLBoolean   (*_sym_eglWaitClient)                (void) = NULL;
static EGLBoolean   (*_sym_eglSurfaceAttrib)             (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value) = NULL;
static void         (*_sym_eglBindTexImage)              (EGLDisplay dpy, EGLSurface surface, EGLint buffer) = NULL;
static EGLBoolean   (*_sym_eglReleaseTexImage)           (EGLDisplay dpy, EGLSurface surface, EGLint buffer) = NULL;
static EGLBoolean   (*_sym_eglSwapInterval)              (EGLDisplay dpy, EGLint interval) = NULL;
static EGLContext   (*_sym_eglCreateContext)             (EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list) = NULL;
static EGLBoolean   (*_sym_eglDestroyContext)            (EGLDisplay dpy, EGLContext ctx) = NULL;
static EGLBoolean   (*_sym_eglMakeCurrent)               (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) = NULL;
static EGLContext   (*_sym_eglGetCurrentContext)         (void) = NULL;
static EGLSurface   (*_sym_eglGetCurrentSurface)         (EGLint readdraw) = NULL;
static EGLDisplay   (*_sym_eglGetCurrentDisplay)         (void) = NULL;
static EGLBoolean   (*_sym_eglWaitGL)                    (void) = NULL;
static EGLBoolean   (*_sym_eglWaitNative)                (EGLint engine) = NULL;
static EGLBoolean   (*_sym_eglSwapBuffers)               (EGLDisplay dpy, EGLSurface surface) = NULL;
static EGLBoolean   (*_sym_eglCopyBuffers)               (EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target) = NULL;
static char const  *(*_sym_eglQueryString)               (EGLDisplay dpy, EGLint name) = NULL;

// Extensions
static void         *(*_sym_eglCreateImage)               (void *a, void *b, GLenum c, void *d, const int *e) = NULL;
static unsigned int  (*_sym_eglDestroyImage)              (void *a, void *b) = NULL;
static void          (*_sym_glEGLImageTargetTexture2DOES) (int a, void *b) = NULL;
static void          (*_sym_glEGLImageTargetRenderbufferStorageOES) (int a, void *b) = NULL;
static void         *(*_sym_eglMapImageSEC)               (void *a, void *b) = NULL;
static unsigned int  (*_sym_eglUnmapImageSEC)             (void *a, void *b) = NULL;
static unsigned int  (*_sym_eglGetImageAttribSEC)         (void *a, void *b, int c, int *d) = NULL;


#else
//------------------------------------------------------//
// GLX APIs... Only ones that are being used.
//--------//
_eng_fn      (*GL(glXGetProcAddress))            (const char* procName) = NULL;

// Standard glX functions
XVisualInfo* (*GL(glXChooseVisual))              (Display* dpy, int screen, int* attribList) = NULL;
GLXContext   (*GL(glXCreateContext))             (Display* dpy, XVisualInfo* vis, GLXContext shareList, Bool direct) = NULL;
void         (*GL(glXDestroyContext))            (Display* dpy, GLXContext ctx) = NULL;
GLXContext   (*GL(glXGetCurrentContext))         (void) = NULL;
GLXDrawable  (*GL(glXGetCurrentDrawable))        (void) = NULL;
Bool         (*GL(glXMakeCurrent))               (Display* dpy, GLXDrawable draw, GLXContext ctx) = NULL;
void         (*GL(glXSwapBuffers))               (Display* dpy, GLXDrawable draw) = NULL;
void         (*GL(glXWaitX))                     (void) = NULL;
void         (*GL(glXWaitGL))                    (void) = NULL;
Bool         (*GL(glXQueryExtension))            (Display* dpy, int* errorb, int *event) = NULL;
const char  *(*GL(glXQueryExtensionsString))     (Display *dpy, int screen) = NULL;

//--------//
GLXFBConfig* (*GL(glXChooseFBConfig))            (Display* dpy, int screen, const int* attribList, int* nitems) = NULL;
GLXFBConfig* (*GL(glXGetFBConfigs))              (Display* dpy, int screen, int* nelements) = NULL;
int          (*GL(glXGetFBConfigAttrib))         (Display* dpy, GLXFBConfig config, int attribute, int* value) = NULL;
XVisualInfo* (*GL(glXGetVisualFromFBConfig))     (Display* dpy, GLXFBConfig config) = NULL;
void         (*GL(glXDestroyWindow))             (Display* dpy, GLXWindow window) = NULL;
Bool         (*GL(glXMakeContextCurrent))        (Display* dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx) = NULL;
//--------//

void         (*GL(glXBindTexImage))              (Display* dpy, GLXDrawable draw, int buffer, int* attribList) = NULL;
void         (*GL(glXReleaseTexImage))           (Display* dpy, GLXDrawable draw, int buffer) = NULL;
int          (*GL(glXGetVideoSync))              (unsigned int* count) = NULL;
int          (*GL(glXWaitVideoSync))             (int divisor, int remainder, unsigned int *count) = NULL;
XID          (*GL(glXCreatePixmap))              (Display* dpy, void* config, Pixmap pixmap, const int* attribList) = NULL;
void         (*GL(glXDestroyPixmap))             (Display* dpy, XID pixmap) = NULL;
void         (*GL(glXQueryDrawable))             (Display* dpy, XID draw, int attribute, unsigned int* value) = NULL;
int          (*GL(glXSwapIntervalSGI))           (int interval) = NULL;
void         (*GL(glXSwapIntervalEXT))           (Display* dpy, GLXDrawable draw, int interval) = NULL;

//------------------//


//------------------------------------------------------//
// Internal GLX APIs... Only ones that are being used.
//--------//
static _eng_fn      (*_sym_glXGetProcAddress)            (const char* procName) = NULL;

// Standard glX functions
static XVisualInfo* (*_sym_glXChooseVisual)              (Display* dpy, int screen, int* attribList) = NULL;
static GLXContext   (*_sym_glXCreateContext)             (Display* dpy, XVisualInfo* vis, GLXContext shareList, Bool direct) = NULL;
static void         (*_sym_glXDestroyContext)            (Display* dpy, GLXContext ctx) = NULL;
static GLXContext   (*_sym_glXGetCurrentContext)         (void) = NULL;
static GLXDrawable  (*_sym_glXGetCurrentDrawable)        (void) = NULL;
static Bool         (*_sym_glXMakeCurrent)               (Display* dpy, GLXDrawable draw, GLXContext ctx) = NULL;
static void         (*_sym_glXSwapBuffers)               (Display* dpy, GLXDrawable draw) = NULL;
static void         (*_sym_glXWaitX)                     (void) = NULL;
static void         (*_sym_glXWaitGL)                    (void) = NULL;
static Bool         (*_sym_glXQueryExtension)            (Display* dpy, int* errorb, int* event) = NULL;
static const char  *(*_sym_glXQueryExtensionsString)     (Display* dpy, int screen);

//--------//
static GLXFBConfig* (*_sym_glXChooseFBConfig)            (Display* dpy, int screen, const int* attribList, int* nitems) = NULL;
static GLXFBConfig* (*_sym_glXGetFBConfigs)              (Display* dpy, int screen, int* nelements) = NULL;
static int          (*_sym_glXGetFBConfigAttrib)         (Display* dpy, GLXFBConfig config, int attribute, int* value) = NULL;
static XVisualInfo* (*_sym_glXGetVisualFromFBConfig)     (Display* dpy, GLXFBConfig config) = NULL;
static void         (*_sym_glXDestroyWindow)             (Display* dpy, GLXWindow window) = NULL;
static Bool         (*_sym_glXMakeContextCurrent)        (Display* dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx) = NULL;
//--------//

static void         (*_sym_glXBindTexImage)              (Display* dpy, GLXDrawable draw, int buffer, int* attribList) = NULL;
static void         (*_sym_glXReleaseTexImage)           (Display* dpy, GLXDrawable draw, int buffer) = NULL;
static int          (*_sym_glXGetVideoSync)              (unsigned int* count) = NULL;
static int          (*_sym_glXWaitVideoSync)             (int divisor, int remainder, unsigned int* count) = NULL;
static XID          (*_sym_glXCreatePixmap)              (Display* dpy, void* config, Pixmap pixmap, const int* attribList) = NULL;
static void         (*_sym_glXDestroyPixmap)             (Display* dpy, XID pixmap) = NULL;
static void         (*_sym_glXQueryDrawable)             (Display* dpy, XID draw, int attribute, unsigned int* value) = NULL;
static int          (*_sym_glXSwapIntervalSGI)           (int interval) = NULL;
static void         (*_sym_glXSwapIntervalEXT)           (Display* dpy, GLXDrawable draw, int interval) = NULL;

//------------------//
#endif

//------------------------------------------------------//


/* version 1: */
void         (*GL(glActiveTexture)) (GLenum texture) = NULL;
void         (*GL(glAttachShader)) (GLuint program, GLuint shader) = NULL;
void         (*GL(glBindAttribLocation)) (GLuint program, GLuint index, const char* name) = NULL;
void         (*GL(glBindBuffer)) (GLenum target, GLuint buffer) = NULL;
void         (*GL(glBindFramebuffer)) (GLenum target, GLuint framebuffer) = NULL;
void         (*GL(glBindRenderbuffer)) (GLenum target, GLuint renderbuffer) = NULL;
void         (*GL(glBindTexture)) (GLenum target, GLuint texture) = NULL;
void         (*GL(glBlendColor)) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
void         (*GL(glBlendEquation)) ( GLenum mode ) = NULL;
void         (*GL(glBlendEquationSeparate)) (GLenum modeRGB, GLenum modeAlpha) = NULL;
void         (*GL(glBlendFunc)) (GLenum sfactor, GLenum dfactor) = NULL;
void         (*GL(glBlendFuncSeparate)) (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) = NULL;
void         (*GL(glBufferData)) (GLenum target, GLsizeiptr size, const void* data, GLenum usage) = NULL;
void         (*GL(glBufferSubData)) (GLenum target, GLintptr offset, GLsizeiptr size, const void* data) = NULL;
GLenum       (*GL(glCheckFramebufferStatus)) (GLenum target) = NULL;
void         (*GL(glClear)) (GLbitfield mask) = NULL;
void         (*GL(glClearColor)) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
void         (*GL(glClearDepthf)) (GLclampf depth) = NULL;
void         (*GL(glClearStencil)) (GLint s) = NULL;
void         (*GL(glColorMask)) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) = NULL;
void         (*GL(glCompileShader)) (GLuint shader) = NULL;
void         (*GL(glCompressedTexImage2D)) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data) = NULL;
void         (*GL(glCompressedTexSubImage2D)) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) = NULL;
void         (*GL(glCopyTexImage2D)) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) = NULL;
void         (*GL(glCopyTexSubImage2D)) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) = NULL;
GLuint       (*GL(glCreateProgram)) (void) = NULL;
GLuint       (*GL(glCreateShader)) (GLenum type) = NULL;
void         (*GL(glCullFace)) (GLenum mode) = NULL;
void         (*GL(glDeleteBuffers)) (GLsizei n, const GLuint* buffers) = NULL;
void         (*GL(glDeleteFramebuffers)) (GLsizei n, const GLuint* framebuffers) = NULL;
void         (*GL(glDeleteProgram)) (GLuint program) = NULL;
void         (*GL(glDeleteRenderbuffers)) (GLsizei n, const GLuint* renderbuffers) = NULL;
void         (*GL(glDeleteShader)) (GLuint shader) = NULL;
void         (*GL(glDeleteTextures)) (GLsizei n, const GLuint* textures) = NULL;
void         (*GL(glDepthFunc)) (GLenum func) = NULL;
void         (*GL(glDepthMask)) (GLboolean flag) = NULL;
void         (*GL(glDepthRangef)) (GLclampf zNear, GLclampf zFar) = NULL;
void         (*GL(glDetachShader)) (GLuint program, GLuint shader) = NULL;
void         (*GL(glDisable)) (GLenum cap) = NULL;
void         (*GL(glDisableVertexAttribArray)) (GLuint index) = NULL;
void         (*GL(glDrawArrays)) (GLenum mode, GLint first, GLsizei count) = NULL;
void         (*GL(glDrawElements)) (GLenum mode, GLsizei count, GLenum type, const void* indices) = NULL;
void         (*GL(glEnable)) (GLenum cap) = NULL;
void         (*GL(glEnableVertexAttribArray)) (GLuint index) = NULL;
void         (*GL(glFinish)) (void) = NULL;
void         (*GL(glFlush)) (void) = NULL;
void         (*GL(glFramebufferRenderbuffer)) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) = NULL;
void         (*GL(glFramebufferTexture2D)) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) = NULL;
void         (*GL(glFrontFace)) (GLenum mode) = NULL;
void         (*GL(glGenBuffers)) (GLsizei n, GLuint* buffers) = NULL;
void         (*GL(glGenerateMipmap)) (GLenum target) = NULL;
void         (*GL(glGenFramebuffers)) (GLsizei n, GLuint* framebuffers) = NULL;
void         (*GL(glGenRenderbuffers)) (GLsizei n, GLuint* renderbuffers) = NULL;
void         (*GL(glGenTextures)) (GLsizei n, GLuint* textures) = NULL;
void         (*GL(glGetActiveAttrib)) (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name) = NULL;
void         (*GL(glGetActiveUniform)) (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name) = NULL;
void         (*GL(glGetAttachedShaders)) (GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) = NULL;
int          (*GL(glGetAttribLocation)) (GLuint program, const char* name) = NULL;
void         (*GL(glGetBooleanv)) (GLenum pname, GLboolean* params) = NULL;
void         (*GL(glGetBufferParameteriv)) (GLenum target, GLenum pname, GLint* params) = NULL;
GLenum       (*GL(glGetError)) (void) = NULL;
void         (*GL(glGetFloatv)) (GLenum pname, GLfloat* params) = NULL;
void         (*GL(glGetFramebufferAttachmentParameteriv)) (GLenum target, GLenum attachment, GLenum pname, GLint* params) = NULL;
void         (*GL(glGetIntegerv)) (GLenum pname, GLint* params) = NULL;
void         (*GL(glGetProgramiv)) (GLuint program, GLenum pname, GLint* params) = NULL;
void         (*GL(glGetProgramInfoLog)) (GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) = NULL;
void         (*GL(glGetRenderbufferParameteriv)) (GLenum target, GLenum pname, GLint* params) = NULL;
void         (*GL(glGetShaderiv)) (GLuint shader, GLenum pname, GLint* params) = NULL;
void         (*GL(glGetShaderInfoLog)) (GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) = NULL;
void         (*GL(glGetShaderPrecisionFormat)) (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) = NULL;
void         (*GL(glGetShaderSource)) (GLuint shader, GLsizei bufsize, GLsizei* length, char* source) = NULL;
const GLubyte *(*GL(glGetString)) (GLenum name) = NULL;
void         (*GL(glGetTexParameterfv)) (GLenum target, GLenum pname, GLfloat* params) = NULL;
void         (*GL(glGetTexParameteriv)) (GLenum target, GLenum pname, GLint* params) = NULL;
void         (*GL(glGetUniformfv)) (GLuint program, GLint location, GLfloat* params) = NULL;
void         (*GL(glGetUniformiv)) (GLuint program, GLint location, GLint* params) = NULL;
int          (*GL(glGetUniformLocation)) (GLuint program, const char* name) = NULL;
void         (*GL(glGetVertexAttribfv)) (GLuint index, GLenum pname, GLfloat* params) = NULL;
void         (*GL(glGetVertexAttribiv)) (GLuint index, GLenum pname, GLint* params) = NULL;
void         (*GL(glGetVertexAttribPointerv)) (GLuint index, GLenum pname, void** pointer) = NULL;
void         (*GL(glHint)) (GLenum target, GLenum mode) = NULL;
GLboolean    (*GL(glIsBuffer)) (GLuint buffer) = NULL;
GLboolean    (*GL(glIsEnabled)) (GLenum cap) = NULL;
GLboolean    (*GL(glIsFramebuffer)) (GLuint framebuffer) = NULL;
GLboolean    (*GL(glIsProgram)) (GLuint program) = NULL;
GLboolean    (*GL(glIsRenderbuffer)) (GLuint renderbuffer) = NULL;
GLboolean    (*GL(glIsShader)) (GLuint shader) = NULL;
GLboolean    (*GL(glIsTexture)) (GLuint texture) = NULL;
void         (*GL(glLineWidth)) (GLfloat width) = NULL;
void         (*GL(glLinkProgram)) (GLuint program) = NULL;
void         (*GL(glPixelStorei)) (GLenum pname, GLint param) = NULL;
void         (*GL(glPolygonOffset)) (GLfloat factor, GLfloat units) = NULL;
void         (*GL(glReadPixels)) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) = NULL;
void         (*GL(glReleaseShaderCompiler)) (void) = NULL;
void         (*GL(glRenderbufferStorage)) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height) = NULL;
void         (*GL(glSampleCoverage)) (GLclampf value, GLboolean invert) = NULL;
void         (*GL(glScissor)) (GLint x, GLint y, GLsizei width, GLsizei height) = NULL;
void         (*GL(glShaderBinary)) (GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary, GLsizei length) = NULL;
void         (*GL(glShaderSource)) (GLuint shader, GLsizei count, const char** string, const GLint* length) = NULL;
void         (*GL(glStencilFunc)) (GLenum func, GLint ref, GLuint mask) = NULL;
void         (*GL(glStencilFuncSeparate)) (GLenum face, GLenum func, GLint ref, GLuint mask) = NULL;
void         (*GL(glStencilMask)) (GLuint mask) = NULL;
void         (*GL(glStencilMaskSeparate)) (GLenum face, GLuint mask) = NULL;
void         (*GL(glStencilOp)) (GLenum fail, GLenum zfail, GLenum zpass) = NULL;
void         (*GL(glStencilOpSeparate)) (GLenum face, GLenum fail, GLenum zfail, GLenum zpass) = NULL;
void         (*GL(glTexImage2D)) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) = NULL;
void         (*GL(glTexParameterf)) (GLenum target, GLenum pname, GLfloat param) = NULL;
void         (*GL(glTexParameterfv)) (GLenum target, GLenum pname, const GLfloat* params) = NULL;
void         (*GL(glTexParameteri)) (GLenum target, GLenum pname, GLint param) = NULL;
void         (*GL(glTexParameteriv)) (GLenum target, GLenum pname, const GLint* params) = NULL;
void         (*GL(glTexSubImage2D)) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) = NULL;
void         (*GL(glUniform1f)) (GLint location, GLfloat x) = NULL;
void         (*GL(glUniform1fv)) (GLint location, GLsizei count, const GLfloat* v) = NULL;
void         (*GL(glUniform1i)) (GLint location, GLint x) = NULL;
void         (*GL(glUniform1iv)) (GLint location, GLsizei count, const GLint* v) = NULL;
void         (*GL(glUniform2f)) (GLint location, GLfloat x, GLfloat y) = NULL;
void         (*GL(glUniform2fv)) (GLint location, GLsizei count, const GLfloat* v) = NULL;
void         (*GL(glUniform2i)) (GLint location, GLint x, GLint y) = NULL;
void         (*GL(glUniform2iv)) (GLint location, GLsizei count, const GLint* v) = NULL;
void         (*GL(glUniform3f)) (GLint location, GLfloat x, GLfloat y, GLfloat z) = NULL;
void         (*GL(glUniform3fv)) (GLint location, GLsizei count, const GLfloat* v) = NULL;
void         (*GL(glUniform3i)) (GLint location, GLint x, GLint y, GLint z) = NULL;
void         (*GL(glUniform3iv)) (GLint location, GLsizei count, const GLint* v) = NULL;
void         (*GL(glUniform4f)) (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) = NULL;
void         (*GL(glUniform4fv)) (GLint location, GLsizei count, const GLfloat* v) = NULL;
void         (*GL(glUniform4i)) (GLint location, GLint x, GLint y, GLint z, GLint w) = NULL;
void         (*GL(glUniform4iv)) (GLint location, GLsizei count, const GLint* v) = NULL;
void         (*GL(glUniformMatrix2fv)) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) = NULL;
void         (*GL(glUniformMatrix3fv)) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) = NULL;
void         (*GL(glUniformMatrix4fv)) (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) = NULL;
void         (*GL(glUseProgram)) (GLuint program) = NULL;
void         (*GL(glValidateProgram)) (GLuint program) = NULL;
void         (*GL(glVertexAttrib1f)) (GLuint indx, GLfloat x) = NULL;
void         (*GL(glVertexAttrib1fv)) (GLuint indx, const GLfloat* values) = NULL;
void         (*GL(glVertexAttrib2f)) (GLuint indx, GLfloat x, GLfloat y) = NULL;
void         (*GL(glVertexAttrib2fv)) (GLuint indx, const GLfloat* values) = NULL;
void         (*GL(glVertexAttrib3f)) (GLuint indx, GLfloat x, GLfloat y, GLfloat z) = NULL;
void         (*GL(glVertexAttrib3fv)) (GLuint indx, const GLfloat* values) = NULL;
void         (*GL(glVertexAttrib4f)) (GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) = NULL;
void         (*GL(glVertexAttrib4fv)) (GLuint indx, const GLfloat* values) = NULL;
void         (*GL(glVertexAttribPointer)) (GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* ptr) = NULL;
void         (*GL(glViewport)) (GLint x, GLint y, GLsizei width, GLsizei height) = NULL;

/* Extensions */
void         (*GL(glGetProgramBinary)) (GLuint program, GLsizei bufsize, GLsizei *length, GLenum *binaryFormat, void *binary) = NULL;
void         (*GL(glProgramBinary)) (GLuint program, GLenum binaryFormat, const void *binary, GLint length) = NULL;
void         (*GL(glProgramParameteri)) (GLuint a, GLuint b, GLint d) = NULL;


//------------------------------------------------------//
// GLES 2.0 APIs...
static void       (*_sym_glActiveTexture)                       (GLenum texture) = NULL;
static void       (*_sym_glAttachShader)                        (GLuint program, GLuint shader) = NULL;
static void       (*_sym_glBindAttribLocation)                  (GLuint program, GLuint index, const char* name) = NULL;
static void       (*_sym_glBindBuffer)                          (GLenum target, GLuint buffer) = NULL;
static void       (*_sym_glBindFramebuffer)                     (GLenum target, GLuint framebuffer) = NULL;
static void       (*_sym_glBindRenderbuffer)                    (GLenum target, GLuint renderbuffer) = NULL;
static void       (*_sym_glBindTexture)                         (GLenum target, GLuint texture) = NULL;
static void       (*_sym_glBlendColor)                          (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
static void       (*_sym_glBlendEquation)                       (GLenum mode) = NULL;
static void       (*_sym_glBlendEquationSeparate)               (GLenum modeRGB, GLenum modeAlpha) = NULL;
static void       (*_sym_glBlendFunc)                           (GLenum sfactor, GLenum dfactor) = NULL;
static void       (*_sym_glBlendFuncSeparate)                   (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) = NULL;
static void       (*_sym_glBufferData)                          (GLenum target, GLsizeiptr size, const void* data, GLenum usage) = NULL;
static void       (*_sym_glBufferSubData)                       (GLenum target, GLintptr offset, GLsizeiptr size, const void* data) = NULL;
static GLenum     (*_sym_glCheckFramebufferStatus)              (GLenum target) = NULL;
static void       (*_sym_glClear)                               (GLbitfield mask) = NULL;
static void       (*_sym_glClearColor)                          (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) = NULL;
static void       (*_sym_glClearDepthf)                         (GLclampf depth) = NULL;
static void       (*_sym_glClearStencil)                        (GLint s) = NULL;
static void       (*_sym_glColorMask)                           (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) = NULL;
static void       (*_sym_glCompileShader)                       (GLuint shader) = NULL;
static void       (*_sym_glCompressedTexImage2D)                (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data) = NULL;
static void       (*_sym_glCompressedTexSubImage2D)             (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) = NULL;
static void       (*_sym_glCopyTexImage2D)                      (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) = NULL;
static void       (*_sym_glCopyTexSubImage2D)                   (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) = NULL;
static GLuint     (*_sym_glCreateProgram)                       (void) = NULL;
static GLuint     (*_sym_glCreateShader)                        (GLenum type) = NULL;
static void       (*_sym_glCullFace)                            (GLenum mode) = NULL;
static void       (*_sym_glDeleteBuffers)                       (GLsizei n, const GLuint* buffers) = NULL;
static void       (*_sym_glDeleteFramebuffers)                  (GLsizei n, const GLuint* framebuffers) = NULL;
static void       (*_sym_glDeleteProgram)                       (GLuint program) = NULL;
static void       (*_sym_glDeleteRenderbuffers)                 (GLsizei n, const GLuint* renderbuffers) = NULL;
static void       (*_sym_glDeleteShader)                        (GLuint shader) = NULL;
static void       (*_sym_glDeleteTextures)                      (GLsizei n, const GLuint* textures) = NULL;
static void       (*_sym_glDepthFunc)                           (GLenum func) = NULL;
static void       (*_sym_glDepthMask)                           (GLboolean flag) = NULL;
static void       (*_sym_glDepthRangef)                         (GLclampf zNear, GLclampf zFar) = NULL;
static void       (*_sym_glDetachShader)                        (GLuint program, GLuint shader) = NULL;
static void       (*_sym_glDisable)                             (GLenum cap) = NULL;
static void       (*_sym_glDisableVertexAttribArray)            (GLuint index) = NULL;
static void       (*_sym_glDrawArrays)                          (GLenum mode, GLint first, GLsizei count) = NULL;
static void       (*_sym_glDrawElements)                        (GLenum mode, GLsizei count, GLenum type, const void* indices) = NULL;
static void       (*_sym_glEnable)                              (GLenum cap) = NULL;
static void       (*_sym_glEnableVertexAttribArray)             (GLuint index) = NULL;
static void       (*_sym_glFinish)                              (void) = NULL;
static void       (*_sym_glFlush)                               (void) = NULL;
static void       (*_sym_glFramebufferRenderbuffer)             (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) = NULL;
static void       (*_sym_glFramebufferTexture2D)                (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) = NULL;
static void       (*_sym_glFrontFace)                           (GLenum mode) = NULL;
static void       (*_sym_glGenBuffers)                          (GLsizei n, GLuint* buffers) = NULL;
static void       (*_sym_glGenerateMipmap)                      (GLenum target) = NULL;
static void       (*_sym_glGenFramebuffers)                     (GLsizei n, GLuint* framebuffers) = NULL;
static void       (*_sym_glGenRenderbuffers)                    (GLsizei n, GLuint* renderbuffers) = NULL;
static void       (*_sym_glGenTextures)                         (GLsizei n, GLuint* textures) = NULL;
static void       (*_sym_glGetActiveAttrib)                     (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name) = NULL;
static void       (*_sym_glGetActiveUniform)                    (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name) = NULL;
static void       (*_sym_glGetAttachedShaders)                  (GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) = NULL;
static int        (*_sym_glGetAttribLocation)                   (GLuint program, const char* name) = NULL;
static void       (*_sym_glGetBooleanv)                         (GLenum pname, GLboolean* params) = NULL;
static void       (*_sym_glGetBufferParameteriv)                (GLenum target, GLenum pname, GLint* params) = NULL;
static GLenum     (*_sym_glGetError)                            (void) = NULL;
static void       (*_sym_glGetFloatv)                           (GLenum pname, GLfloat* params) = NULL;
static void       (*_sym_glGetFramebufferAttachmentParameteriv) (GLenum target, GLenum attachment, GLenum pname, GLint* params) = NULL;
static void       (*_sym_glGetIntegerv)                         (GLenum pname, GLint* params) = NULL;
static void       (*_sym_glGetProgramiv)                        (GLuint program, GLenum pname, GLint* params) = NULL;
static void       (*_sym_glGetProgramInfoLog)                   (GLuint program, GLsizei bufsize, GLsizei* length, char* infolog) = NULL;
static void       (*_sym_glGetRenderbufferParameteriv)          (GLenum target, GLenum pname, GLint* params) = NULL;
static void       (*_sym_glGetShaderiv)                         (GLuint shader, GLenum pname, GLint* params) = NULL;
static void       (*_sym_glGetShaderInfoLog)                    (GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog) = NULL;
static void       (*_sym_glGetShaderPrecisionFormat)            (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) = NULL;
static void       (*_sym_glGetShaderSource)                     (GLuint shader, GLsizei bufsize, GLsizei* length, char* source) = NULL;
static const GLubyte *(*_sym_glGetString)                           (GLenum name) = NULL;
static void       (*_sym_glGetTexParameterfv)                   (GLenum target, GLenum pname, GLfloat* params) = NULL;
static void       (*_sym_glGetTexParameteriv)                   (GLenum target, GLenum pname, GLint* params) = NULL;
static void       (*_sym_glGetUniformfv)                        (GLuint program, GLint location, GLfloat* params) = NULL;
static void       (*_sym_glGetUniformiv)                        (GLuint program, GLint location, GLint* params) = NULL;
static int        (*_sym_glGetUniformLocation)                  (GLuint program, const char* name) = NULL;
static void       (*_sym_glGetVertexAttribfv)                   (GLuint index, GLenum pname, GLfloat* params) = NULL;
static void       (*_sym_glGetVertexAttribiv)                   (GLuint index, GLenum pname, GLint* params) = NULL;
static void       (*_sym_glGetVertexAttribPointerv)             (GLuint index, GLenum pname, void** pointer) = NULL;
static void       (*_sym_glHint)                                (GLenum target, GLenum mode) = NULL;
static GLboolean  (*_sym_glIsBuffer)                            (GLuint buffer) = NULL;
static GLboolean  (*_sym_glIsEnabled)                           (GLenum cap) = NULL;
static GLboolean  (*_sym_glIsFramebuffer)                       (GLuint framebuffer) = NULL;
static GLboolean  (*_sym_glIsProgram)                           (GLuint program) = NULL;
static GLboolean  (*_sym_glIsRenderbuffer)                      (GLuint renderbuffer) = NULL;
static GLboolean  (*_sym_glIsShader)                            (GLuint shader) = NULL;
static GLboolean  (*_sym_glIsTexture)                           (GLuint texture) = NULL;
static void       (*_sym_glLineWidth)                           (GLfloat width) = NULL;
static void       (*_sym_glLinkProgram)                         (GLuint program) = NULL;
static void       (*_sym_glPixelStorei)                         (GLenum pname, GLint param) = NULL;
static void       (*_sym_glPolygonOffset)                       (GLfloat factor, GLfloat units) = NULL;
static void       (*_sym_glReadPixels)                          (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) = NULL;
static void       (*_sym_glReleaseShaderCompiler)               (void) = NULL;
static void       (*_sym_glRenderbufferStorage)                 (GLenum target, GLenum internalformat, GLsizei width, GLsizei height) = NULL;
static void       (*_sym_glSampleCoverage)                      (GLclampf value, GLboolean invert) = NULL;
static void       (*_sym_glScissor)                             (GLint x, GLint y, GLsizei width, GLsizei height) = NULL;
static void       (*_sym_glShaderBinary)                        (GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary, GLsizei length) = NULL;
static void       (*_sym_glShaderSource)                        (GLuint shader, GLsizei count, const char** string, const GLint* length) = NULL;
static void       (*_sym_glStencilFunc)                         (GLenum func, GLint ref, GLuint mask) = NULL;
static void       (*_sym_glStencilFuncSeparate)                 (GLenum face, GLenum func, GLint ref, GLuint mask) = NULL;
static void       (*_sym_glStencilMask)                         (GLuint mask) = NULL;
static void       (*_sym_glStencilMaskSeparate)                 (GLenum face, GLuint mask) = NULL;
static void       (*_sym_glStencilOp)                           (GLenum fail, GLenum zfail, GLenum zpass) = NULL;
static void       (*_sym_glStencilOpSeparate)                   (GLenum face, GLenum fail, GLenum zfail, GLenum zpass) = NULL;
static void       (*_sym_glTexImage2D)                          (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) = NULL;
static void       (*_sym_glTexParameterf)                       (GLenum target, GLenum pname, GLfloat param) = NULL;
static void       (*_sym_glTexParameterfv)                      (GLenum target, GLenum pname, const GLfloat* params) = NULL;
static void       (*_sym_glTexParameteri)                       (GLenum target, GLenum pname, GLint param) = NULL;
static void       (*_sym_glTexParameteriv)                      (GLenum target, GLenum pname, const GLint* params) = NULL;
static void       (*_sym_glTexSubImage2D)                       (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) = NULL;
static void       (*_sym_glUniform1f)                           (GLint location, GLfloat x) = NULL;
static void       (*_sym_glUniform1fv)                          (GLint location, GLsizei count, const GLfloat* v) = NULL;
static void       (*_sym_glUniform1i)                           (GLint location, GLint x) = NULL;
static void       (*_sym_glUniform1iv)                          (GLint location, GLsizei count, const GLint* v) = NULL;
static void       (*_sym_glUniform2f)                           (GLint location, GLfloat x, GLfloat y) = NULL;
static void       (*_sym_glUniform2fv)                          (GLint location, GLsizei count, const GLfloat* v) = NULL;
static void       (*_sym_glUniform2i)                           (GLint location, GLint x, GLint y) = NULL;
static void       (*_sym_glUniform2iv)                          (GLint location, GLsizei count, const GLint* v) = NULL;
static void       (*_sym_glUniform3f)                           (GLint location, GLfloat x, GLfloat y, GLfloat z) = NULL;
static void       (*_sym_glUniform3fv)                          (GLint location, GLsizei count, const GLfloat* v) = NULL;
static void       (*_sym_glUniform3i)                           (GLint location, GLint x, GLint y, GLint z) = NULL;
static void       (*_sym_glUniform3iv)                          (GLint location, GLsizei count, const GLint* v) = NULL;
static void       (*_sym_glUniform4f)                           (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) = NULL;
static void       (*_sym_glUniform4fv)                          (GLint location, GLsizei count, const GLfloat* v) = NULL;
static void       (*_sym_glUniform4i)                           (GLint location, GLint x, GLint y, GLint z, GLint w) = NULL;
static void       (*_sym_glUniform4iv)                          (GLint location, GLsizei count, const GLint* v) = NULL;
static void       (*_sym_glUniformMatrix2fv)                    (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) = NULL;
static void       (*_sym_glUniformMatrix3fv)                    (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) = NULL;
static void       (*_sym_glUniformMatrix4fv)                    (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) = NULL;
static void       (*_sym_glUseProgram)                          (GLuint program) = NULL;
static void       (*_sym_glValidateProgram)                     (GLuint program) = NULL;
static void       (*_sym_glVertexAttrib1f)                      (GLuint indx, GLfloat x) = NULL;
static void       (*_sym_glVertexAttrib1fv)                     (GLuint indx, const GLfloat* values) = NULL;
static void       (*_sym_glVertexAttrib2f)                      (GLuint indx, GLfloat x, GLfloat y) = NULL;
static void       (*_sym_glVertexAttrib2fv)                     (GLuint indx, const GLfloat* values) = NULL;
static void       (*_sym_glVertexAttrib3f)                      (GLuint indx, GLfloat x, GLfloat y, GLfloat z) = NULL;
static void       (*_sym_glVertexAttrib3fv)                     (GLuint indx, const GLfloat* values) = NULL;
static void       (*_sym_glVertexAttrib4f)                      (GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) = NULL;
static void       (*_sym_glVertexAttrib4fv)                     (GLuint indx, const GLfloat* values) = NULL;
static void       (*_sym_glVertexAttribPointer)                 (GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* ptr) = NULL;
static void       (*_sym_glViewport)                            (GLint x, GLint y, GLsizei width, GLsizei height) = NULL;

// GLES Extensions...
static void       (*_sym_glGetProgramBinary)                    (GLuint a, GLsizei b, GLsizei* c, GLenum* d, void* e) = NULL;
static void       (*_sym_glProgramBinary)                       (GLuint a, GLenum b, const void* c, GLint d) = NULL;
static void       (*_sym_glProgramParameteri)                   (GLuint a, GLuint b, GLint d) = NULL;


//------------------------------------------------------//
// Inline function for debugging
static inline void thread_check()
{
   if (current_tid == 0)
      current_tid = syscall(__NR_gettid);
   else
      EVAS_GL_DEBUG_ASSERT(current_tid == syscall(__NR_gettid));
}

//-------------------------------//
// Get Type
// 0 = glGet
// 1 = glGetVertexAttrib
// 2 = glGetVertexAttribPointer
//-------------------------------//
/*
static void
get_state(int index, GLenum state, int get_type, int val_type, void *value)
{
   int i;

   if (get_type == 0)
     {
        // Integer Type
        if (val_type == 0)
          {
             GLint real_val[4] = {-1, -1, -1, -1};
             GLint *curr_val = (GLint*)value;
             _sym_glGetIntegerv(state, real_val);
          }
        // Boolean Type
        else if (val_type == 1)
          {
             GLboolean real_val[4] = {3, 3, 3, 3};
             GLboolean *curr_val = (GLboolean*)value;
             _sym_glGetBooleanv(state, real_val);
          }
        // Float Type
        else if (val_type == 2)
          {
             GLfloat real_val[4] = {-1, -1, -1, -1};
             GLfloat *curr_val = (GLfloat*)value;
             _sym_glGetFloatv(state, real_val);
          }
        else
           fprintf(stderr, "Error Setting State: %d...\n", state);
     }
   else if (get_type == 1)
     {
        // GetVertexAttrib
        if (val_type == 0)
          {
             GLint real_val[4] = {-1, -1, -1, -1};
             GLint *curr_val = (GLint*)value;
             _sym_glGetVertexAttribiv(index, state, real_val);
          }
        else if (val_type == 2)
          {
             GLfloat real_val[4] = {-1, -1, -1, -1};
             GLfloat *curr_val = (GLfloat*)value;
             _sym_glGetVertexAttribfv(index, state, real_val);
          }
     }
   else if (get_type == 2)
     {
        GLint real_val[4] = {-1, -1, -1, -1};
        GLint *curr_val = (GLint*)value;
        _sym_glGetVertexAttribPointerv(index, state, real_val);

     }
}

*/



#if 0
//------------------------------------------------------//
static int
init_states(EvasGlueContext ctx)
{
   int i;

   if (!ctx)
     {
        ERR("Context NULL\n");
        return 0;
     }

   // Set Magic and other inits
   ctx->magic      = MAGIC_GLFAST;
   //ctx->initialized = 1;

#define GET_STATE(glstate, type, evglstate, num, all) \
   print_state(#glstate, glstate, type, num, &(current_ctx->evglstate), all, 0, 0);


   fprintf(stderr, "---------------Current Actual GL Context %p States----------------\n", current_ctx);
   fprintf(stderr, "Print GL States Called from %s @ line %d\n", func_name, line_num);
   fprintf(stderr, "---TID: %d\n\n", syscall(__NR_gettid));
//-----------------------------------------------------------//


   GET_STATE(GL_ARRAY_BUFFER_BINDING, 0);
   GET_STATE(GL_ELEMENT_ARRAY_BUFFER_BINDING, 0);
   GET_STATE(GL_FRAMEBUFFER_BINDING, 0);
   GET_STATE(GL_RENDERBUFFER_BINDING, 0);

   GET_STATE(GL_BLEND, 1);
   GET_STATE(GL_CULL_FACE, 1);
   GET_STATE(GL_DEPTH_TEST, 1);
   GET_STATE(GL_DITHER, 1);

   GET_STATE(GL_POLYGON_OFFSET_FILL, 1);
   GET_STATE(GL_SAMPLE_ALPHA_TO_COVERAGE, 1);
   GET_STATE(GL_SAMPLE_COVERAGE, 1);
   GET_STATE(GL_SCISSOR_TEST, 1);
   GET_STATE(GL_STENCIL_TEST, 1);

   GET_STATE(GL_VIEWPORT, 0);
   GET_STATE(GL_CURRENT_PROGRAM, 0);

   GET_STATE(GL_COLOR_CLEAR_VALUE, 2);

   GET_STATE(GL_COLOR_WRITEMASK, 1);

   GET_STATE(GL_DEPTH_RANGE, 2);
   GET_STATE(GL_DEPTH_CLEAR_VALUE, 2);
   GET_STATE(GL_DEPTH_FUNC, 0);
   GET_STATE(GL_DEPTH_WRITEMASK, 1);
   GET_STATE(GL_CULL_FACE_MODE, 0);

   int active_tex;
   _sym_glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);

   for (i=0; i<MAX_TEXTURE_UNITS; ++i)
     {
        _sym_glActiveTexture(GL_TEXTURE0 + i);
        GET_STATE(GL_TEXTURE_BINDING_2D, 0);
     }
   for (i=0; i<MAX_TEXTURE_UNITS; ++i)
     {
        _sym_glActiveTexture(GL_TEXTURE0 + i);
        GET_STATE(GL_TEXTURE_BINDING_CUBE_MAP, 0);
     }
   _sym_glActiveTexture(active_tex);

   GET_STATE(GL_ACTIVE_TEXTURE, 0);
   GET_STATE(GL_GENERATE_MIPMAP_HINT, 0);
   //GET_STATE(GL_TEXTURE_BINDING_, 0);
   //GET_STATE(GL_TEXTURE_BINDING_CUBE_MAP, 0);

   GET_STATE(GL_BLEND_COLOR, 2);
   GET_STATE(GL_BLEND_SRC_RGB, 0);
   GET_STATE(GL_BLEND_SRC_ALPHA, 0);
   GET_STATE(GL_BLEND_DST_RGB, 0);
   GET_STATE(GL_BLEND_DST_ALPHA, 0);
   GET_STATE(GL_BLEND_EQUATION_RGB, 0);
   GET_STATE(GL_BLEND_EQUATION_ALPHA, 0);

   GET_STATE(GL_STENCIL_FUNC, 0);
   GET_STATE(GL_STENCIL_REF, 0);
   GET_STATE(GL_STENCIL_VALUE_MASK, 3);
   GET_STATE(GL_STENCIL_FAIL, 0);
   GET_STATE(GL_STENCIL_PASS_DEPTH_FAIL, 0);
   GET_STATE(GL_STENCIL_PASS_DEPTH_PASS, 0);
   GET_STATE(GL_STENCIL_WRITEMASK, 3);

   GET_STATE(GL_STENCIL_BACK_FUNC, 0);
   GET_STATE(GL_STENCIL_BACK_REF, 0);
   GET_STATE(GL_STENCIL_BACK_VALUE_MASK, 3);
   GET_STATE(GL_STENCIL_BACK_FAIL, 0);
   GET_STATE(GL_STENCIL_BACK_PASS_DEPTH_FAIL, 0);
   GET_STATE(GL_STENCIL_BACK_PASS_DEPTH_PASS, 0);
   GET_STATE(GL_STENCIL_BACK_WRITEMASK, 3);

   GET_STATE(GL_STENCIL_CLEAR_VALUE, 0);

   GET_STATE(GL_FRONT_FACE, 0);
   GET_STATE(GL_LINE_WIDTH, 2);
   GET_STATE(GL_POLYGON_OFFSET_FACTOR, 2);
   GET_STATE(GL_POLYGON_OFFSET_UNITS, 2);
   GET_STATE(GL_SAMPLE_COVERAGE_VALUE, 2);
   GET_STATE(GL_SAMPLE_COVERAGE_INVERT, 1);

   GET_STATE(GL_SCISSOR_BOX, 0);
   GET_STATE(GL_PACK_ALIGNMENT, 0);
   GET_STATE(GL_UNPACK_ALIGNMENT, 0);

   for (i=0; i<MAX_VERTEX_ATTRIBS; ++i)
     {
        //fprintf(stderr, "--Vertex Attrib Array Index: %d ----\n", i);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, 0, vertex_array[i].buf_id, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, 0, vertex_array[i].enabled, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, 0, vertex_array[i].size, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, 0, vertex_array[i].type, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, 0, vertex_array[i].normalized, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, 0, vertex_array[i].stride, 1, print_all);
        PRINT_VA_POINTER(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, 0, vertex_array[i].pointer, print_all);
     }

   for (i=0; i<MAX_VERTEX_ATTRIBS; ++i)
     {
        PRINT_VA_STATE(i, GL_CURRENT_VERTEX_ATTRIB, 2, vertex_attrib[i].value[0], 4, print_all);
     }

   fprintf(stderr, "-----------------------------------------------------------\n\n", current_ctx);
#undef GET_STATE
#undef PRINT_VA_STATE
#undef PRINT_VA_POINTER

   //-------------------//
   ctx->num_tex_units                          = MAX_TEXTURE_UNITS;
   ctx->num_vertex_attribs                     = MAX_VERTEX_ATTRIBS;

   //----------------------------------------//
   // GL States
   // Bind Functions
   // glBind {Buffer, Framebuffer, Renderbuffer, Texture}
   ctx->gl_array_buffer_binding                = 0;
   ctx->gl_element_array_buffer_binding        = 0;
   ctx->gl_framebuffer_binding                 = 0;
   ctx->gl_renderbuffer_binding                = 0;

   // Enable States
   // glEnable()
   ctx->gl_blend                               = GL_FALSE;
   ctx->gl_cull_face                           = GL_FALSE;
   ctx->gl_depth_test                          = GL_FALSE;
   ctx->gl_dither                              = GL_TRUE;
   ctx->gl_polygon_offset_fill                 = GL_FALSE;
   ctx->gl_sample_alpha_to_coverage            = GL_FALSE;
   ctx->gl_sample_coverage                     = GL_FALSE;
   ctx->gl_scissor_test                        = GL_FALSE;
   ctx->gl_stencil_test                        = GL_FALSE;

   ctx->gl_cull_face_mode                      = GL_BACK;

   // Viewport - Set it to 0
   ctx->gl_viewport[0]                         = 0;       // (0,0,w,h)
   ctx->gl_viewport[1]                         = 0;       // (0,0,w,h)
   ctx->gl_viewport[2]                         = 0;       // (0,0,w,h)
   ctx->gl_viewport[3]                         = 0;       // (0,0,w,h)

   // Program (Shaders)
   ctx->gl_current_program                     = 0;

   // Texture
   //   ctx->tex_state[MAX_TEXTURE_UNITS]
   ctx->gl_active_texture                      = GL_TEXTURE0;
   ctx->gl_generate_mipmap_hint                = GL_DONT_CARE;
   //ctx->gl_texture_binding_2d                  = 0;
   //ctx->gl_texture_binding_cube_map            = 0;


   // Clear Color
   ctx->gl_color_clear_value[0]                = 0;
   ctx->gl_color_clear_value[1]                = 0;
   ctx->gl_color_clear_value[2]                = 0;
   ctx->gl_color_clear_value[3]                = 0;
   ctx->gl_color_writemask[0]                  = GL_TRUE;
   ctx->gl_color_writemask[1]                  = GL_TRUE;
   ctx->gl_color_writemask[2]                  = GL_TRUE;
   ctx->gl_color_writemask[3]                  = GL_TRUE;

   // Depth
   ctx->gl_depth_range[0]                      = 0;
   ctx->gl_depth_range[1]                      = 1;
   ctx->gl_depth_clear_value                   = 1;
   ctx->gl_depth_func                          = GL_LESS;
   ctx->gl_depth_writemask                     = GL_TRUE;

   // Blending
   ctx->gl_blend_color[0]                      = 0;
   ctx->gl_blend_color[1]                      = 0;
   ctx->gl_blend_color[2]                      = 0;
   ctx->gl_blend_color[3]                      = 0;
   ctx->gl_blend_src_rgb                       = GL_ONE;
   ctx->gl_blend_src_alpha                     = GL_ONE;
   ctx->gl_blend_dst_rgb                       = GL_ZERO;
   ctx->gl_blend_dst_alpha                     = GL_ZERO;
   ctx->gl_blend_equation_rgb                  = GL_FUNC_ADD;
   ctx->gl_blend_equation_alpha                = GL_FUNC_ADD;

   // Stencil
   ctx->gl_stencil_fail                        = GL_KEEP;
   ctx->gl_stencil_func                        = GL_ALWAYS;
   ctx->gl_stencil_pass_depth_fail             = GL_KEEP;
   ctx->gl_stencil_pass_depth_pass             = GL_KEEP;
   ctx->gl_stencil_ref                         = 0;
   ctx->gl_stencil_value_mask                  = 0xffffffff;
   ctx->gl_stencil_writemask                   = 0xffffffff;

   ctx->gl_stencil_back_fail                   = GL_KEEP;
   ctx->gl_stencil_back_func                   = GL_ALWAYS;
   ctx->gl_stencil_back_pass_depth_fail        = GL_KEEP;
   ctx->gl_stencil_back_pass_depth_pass        = GL_KEEP;
   ctx->gl_stencil_back_ref                    = 0;
   ctx->gl_stencil_back_value_mask             = 0xffffffff;
   ctx->gl_stencil_back_writemask              = 0xffffffff;
   ctx->gl_stencil_clear_value                 = 0;

   // Misc.
   ctx->gl_front_face                          = GL_CCW;
   ctx->gl_line_width                          = 1;
   ctx->gl_polygon_offset_factor               = 0;
   ctx->gl_polygon_offset_units                = 0;

   ctx->gl_sample_coverage_value               = 1.0;
   ctx->gl_sample_coverage_invert              = GL_FALSE;
   ctx->gl_scissor_box[0]                      = 0;
   ctx->gl_scissor_box[1]                      = 0;
   ctx->gl_scissor_box[2]                      = 0;      // Supposed to be w
   ctx->gl_scissor_box[3]                      = 0;      // Supposed to be h

   ctx->gl_pack_alignment                      = 4;
   ctx->gl_unpack_alignment                    = 4;


   // Vertex Attrib Array
   for (i = 0; i < MAX_VERTEX_ATTRIBS; i++)
     {
        ctx->vertex_array[i].modified          = GL_FALSE;
        ctx->vertex_array[i].enabled           = GL_FALSE;
        ctx->vertex_array[i].size              = 4;
        ctx->vertex_array[i].type              = GL_FLOAT;
        ctx->vertex_array[i].normalized        = GL_FALSE;
        ctx->vertex_array[i].stride            = GL_FALSE;
        ctx->vertex_array[i].pointer           = NULL;

        ctx->vertex_attrib[i].modified         = GL_FALSE;
        ctx->vertex_attrib[i].value[0]         = 0;
        ctx->vertex_attrib[i].value[1]         = 0;
        ctx->vertex_attrib[i].value[2]         = 0;
        ctx->vertex_attrib[i].value[3]         = 1;
     }

   ctx->gl_current_vertex_attrib[0]            = 0;
   ctx->gl_current_vertex_attrib[1]            = 0;
   ctx->gl_current_vertex_attrib[2]            = 0;
   ctx->gl_current_vertex_attrib[3]            = 1;

   return 1;
}

#endif






// For internal fastpath
static int
init_context_states(EvasGlueContext ctx)
{
   int i;

   if (!ctx)
     {
        ERR("Context NULL\n");
        return 0;
     }

   // Set Magic and other inits
   ctx->magic      = MAGIC_GLFAST;
   //ctx->initialized = 0;

   //-------------------//
   ctx->num_tex_units                          = MAX_TEXTURE_UNITS;
   ctx->num_vertex_attribs                     = MAX_VERTEX_ATTRIBS;

   //----------------------------------------//
   // GL States
   // Bind Functions
   // glBind {Buffer, Framebuffer, Renderbuffer, Texture}
   ctx->gl_array_buffer_binding                = 0;
   ctx->gl_element_array_buffer_binding        = 0;
   ctx->gl_framebuffer_binding                 = 0;
   ctx->gl_renderbuffer_binding                = 0;

   // Enable States
   // glEnable()
   ctx->gl_blend                               = GL_FALSE;
   ctx->gl_cull_face                           = GL_FALSE;
   ctx->gl_depth_test                          = GL_FALSE;
   ctx->gl_dither                              = GL_TRUE;
   ctx->gl_polygon_offset_fill                 = GL_FALSE;
   ctx->gl_sample_alpha_to_coverage            = GL_FALSE;
   ctx->gl_sample_coverage                     = GL_FALSE;
   ctx->gl_scissor_test                        = GL_FALSE;
   ctx->gl_stencil_test                        = GL_FALSE;

   ctx->gl_cull_face_mode                      = GL_BACK;

   // Viewport - Set it to 0
   ctx->gl_viewport[0]                         = 0;       // (0,0,w,h)
   ctx->gl_viewport[1]                         = 0;       // (0,0,w,h)
   ctx->gl_viewport[2]                         = 0;       // (0,0,w,h)
   ctx->gl_viewport[3]                         = 0;       // (0,0,w,h)

   // Program (Shaders)
   ctx->gl_current_program                     = 0;

   // Texture
   //   ctx->tex_state[MAX_TEXTURE_UNITS]
   ctx->gl_active_texture                      = GL_TEXTURE0;
   ctx->gl_generate_mipmap_hint                = GL_DONT_CARE;
   //ctx->gl_texture_binding_2d                  = 0;
   //ctx->gl_texture_binding_cube_map            = 0;


   // Clear Color
   ctx->gl_color_clear_value[0]                = 0;
   ctx->gl_color_clear_value[1]                = 0;
   ctx->gl_color_clear_value[2]                = 0;
   ctx->gl_color_clear_value[3]                = 0;
   ctx->gl_color_writemask[0]                  = GL_TRUE;
   ctx->gl_color_writemask[1]                  = GL_TRUE;
   ctx->gl_color_writemask[2]                  = GL_TRUE;
   ctx->gl_color_writemask[3]                  = GL_TRUE;

   // Depth
   ctx->gl_depth_range[0]                      = 0;
   ctx->gl_depth_range[1]                      = 1;
   ctx->gl_depth_clear_value                   = 1;
   ctx->gl_depth_func                          = GL_LESS;
   ctx->gl_depth_writemask                     = GL_TRUE;

   // Blending
   ctx->gl_blend_color[0]                      = 0;
   ctx->gl_blend_color[1]                      = 0;
   ctx->gl_blend_color[2]                      = 0;
   ctx->gl_blend_color[3]                      = 0;
   ctx->gl_blend_src_rgb                       = GL_ONE;
   ctx->gl_blend_src_alpha                     = GL_ONE;
   ctx->gl_blend_dst_rgb                       = GL_ZERO;
   ctx->gl_blend_dst_alpha                     = GL_ZERO;
   ctx->gl_blend_equation_rgb                  = GL_FUNC_ADD;
   ctx->gl_blend_equation_alpha                = GL_FUNC_ADD;

   // Stencil
   ctx->gl_stencil_fail                        = GL_KEEP;
   ctx->gl_stencil_func                        = GL_ALWAYS;
   ctx->gl_stencil_pass_depth_fail             = GL_KEEP;
   ctx->gl_stencil_pass_depth_pass             = GL_KEEP;
   ctx->gl_stencil_ref                         = 0;
   ctx->gl_stencil_value_mask                  = 0xffffffff;
   ctx->gl_stencil_writemask                   = 0xffffffff;

   ctx->gl_stencil_back_fail                   = GL_KEEP;
   ctx->gl_stencil_back_func                   = GL_ALWAYS;
   ctx->gl_stencil_back_pass_depth_fail        = GL_KEEP;
   ctx->gl_stencil_back_pass_depth_pass        = GL_KEEP;
   ctx->gl_stencil_back_ref                    = 0;
   ctx->gl_stencil_back_value_mask             = 0xffffffff;
   ctx->gl_stencil_back_writemask              = 0xffffffff;
   ctx->gl_stencil_clear_value                 = 0;

   // Misc.
   ctx->gl_front_face                          = GL_CCW;
   ctx->gl_line_width                          = 1;
   ctx->gl_polygon_offset_factor               = 0;
   ctx->gl_polygon_offset_units                = 0;

   ctx->gl_sample_coverage_value               = 1.0;
   ctx->gl_sample_coverage_invert              = GL_FALSE;
   ctx->gl_scissor_box[0]                      = 0;
   ctx->gl_scissor_box[1]                      = 0;
   ctx->gl_scissor_box[2]                      = 0;      // Supposed to be w
   ctx->gl_scissor_box[3]                      = 0;      // Supposed to be h

   ctx->gl_pack_alignment                      = 4;
   ctx->gl_unpack_alignment                    = 4;


   // Vertex Attrib Array
   for (i = 0; i < MAX_VERTEX_ATTRIBS; i++)
     {
        ctx->vertex_array[i].modified          = GL_FALSE;
        ctx->vertex_array[i].enabled           = GL_FALSE;
        ctx->vertex_array[i].size              = 4;
        ctx->vertex_array[i].type              = GL_FLOAT;
        ctx->vertex_array[i].normalized        = GL_FALSE;
        ctx->vertex_array[i].stride            = GL_FALSE;
        ctx->vertex_array[i].pointer           = NULL;

        ctx->vertex_attrib[i].modified         = GL_FALSE;
        ctx->vertex_attrib[i].value[0]         = 0;
        ctx->vertex_attrib[i].value[1]         = 0;
        ctx->vertex_attrib[i].value[2]         = 0;
        ctx->vertex_attrib[i].value[3]         = 1;
     }

   ctx->gl_current_vertex_attrib[0]            = 0;
   ctx->gl_current_vertex_attrib[1]            = 0;
   ctx->gl_current_vertex_attrib[2]            = 0;
   ctx->gl_current_vertex_attrib[3]            = 1;

   return 1;
}

//-------------------------------//
// Get Type
// 0 = glGet
// 1 = glGetVertexAttrib
// 2 = glGetVertexAttribPointer
//-------------------------------//

typedef enum _State_Type
{
   STATE_INT,
   STATE_HEX,
   STATE_BOOLEAN,
   STATE_FLOAT,
   VA_STATE_INT,
   VA_STATE_FLOAT,
   VA_STATE_POINTER,
} State_Type;

static void
print_state_bool(const char * name, int index, GLboolean *val1,
                 GLboolean *val2, int num, State_Type state_type, int print_all)
{
   int i;

   for (i=0; i<num; ++i)
      if ( (print_all) || (val1[i] != val2[i]) )
         switch (state_type)
           {
            case STATE_BOOLEAN:
               fprintf(stderr, "\t %-25.25s    : %10d    %10d\n",
                       name, val1[i], val2[i]);
               break;
            default:
               fprintf(stderr, "\t Error: Invalid State Print Bool Type.\n");
           }


}

static void
print_state_int(const char * name, int index, GLint *val1,
            GLint *val2, int num, State_Type state_type, int print_all)
{
   int i;

   for (i=0; i<num; ++i)
      if ( (print_all) || (val1[i] != val2[i]) )
         switch (state_type)
           {
            case STATE_INT:
               fprintf(stderr, "\t %-25.25s    : %10d    %10d\n",
                       name, val1[i], val2[i]);
               break;
            case STATE_HEX:
               fprintf(stderr, "\t %-25.25s    : %10x    %10x\n",
                       name, val1[i], val2[i]);
               break;
            case VA_STATE_INT:
               if (num > 1)
                  fprintf(stderr, "\t %-25.25s[%-2.2d||%-1.1d]: %10d    %10d\n",
                          name, index, i, val1[i], val2[i]);
               else
                  fprintf(stderr, "\t %-25.25s[%-2.2d]   : %10d    %10d\n",
                          name, index, val1[i], val2[i]);
               break;
            case VA_STATE_POINTER:
               fprintf(stderr, "\t %-25.25s[%-2.2d]   : %10d    %10d\n",
                       name, index, val1[i], val2[i]);
               break;
            default:
               fprintf(stderr, "\t Error: Invalid State Print Type.\n");
           }
}

static void
print_state_float(const char * name, int index, GLfloat *val1,
                  GLfloat *val2, int num, State_Type state_type, int print_all)
{
   int i;

   for (i=0; i<num; ++i)
      if ( (print_all) || (val1[i] != val2[i]) )
         switch (state_type)
           {
            case STATE_FLOAT:
               fprintf(stderr, "\t %-25.25s    : %10.1f    %10.1f\n",
                       name, val1[i], val2[i]);
               break;
            case VA_STATE_FLOAT:
               if (num > 1)
                  fprintf(stderr, "\t %-25.25s[%-2.2d||%-1.1d]: %10.1f    %10.1f\n",
                          name, index, i, val1[i], val2[i]);
               else
                  fprintf(stderr, "\t %-25.25s[%-2.2d]   : %10.1f    %10.1f\n",
                          name, index, val1[i], val2[i]);

               break;
            default:
               fprintf(stderr, "\t Error: Invalid State Print Float Type.\n");
           }
}

/*
static void
get_state(GLenum state, State_Type type, int index, void *value)
{
   switch (type)
     {
      case STATE_INT | STATE_HEX:
        _sym_glGetIntegerv(state, (GLint*)value);
        break;

      case STATE_BOOLEAN:
        _sym_glGetBooleanv(state, (GLboolean*)value);
        break;

      case STATE_FLOAT:
        _sym_glGetFloatv(state, (GLfloat*)value);
        break;

      case VA_STATE_FLOAT:
        _sym_glGetVertexAttribiv(index, state, (GLfloat*)value);
        break;

      case VA_STATE_POINTER:
        _sym_glGetVertexAttribPointerv(index, state, (GLfloat*)value);
        break;

      case default:
        fprintf(stderr, "\t Error: Invalid State Print Type.\n");
     }
}
*/

static void
print_get_state(const char *state_name, GLenum state, State_Type type, int index, int num, void *value, int all)
{
   switch (type)
     {
      case STATE_INT:
      case STATE_HEX:
           {
              GLint real_val[4] = {-1, -1, -1, -1};
              GLint *curr_val = (GLint*)value;
              _sym_glGetIntegerv(state, real_val);
              print_state_int(state_name, index, real_val, curr_val, num, type, all);
           }
         break;

      case STATE_BOOLEAN:
           {
              GLboolean real_val[4] = {3, 3, 3, 3};
              GLboolean *curr_val = (GLboolean*)value;
              _sym_glGetBooleanv(state, real_val);
              print_state_bool(state_name, index, real_val, curr_val, num, type, all);
           }
         break;

      case STATE_FLOAT:
           {
              GLfloat real_val[4] = {-1, -1, -1, -1};
              GLfloat *curr_val = (GLfloat*)value;
              _sym_glGetFloatv(state, real_val);
              print_state_float(state_name, index, real_val, curr_val, num, type, all);
           }
         break;

      case VA_STATE_INT:
           {
              GLint real_val[4] = {-1, -1, -1, -1};
              GLint *curr_val = (GLint*)value;
              _sym_glGetVertexAttribiv(index, state, real_val);
              print_state_int(state_name, index, real_val, curr_val, num, type, all);
           }
         break;

      case VA_STATE_FLOAT:
           {
              GLfloat real_val[4] = {-1, -1, -1, -1};
              GLfloat *curr_val = (GLfloat*)value;
              _sym_glGetVertexAttribfv(index, state, real_val);
              print_state_float(state_name, index, real_val, curr_val, num, type, all);
           }
         break;

      case VA_STATE_POINTER:
           {
              GLint real_val[4] = {-1, -1, -1, -1};
              GLint *curr_val = (GLint*)value;
              _sym_glGetVertexAttribPointerv(index, state, real_val);
              print_state_int(state_name, index, (GLint*)real_val, (GLint*)curr_val, num, type, all);
           }
         break;

      default:
         fprintf(stderr, "\t Error: Invalid State Print Type.\n");
     }
}

/*
static void
print_state(const char *state_name, GLenum state, State_Type print_type, int num, void *value, int all, int get_type, int index)
{
   int i;

   if (get_type == 0)
     {
        GLint real_val[4] = {-1, -1, -1, -1};
        GLint *curr_val = (GLint*)value;
        _sym_glGetIntegerv(state, real_val);
        print_state(state_name, index, real_val, curr_val, num, print_type, all);

        // Boolean Type
        else if (print_type == 1)
          {
             GLboolean real_val[4] = {3, 3, 3, 3};
             GLboolean *curr_val = (GLboolean*)value;
             _sym_glGetBooleanv(state, real_val);
             for (i=0; i<num; ++i)
                if ( (all) || (real_val[i] != curr_val[i]) )
                   fprintf(stderr, "\t %-25.25s    : %10d    %10d\n",
                           state_name, real_val[i], curr_val[i]);
          }
        // Float Type
        else if (print_type == 2)
          {
             GLfloat real_val[4] = {-1, -1, -1, -1};
             GLfloat *curr_val = (GLfloat*)value;
             _sym_glGetFloatv(state, real_val);
             for (i=0; i<num; ++i)
                if ( (all) || (real_val[i] != curr_val[i]) )
                   fprintf(stderr, "\t %-25.25s    : %10.1f    %10.1f\n",
                           state_name, real_val[i], curr_val[i]);
          }
        else
           fprintf(stderr, "Error Priting State: %s...\n", state_name);
     }
   else if (get_type == 1)
     {
        // GetVertexAttrib
        if (print_type == 0)
          {
             GLint real_val[4] = {-1, -1, -1, -1};
             GLint *curr_val = (GLint*)value;
             _sym_glGetVertexAttribiv(index, state, real_val);

             for (i=0; i<num; ++i)
                if ( (all) || (real_val[i] != curr_val[i]) )
                   if (num > 1)
                      fprintf(stderr, "\t %-25.25s[%-2.2d||%-1.1d]: %10d    %10d\n",
                              state_name, index, i, real_val[i], curr_val[i]);
                   else
                      fprintf(stderr, "\t %-25.25s[%-2.2d]   : %10d    %10d\n",
                              state_name, index, real_val[i], curr_val[i]);
          }
        else if (print_type == 2)
          {
             GLfloat real_val[4] = {-1, -1, -1, -1};
             GLfloat *curr_val = (GLfloat*)value;
             _sym_glGetVertexAttribfv(index, state, real_val);
             for (i=0; i<num; ++i)
                if ( (all) || (real_val[i] != curr_val[i]) )
                   if (num > 1)
                      fprintf(stderr, "\t %-25.25s[%-2.2d||%-1.1d]: %10.1f    %10.1f\n",
                              state_name, index, i, real_val[i], curr_val[i]);
                   else
                      fprintf(stderr, "\t %-25.25s[%-2.2d]   : %10.1f    %10.1f\n",
                              state_name, index, real_val[i], curr_val[i]);
          }
     }
   else if (get_type == 2)
     {
        GLint real_val[4] = {-1, -1, -1, -1};
        GLint *curr_val = (GLint*)value;
        _sym_glGetVertexAttribPointerv(index, state, real_val);

        for (i=0; i<num; ++i)
           if ( (all) || (real_val[i] != curr_val[i]) )
              fprintf(stderr, "\t %-25.25s[%-2.2d]   : %10d    %10d\n",
                      state_name, index, real_val[i], curr_val[i]);
     }

}
*/


void
print_gl_states(const char* func_name, int line_num )
{
   int i;

   if (!log_opt) return;

   if (!current_ctx)
     {
        ERR("Error Printing. No Current Context:");
        return;
     }


#define PRINT_STATE(glstate, type, evglstate, num, all) \
   print_get_state(#glstate, glstate, type, 0, num, (void*)&(current_ctx->evglstate), all);

#define PRINT_VA_STATE(index, glstate, type, evglstate, num, all) \
   print_get_state(#glstate, glstate, type, index, num, (void*)&(current_ctx->evglstate), all);


   fprintf(stderr, "---------------Current Actual GL Context %p States----------------\n", current_ctx);
   fprintf(stderr, "Print GL States Called from %s @ line %d\n", func_name, line_num);
   fprintf(stderr, "---TID: %d\n\n", syscall(__NR_gettid));
//-----------------------------------------------------------//


   int print_all = 0;

   if (log_opt==2) print_all = 1;

   PRINT_STATE(GL_ARRAY_BUFFER_BINDING, STATE_INT, gl_array_buffer_binding, 1, print_all);
   PRINT_STATE(GL_ELEMENT_ARRAY_BUFFER_BINDING, STATE_INT, gl_element_array_buffer_binding, 1, print_all);
   PRINT_STATE(GL_FRAMEBUFFER_BINDING, STATE_INT, gl_framebuffer_binding, 1, print_all);
   PRINT_STATE(GL_RENDERBUFFER_BINDING, STATE_INT, gl_renderbuffer_binding, 1, print_all);

   PRINT_STATE(GL_BLEND, STATE_BOOLEAN, gl_blend, 1, print_all);
   PRINT_STATE(GL_CULL_FACE, STATE_BOOLEAN, gl_cull_face, 1, print_all);
   PRINT_STATE(GL_DEPTH_TEST, STATE_BOOLEAN, gl_depth_test, 1, print_all);
   PRINT_STATE(GL_DITHER, STATE_BOOLEAN, gl_dither, 1, print_all);

   PRINT_STATE(GL_POLYGON_OFFSET_FILL, STATE_BOOLEAN, gl_polygon_offset_fill, 1, print_all);
   PRINT_STATE(GL_SAMPLE_ALPHA_TO_COVERAGE, STATE_BOOLEAN, gl_sample_alpha_to_coverage, 1, print_all);
   PRINT_STATE(GL_SAMPLE_COVERAGE, STATE_BOOLEAN, gl_sample_coverage, 1, print_all);
   PRINT_STATE(GL_SCISSOR_TEST, STATE_BOOLEAN, gl_scissor_test, 1, print_all);
   PRINT_STATE(GL_STENCIL_TEST, STATE_BOOLEAN, gl_stencil_test, 1, print_all);

   PRINT_STATE(GL_VIEWPORT, STATE_INT, gl_viewport[0], 4, print_all);
   PRINT_STATE(GL_CURRENT_PROGRAM, STATE_INT, gl_current_program, 1, print_all);

   PRINT_STATE(GL_COLOR_CLEAR_VALUE, STATE_FLOAT, gl_color_clear_value[0], 4, print_all);

   PRINT_STATE(GL_COLOR_WRITEMASK, STATE_BOOLEAN, gl_color_writemask[0], 4, print_all);

   PRINT_STATE(GL_DEPTH_RANGE, STATE_FLOAT, gl_depth_range[0], 2, print_all);
   PRINT_STATE(GL_DEPTH_CLEAR_VALUE, STATE_FLOAT, gl_depth_clear_value, 1, print_all);
   PRINT_STATE(GL_DEPTH_FUNC, STATE_INT, gl_depth_func, 1, print_all);
   PRINT_STATE(GL_DEPTH_WRITEMASK, STATE_BOOLEAN, gl_depth_writemask, 1, print_all);
   PRINT_STATE(GL_CULL_FACE_MODE, STATE_INT, gl_cull_face_mode, 1, print_all);

   int active_tex;
   _sym_glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);

   for (i=0; i<MAX_TEXTURE_UNITS; ++i)
     {
        _sym_glActiveTexture(GL_TEXTURE0 + i);
        PRINT_STATE(GL_TEXTURE_BINDING_2D, STATE_INT, tex_2d_state[i], 1, print_all);
     }
   for (i=0; i<MAX_TEXTURE_UNITS; ++i)
     {
        _sym_glActiveTexture(GL_TEXTURE0 + i);
        PRINT_STATE(GL_TEXTURE_BINDING_CUBE_MAP, STATE_INT, tex_cube_map_state[i], 1, print_all);
     }
   _sym_glActiveTexture(active_tex);

   PRINT_STATE(GL_ACTIVE_TEXTURE, STATE_INT, gl_active_texture, 1, print_all);
   PRINT_STATE(GL_GENERATE_MIPMAP_HINT, STATE_INT, gl_generate_mipmap_hint, 1, print_all);
   //PRINT_STATE(GL_TEXTURE_BINDING_, 0, gl_texture_binding_2D, 1);
   //PRINT_STATE(GL_TEXTURE_BINDING_CUBE_MAP, 0, gl_texture_binding_cube_map, 1);

   PRINT_STATE(GL_BLEND_COLOR, STATE_FLOAT, gl_blend_color[0], 4, print_all);
   PRINT_STATE(GL_BLEND_SRC_RGB, STATE_INT, gl_blend_src_rgb, 1, print_all);
   PRINT_STATE(GL_BLEND_SRC_ALPHA, STATE_INT, gl_blend_src_alpha, 1, print_all);
   PRINT_STATE(GL_BLEND_DST_RGB, STATE_INT, gl_blend_dst_rgb, 1, print_all);
   PRINT_STATE(GL_BLEND_DST_ALPHA, STATE_INT, gl_blend_dst_alpha, 1, print_all);
   PRINT_STATE(GL_BLEND_EQUATION_RGB, STATE_INT, gl_blend_equation_rgb, 1, print_all);
   PRINT_STATE(GL_BLEND_EQUATION_ALPHA, STATE_INT, gl_blend_equation_alpha, 1, print_all);

   PRINT_STATE(GL_STENCIL_FUNC, STATE_INT, gl_stencil_func, 1, print_all);
   PRINT_STATE(GL_STENCIL_REF, STATE_INT, gl_stencil_ref, 1, print_all);
   PRINT_STATE(GL_STENCIL_VALUE_MASK, STATE_HEX, gl_stencil_value_mask, 1, print_all);
   PRINT_STATE(GL_STENCIL_FAIL, STATE_INT, gl_stencil_fail, 1, print_all);
   PRINT_STATE(GL_STENCIL_PASS_DEPTH_FAIL, STATE_INT, gl_stencil_pass_depth_fail, 1, print_all);
   PRINT_STATE(GL_STENCIL_PASS_DEPTH_PASS, STATE_INT, gl_stencil_pass_depth_pass, 1, print_all);
   PRINT_STATE(GL_STENCIL_WRITEMASK, STATE_HEX, gl_stencil_writemask, 1, print_all);

   PRINT_STATE(GL_STENCIL_BACK_FUNC, STATE_INT, gl_stencil_back_func, 1, print_all);
   PRINT_STATE(GL_STENCIL_BACK_REF, STATE_INT, gl_stencil_back_ref, 1, print_all);
   PRINT_STATE(GL_STENCIL_BACK_VALUE_MASK, STATE_HEX, gl_stencil_back_value_mask, 1, print_all);
   PRINT_STATE(GL_STENCIL_BACK_FAIL, STATE_INT, gl_stencil_back_fail, 1, print_all);
   PRINT_STATE(GL_STENCIL_BACK_PASS_DEPTH_FAIL, STATE_INT, gl_stencil_back_pass_depth_fail, 1, print_all);
   PRINT_STATE(GL_STENCIL_BACK_PASS_DEPTH_PASS, STATE_INT, gl_stencil_back_pass_depth_pass, 1, print_all);
   PRINT_STATE(GL_STENCIL_BACK_WRITEMASK, STATE_HEX, gl_stencil_back_writemask, 1, print_all);

   PRINT_STATE(GL_STENCIL_CLEAR_VALUE, STATE_INT, gl_stencil_clear_value, 1, print_all);

   PRINT_STATE(GL_FRONT_FACE, STATE_INT, gl_front_face, 1, print_all);
   PRINT_STATE(GL_LINE_WIDTH, STATE_FLOAT, gl_line_width, 1, print_all);
   PRINT_STATE(GL_POLYGON_OFFSET_FACTOR, STATE_FLOAT, gl_polygon_offset_factor, 1, print_all);
   PRINT_STATE(GL_POLYGON_OFFSET_UNITS, STATE_FLOAT, gl_polygon_offset_units, 1, print_all);
   PRINT_STATE(GL_SAMPLE_COVERAGE_VALUE, STATE_FLOAT, gl_sample_coverage_value, 1, print_all);
   PRINT_STATE(GL_SAMPLE_COVERAGE_INVERT, STATE_BOOLEAN, gl_sample_coverage_invert, 1, print_all);

   PRINT_STATE(GL_SCISSOR_BOX, STATE_INT, gl_scissor_box[0], 4, print_all);
   PRINT_STATE(GL_PACK_ALIGNMENT, STATE_INT, gl_pack_alignment, 1, print_all);
   PRINT_STATE(GL_UNPACK_ALIGNMENT, STATE_INT, gl_unpack_alignment, 1, print_all);

   for (i=0; i<MAX_VERTEX_ATTRIBS; ++i)
     {
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, VA_STATE_INT, vertex_array[i].buf_id, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, VA_STATE_INT, vertex_array[i].enabled, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, VA_STATE_INT, vertex_array[i].size, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, VA_STATE_INT, vertex_array[i].type, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, VA_STATE_INT, vertex_array[i].normalized, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, VA_STATE_INT, vertex_array[i].stride, 1, print_all);
        PRINT_VA_STATE(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, VA_STATE_POINTER, vertex_array[i].pointer, 1, print_all);
     }

   for (i=0; i<MAX_VERTEX_ATTRIBS; ++i)
     {
        PRINT_VA_STATE(i, GL_CURRENT_VERTEX_ATTRIB, VA_STATE_FLOAT, vertex_attrib[i].value[0], 4, print_all);
     }

   fprintf(stderr, "-----------------------------------------------------------\n\n", current_ctx);
#undef PRINT_STATE
   /*
#undef PRINT_VA_STATE
#undef PRINT_VA_POINTER
*/
}


static void
make_context_current(EvasGlueContext oldctx, EvasGlueContext newctx)
{
   unsigned char flag = 0;
   int i = 0;

   // Return if they're the same
   if (oldctx==newctx) return;

#ifdef EVAS_GL_DEBUG
   #define STATE_COMPARE(state) \
      if (1)
   #define STATES_COMPARE(state_ptr, bytes) \
      if (1)
#else
   #define STATE_COMPARE(state) \
      if ((oldctx->state) != (newctx->state))
   #define STATES_COMPARE(state_ptr, bytes) \
      if ((memcmp((oldctx->state_ptr), (newctx->state_ptr), (bytes))) != 0)
#endif

   _sym_glFlush();

   //------------------//
   // _bind_flag
   flag = oldctx->_bind_flag | newctx->_bind_flag;
   if (COMPARE_OPT_SKIP flag)
     {
        STATE_COMPARE(gl_array_buffer_binding)
          {
             _sym_glBindBuffer(GL_ARRAY_BUFFER, newctx->gl_array_buffer_binding);
          }
        STATE_COMPARE(gl_element_array_buffer_binding)
          {
             _sym_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newctx->gl_element_array_buffer_binding);
          }
        STATE_COMPARE(gl_framebuffer_binding)
          {
             _sym_glBindFramebuffer(GL_FRAMEBUFFER, newctx->gl_framebuffer_binding);
          }
        STATE_COMPARE(gl_renderbuffer_binding)
          {
             _sym_glBindRenderbuffer(GL_RENDERBUFFER, newctx->gl_renderbuffer_binding);
          }
     }

   //------------------//
   // Enable States
   // _enable_flag1
   flag = oldctx->_enable_flag1 | newctx->_enable_flag1;
   if (COMPARE_OPT_SKIP flag)
     {
        STATE_COMPARE(gl_blend)
          {
             if (newctx->gl_blend)
                _sym_glEnable(GL_BLEND);
             else
                _sym_glDisable(GL_BLEND);
          }
        STATE_COMPARE(gl_cull_face)
          {
             if (newctx->gl_cull_face)
                _sym_glEnable(GL_CULL_FACE);
             else
                _sym_glDisable(GL_CULL_FACE);
          }
        STATE_COMPARE(gl_depth_test)
          {
             if (newctx->gl_depth_test)
                _sym_glEnable(GL_DEPTH_TEST);
             else
                _sym_glDisable(GL_DEPTH_TEST);
          }
        STATE_COMPARE(gl_dither)
          {
             if (newctx->gl_dither)
                _sym_glEnable(GL_DITHER);
             else
                _sym_glDisable(GL_DITHER);
          }
     }

   // _enable_flag2
   flag = oldctx->_enable_flag2 | newctx->_enable_flag2;
   if (COMPARE_OPT_SKIP flag)
     {
        STATE_COMPARE(gl_polygon_offset_fill)
          {
             if (newctx->gl_polygon_offset_fill)
                _sym_glEnable(GL_POLYGON_OFFSET_FILL);
             else
                _sym_glDisable(GL_POLYGON_OFFSET_FILL);
          }
        STATE_COMPARE(gl_sample_alpha_to_coverage)
          {
             if (newctx->gl_sample_alpha_to_coverage)
                _sym_glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
             else
                _sym_glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
          }
        STATE_COMPARE(gl_sample_coverage)
          {
             if (newctx->gl_sample_coverage)
                _sym_glEnable(GL_SAMPLE_COVERAGE);
             else
                _sym_glDisable(GL_SAMPLE_COVERAGE);
          }
        STATE_COMPARE(gl_scissor_test)
          {
             if (newctx->gl_scissor_test)
                _sym_glEnable(GL_SCISSOR_TEST);
             else
                _sym_glDisable(GL_SCISSOR_TEST);
          }
        STATE_COMPARE(gl_stencil_test)
          {
             if (newctx->gl_stencil_test)
                _sym_glEnable(GL_STENCIL_TEST);
             else
                _sym_glDisable(GL_STENCIL_TEST);
          }
     }

   //------------------//
   // _clear_flag1
   flag = oldctx->_clear_flag1 | newctx->_clear_flag1;
   if (COMPARE_OPT_SKIP flag)
     {
        // Viewport.
        STATES_COMPARE(gl_viewport, 4*sizeof(GLint))
          {
             _sym_glViewport(newctx->gl_viewport[0],
                             newctx->gl_viewport[1],
                             newctx->gl_viewport[2],
                             newctx->gl_viewport[3]);
          }
        STATE_COMPARE(gl_current_program)
          {
             _sym_glUseProgram(newctx->gl_current_program);
          }
        STATES_COMPARE(gl_color_clear_value, 4*sizeof(GLclampf))
          {
             _sym_glClearColor(newctx->gl_color_clear_value[0],
                               newctx->gl_color_clear_value[1],
                               newctx->gl_color_clear_value[2],
                               newctx->gl_color_clear_value[3]);
          }
     }


   // _clear_flag2
   flag = oldctx->_clear_flag2 | newctx->_clear_flag2;
   if (COMPARE_OPT_SKIP flag)
     {
        STATES_COMPARE(gl_color_writemask, 4*sizeof(GLboolean))
          {
             _sym_glColorMask(newctx->gl_color_writemask[0],
                              newctx->gl_color_writemask[1],
                              newctx->gl_color_writemask[2],
                              newctx->gl_color_writemask[3]);
          }
        STATES_COMPARE(gl_depth_range, 2*sizeof(GLclampf))
          {
             _sym_glDepthRangef(newctx->gl_depth_range[0],
                                newctx->gl_depth_range[1]);
          }
        STATE_COMPARE(gl_depth_clear_value)
          {
             _sym_glClearDepthf(newctx->gl_depth_clear_value);
          }
        STATE_COMPARE(gl_depth_func)
          {
             _sym_glDepthFunc(newctx->gl_depth_func);
          }
        STATE_COMPARE(gl_depth_writemask)
          {
             _sym_glDepthMask(newctx->gl_depth_writemask);
          }
        STATE_COMPARE(gl_cull_face_mode)
          {
             _sym_glCullFace(newctx->gl_cull_face_mode);
          }

     }
   //------------------//
   // Texture here...
   flag = oldctx->_tex_flag1 | newctx->_tex_flag1;
   if (COMPARE_OPT_SKIP flag)
     {

        for (i = 0; i < oldctx->num_tex_units; i++)
          {
             STATE_COMPARE(tex_2d_state[i])
               {
                  _sym_glActiveTexture(GL_TEXTURE0+i);
                  _sym_glBindTexture(GL_TEXTURE_2D, newctx->tex_2d_state[i]);
               }

             STATE_COMPARE(tex_cube_map_state[i])
               {
                  _sym_glActiveTexture(GL_TEXTURE0+i);
                  _sym_glBindTexture(GL_TEXTURE_CUBE_MAP, newctx->tex_cube_map_state[i]);
               }
          }

        // Restore active texture
        _sym_glActiveTexture(newctx->gl_active_texture);

        STATE_COMPARE(gl_generate_mipmap_hint)
          {
             _sym_glHint(GL_GENERATE_MIPMAP_HINT, newctx->gl_generate_mipmap_hint);
          }
     }


   //------------------//
   flag = oldctx->_blend_flag | newctx->_blend_flag;
   if (COMPARE_OPT_SKIP flag)
     {
        STATES_COMPARE(gl_blend_color, 4*sizeof(GLclampf))
          {
             _sym_glBlendColor(newctx->gl_blend_color[0],
                               newctx->gl_blend_color[1],
                               newctx->gl_blend_color[2],
                               newctx->gl_blend_color[3]);
          }
        if ( COMPARE_OPT_SKIP
            (oldctx->gl_blend_src_rgb != newctx->gl_blend_src_rgb) ||
            (oldctx->gl_blend_dst_rgb != newctx->gl_blend_dst_rgb) ||
            (oldctx->gl_blend_src_alpha != newctx->gl_blend_src_alpha) ||
            (oldctx->gl_blend_dst_alpha != newctx->gl_blend_dst_alpha))
          {
             _sym_glBlendFuncSeparate(newctx->gl_blend_src_rgb,
                                      newctx->gl_blend_dst_rgb,
                                      newctx->gl_blend_src_alpha,
                                      newctx->gl_blend_dst_alpha);
          }
        if ( COMPARE_OPT_SKIP
            (oldctx->gl_blend_equation_rgb != newctx->gl_blend_equation_rgb) ||
            (oldctx->gl_blend_equation_alpha != newctx->gl_blend_equation_alpha))
          {
             _sym_glBlendEquationSeparate(newctx->gl_blend_equation_rgb, newctx->gl_blend_equation_alpha);
          }

     }

   //------------------//
   // _stencil_flag1
   flag = oldctx->_stencil_flag1 | newctx->_stencil_flag1;
   if (COMPARE_OPT_SKIP flag)
     {
        if ( COMPARE_OPT_SKIP
            (oldctx->gl_stencil_func != newctx->gl_stencil_func) ||
            (oldctx->gl_stencil_ref  != newctx->gl_stencil_ref)  ||
            (oldctx->gl_stencil_value_mask != newctx->gl_stencil_value_mask))
          {
             _sym_glStencilFuncSeparate(GL_FRONT,
                                        newctx->gl_stencil_func,
                                        newctx->gl_stencil_ref,
                                        newctx->gl_stencil_value_mask);
          }
        if ( COMPARE_OPT_SKIP
            (oldctx->gl_stencil_fail != newctx->gl_stencil_fail) ||
            (oldctx->gl_stencil_pass_depth_fail != newctx->gl_stencil_pass_depth_fail) ||
            (oldctx->gl_stencil_pass_depth_pass != newctx->gl_stencil_pass_depth_pass))
          {
             _sym_glStencilOpSeparate(GL_FRONT,
                                      newctx->gl_stencil_fail,
                                      newctx->gl_stencil_pass_depth_fail,
                                      newctx->gl_stencil_pass_depth_pass);
          }
        STATE_COMPARE(gl_stencil_writemask)
          {
             _sym_glStencilMaskSeparate(GL_FRONT, newctx->gl_stencil_writemask);
          }
     }


   // _stencil_flag1
   flag = oldctx->_stencil_flag2 | newctx->_stencil_flag2;
   if (COMPARE_OPT_SKIP flag)
     {
        if ( COMPARE_OPT_SKIP
            (oldctx->gl_stencil_back_func != newctx->gl_stencil_back_func) ||
            (oldctx->gl_stencil_back_ref  != newctx->gl_stencil_back_ref)  ||
            (oldctx->gl_stencil_back_value_mask != newctx->gl_stencil_back_value_mask))
          {
             _sym_glStencilFuncSeparate(GL_BACK,
                                        newctx->gl_stencil_back_func,
                                        newctx->gl_stencil_back_ref,
                                        newctx->gl_stencil_back_value_mask);
          }
        if ( COMPARE_OPT_SKIP
            (oldctx->gl_stencil_back_fail != newctx->gl_stencil_back_fail) ||
            (oldctx->gl_stencil_back_pass_depth_fail != newctx->gl_stencil_back_pass_depth_fail) ||
            (oldctx->gl_stencil_back_pass_depth_pass != newctx->gl_stencil_back_pass_depth_pass))
          {
             _sym_glStencilOpSeparate(GL_BACK,
                                      newctx->gl_stencil_back_fail,
                                      newctx->gl_stencil_back_pass_depth_fail,
                                      newctx->gl_stencil_back_pass_depth_pass);
          }
        STATE_COMPARE(gl_stencil_back_writemask)
          {
             _sym_glStencilMaskSeparate(GL_BACK, newctx->gl_stencil_back_writemask);
          }
        STATE_COMPARE(gl_stencil_clear_value)
          {
             _sym_glClearStencil(newctx->gl_stencil_clear_value);
          }
     }

   //------------------//
   // _misc_flag1
   flag = oldctx->_misc_flag1 | newctx->_misc_flag1;
   if (COMPARE_OPT_SKIP flag)
     {
        STATE_COMPARE(gl_front_face)
          {
             _sym_glFrontFace(newctx->gl_front_face);
          }
        STATE_COMPARE(gl_line_width)
          {
             _sym_glLineWidth(newctx->gl_line_width);
          }
        if ( COMPARE_OPT_SKIP
            (oldctx->gl_polygon_offset_factor != newctx->gl_polygon_offset_factor) ||
            (oldctx->gl_polygon_offset_units  != newctx->gl_polygon_offset_units))
          {
             _sym_glPolygonOffset(newctx->gl_polygon_offset_factor,
                                  newctx->gl_polygon_offset_units);
          }
        if ( COMPARE_OPT_SKIP
            (oldctx->gl_sample_coverage_value  != newctx->gl_sample_coverage_value) ||
            (oldctx->gl_sample_coverage_invert != newctx->gl_sample_coverage_invert))
          {
             _sym_glSampleCoverage(newctx->gl_sample_coverage_value,
                                   newctx->gl_sample_coverage_invert);
          }
     }

   // _misc_flag2
   flag = oldctx->_misc_flag2 | newctx->_misc_flag2;
   if (COMPARE_OPT_SKIP flag)
     {
        STATES_COMPARE(gl_scissor_box, 4*sizeof(GLint))
          {
             _sym_glScissor(newctx->gl_scissor_box[0],
                            newctx->gl_scissor_box[1],
                            newctx->gl_scissor_box[2],
                            newctx->gl_scissor_box[3]);
          }
        STATE_COMPARE(gl_pack_alignment)
          {
             _sym_glPixelStorei(GL_PACK_ALIGNMENT, newctx->gl_pack_alignment);
          }
        STATE_COMPARE(gl_unpack_alignment)
          {
             _sym_glPixelStorei(GL_UNPACK_ALIGNMENT, newctx->gl_unpack_alignment);
          }
     }

   // _varray_flag
   flag = oldctx->_vattrib_flag | newctx->_vattrib_flag;
   if (COMPARE_OPT_SKIP flag)
     {
        for (i = 0; i < oldctx->num_vertex_attribs; i++)
          {
             if ( COMPARE_OPT_SKIP
                 newctx->vertex_array[i].buf_id != oldctx->vertex_array[i].buf_id)
               {
                  _sym_glBindBuffer(GL_ARRAY_BUFFER, newctx->vertex_array[i].buf_id);
               }
             else _sym_glBindBuffer(GL_ARRAY_BUFFER, 0);

             _sym_glVertexAttribPointer(i,
                                        newctx->vertex_array[i].size,
                                        newctx->vertex_array[i].type,
                                        newctx->vertex_array[i].normalized,
                                        newctx->vertex_array[i].stride,
                                        newctx->vertex_array[i].pointer);

             STATES_COMPARE(vertex_attrib[i].value, 4*sizeof(GLfloat))
               {
                _sym_glVertexAttrib4fv(i, newctx->vertex_attrib[i].value);
               }

             if (newctx->vertex_array[i].enabled == GL_TRUE)
               {
                  _sym_glEnableVertexAttribArray(i);
               }
             else
               {
                  _sym_glDisableVertexAttribArray(i);
               }
          }

        STATE_COMPARE(gl_array_buffer_binding)
          {
             _sym_glBindBuffer(GL_ARRAY_BUFFER, newctx->gl_array_buffer_binding);
          }
        STATE_COMPARE(gl_element_array_buffer_binding)
          {
             _sym_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newctx->gl_element_array_buffer_binding);
          }

     }
#undef STATE_COMPARE
#undef STATES_COMPARE
}
//----------------------------------------------------------------//
//                                                                //
//                      Wrapped Glue Functions                    //
//                                                                //
//----------------------------------------------------------------//

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)

//----------------------------------------------------------------//
//                      Wrapped EGL Functions                     //
//----------------------------------------------------------------//
static _eng_fn
evgl_eglGetProcAddress(const char* procname)
{
   return _sym_eglGetProcAddress(procname);
}

static EGLint
evgl_eglGetError(void)
{
   return _sym_eglGetError();
}

static EGLDisplay
evgl_eglGetDisplay(EGLNativeDisplayType display_id)
{
   EVAS_GL_START_LOG(mc_log);

   EGLDisplay result;
   result = _sym_eglGetDisplay(display_id);

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static EGLBoolean
evgl_eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglInitialize(dpy, major, minor);

   return result;
}

static EGLBoolean
evgl_eglTerminate(EGLDisplay dpy)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglTerminate(dpy);

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static EGLBoolean
evgl_eglChooseConfig(EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, EGLint config_size, EGLint* num_config)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;

   result = _sym_eglChooseConfig(dpy, attrib_list, configs, config_size, num_config);

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static EGLSurface
evgl_eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint* attrib_list)
{
   EVAS_GL_START_LOG(mc_log);

   EGLSurface result;
   result = _sym_eglCreateWindowSurface(dpy, config, win, attrib_list);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLSurface
evgl_eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint* attrib_list)
{
   EVAS_GL_START_LOG(mc_log);
   EGLSurface result;
   result = _sym_eglCreatePixmapSurface(dpy, config, pixmap, attrib_list);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglDestroySurface(dpy, surface);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglBindAPI(EGLenum api)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglBindAPI(api);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglWaitClient(void)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglWaitClient();

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglSurfaceAttrib(dpy, surface, attribute, value);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static void
evgl_eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_eglBindTexImage(dpy, surface, buffer);

   EVAS_GL_END_LOG(mc_log);
}

static EGLBoolean
evgl_eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglReleaseTexImage(dpy, surface, buffer);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglSwapInterval(dpy, interval);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLContext
evgl_eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglCreateContext(dpy, config, share_context, attrib_list);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglDestroyContext(dpy, ctx);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
   /*
   if (_sym_eglMakeCurrent(dpy, draw, read, ctx))
     {
        current_egl_context = ctx;
        current_surf = draw;
        return EGL_TRUE;

   EVAS_GL_END_LOG(mc_log);
   return result;
}
   else return EGL_FALSE;
   */

   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglMakeCurrent(dpy, draw, read, ctx);

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static EGLContext
evgl_eglGetCurrentContext(void)
{
   EVAS_GL_START_LOG(mc_log);
   //return current_egl_context;
   EGLContext result;
   result = _sym_eglGetCurrentContext();

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLSurface
evgl_eglGetCurrentSurface(EGLint readdraw)
{
   EVAS_GL_START_LOG(mc_log);
   EGLSurface result;
   result = _sym_eglGetCurrentSurface(readdraw);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLDisplay
evgl_eglGetCurrentDisplay(void)
{
   EVAS_GL_START_LOG(mc_log);

   EGLDisplay result;
   result = _sym_eglGetCurrentDisplay();

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglWaitGL(void)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglWaitGL();

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglWaitNative(EGLint engine)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglWaitNative(engine);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
   EVAS_GL_START_LOG(mc_log);

   frame_count++;
   EGLBoolean result;
   result = _sym_eglSwapBuffers(dpy, surface);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static EGLBoolean
evgl_eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
{
   EVAS_GL_START_LOG(mc_log);

   EGLBoolean result;
   result = _sym_eglCopyBuffers(dpy, surface, target);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static char const *
evgl_eglQueryString(EGLDisplay dpy, EGLint name)
{
   EVAS_GL_START_LOG(mc_log);

   char const * result;
   result = _sym_eglQueryString(dpy, name);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static void
evgl_eglCreateImage (void *a, void *b, GLenum c, void *d, const int *e)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_eglCreateImage(a, b, c, d, e);

   EVAS_GL_END_LOG(mc_log);
}

static unsigned int
evgl_eglDestroyImage (void *a, void *b)
{
   EVAS_GL_START_LOG(mc_log);

   unsigned int result;
   result = _sym_eglDestroyImage(a, b);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static void
evgl_glEGLImageTargetTexture2DOES (int a, void *b)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glEGLImageTargetTexture2DOES(a, b);

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glEGLImageTargetRenderbufferStorageOES (int a, void *b)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glEGLImageTargetRenderbufferStorageOES(a, b);

   EVAS_GL_END_LOG(mc_log);
}

static void *
evgl_eglMapImageSEC (void *a, void *b)
{
   EVAS_GL_START_LOG(mc_log);

   void * result;
   result = _sym_eglMapImageSEC(a, b);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static unsigned int
evgl_eglUnmapImageSEC (void *a, void *b)
{
   EVAS_GL_START_LOG(mc_log);

   unsigned int result;
   result = _sym_eglUnmapImageSEC(a, b);

   EVAS_GL_END_LOG(mc_log);
   return result;
}

static unsigned int
evgl_eglGetImageAttribSEC (void *a, void *b, int c, int *d)
{
   EVAS_GL_START_LOG(mc_log);

   unsigned int result;
   result = _sym_eglGetImageAttribSEC(a, b, c, d);

   EVAS_GL_END_LOG(mc_log);
   return result;
}
//----------------------------------------------------------------//
//                   Fastpath EGL Functions                       //
//    The functions have prefix 'fpgl_' for (fastpath gl)         //
//----------------------------------------------------------------//
static EGLContext
fpgl_eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);

   EvasGlueContext ctx;

   // Create a global context if it hasn't been created
   if (!global_ctx)
     {
        global_dpy = dpy;
        // Create a global context if it hasn't been created yet
        global_ctx = _sym_eglCreateContext(dpy, config, NULL, attrib_list);

        if (!global_ctx)
          {
             ERR("Failed creating a glX global context for FastPath.\n");
             return 0;
          }
        //fprintf(stderr, "---TID: %d\n", syscall(__NR_gettid));
     }

   // Allocate a new context
   ctx = calloc(1, sizeof(struct _EvasGlueContext));
   if (!ctx)
     {
        ERR("Error creating a new EvasGlueContext.\n");
        return NULL;
     }

   ctx->initialized = 0;
   ctx->magic      = MAGIC_GLFAST;
   /*
   // Initialize context states
   if (!init_context_states(ctx))
     {
        ERR("Error intialing intial context\n");
        free(ctx);
        return NULL;
     }
     */

   ctx_ref_count++;

   return (EGLContext)ctx;
}

static EGLBoolean
fpgl_eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);

   EvasGlueContext ectx = (EvasGlueContext)ctx;

   if (ctx!=NULL)
     {
        if (ectx->magic != MAGIC_GLFAST)
          {
             ERR("Magic Check Failed!!!\n");
             return EGL_FALSE;
          }

        if (ectx == current_ctx)
          {
             DBG("Destroying current context... %d\n", ctx_ref_count);
             if ((real_current_ctx) && (real_current_ctx->destroyed))
                free(real_current_ctx);
             real_current_ctx = current_ctx;
             real_current_ctx->destroyed = 1;
             current_ctx = NULL;
          }
        else
          {
             if (ectx)
                free(ectx);
          }

        if (!(--ctx_ref_count))
          {
             if ((real_current_ctx) && (real_current_ctx->destroyed))
                free(real_current_ctx);

             DBG("Destroying the global context...\n");
             _sym_eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
             _sym_eglDestroyContext(dpy, global_ctx);
             global_ctx = NULL;
             current_ctx = NULL;
             real_current_ctx = NULL;
          }

        return EGL_TRUE;
     }
   else
     {
        ERR("Invalid Context.\n");
        return EGL_FALSE;
     }

   EVAS_GL_END_LOG(mc_log);
}

static EGLBoolean
fpgl_eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);

   // Do a Make Current NULL  (This is to unreference the currently bound surface)
   if (!_sym_eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
     {
        ERR("Error making context current with the drawable.\n");
        return EGL_FALSE;
     }

   if (current_ctx)
     {
        if ((real_current_ctx) && (real_current_ctx->destroyed))
           free(real_current_ctx);
        real_current_ctx = current_ctx;
     }

   current_ctx = NULL;
   current_surf = EGL_NO_SURFACE;

   EGLBoolean result;
   result = _sym_eglDestroySurface(dpy, surface);

   EVAS_GL_END_LOG(mc_log);
   return result;
}


static EGLBoolean
fpgl_eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);

   EvasGlueContext ectx = (EvasGlueContext)ctx;

   current_surf = _sym_eglGetCurrentSurface(EGL_DRAW);

   // Check if the values are null
   if ((draw == EGL_NO_SURFACE) || (ctx == NULL))
     {
        if (current_ctx)
          {
             if ((real_current_ctx) && (real_current_ctx->destroyed))
                free(real_current_ctx);
             real_current_ctx = current_ctx;
          }

        current_ctx = NULL;
        current_surf = EGL_NO_SURFACE;
        EVAS_GL_END_LOG(mc_log);

        return EGL_TRUE;
     }

   // Check if the object is correct
   if (ectx->magic != MAGIC_GLFAST)
     {
        ERR("Magic Check Failed!!!\n");
        EVAS_GL_END_LOG(mc_log);
        return EGL_FALSE;
     }


   // If it's a first time or drawable changed, do a make current
   if (!global_ctx_initted)
     {
        if (!_sym_eglMakeCurrent(dpy, draw, read, global_ctx))
          {
             ERR("Error making context current with the drawable.\n");
             EVAS_GL_END_LOG(mc_log);
             return False;
          }

        current_ctx      = ectx;
        current_surf     = draw;
        real_current_ctx = current_ctx;

        global_ctx_initted = 1;
     }


   if (!current_ctx) current_ctx = real_current_ctx;

   // If drawable changed, do a make current
   if (current_surf != draw)
     {
        if (!_sym_eglMakeCurrent(dpy, draw, read, global_ctx))
          {
             ERR("Error making context current with the drawable.\n");
             EVAS_GL_END_LOG(mc_log);
             return False;
          }
        current_surf = draw;
     }

   // If it's first time...
   if (ectx->initialized == 0)
     {
        if (!init_context_states(ectx))
          {
             ERR("Error intialing intial context\n");
             return False;
          }

        // FIXME!!!:
        // Actually, i need to query the drawable size...
        // Do some initializations that required make_current
        _sym_glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &ectx->num_tex_units);
        _sym_glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &ectx->num_vertex_attribs);
        _sym_glGetIntegerv(GL_VIEWPORT, ectx->gl_viewport);
        _sym_glGetIntegerv(GL_SCISSOR_BOX, ectx->gl_scissor_box);
        DBG("----Num Tex Units: %d, Num Vertex Attribs: %d \n", ectx->num_tex_units, ectx->num_vertex_attribs);

        ectx->initialized = 1;
     }

   // if context is same, return
   if ( (current_ctx == ectx) && (current_surf == draw) )
     {
        EVAS_GL_END_LOG(mc_log);
        return EGL_TRUE;
     }

   make_context_current(current_ctx, ectx);

   current_ctx = ectx;
   current_surf = draw;

   EVAS_GL_END_LOG(mc_log);

   return EGL_TRUE;
}

static EGLContext
fpgl_eglGetCurrentContext(void)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);

   EGLContext result;
   result = (EGLContext)current_ctx;

   EVAS_GL_END_LOG(mc_log);

   return result;
   //return _sym_eglGetCurrentContext();
}

static EGLSurface
fpgl_eglGetCurrentSurface(EGLint readdraw)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);

   EGLSurface result;
   result = (EGLSurface)current_surf;

   EVAS_GL_END_LOG(mc_log);

   return result;
   //return _sym_eglGetCurrentSurface(readdraw);
}

// FIXME!!!!
static void
fpgl_glEGLImageTargetTexture2DOES (int a, void *b)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);

   _sym_glEGLImageTargetTexture2DOES(a, b);

   EVAS_GL_END_LOG(mc_log);
}

// FIXME!!!!
static void
fpgl_glEGLImageTargetRenderbufferStorageOES (int a, void *b)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);

   _sym_glEGLImageTargetRenderbufferStorageOES(a, b);

   EVAS_GL_END_LOG(mc_log);
}


#else

//----------------------------------------------------------------//
//                      Wrapped GLX Functions                     //
//----------------------------------------------------------------//
static _eng_fn
evgl_glXGetProcAddress(const char* procName)
{
   return _sym_glXGetProcAddress(procName);
}

static XVisualInfo*
evgl_glXChooseVisual(Display* dpy, int screen, int* attribList)
{
   return _sym_glXChooseVisual(dpy, screen, attribList);
}

static GLXContext
evgl_glXCreateContext(Display* dpy, XVisualInfo* vis, GLXContext shareList, Bool direct)
{
   return _sym_glXCreateContext(dpy, vis, shareList, direct);
}

static void
evgl_glXDestroyContext(Display* dpy, GLXContext ctx)
{
   _sym_glXDestroyContext(dpy, ctx);
}

static GLXContext
evgl_glXGetCurrentContext(void)
{
   return _sym_glXGetCurrentContext();
}

static GLXDrawable
evgl_glXGetCurrentDrawable(void)
{
   return _sym_glXGetCurrentDrawable();
}


static Bool
evgl_glXMakeCurrent(Display* dpy, GLXDrawable draw, GLXContext ctx)
{
   return _sym_glXMakeCurrent(dpy, draw, ctx);
}

static void
evgl_glXSwapBuffers(Display* dpy, GLXDrawable draw)
{
   _sym_glXSwapBuffers(dpy, draw);
}

static void
evgl_glXWaitX(void)
{
   _sym_glXWaitX();
}

static void
evgl_glXWaitGL(void)
{
   _sym_glXWaitGL();
}

static Bool
evgl_glXQueryExtension(Display* dpy, int* errorb, int* event)
{
   return _sym_glXQueryExtension(dpy, errorb, event);
}

static const char *
evgl_glXQueryExtensionsString(Display *dpy, int screen)
{
   return _sym_glXQueryExtensionsString(dpy, screen);
}

static GLXFBConfig*
evgl_glXChooseFBConfig(Display* dpy, int screen, const int* attribList, int* nitems)
{
   return _sym_glXChooseFBConfig(dpy, screen, attribList, nitems);
}

static GLXFBConfig*
evgl_glXGetFBConfigs(Display* dpy, int screen, int* nelements)
{
   return _sym_glXGetFBConfigs(dpy, screen, nelements);
}

//!!!!! FIXME Called too many times
static int
evgl_glXGetFBConfigAttrib(Display* dpy, GLXFBConfig config, int attribute, int* value)
{
   return _sym_glXGetFBConfigAttrib(dpy, config, attribute, value);
}

//!!!!! FIXME Called too many times
static XVisualInfo*
evgl_glXGetVisualFromFBConfig(Display* dpy, GLXFBConfig config)
{
   return _sym_glXGetVisualFromFBConfig(dpy, config);
}

static void
evgl_glXDestroyWindow(Display* dpy, GLXWindow window)
{
   _sym_glXDestroyWindow(dpy, window);
}

static Bool
evgl_glXMakeContextCurrent(Display* dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx)
{
   return _sym_glXMakeContextCurrent(dpy, draw, read, ctx);
}

static void
evgl_glXBindTexImage(Display* dpy, GLXDrawable draw, int buffer, int* attribList)
{
   _sym_glXBindTexImage(dpy, draw, buffer, attribList);
}

static void
evgl_glXReleaseTexImage(Display* dpy, GLXDrawable draw, int buffer)
{
   _sym_glXReleaseTexImage(dpy, draw, buffer);
}

static int
evgl_glXGetVideoSync(unsigned int* count)
{
   return _sym_glXGetVideoSync(count);
}

static int
evgl_glXWaitVideoSync(int divisor, int remainder, unsigned int* count)
{
   return _sym_glXWaitVideoSync(divisor, remainder, count);
}

static XID
evgl_glXCreatePixmap(Display* dpy, void* config, Pixmap pixmap, const int* attribList)
{
   return _sym_glXCreatePixmap(dpy, config, pixmap, attribList);
}

static void
evgl_glXDestroyPixmap(Display* dpy, XID pixmap)
{
   _sym_glXDestroyPixmap(dpy, pixmap);
}

static void
evgl_glXQueryDrawable(Display* dpy, XID draw, int attribute, unsigned int* value)
{
   _sym_glXQueryDrawable(dpy, draw, attribute, value);
}

static int
evgl_glXSwapIntervalSGI(int interval)
{
   return _sym_glXSwapIntervalSGI(interval);
}

static void
evgl_glXSwapIntervalEXT(Display* dpy, GLXDrawable draw, int interval)
{
   _sym_glXSwapIntervalEXT(dpy, draw, interval);
}


//----------------------------------------------------------------//
//                   Fastpath GLX Functions                       //
//       The functions have prefix 'fpgl_' for (fastpath gl)      //
//----------------------------------------------------------------//
static GLXContext
fpgl_glXCreateContext(Display* dpy, XVisualInfo* vis, GLXContext shareList, Bool direct)
{
   THREAD_CHECK_DEBUG();

   EvasGlueContext ctx;

   // Create a global context if it hasn't been created
   if (!global_ctx)
     {
        global_dpy = dpy;
        // Create a global context if it hasn't been created yet
        global_ctx = _sym_glXCreateContext(dpy, vis, NULL, 1);
        if (!global_ctx)
          {
             ERR("Failed creating a glX global context for FastPath.\n");
             return 0;
          }
        ctx_ref_count++;
     }

   // Allocate a new context
   ctx = calloc(1, sizeof(struct _EvasGlueContext));
   if (!ctx)
     {
        ERR("Error creating a new EvasGlueContext.\n");
        return NULL;
     }

   if (!init_context_states(ctx))
     {
        ERR("Error intialing intial context\n");
        free(ctx);
        return NULL;
     }

   ctx_ref_count++;

   return (GLXContext)ctx;

}

static void
fpgl_glXDestroyContext(Display* dpy, GLXContext ctx)
{
   THREAD_CHECK_DEBUG();

   EvasGlueContext ectx = (EvasGlueContext)ctx;

   if (ctx!=NULL)
     {
        if (ectx->magic != MAGIC_GLFAST)
          {
             ERR("Magic Check Failed!!!\n");
             return;
          }

        if (ectx == current_ctx)
          {
             DBG("Destroying current context... %d\n", ctx_ref_count);
             if ((real_current_ctx) && (real_current_ctx->destroyed))
                free(real_current_ctx);
             real_current_ctx = current_ctx;
             real_current_ctx->destroyed = 1;
             current_ctx = NULL;
          }
        else
          {
             if (ectx)
                free(ectx);
          }

        if (!(--ctx_ref_count))
          {
             if ((real_current_ctx) && (real_current_ctx->destroyed))
                free(real_current_ctx);

             DBG("Destroying the global context...\n");
             _sym_glXDestroyContext(dpy, global_ctx);
             global_ctx = NULL;
             current_ctx = NULL;
             real_current_ctx = NULL;
          }
     }
}


static GLXContext
fpgl_glXGetCurrentContext(void)
{
   THREAD_CHECK_DEBUG();

   return (GLXContext)current_ctx;
}

static GLXDrawable
fpgl_glXGetCurrentDrawable(void)
{
   THREAD_CHECK_DEBUG();

   //return _sym_glXGetCurrentDrawable();
   return (GLXDrawable)current_surf;
}


static Bool
fpgl_glXMakeCurrent(Display* dpy, GLXDrawable draw, GLXContext ctx)
{
   THREAD_CHECK_DEBUG();

   EvasGlueContext ectx = (EvasGlueContext)ctx;

   // Check if the values are null
   if ((draw == None) || (ctx == NULL))
     {
        if (current_ctx)
          {
             if ((real_current_ctx) && (real_current_ctx->destroyed))
                free(real_current_ctx);
             real_current_ctx = current_ctx;
          }

        /*
        if (!_sym_glXMakeCurrent(dpy, None, NULL))
          {
             ERR("Error making context current with the drawable.\n");
             return False;
          }
          */

        current_ctx = NULL;
        current_surf = None;

        return 1;
     }

   // Check if the object is correct
   if (ectx->magic != MAGIC_GLFAST)
     {
        ERR("Magic Check Failed!!!\n");
        return 0;
     }


   // If it's the first time
   if (!global_ctx_initted)
     {
        if (!_sym_glXMakeCurrent(dpy, draw, global_ctx))
          {
             ERR("Error making context current with the drawable.\n");
             return False;
          }

        current_ctx  = ectx;
        current_surf = draw;

        real_current_ctx = current_ctx;

        global_ctx_initted = 1;
     }


   if (!current_ctx) current_ctx = real_current_ctx;

   // If drawable changed, do a make current
   if (current_surf != draw)
     {
        if (!_sym_glXMakeCurrent(dpy, draw, global_ctx))
          {
             ERR("Error making context current with the drawable.\n");
             return False;
          }
        current_surf = draw;
     }

   // If it's first time...
   if (ectx->initialized == 1)
     {
        // FIXME!!!:
        // Actually, i need to query the drawable size...
        // Do some initializations that required make_current
        _sym_glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &ectx->num_tex_units);
        _sym_glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &ectx->num_vertex_attribs);
        _sym_glGetIntegerv(GL_VIEWPORT, ectx->gl_viewport);
        _sym_glGetIntegerv(GL_SCISSOR_BOX, ectx->gl_scissor_box);
        DBG("----Num Tex Units: %d, Num Vertex Attribs: %d \n", ectx->num_tex_units, ectx->num_vertex_attribs);

        ectx->initialized = 0;
     }

   // if context is same, return
   if ( (current_ctx == ectx) && (current_surf == draw) )
     {
        return True;
     }

   make_context_current(current_ctx, ectx);

   current_ctx = ectx;
   current_surf = draw;

   return True;
}

static Bool
fpgl_glXMakeContextCurrent(Display* dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx)
{
   THREAD_CHECK_DEBUG();

   ERR("NOT IMPLEMENTED YET!!! GLX Function Wrapped. Not fastpathed yet...\n");
   return _sym_glXMakeContextCurrent(dpy, draw, read, ctx);
}


#endif // (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)



//----------------------------------------------------------------//
//                                                                //
//                      Wrapped GL Functions                      //
//                                                                //
//----------------------------------------------------------------//

static void
evgl_glActiveTexture(GLenum texture)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glActiveTexture(texture);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glAttachShader(GLuint program, GLuint shader)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glAttachShader(program, shader);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBindAttribLocation(GLuint program, GLuint index, const char* name)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBindAttribLocation(program, index, name);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBindBuffer(GLenum target, GLuint buffer)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBindBuffer(target, buffer);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBindFramebuffer(target, framebuffer);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBindRenderbuffer(target, renderbuffer);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBindTexture(GLenum target, GLuint texture)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBindTexture(target, texture);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBlendColor(red, green, blue, alpha);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBlendEquation(GLenum mode)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBlendEquation(mode);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBlendEquationSeparate(modeRGB, modeAlpha);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBlendFunc(GLenum sfactor, GLenum dfactor)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBlendFunc(sfactor, dfactor);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBufferData(target, size, data, usage);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glBufferSubData(target, offset, size, data);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static GLenum
evgl_glCheckFramebufferStatus(GLenum target)
{
   EVAS_GL_START_LOG(mc_log);

   GLenum result;

   result = _sym_glCheckFramebufferStatus(target);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static void
evgl_glClear(GLbitfield mask)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glClear(mask);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glClearColor(red, green, blue, alpha);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glClearDepthf(GLclampf depth)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glClearDepthf(depth);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glClearStencil(GLint s)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glClearStencil(s);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glColorMask(red, green, blue, alpha);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glCompileShader(GLuint shader)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glCompileShader(shader);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static GLuint
evgl_glCreateProgram(void)
{
   EVAS_GL_START_LOG(mc_log);

   GLuint program;

   program = _sym_glCreateProgram();
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return program;
}

static GLuint
evgl_glCreateShader(GLenum type)
{
   EVAS_GL_START_LOG(mc_log);

   GLuint shader;

   shader = _sym_glCreateShader(type);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return shader;
}

static void
evgl_glCullFace(GLenum mode)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glCullFace(mode);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDeleteBuffers(GLsizei n, const GLuint* buffers)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDeleteBuffers(n, buffers);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDeleteFramebuffers(n, framebuffers);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDeleteProgram(GLuint program)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDeleteProgram(program);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDeleteRenderbuffers(n, renderbuffers);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDeleteShader(GLuint shader)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDeleteShader(shader);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDeleteTextures(GLsizei n, const GLuint* textures)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDeleteTextures(n, textures);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDepthFunc(GLenum func)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDepthFunc(func);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDepthMask(GLboolean flag)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDepthMask(flag);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDepthRangef(GLclampf zNear, GLclampf zFar)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDepthRangef(zNear, zFar);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDetachShader(GLuint program, GLuint shader)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDetachShader(program, shader);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDisable(GLenum cap)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDisable(cap);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDisableVertexAttribArray(GLuint index)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDisableVertexAttribArray(index);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDrawArrays(mode, first, count);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glDrawElements(mode, count, type, indices);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glEnable(GLenum cap)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glEnable(cap);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glEnableVertexAttribArray(GLuint index)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glEnableVertexAttribArray(index);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glFinish(void)
{
   //EVAS_GL_START_LOG(mc_log);

   _sym_glFinish();
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   //EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glFlush(void)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glFlush();
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glFramebufferTexture2D(target, attachment, textarget, texture, level);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glFrontFace(GLenum mode)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glFrontFace(mode);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetVertexAttribfv(index, pname, params);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetVertexAttribiv(index, pname, params);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetVertexAttribPointerv(index, pname, pointer);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

}

static void
evgl_glHint(GLenum target, GLenum mode)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glHint(target, mode);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGenBuffers(GLsizei n, GLuint* buffers)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGenBuffers(n, buffers);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGenerateMipmap(GLenum target)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGenerateMipmap(target);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGenFramebuffers(GLsizei n, GLuint* framebuffers)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGenFramebuffers(n, framebuffers);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGenRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGenRenderbuffers(n, renderbuffers);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGenTextures(GLsizei n, GLuint* textures)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGenTextures(n, textures);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetActiveAttrib(program, index, bufsize, length, size, type, name);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetActiveUniform(GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, char* name)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetActiveUniform(program, index, bufsize, length, size, type, name);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetAttachedShaders(GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetAttachedShaders(program, maxcount, count, shaders);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static int
evgl_glGetAttribLocation(GLuint program, const char* name)
{
   EVAS_GL_START_LOG(mc_log);

   int location;

   location = _sym_glGetAttribLocation(program, name);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return location;
}

static void
evgl_glGetBooleanv(GLenum pname, GLboolean* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetBooleanv(pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetBufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetBufferParameteriv(target, pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static GLenum
evgl_glGetError(void)
{
   EVAS_GL_START_LOG(mc_log);

   return _sym_glGetError();
}

static void
evgl_glGetFloatv(GLenum pname, GLfloat* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetFloatv(pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetIntegerv(GLenum pname, GLint* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetIntegerv(pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetProgramiv(GLuint program, GLenum pname, GLint* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetProgramiv(program, pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei* length, char* infolog)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetProgramInfoLog(program, bufsize, length, infolog);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetRenderbufferParameteriv(target, pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetShaderiv(GLuint shader, GLenum pname, GLint* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetShaderiv(shader, pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei* length, char* infolog)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetShaderInfoLog(shader, bufsize, length, infolog);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision)
{
   EVAS_GL_START_LOG(mc_log);

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
   _sym_glGetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
#else
   if (range)
     {
        range[0] = -126; // floor(log2(FLT_MIN))
        range[1] = 127; // floor(log2(FLT_MAX))
     }
   if (precision)
     {
        precision[0] = 24; // floor(-log2((1.0/16777218.0)));
     }
   return;
   shadertype = precisiontype = 0;
#endif

}

static void
evgl_glGetShaderSource(GLuint shader, GLsizei bufsize, GLsizei* length, char* source)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetShaderSource(shader, bufsize, length, source);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static const GLubyte *
evgl_glGetString(GLenum name)
{
   EVAS_GL_START_LOG(mc_log);

   const GLubyte *str;

   str =_sym_glGetString(name);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return str;
}

static void
evgl_glGetTexParameterfv(GLenum target, GLenum pname, GLfloat* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetTexParameterfv(target, pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetTexParameteriv(GLenum target, GLenum pname, GLint* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetTexParameteriv(target, pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetUniformfv(GLuint program, GLint location, GLfloat* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetUniformfv(program, location, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glGetUniformiv(GLuint program, GLint location, GLint* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetUniformiv(program, location, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static int
evgl_glGetUniformLocation(GLuint program, const char* name)
{
   EVAS_GL_START_LOG(mc_log);

   int location;

   location = _sym_glGetUniformLocation(program, name);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return location;
}

static GLboolean
evgl_glIsBuffer(GLuint buffer)
{
   EVAS_GL_START_LOG(mc_log);

   GLboolean result;

   result =  _sym_glIsBuffer(buffer);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static GLboolean
evgl_glIsEnabled(GLenum cap)
{
   EVAS_GL_START_LOG(mc_log);

   GLboolean result;

   result =  _sym_glIsEnabled(cap);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static GLboolean
evgl_glIsFramebuffer(GLuint framebuffer)
{
   EVAS_GL_START_LOG(mc_log);

   GLboolean result;

   result = _sym_glIsFramebuffer(framebuffer);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static GLboolean
evgl_glIsProgram(GLuint program)
{
   EVAS_GL_START_LOG(mc_log);

   GLboolean result;

   result =  _sym_glIsProgram(program);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static GLboolean
evgl_glIsRenderbuffer(GLuint renderbuffer)
{
   EVAS_GL_START_LOG(mc_log);

   GLboolean result;

   result =  _sym_glIsRenderbuffer(renderbuffer);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static GLboolean
evgl_glIsShader(GLuint shader)
{
   EVAS_GL_START_LOG(mc_log);

   GLboolean result;

   result =  _sym_glIsShader(shader);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static GLboolean
evgl_glIsTexture(GLuint texture)
{
   EVAS_GL_START_LOG(mc_log);

   GLboolean result;

   result = _sym_glIsTexture(texture);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);

   return result;
}

static void
evgl_glLineWidth(GLfloat width)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glLineWidth(width);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glLinkProgram(GLuint program)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glLinkProgram(program);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glPixelStorei(GLenum pname, GLint param)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glPixelStorei(pname, param);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glPolygonOffset(GLfloat factor, GLfloat units)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glPolygonOffset(factor, units);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glReadPixels(x, y, width, height, format, type, pixels);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glReleaseShaderCompiler(void)
{
   EVAS_GL_START_LOG(mc_log);

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
   _sym_glReleaseShaderCompiler();
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
#else
   //FIXME!!! need something here?

#endif
}

static void
evgl_glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glRenderbufferStorage(target, internalformat, width, height);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glSampleCoverage(GLclampf value, GLboolean invert)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glSampleCoverage(value, invert);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glScissor(x, y, width, height);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glShaderBinary(GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary, GLsizei length)
{
   EVAS_GL_START_LOG(mc_log);

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
   _sym_glShaderBinary(n, shaders, binaryformat, binary, length);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
#else
// FIXME: need to dlsym/getprocaddress for this
   return;
/*
   n = binaryformat = length = 0;
   shaders = binary = 0;
*/
#endif
}

static void
evgl_glShaderSource(GLuint shader, GLsizei count, const char** string, const GLint* length)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glShaderSource(shader, count, string, length);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glStencilFunc(func, ref, mask);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glStencilFuncSeparate(face, func, ref, mask);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glStencilMask(GLuint mask)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glStencilMask(mask);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glStencilMaskSeparate(GLenum face, GLuint mask)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glStencilMaskSeparate(face, mask);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glStencilOp(fail, zfail, zpass);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glStencilOpSeparate(face, fail, zfail, zpass);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glTexParameterf(target, pname, param);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glTexParameterfv(target, pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glTexParameteri(GLenum target, GLenum pname, GLint param)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glTexParameteri(target, pname, param);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glTexParameteriv(GLenum target, GLenum pname, const GLint* params)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glTexParameteriv(target, pname, params);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform1f(GLint location, GLfloat x)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform1f(location, x);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform1fv(GLint location, GLsizei count, const GLfloat* v)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform1fv(location, count, v);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform1i(GLint location, GLint x)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform1i(location, x);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform1iv(GLint location, GLsizei count, const GLint* v)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform1iv(location, count, v);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform2f(GLint location, GLfloat x, GLfloat y)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform2f(location, x, y);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform2fv(GLint location, GLsizei count, const GLfloat* v)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform2fv(location, count, v);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform2i(GLint location, GLint x, GLint y)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform2i(location, x, y);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform2iv(GLint location, GLsizei count, const GLint* v)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform2iv(location, count, v);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform3f(location, x, y, z);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform3fv(GLint location, GLsizei count, const GLfloat* v)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform3fv(location, count, v);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform3i(GLint location, GLint x, GLint y, GLint z)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform3i(location, x, y, z);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform3iv(GLint location, GLsizei count, const GLint* v)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform3iv(location, count, v);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform4f(location, x, y, z, w);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform4fv(GLint location, GLsizei count, const GLfloat* v)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform4fv(location, count, v);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform4i(location, x, y, z, w);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniform4iv(GLint location, GLsizei count, const GLint* v)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniform4iv(location, count, v);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniformMatrix2fv(location, count, transpose, value);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniformMatrix3fv(location, count, transpose, value);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUniformMatrix4fv(location, count, transpose, value);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glUseProgram(GLuint program)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glUseProgram(program);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glValidateProgram(GLuint program)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glValidateProgram(program);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glVertexAttrib1f(GLuint indx, GLfloat x)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glVertexAttrib1f(indx, x);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glVertexAttrib1fv(GLuint indx, const GLfloat* values)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glVertexAttrib1fv(indx, values);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glVertexAttrib2f(GLuint indx, GLfloat x, GLfloat y)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glVertexAttrib2f(indx, x, y);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glVertexAttrib2fv(GLuint indx, const GLfloat* values)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glVertexAttrib2fv(indx, values);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glVertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glVertexAttrib3f(indx, x, y, z);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glVertexAttrib3fv(GLuint indx, const GLfloat* values)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glVertexAttrib3fv(indx, values);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glVertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glVertexAttrib4f(indx, x, y, z, w);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glVertexAttrib4fv(GLuint indx, const GLfloat* values)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glVertexAttrib4fv(indx, values);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glVertexAttribPointer(GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* ptr)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glViewport(x, y, width, height);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}


// GLES Extensions...
static void
evgl_glGetProgramBinary(GLuint program, GLsizei bufsize, GLsizei* length, GLenum* binaryFormat, void* binary)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glGetProgramBinary(program, bufsize, length, binaryFormat, binary);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
evgl_glProgramBinary(GLuint program, GLenum binaryFormat, const void* binary, GLint length)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glProgramBinary(program, binaryFormat, binary, length);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}


static void
evgl_glProgramParameteri(GLuint program, GLuint pname, GLint value)
{
   EVAS_GL_START_LOG(mc_log);

   _sym_glProgramParameteri(program, pname, value);
   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

//----------------------------------------------------------------//
//                                                                //
//                      Fastpath GL Functions                     //
// The functions have prefix 'fpgl_' for (fastpath gl)            //
//                                                                //
//----------------------------------------------------------------//

#ifdef EVAS_GL_DEBUG
   #define CURR_STATE_COMPARE(curr_state, state ) \
      if (1)
#else
   #define CURR_STATE_COMPARE(curr_state, state ) \
      if ((current_ctx->curr_state) != (state))
#endif


static void
fpgl_glActiveTexture(GLenum texture)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   CURR_STATE_COMPARE(gl_active_texture, texture)
     {
        _sym_glActiveTexture(texture);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_tex_flag1 |= FLAG_BIT_1;
        current_ctx->gl_active_texture = texture;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glBindBuffer(GLenum target, GLuint buffer)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if (target == GL_ARRAY_BUFFER)
     {
        CURR_STATE_COMPARE(gl_array_buffer_binding, buffer)
          {
             _sym_glBindBuffer(target, buffer);

             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             /*
                if (buffer == 0)
                current_ctx->_bind_flag &= (~FLAG_BIT_0);
                else
              */
             current_ctx->_bind_flag |= FLAG_BIT_0;
             current_ctx->gl_array_buffer_binding = buffer;
          }
     }
   else if (target == GL_ELEMENT_ARRAY_BUFFER)
     {
        CURR_STATE_COMPARE(gl_element_array_buffer_binding, buffer)
          {
             _sym_glBindBuffer(target, buffer);
             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             /*
                if (buffer == 0)
                current_ctx->_bind_flag &= (~FLAG_BIT_1);
                else
              */
             current_ctx->_bind_flag |= FLAG_BIT_1;
             current_ctx->gl_element_array_buffer_binding = buffer;
          }
     }
   else
     {
        // For error recording
        _sym_glBindBuffer(target, buffer);
        GLERR(__FUNCTION__, __FILE__, __LINE__, "");
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glBindFramebuffer(GLenum target, GLuint framebuffer)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if (target == GL_FRAMEBUFFER)
     {
        CURR_STATE_COMPARE(gl_framebuffer_binding, framebuffer)
          {
             _sym_glBindFramebuffer(target, framebuffer);
             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             /*
                if (framebuffer == 0)
                current_ctx->_bind_flag &= (~FLAG_BIT_2);
                else
              */
             current_ctx->_bind_flag |= FLAG_BIT_2;
             current_ctx->gl_framebuffer_binding = framebuffer;
          }
     }
   else
     {
        // For error recording
        _sym_glBindFramebuffer(target, framebuffer);
        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if (target == GL_RENDERBUFFER)
     {
        CURR_STATE_COMPARE(gl_renderbuffer_binding, renderbuffer)
          {
             _sym_glBindRenderbuffer(target, renderbuffer);
             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             if (renderbuffer == 0)
                current_ctx->_bind_flag &= (~FLAG_BIT_3);
             else
                current_ctx->_bind_flag |= FLAG_BIT_3;
             current_ctx->gl_renderbuffer_binding = renderbuffer;
          }
     }
   else
     {
        // For error recording
        _sym_glBindRenderbuffer(target, renderbuffer);
        GLERR(__FUNCTION__, __FILE__, __LINE__, "");
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glBindTexture(GLenum target, GLuint texture)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   int tex_idx;

   tex_idx = current_ctx->gl_active_texture - GL_TEXTURE0;

   EVAS_GL_DEBUG_ASSERT(tex_idx < MAX_TEXTURE_UNITS);

   if (target == GL_TEXTURE_2D)
     {
        CURR_STATE_COMPARE(tex_2d_state[tex_idx], texture)
          {
             _sym_glBindTexture(target, texture);

             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             /*
                if (texture == 0)
                current_ctx->_tex_flag1 &= (~FLAG_BIT_3);
                else
              */
             current_ctx->_tex_flag1 |= FLAG_BIT_3;

             current_ctx->tex_2d_state[tex_idx] = texture;
          }
     }
   else if (target == GL_TEXTURE_CUBE_MAP)
     {
        CURR_STATE_COMPARE(tex_cube_map_state[tex_idx], texture)
          {
             _sym_glBindTexture(target, texture);

             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             /*
                if (texture == 0)
                current_ctx->_tex_flag1 &= (~FLAG_BIT_4);
                else
              */
             current_ctx->_tex_flag1 |= FLAG_BIT_4;

             current_ctx->gl_texture_binding_cube_map = texture;

             current_ctx->tex_cube_map_state[tex_idx] = texture;
             //current_ctx->tex_state[tex_idx].tex_unit = GL_TEXTURE_CUBE_MAP;
             //current_ctx->tex_state[tex_idx].tex_id = texture;
          }
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_blend_color[0] != red) ||
       (current_ctx->gl_blend_color[1] != green) ||
       (current_ctx->gl_blend_color[2] != blue) ||
       (current_ctx->gl_blend_color[3] != alpha))
     {
        _sym_glBlendColor(red, green, blue, alpha);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_blend_flag |= FLAG_BIT_0;
        current_ctx->gl_blend_color[0] = red;
        current_ctx->gl_blend_color[1] = green;
        current_ctx->gl_blend_color[2] = blue;
        current_ctx->gl_blend_color[3] = alpha;
     }

   EVAS_GL_END_LOG(mc_log);
}

//!!! Optimze?
static void
fpgl_glBlendEquation(GLenum mode)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glBlendEquation(mode);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   current_ctx->_blend_flag |= (FLAG_BIT_5 | FLAG_BIT_6);
   _sym_glGetIntegerv(GL_BLEND_EQUATION_RGB,   (GLint*)&(current_ctx->gl_blend_equation_rgb));
   _sym_glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&(current_ctx->gl_blend_equation_alpha));

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_blend_equation_rgb != modeRGB) ||
       (current_ctx->gl_blend_equation_alpha != modeAlpha))
     {
        _sym_glBlendEquationSeparate(modeRGB, modeAlpha);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_blend_flag |= (FLAG_BIT_5 | FLAG_BIT_6);
        current_ctx->gl_blend_equation_rgb    = modeRGB;
        current_ctx->gl_blend_equation_alpha  = modeAlpha;
     }

   EVAS_GL_END_LOG(mc_log);
}

//!!! Optimze?
static void
fpgl_glBlendFunc(GLenum sfactor, GLenum dfactor)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glBlendFunc(sfactor, dfactor);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   current_ctx->_blend_flag |= (FLAG_BIT_1 | FLAG_BIT_2 | FLAG_BIT_3 | FLAG_BIT_4);
   _sym_glGetIntegerv(GL_BLEND_SRC_RGB,   (GLint*)&(current_ctx->gl_blend_src_rgb));
   _sym_glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&(current_ctx->gl_blend_src_alpha));
   _sym_glGetIntegerv(GL_BLEND_DST_RGB,   (GLint*)&(current_ctx->gl_blend_dst_rgb));
   _sym_glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&(current_ctx->gl_blend_dst_alpha));

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_blend_src_rgb != srcRGB) ||
       (current_ctx->gl_blend_dst_rgb != dstRGB) ||
       (current_ctx->gl_blend_src_alpha != srcAlpha) ||
       (current_ctx->gl_blend_dst_alpha != dstAlpha))
     {
        _sym_glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_blend_flag |= (FLAG_BIT_1 | FLAG_BIT_2 | FLAG_BIT_3 | FLAG_BIT_4);
        current_ctx->gl_blend_src_rgb   = srcRGB;
        current_ctx->gl_blend_dst_rgb   = dstRGB;
        current_ctx->gl_blend_src_alpha = srcAlpha;
        current_ctx->gl_blend_dst_alpha = dstAlpha;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_color_clear_value[0] != red) ||
       (current_ctx->gl_color_clear_value[1] != green) ||
       (current_ctx->gl_color_clear_value[2] != blue) ||
       (current_ctx->gl_color_clear_value[3] != alpha))
     {
        _sym_glClearColor(red, green, blue, alpha);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_clear_flag1 |= (FLAG_BIT_2);
        current_ctx->gl_color_clear_value[0] = red;
        current_ctx->gl_color_clear_value[1] = green;
        current_ctx->gl_color_clear_value[2] = blue;
        current_ctx->gl_color_clear_value[3] = alpha;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glClearDepthf(GLclampf depth)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   CURR_STATE_COMPARE(gl_depth_clear_value, depth)
     {
        _sym_glClearDepthf(depth);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_clear_flag2 |= FLAG_BIT_2;
        current_ctx->gl_depth_clear_value = depth;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glClearStencil(GLint s)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   CURR_STATE_COMPARE(gl_stencil_clear_value, s)
     {
        _sym_glClearStencil(s);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_stencil_flag2 |= FLAG_BIT_7;
        current_ctx->gl_stencil_clear_value = s;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_color_writemask[0] != red) ||
       (current_ctx->gl_color_writemask[1] != green) ||
       (current_ctx->gl_color_writemask[2] != blue) ||
       (current_ctx->gl_color_writemask[3] != alpha))
     {
        _sym_glColorMask(red, green, blue, alpha);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_clear_flag2 |= FLAG_BIT_0;
        current_ctx->gl_color_writemask[0] = red;
        current_ctx->gl_color_writemask[1] = green;
        current_ctx->gl_color_writemask[2] = blue;
        current_ctx->gl_color_writemask[3] = alpha;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glCullFace(GLenum mode)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   CURR_STATE_COMPARE(gl_cull_face_mode, mode)
     {
        _sym_glCullFace(mode);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_clear_flag2 |= FLAG_BIT_5;
        current_ctx->gl_cull_face_mode = mode;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glDepthFunc(GLenum func)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   CURR_STATE_COMPARE(gl_depth_func, func)
     {
        _sym_glDepthFunc(func);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_clear_flag2 |= FLAG_BIT_3;
        current_ctx->gl_depth_func = func;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glDepthMask(GLboolean flag)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   CURR_STATE_COMPARE(gl_depth_writemask, flag)
     {
        _sym_glDepthMask(flag);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_clear_flag2 |= FLAG_BIT_4;
        current_ctx->gl_depth_writemask = flag;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glDepthRangef(GLclampf zNear, GLclampf zFar)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ((current_ctx->gl_depth_range[0] != zNear) ||
       (current_ctx->gl_depth_range[1] != zFar))
     {
        _sym_glDepthRangef(zNear, zFar);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_clear_flag2 |= FLAG_BIT_1;
        current_ctx->gl_depth_range[0] = zNear;
        current_ctx->gl_depth_range[1] = zFar;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glDisable(GLenum cap)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   switch(cap)
     {
      case GL_BLEND:
         CURR_STATE_COMPARE(gl_blend, GL_FALSE)
           {
              _sym_glDisable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");
              current_ctx->_enable_flag1 &= (~FLAG_BIT_0);
              current_ctx->gl_blend = GL_FALSE;
           }
         break;
      case GL_CULL_FACE:
         CURR_STATE_COMPARE(gl_cull_face, GL_FALSE)
           {
              _sym_glDisable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");
              current_ctx->_enable_flag1 &= (~FLAG_BIT_1);
              current_ctx->gl_cull_face = GL_FALSE;
           }
         break;
      case GL_DEPTH_TEST:
         CURR_STATE_COMPARE(gl_depth_test, GL_FALSE)
           {
              _sym_glDisable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");
              current_ctx->_enable_flag1 &= (~FLAG_BIT_2);
              current_ctx->gl_depth_test = GL_FALSE;
           }
         break;
      case GL_DITHER:
         CURR_STATE_COMPARE(gl_dither, GL_FALSE)
           {
              _sym_glDisable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");
              current_ctx->_enable_flag1 &= (~FLAG_BIT_3);
              current_ctx->gl_dither = GL_FALSE;
           }
         break;
      case GL_POLYGON_OFFSET_FILL:
         CURR_STATE_COMPARE(gl_polygon_offset_fill, GL_FALSE)
           {
              _sym_glDisable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");
              current_ctx->_enable_flag2 &= (~FLAG_BIT_0);
              current_ctx->gl_polygon_offset_fill = GL_FALSE;
           }
         break;
      case GL_SAMPLE_ALPHA_TO_COVERAGE:
         CURR_STATE_COMPARE(gl_sample_alpha_to_coverage, GL_FALSE)
           {
              _sym_glDisable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");
              current_ctx->_enable_flag2 &= (~FLAG_BIT_1);
              current_ctx->gl_sample_alpha_to_coverage = GL_FALSE;
           }
         break;
      case GL_SAMPLE_COVERAGE:
         CURR_STATE_COMPARE(gl_sample_coverage, GL_FALSE)
           {
              _sym_glDisable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");
              current_ctx->_enable_flag2 &= (~FLAG_BIT_2);
              current_ctx->gl_sample_coverage = GL_FALSE;
           }
         break;
      case GL_SCISSOR_TEST:
         CURR_STATE_COMPARE(gl_scissor_test, GL_FALSE)
           {
              _sym_glDisable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");
              current_ctx->_enable_flag2 &= (~FLAG_BIT_3);
              current_ctx->gl_scissor_test = GL_FALSE;
           }
         break;
      case GL_STENCIL_TEST:
         CURR_STATE_COMPARE(gl_stencil_test, GL_FALSE)
           {
              _sym_glDisable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");
              current_ctx->_enable_flag2 &= (~FLAG_BIT_4);
              current_ctx->gl_stencil_test = GL_FALSE;
           }
         break;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glDisableVertexAttribArray(GLuint index)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);



   _sym_glDisableVertexAttribArray(index);

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_1;
   current_ctx->vertex_array[index].enabled    = GL_FALSE;
   //current_ctx->vertex_array[index].modified   = GL_FALSE;

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glDrawArrays(mode, first, count);

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glDrawElements(mode, count, type, indices);

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glEnable(GLenum cap)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   switch(cap)
     {
      case GL_BLEND:
         CURR_STATE_COMPARE(gl_blend, GL_TRUE)
           {
              _sym_glEnable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");

              current_ctx->_enable_flag1 |= FLAG_BIT_0;
              current_ctx->gl_blend = GL_TRUE;
           }
         break;
      case GL_CULL_FACE:
         CURR_STATE_COMPARE(gl_cull_face, GL_TRUE)
           {
              _sym_glEnable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");


              current_ctx->_enable_flag1 |= FLAG_BIT_1;
              current_ctx->gl_cull_face = GL_TRUE;
           }
         break;
      case GL_DEPTH_TEST:
         CURR_STATE_COMPARE(gl_depth_test, GL_TRUE)
           {
              _sym_glEnable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");


              current_ctx->_enable_flag1 |= FLAG_BIT_2;
              current_ctx->gl_depth_test = GL_TRUE;
           }
         break;
      case GL_DITHER:
         CURR_STATE_COMPARE(gl_dither, GL_TRUE)
           {
              _sym_glEnable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");

              current_ctx->_enable_flag1 |= FLAG_BIT_3;
              current_ctx->gl_dither = GL_TRUE;
           }
         break;
      case GL_POLYGON_OFFSET_FILL:
         CURR_STATE_COMPARE(gl_polygon_offset_fill, GL_TRUE)
           {
              _sym_glEnable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");


              current_ctx->_enable_flag2 |= FLAG_BIT_0;
              current_ctx->gl_polygon_offset_fill = GL_TRUE;
           }
         break;
      case GL_SAMPLE_ALPHA_TO_COVERAGE:
         CURR_STATE_COMPARE(gl_sample_alpha_to_coverage, GL_TRUE)
           {
              _sym_glEnable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");


              current_ctx->_enable_flag2 |= FLAG_BIT_1;
              current_ctx->gl_sample_alpha_to_coverage = GL_TRUE;
           }
         break;
      case GL_SAMPLE_COVERAGE:
         CURR_STATE_COMPARE(gl_sample_coverage, GL_TRUE)
           {
              _sym_glEnable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");


              current_ctx->_enable_flag2 |= FLAG_BIT_2;
              current_ctx->gl_sample_coverage = GL_TRUE;
           }
         break;
      case GL_SCISSOR_TEST:
         CURR_STATE_COMPARE(gl_scissor_test, GL_TRUE)
           {
              _sym_glEnable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");


              current_ctx->_enable_flag2 |= FLAG_BIT_3;
              current_ctx->gl_scissor_test = GL_TRUE;
           }
         break;
      case GL_STENCIL_TEST:
         CURR_STATE_COMPARE(gl_stencil_test, GL_TRUE)
           {
              _sym_glEnable(cap);
              GLERR(__FUNCTION__, __FILE__, __LINE__, "");


              current_ctx->_enable_flag2 |= FLAG_BIT_4;
              current_ctx->gl_stencil_test = GL_TRUE;
           }
         break;
      default:
         DBG("Unsupported GL Enable Cap: %d", cap);
     }

   EVAS_GL_END_LOG(mc_log);
}

// Optimze?
static void
fpgl_glEnableVertexAttribArray(GLuint index)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glEnableVertexAttribArray(index);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_1;
   current_ctx->vertex_array[index].enabled    = GL_TRUE;
   //current_ctx->vertex_array[index].modified   = GL_FALSE;


   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glFrontFace(GLenum mode)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   CURR_STATE_COMPARE(gl_front_face, mode)
     {
        _sym_glFrontFace(mode);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_misc_flag1 |= FLAG_BIT_0;
        current_ctx->gl_front_face = mode;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glGetVertexAttribfv(index, pname, params);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glGetVertexAttribiv(GLuint index, GLenum pname, GLint* params)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glGetVertexAttribiv(index, pname, params);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glGetVertexAttribPointerv(GLuint index, GLenum pname, void** pointer)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glGetVertexAttribPointerv(index, pname, pointer);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_END_LOG(mc_log);
}

// Fix Maybe?
static void
fpgl_glHint(GLenum target, GLenum mode)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if (target == GL_GENERATE_MIPMAP_HINT)
     {
        CURR_STATE_COMPARE(gl_generate_mipmap_hint, mode)
          {
             _sym_glHint(target, mode);

             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             current_ctx->_tex_flag1 |= FLAG_BIT_2;
             current_ctx->gl_generate_mipmap_hint = mode;
          }
     }
   else
     {
        // For GL Error to be picked up
        _sym_glHint(target, mode);
        GLERR(__FUNCTION__, __FILE__, __LINE__, "");
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glLineWidth(GLfloat width)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   CURR_STATE_COMPARE(gl_line_width, width)
     {
        _sym_glLineWidth(width);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_misc_flag1 |= FLAG_BIT_1;
        current_ctx->gl_line_width = width;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glPixelStorei(GLenum pname, GLint param)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if (pname == GL_PACK_ALIGNMENT)
     {
        CURR_STATE_COMPARE(gl_pack_alignment, param)
          {
             _sym_glPixelStorei(pname, param);

             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             current_ctx->_misc_flag2 |= FLAG_BIT_1;
             current_ctx->gl_pack_alignment = param;
          }
     }
   else if (pname == GL_UNPACK_ALIGNMENT)
     {
        CURR_STATE_COMPARE(gl_unpack_alignment, param)
          {
             _sym_glPixelStorei(pname, param);

             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             current_ctx->_misc_flag2 |= FLAG_BIT_2;
             current_ctx->gl_unpack_alignment = param;
          }
     }
   else
     {
        // For GL Error to be picked up
        _sym_glPixelStorei(pname, param);
        GLERR(__FUNCTION__, __FILE__, __LINE__, "");
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glPolygonOffset(GLfloat factor, GLfloat units)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_polygon_offset_factor != factor) ||
       (current_ctx->gl_polygon_offset_units != units))
     {
        _sym_glPolygonOffset(factor, units);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_misc_flag1 |= (FLAG_BIT_2 | FLAG_BIT_3);
        current_ctx->gl_polygon_offset_factor = factor;
        current_ctx->gl_polygon_offset_units  = units;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glSampleCoverage(GLclampf value, GLboolean invert)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_sample_coverage_value != value) ||
       (current_ctx->gl_sample_coverage_invert != invert))
     {
        _sym_glSampleCoverage(value, invert);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_misc_flag1 |= (FLAG_BIT_4 | FLAG_BIT_5);
        current_ctx->gl_sample_coverage_value  = value;
        current_ctx->gl_sample_coverage_invert = invert;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_scissor_box[0] != x) ||
       (current_ctx->gl_scissor_box[1] != y) ||
       (current_ctx->gl_scissor_box[2] != width) ||
       (current_ctx->gl_scissor_box[3] != height))
     {
        _sym_glScissor(x, y, width, height);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_misc_flag2 |= FLAG_BIT_0;
        current_ctx->gl_scissor_box[0] = x;
        current_ctx->gl_scissor_box[1] = y;
        current_ctx->gl_scissor_box[2] = width;
        current_ctx->gl_scissor_box[3] = height;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_stencil_func != func) ||
       (current_ctx->gl_stencil_ref != ref) ||
       (current_ctx->gl_stencil_value_mask != mask) ||
       (current_ctx->gl_stencil_back_func != func) ||
       (current_ctx->gl_stencil_back_ref != ref) ||
       (current_ctx->gl_stencil_back_value_mask != mask))
     {
        _sym_glStencilFunc(func, ref, mask);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_stencil_flag1 |= (FLAG_BIT_0 | FLAG_BIT_1 | FLAG_BIT_2);
        current_ctx->gl_stencil_func             = func;
        current_ctx->gl_stencil_ref              = ref;
        current_ctx->gl_stencil_value_mask       = mask;

        current_ctx->_stencil_flag2 |= (FLAG_BIT_0 | FLAG_BIT_1 | FLAG_BIT_2);
        current_ctx->gl_stencil_back_func        = func;
        current_ctx->gl_stencil_back_ref         = ref;
        current_ctx->gl_stencil_back_value_mask  = mask;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ((face == GL_FRONT) || (face == GL_FRONT_AND_BACK))
     {
        if ( COMPARE_OPT_SKIP
            (current_ctx->gl_stencil_func != func) ||
            (current_ctx->gl_stencil_ref != ref) ||
            (current_ctx->gl_stencil_value_mask != mask))
          {
             _sym_glStencilFuncSeparate(face, func, ref, mask);

             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             current_ctx->_stencil_flag1 |= (FLAG_BIT_0 | FLAG_BIT_1 | FLAG_BIT_2);

             current_ctx->gl_stencil_func             = func;
             current_ctx->gl_stencil_ref              = ref;
             current_ctx->gl_stencil_value_mask       = mask;
          }
     }

   if ((face == GL_BACK) || (face == GL_FRONT_AND_BACK))
     {
        if ( COMPARE_OPT_SKIP
            (current_ctx->gl_stencil_back_func != func) ||
            (current_ctx->gl_stencil_back_ref != ref) ||
            (current_ctx->gl_stencil_back_value_mask != mask))
          {
             _sym_glStencilFuncSeparate(face, func, ref, mask);

             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             current_ctx->_stencil_flag2 |= (FLAG_BIT_0 | FLAG_BIT_1 | FLAG_BIT_2);

             current_ctx->gl_stencil_back_func        = func;
             current_ctx->gl_stencil_back_ref         = ref;
             current_ctx->gl_stencil_back_value_mask  = mask;
          }
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glStencilMask(GLuint mask)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_stencil_writemask != mask) ||
       (current_ctx->gl_stencil_back_writemask != mask))
     {
        _sym_glStencilMask(mask);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_stencil_flag1 |= (FLAG_BIT_6);
        current_ctx->_stencil_flag2 |= (FLAG_BIT_6);

        current_ctx->gl_stencil_writemask        = mask;
        current_ctx->gl_stencil_back_writemask   = mask;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glStencilMaskSeparate(GLenum face, GLuint mask)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ((face == GL_FRONT) || (face == GL_FRONT_AND_BACK))
     {
        if ( COMPARE_OPT_SKIP
           current_ctx->gl_stencil_writemask != mask)
          {
             _sym_glStencilMaskSeparate(face, mask);

             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             current_ctx->_stencil_flag1 |= (FLAG_BIT_6);
             current_ctx->gl_stencil_writemask = mask;
          }
     }

   if ((face == GL_BACK) || (face == GL_FRONT_AND_BACK))
     {
        if ( COMPARE_OPT_SKIP
            current_ctx->gl_stencil_back_writemask != mask)
          {
             _sym_glStencilMaskSeparate(face, mask);

             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             current_ctx->_stencil_flag2 |= (FLAG_BIT_6);
             current_ctx->gl_stencil_back_writemask   = mask;
          }
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_stencil_fail != fail) ||
       (current_ctx->gl_stencil_pass_depth_fail != zfail) ||
       (current_ctx->gl_stencil_pass_depth_pass != zpass) ||
       (current_ctx->gl_stencil_back_fail != fail) ||
       (current_ctx->gl_stencil_back_pass_depth_fail != zfail) ||
       (current_ctx->gl_stencil_back_pass_depth_pass != zpass))
     {
        _sym_glStencilOp(fail, zfail, zpass);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_stencil_flag1 |= (FLAG_BIT_3 | FLAG_BIT_4 | FLAG_BIT_5);
        current_ctx->gl_stencil_fail              = fail;
        current_ctx->gl_stencil_pass_depth_fail   = zfail;
        current_ctx->gl_stencil_pass_depth_pass   = zpass;

        current_ctx->_stencil_flag2 |= (FLAG_BIT_3 | FLAG_BIT_4 | FLAG_BIT_5);
        current_ctx->gl_stencil_back_fail         = fail;
        current_ctx->gl_stencil_back_pass_depth_fail   = zfail;
        current_ctx->gl_stencil_back_pass_depth_pass   = zpass;
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);



   if ((face == GL_FRONT) || (face == GL_FRONT_AND_BACK))
     {
        if ( COMPARE_OPT_SKIP
            (current_ctx->gl_stencil_fail != fail) ||
            (current_ctx->gl_stencil_pass_depth_fail != zfail) ||
            (current_ctx->gl_stencil_pass_depth_pass != zpass))
          {
             _sym_glStencilOpSeparate(face, fail, zfail, zpass);
             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             current_ctx->_stencil_flag1 |= (FLAG_BIT_3 | FLAG_BIT_4 | FLAG_BIT_5);
             current_ctx->gl_stencil_fail              = fail;
             current_ctx->gl_stencil_pass_depth_fail   = zfail;
             current_ctx->gl_stencil_pass_depth_pass   = zpass;
          }
     }

   if ((face == GL_BACK) || (face == GL_FRONT_AND_BACK))
     {
        if ( COMPARE_OPT_SKIP
            (current_ctx->gl_stencil_back_fail != fail) ||
            (current_ctx->gl_stencil_back_pass_depth_fail != zfail) ||
            (current_ctx->gl_stencil_back_pass_depth_pass != zpass))
          {
             _sym_glStencilOpSeparate(face, fail, zfail, zpass);
             GLERR(__FUNCTION__, __FILE__, __LINE__, "");

             current_ctx->_stencil_flag2 |= (FLAG_BIT_3 | FLAG_BIT_4 | FLAG_BIT_5);
             current_ctx->gl_stencil_back_fail         = fail;
             current_ctx->gl_stencil_back_pass_depth_fail   = zfail;
             current_ctx->gl_stencil_back_pass_depth_pass   = zpass;
          }
     }
   else
     {
        // For picking up error purpose
        _sym_glStencilOpSeparate(face, fail, zfail, zpass);
        GLERR(__FUNCTION__, __FILE__, __LINE__, "");
     }

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glUseProgram(GLuint program)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   CURR_STATE_COMPARE(gl_current_program, program)
     {
        _sym_glUseProgram(program);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_clear_flag1 |= FLAG_BIT_1;
        current_ctx->gl_current_program = program;
     }

   EVAS_GL_END_LOG(mc_log);
}

// Optmize?
static void
fpgl_glVertexAttrib1f(GLuint index, GLfloat x)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glVertexAttrib1f(index, x);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_0;
   current_ctx->vertex_attrib[index].modified = GL_TRUE;
   current_ctx->vertex_attrib[index].value[0] = x;
   current_ctx->vertex_attrib[index].value[1] = 0;
   current_ctx->vertex_attrib[index].value[2] = 0;
   current_ctx->vertex_attrib[index].value[3] = 1;

   EVAS_GL_END_LOG(mc_log);
}

// Optmize?
static void
fpgl_glVertexAttrib1fv(GLuint index, const GLfloat* values)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glVertexAttrib1fv(index, values);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_0;
   current_ctx->vertex_attrib[index].modified = GL_TRUE;
   current_ctx->vertex_attrib[index].value[0] = values[0];
   current_ctx->vertex_attrib[index].value[1] = 0;
   current_ctx->vertex_attrib[index].value[2] = 0;
   current_ctx->vertex_attrib[index].value[3] = 1;

   EVAS_GL_END_LOG(mc_log);
}

// Optmize?
static void
fpgl_glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glVertexAttrib2f(index, x, y);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_0;
   current_ctx->vertex_attrib[index].modified = GL_TRUE;
   current_ctx->vertex_attrib[index].value[0] = x;
   current_ctx->vertex_attrib[index].value[1] = y;
   current_ctx->vertex_attrib[index].value[2] = 0;
   current_ctx->vertex_attrib[index].value[3] = 1;

   EVAS_GL_END_LOG(mc_log);
}

// Optmize?
static void
fpgl_glVertexAttrib2fv(GLuint index, const GLfloat* values)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glVertexAttrib2fv(index, values);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_0;
   current_ctx->vertex_attrib[index].modified = GL_TRUE;
   current_ctx->vertex_attrib[index].value[0] = values[0];
   current_ctx->vertex_attrib[index].value[1] = values[1];
   current_ctx->vertex_attrib[index].value[2] = 0;
   current_ctx->vertex_attrib[index].value[3] = 1;

   EVAS_GL_END_LOG(mc_log);
}

// Optmize?
static void
fpgl_glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glVertexAttrib3f(index, x, y, z);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_0;
   current_ctx->vertex_attrib[index].modified = GL_TRUE;
   current_ctx->vertex_attrib[index].value[0] = x;
   current_ctx->vertex_attrib[index].value[1] = y;
   current_ctx->vertex_attrib[index].value[2] = z;
   current_ctx->vertex_attrib[index].value[3] = 1;

   EVAS_GL_END_LOG(mc_log);
}

// Optmize?
static void
fpgl_glVertexAttrib3fv(GLuint index, const GLfloat* values)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glVertexAttrib3fv(index, values);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_0;
   current_ctx->vertex_attrib[index].modified = GL_TRUE;
   current_ctx->vertex_attrib[index].value[0] = values[0];
   current_ctx->vertex_attrib[index].value[1] = values[1];
   current_ctx->vertex_attrib[index].value[2] = values[2];
   current_ctx->vertex_attrib[index].value[3] = 1;

   EVAS_GL_END_LOG(mc_log);
}

// Optmize?
static void
fpgl_glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glVertexAttrib4f(index, x, y, z, w);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_0;
   current_ctx->vertex_attrib[index].modified = GL_TRUE;
   current_ctx->vertex_attrib[index].value[0] = x;
   current_ctx->vertex_attrib[index].value[1] = y;
   current_ctx->vertex_attrib[index].value[2] = z;
   current_ctx->vertex_attrib[index].value[3] = w;

   EVAS_GL_END_LOG(mc_log);
}

// Optmize?
static void
fpgl_glVertexAttrib4fv(GLuint index, const GLfloat* values)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glVertexAttrib4fv(index, values);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_0;
   current_ctx->vertex_attrib[index].modified = GL_TRUE;
   current_ctx->vertex_attrib[index].value[0] = values[0];
   current_ctx->vertex_attrib[index].value[1] = values[1];
   current_ctx->vertex_attrib[index].value[2] = values[2];
   current_ctx->vertex_attrib[index].value[3] = values[3];

   EVAS_GL_END_LOG(mc_log);
}


// Optmize?
static void
fpgl_glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* ptr)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   _sym_glVertexAttribPointer(index, size, type, normalized, stride, ptr);

   GLERR(__FUNCTION__, __FILE__, __LINE__, "");

   EVAS_GL_DEBUG_ASSERT(index < MAX_VERTEX_ATTRIBS);

   current_ctx->_vattrib_flag |= FLAG_BIT_1;
   current_ctx->vertex_array[index].modified   = GL_TRUE;
   current_ctx->vertex_array[index].buf_id     = current_ctx->gl_array_buffer_binding;
   current_ctx->vertex_array[index].size       = size;
   current_ctx->vertex_array[index].type       = type;
   current_ctx->vertex_array[index].normalized = normalized;
   current_ctx->vertex_array[index].stride     = stride;
   current_ctx->vertex_array[index].pointer    = ptr;

   EVAS_GL_END_LOG(mc_log);
}

static void
fpgl_glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
   THREAD_CHECK_DEBUG();
   EVAS_GL_START_LOG(mc_log);


   if ( COMPARE_OPT_SKIP
       (current_ctx->gl_viewport[0] != x) ||
       (current_ctx->gl_viewport[1] != y) ||
       (current_ctx->gl_viewport[2] != width) ||
       (current_ctx->gl_viewport[3] != height))
     {
        _sym_glViewport(x, y, width, height);

        GLERR(__FUNCTION__, __FILE__, __LINE__, "");

        current_ctx->_clear_flag1 |= FLAG_BIT_0;
        current_ctx->gl_viewport[0] = x;
        current_ctx->gl_viewport[1] = y;
        current_ctx->gl_viewport[2] = width;
        current_ctx->gl_viewport[3] = height;
     }

   EVAS_GL_END_LOG(mc_log);
}


//----------------------------------------------------------------//
//                                                                //
//                      Load Symbols                              //
//                                                                //
//----------------------------------------------------------------//
static void
sym_missing(void)
{
   ERR("GL symbols missing!\n");
}

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
//------------------------------------------------------//
// EGL
static int
glue_sym_init(void)
{
   //------------------------------------------------//
   // Use gl_lib_handle for finding both GL and GLX symbols
#define FINDSYM(dst, sym, typ) \
   if ((!dst) && (_sym_eglGetProcAddress)) dst = (typeof(dst))_sym_eglGetProcAddress(sym); \
   if (!dst) dst = (typeof(dst))dlsym(egl_lib_handle, sym); \
   if (!dst) DBG("Error loading %s\n", sym);
#define FALLBAK(dst, typ) if (!dst) dst = (typeof(dst))sym_missing;

   FINDSYM(_sym_eglGetProcAddress, "eglGetProcAddress", glsym_func_eng_fn);

   FINDSYM(_sym_eglGetError, "eglGetError", glsym_func_int);
   FINDSYM(_sym_eglGetDisplay, "eglGetDisplay", glsym_func_egldpy);
   FINDSYM(_sym_eglInitialize, "eglInitialize", glsym_func_bool);
   FINDSYM(_sym_eglTerminate, "eglTerminate", glsym_func_bool);
   FINDSYM(_sym_eglChooseConfig, "eglChooseConfig", glsym_func_bool);
   FINDSYM(_sym_eglCreateWindowSurface, "eglCreateWindowSurface", glsym_func_eglsfc);
   FINDSYM(_sym_eglCreatePixmapSurface, "eglCreatePixmapSurface", glsym_func_eglsfc);
   FINDSYM(_sym_eglDestroySurface, "eglDestroySurface", glsym_func_bool);
   FINDSYM(_sym_eglBindAPI, "eglBindAPI", glsym_func_bool);
   FINDSYM(_sym_eglWaitClient, "eglWaitClient", glsym_func_bool);
   FINDSYM(_sym_eglSurfaceAttrib, "eglSurfaceAttrib", glsym_func_bool);
   FINDSYM(_sym_eglBindTexImage, "eglBindTexImage", glsym_func_void);
   FINDSYM(_sym_eglReleaseTexImage, "eglReleaseTexImage", glsym_func_bool);
   FINDSYM(_sym_eglSwapInterval, "eglSwapInterval", glsym_func_bool);
   FINDSYM(_sym_eglCreateContext, "eglCreateContext", glsym_func_eglctx);
   FINDSYM(_sym_eglDestroyContext, "eglDestroyContext", glsym_func_bool);
   FINDSYM(_sym_eglMakeCurrent, "eglMakeCurrent", glsym_func_bool);
   FINDSYM(_sym_eglGetCurrentContext, "eglGetCurrentContext", glsym_func_eglctx);
   FINDSYM(_sym_eglGetCurrentSurface, "eglGetCurrentSurface", glsym_func_eglsfc);
   FINDSYM(_sym_eglGetCurrentDisplay, "eglGetCurrentDisplay", glsym_func_egldpy);
   FINDSYM(_sym_eglWaitGL, "eglWaitGL", glsym_func_bool);
   FINDSYM(_sym_eglWaitNative, "eglWaitNative", glsym_func_bool);
   FINDSYM(_sym_eglSwapBuffers, "eglSwapBuffers", glsym_func_bool);
   FINDSYM(_sym_eglCopyBuffers, "eglCopyBuffers", glsym_func_bool);
   FINDSYM(_sym_eglQueryString, "eglQueryString", glsym_func_char_const_ptr);
   //FINDSYM(_sym_eglGetProcAddress, "eglGetProcAddress", glsym_func_eng_fn);

   //----------------//
   FINDSYM(_sym_eglGetProcAddress, "eglGetProcAddressEXT", glsym_func_eng_fn);
   FINDSYM(_sym_eglGetProcAddress, "eglGetProcAddressARB", glsym_func_eng_fn);
   FINDSYM(_sym_eglGetProcAddress, "eglGetProcAddressKHR", glsym_func_eng_fn);

   FINDSYM(_sym_eglCreateImage, "eglCreateImage", glsym_func_void_ptr);
   FINDSYM(_sym_eglCreateImage, "eglCreateImageEXT", glsym_func_void_ptr);
   FINDSYM(_sym_eglCreateImage, "eglCreateImageARB", glsym_func_void_ptr);
   FINDSYM(_sym_eglCreateImage, "eglCreateImageKHR", glsym_func_void_ptr);

   FINDSYM(_sym_eglDestroyImage, "eglDestroyImage", glsym_func_uint);
   FINDSYM(_sym_eglDestroyImage, "eglDestroyImageEXT", glsym_func_uint);
   FINDSYM(_sym_eglDestroyImage, "eglDestroyImageARB", glsym_func_uint);
   FINDSYM(_sym_eglDestroyImage, "eglDestroyImageKHR", glsym_func_uint);

   FINDSYM(_sym_eglMapImageSEC, "eglMapImageSEC", glsym_func_void_ptr);

   FINDSYM(_sym_eglUnmapImageSEC, "eglUnmapImageSEC", glsym_func_uint);

   FINDSYM(_sym_eglGetImageAttribSEC, "eglGetImageAttribSEC", glsym_func_uint);

   FINDSYM(_sym_glEGLImageTargetTexture2DOES, "glEGLImageTargetTexture2DOES", glsym_func_void);
   FINDSYM(_sym_glEGLImageTargetRenderbufferStorageOES, "glEGLImageTargetRenderbufferStorageOES", glsym_func_void);

#undef FINDSYM
#undef FALLBAK

   return 1;
}


#else
//------------------------------------------------------//
// GLX
static int
glue_sym_init(void)
{
   //------------------------------------------------//
   // Use gl_lib_handle for finding both GL and GLX symbols
#define FINDSYM(dst, sym, typ) \
   if ((!dst) && (_sym_glXGetProcAddress)) dst = (typeof(dst))_sym_glXGetProcAddress(sym); \
   if (!dst) dst = (typeof(dst))dlsym(gl_lib_handle, sym); \
   if (!dst) DBG("Error loading %s\n", sym);
#define FALLBAK(dst, typ) if (!dst) dst = (typeof(dst))sym_missing;

   //------------------------------------------------------//
   // GLX APIs... Only ones that are being used.
   FINDSYM(_sym_glXGetProcAddress, "glXGetProcAddress", glsym_func_eng_fn);
   FINDSYM(_sym_glXGetProcAddress, "glXGetProcAddressEXT", glsym_func_eng_fn);
   FINDSYM(_sym_glXGetProcAddress, "glXGetProcAddressARB", glsym_func_eng_fn);

   // Standard Functions
   FINDSYM(_sym_glXChooseVisual, "glXChooseVisual", glsym_func_xvisinfo_ptr);
   FINDSYM(_sym_glXCreateContext, "glXCreateContext", glsym_func_glxctx);
   FINDSYM(_sym_glXDestroyContext, "glXDestroyContext", glsym_func_void);
   FINDSYM(_sym_glXGetCurrentContext, "glXGetCurrentContext", glsym_func_glxctx);
   FINDSYM(_sym_glXGetCurrentDrawable, "glXGetCurrentDrawable", glsym_func_glxdraw);
   FINDSYM(_sym_glXMakeCurrent, "glXMakeCurrent", glsym_func_bool);
   FINDSYM(_sym_glXSwapBuffers, "glXSwapBuffers", glsym_func_void);
   FINDSYM(_sym_glXWaitX, "glXWaitX", glsym_func_void);
   FINDSYM(_sym_glXWaitGL, "glXWaitGL", glsym_func_void);
   FINDSYM(_sym_glXQueryExtension, "glXQueryExtension", glsym_func_bool);
   FINDSYM(_sym_glXQueryExtensionsString, "glXQueryExtensionsString", glsym_func_const_char_ptr);

   // 1.3 and later
   FINDSYM(_sym_glXChooseFBConfig, "glXChooseFBConfig", glsym_func_glxfbcfg_ptr);
   FINDSYM(_sym_glXGetFBConfigs, "glXGetFBConfigs", glsym_func_glxfbcfg_ptr);
   FINDSYM(_sym_glXGetFBConfigAttrib, "glXGetFBConfigAttrib", glsym_func_int);
   FINDSYM(_sym_glXGetVisualFromFBConfig, "glXGetVisualFromFBConfig", glsym_func_xvisinfo_ptr);
   FINDSYM(_sym_glXDestroyWindow, "glXDestroyWindow", glsym_func_void);
   FINDSYM(_sym_glXMakeContextCurrent, "glXMakeContextCurrent", glsym_func_bool);


   // Extension functions
   FINDSYM(_sym_glXBindTexImage, "glXBindTexImage", glsym_func_void);
   FINDSYM(_sym_glXBindTexImage, "glXBindTexImageEXT", glsym_func_void);
   FINDSYM(_sym_glXBindTexImage, "glXBindTexImageARB", glsym_func_void);

   FINDSYM(_sym_glXReleaseTexImage, "glXReleaseTexImage", glsym_func_void);
   FINDSYM(_sym_glXReleaseTexImage, "glXReleaseTexImageEXT", glsym_func_void);
   FINDSYM(_sym_glXReleaseTexImage, "glXReleaseTexImageARB", glsym_func_void);

   FINDSYM(_sym_glXGetVideoSync, "glXGetVideoSyncSGI", glsym_func_int);

   FINDSYM(_sym_glXWaitVideoSync, "glXWaitVideoSyncSGI", glsym_func_int);

   FINDSYM(_sym_glXCreatePixmap, "glXCreatePixmap", glsym_func_xid);
   FINDSYM(_sym_glXCreatePixmap, "glXCreatePixmapEXT", glsym_func_xid);
   FINDSYM(_sym_glXCreatePixmap, "glXCreatePixmapARB", glsym_func_xid);

   FINDSYM(_sym_glXDestroyPixmap, "glXDestroyPixmap", glsym_func_void);
   FINDSYM(_sym_glXDestroyPixmap, "glXDestroyPixmapEXT", glsym_func_void);
   FINDSYM(_sym_glXDestroyPixmap, "glXDestroyPixmapARB", glsym_func_void);

   FINDSYM(_sym_glXQueryDrawable, "glXQueryDrawable", glsym_func_void);
   FINDSYM(_sym_glXQueryDrawable, "glXQueryDrawableEXT", glsym_func_void);
   FINDSYM(_sym_glXQueryDrawable, "glXQueryDrawableARB", glsym_func_void);

   FINDSYM(_sym_glXSwapIntervalSGI, "glXSwapIntervalMESA", glsym_func_int);
   FINDSYM(_sym_glXSwapIntervalSGI, "glXSwapIntervalSGI", glsym_func_int);

   //----------//
   FINDSYM(_sym_glXSwapIntervalEXT, "glXSwapIntervalEXT", glsym_func_void);


#undef FINDSYM
#undef FALLBAK

   return 1;
}

#endif

static int
gl_sym_init(void)
{

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)

   //------------------------------------------------//
   // Use eglGetProcAddress
#  define FINDSYM(dst, sym, typ) \
   if ((!dst) && (_sym_eglGetProcAddress)) dst = (typeof(dst))_sym_eglGetProcAddress(sym); \
   if (!dst) dst = (typeof(dst))dlsym(gl_lib_handle, sym); \
   if (!dst) DBG("Error loading %s\n", sym);
#  define FALLBAK(dst, typ) \
   if (!dst) \
     { \
        DBG("Symbol not found so falling back."); \
        dst = (typeof(dst))sym_missing; \
     }

   FINDSYM(_sym_glGetProgramBinary, "glGetProgramBinary", glsym_func_void);
   FINDSYM(_sym_glGetProgramBinary, "glGetProgramBinaryEXT", glsym_func_void);
   FINDSYM(_sym_glGetProgramBinary, "glGetProgramBinaryARB", glsym_func_void);
   FINDSYM(_sym_glGetProgramBinary, "glGetProgramBinaryOES", glsym_func_void);
   FINDSYM(_sym_glGetProgramBinary, "glGetProgramBinaryKHR", glsym_func_void);

   FINDSYM(_sym_glProgramBinary, "glProgramBinary", glsym_func_void);
   FINDSYM(_sym_glProgramBinary, "glProgramBinaryEXT", glsym_func_void);
   FINDSYM(_sym_glProgramBinary, "glProgramBinaryARB", glsym_func_void);
   FINDSYM(_sym_glProgramBinary, "glProgramBinaryOES", glsym_func_void);
   FINDSYM(_sym_glProgramBinary, "glProgramBinaryKHR", glsym_func_void);

   FINDSYM(_sym_glProgramParameteri, "glProgramParameteri", glsym_func_void);
   FINDSYM(_sym_glProgramParameteri, "glProgramParameteriEXT", glsym_func_void);
   FINDSYM(_sym_glProgramParameteri, "glProgramParameteriARB", glsym_func_void);
   FINDSYM(_sym_glProgramParameteri, "glProgramParameteriOES", glsym_func_void);
   FINDSYM(_sym_glProgramParameteri, "glProgramParameteriKHR", glsym_func_void);

#else

   //------------------------------------------------//
   // Use gl_lib_handle for finding both GL and GLX symbols
   // Try eglGetProcAddress
#  define FINDSYM(dst, sym, typ) \
   if ((!dst) && (_sym_glXGetProcAddress)) dst = (typeof(dst))_sym_glXGetProcAddress(sym); \
   if (!dst) dst = (typeof(dst))dlsym(gl_lib_handle, sym); \
   if (!dst) DBG("Error loading %s\n", sym);
#  define FALLBAK(dst, typ) \
   if (!dst) \
     { \
        ERR("Symbol not found so falling back."); \
        dst = (typeof(dst))sym_missing; \
        exit(1); \
     }

   //----------//
   FINDSYM(_sym_glGetProgramBinary, "glGetProgramBinary", glsym_func_void);
   FINDSYM(_sym_glGetProgramBinary, "glGetProgramBinaryEXT", glsym_func_void);
   FINDSYM(_sym_glGetProgramBinary, "glGetProgramBinaryARB", glsym_func_void);
   FINDSYM(_sym_glGetProgramBinary, "glGetProgramBinaryOES", glsym_func_void);

   FINDSYM(_sym_glProgramBinary, "glProgramBinary", glsym_func_void);
   FINDSYM(_sym_glProgramBinary, "glProgramBinaryEXT", glsym_func_void);
   FINDSYM(_sym_glProgramBinary, "glProgramBinaryARB", glsym_func_void);

   FINDSYM(_sym_glProgramParameteri, "glProgramParameteri", glsym_func_void);
   FINDSYM(_sym_glProgramParameteri, "glProgramParameteriEXT", glsym_func_void);
   FINDSYM(_sym_glProgramParameteri, "glProgramParameteriARB", glsym_func_void);

#endif

   //------------------------------------------------------//
   // GLES 2.0 APIs...
   FINDSYM(_sym_glActiveTexture, "glActiveTexture", glsym_func_void);
   FALLBAK(_sym_glActiveTexture, glsym_func_void);

   FINDSYM(_sym_glAttachShader, "glAttachShader", glsym_func_void);
   FALLBAK(_sym_glAttachShader, glsym_func_void);

   FINDSYM(_sym_glBindAttribLocation, "glBindAttribLocation", glsym_func_void);
   FALLBAK(_sym_glBindAttribLocation, glsym_func_void);

   FINDSYM(_sym_glBindBuffer, "glBindBuffer", glsym_func_void);
   FALLBAK(_sym_glBindBuffer, glsym_func_void);

   FINDSYM(_sym_glBindFramebuffer, "glBindFramebuffer", glsym_func_void);
   FALLBAK(_sym_glBindFramebuffer, glsym_func_void);

   FINDSYM(_sym_glBindRenderbuffer, "glBindRenderbuffer", glsym_func_void);
   FALLBAK(_sym_glBindRenderbuffer, glsym_func_void);

   FINDSYM(_sym_glBindTexture, "glBindTexture", glsym_func_void);
   FALLBAK(_sym_glBindTexture, glsym_func_void);

   FINDSYM(_sym_glBlendColor, "glBlendColor", glsym_func_void);
   FALLBAK(_sym_glBlendColor, glsym_func_void);

   FINDSYM(_sym_glBlendEquation, "glBlendEquation", glsym_func_void);
   FALLBAK(_sym_glBlendEquation, glsym_func_void);

   FINDSYM(_sym_glBlendEquationSeparate, "glBlendEquationSeparate", glsym_func_void);
   FALLBAK(_sym_glBlendEquationSeparate, glsym_func_void);

   FINDSYM(_sym_glBlendFunc, "glBlendFunc", glsym_func_void);
   FALLBAK(_sym_glBlendFunc, glsym_func_void);

   FINDSYM(_sym_glBlendFuncSeparate, "glBlendFuncSeparate", glsym_func_void);
   FALLBAK(_sym_glBlendFuncSeparate, glsym_func_void);

   FINDSYM(_sym_glBufferData, "glBufferData", glsym_func_void);
   FALLBAK(_sym_glBufferData, glsym_func_void);

   FINDSYM(_sym_glBufferSubData, "glBufferSubData", glsym_func_void);
   FALLBAK(_sym_glBufferSubData, glsym_func_void);

   FINDSYM(_sym_glCheckFramebufferStatus, "glCheckFramebufferStatus", glsym_func_uint);
   FALLBAK(_sym_glCheckFramebufferStatus, glsym_func_uint);

   FINDSYM(_sym_glClear, "glClear", glsym_func_void);
   FALLBAK(_sym_glClear, glsym_func_void);

   FINDSYM(_sym_glClearColor, "glClearColor", glsym_func_void);
   FALLBAK(_sym_glClearColor, glsym_func_void);

   FINDSYM(_sym_glClearDepthf, "glClearDepthf", glsym_func_void);
   FINDSYM(_sym_glClearDepthf, "glClearDepth", glsym_func_void);
   FALLBAK(_sym_glClearDepthf, glsym_func_void);

   FINDSYM(_sym_glClearStencil, "glClearStencil", glsym_func_void);
   FALLBAK(_sym_glClearStencil, glsym_func_void);

   FINDSYM(_sym_glColorMask, "glColorMask", glsym_func_void);
   FALLBAK(_sym_glColorMask, glsym_func_void);

   FINDSYM(_sym_glCompileShader, "glCompileShader", glsym_func_void);
   FALLBAK(_sym_glCompileShader, glsym_func_void);

   FINDSYM(_sym_glCompressedTexImage2D, "glCompressedTexImage2D", glsym_func_void);
   FALLBAK(_sym_glCompressedTexImage2D, glsym_func_void);

   FINDSYM(_sym_glCompressedTexSubImage2D, "glCompressedTexSubImage2D", glsym_func_void);
   FALLBAK(_sym_glCompressedTexSubImage2D, glsym_func_void);

   FINDSYM(_sym_glCopyTexImage2D, "glCopyTexImage2D", glsym_func_void);
   FALLBAK(_sym_glCopyTexImage2D, glsym_func_void);

   FINDSYM(_sym_glCopyTexSubImage2D, "glCopyTexSubImage2D", glsym_func_void);
   FALLBAK(_sym_glCopyTexSubImage2D, glsym_func_void);

   FINDSYM(_sym_glCreateProgram, "glCreateProgram", glsym_func_uint);
   FALLBAK(_sym_glCreateProgram, glsym_func_uint);

   FINDSYM(_sym_glCreateShader, "glCreateShader", glsym_func_uint);
   FALLBAK(_sym_glCreateShader, glsym_func_uint);

   FINDSYM(_sym_glCullFace, "glCullFace", glsym_func_void);
   FALLBAK(_sym_glCullFace, glsym_func_void);

   FINDSYM(_sym_glDeleteBuffers, "glDeleteBuffers", glsym_func_void);
   FALLBAK(_sym_glDeleteBuffers, glsym_func_void);

   FINDSYM(_sym_glDeleteFramebuffers, "glDeleteFramebuffers", glsym_func_void);
   FALLBAK(_sym_glDeleteFramebuffers, glsym_func_void);

   FINDSYM(_sym_glDeleteProgram, "glDeleteProgram", glsym_func_void);
   FALLBAK(_sym_glDeleteProgram, glsym_func_void);

   FINDSYM(_sym_glDeleteRenderbuffers, "glDeleteRenderbuffers", glsym_func_void);
   FALLBAK(_sym_glDeleteRenderbuffers, glsym_func_void);

   FINDSYM(_sym_glDeleteShader, "glDeleteShader", glsym_func_void);
   FALLBAK(_sym_glDeleteShader, glsym_func_void);

   FINDSYM(_sym_glDeleteTextures, "glDeleteTextures", glsym_func_void);
   FALLBAK(_sym_glDeleteTextures, glsym_func_void);

   FINDSYM(_sym_glDepthFunc, "glDepthFunc", glsym_func_void);
   FALLBAK(_sym_glDepthFunc, glsym_func_void);

   FINDSYM(_sym_glDepthMask, "glDepthMask", glsym_func_void);
   FALLBAK(_sym_glDepthMask, glsym_func_void);

   FINDSYM(_sym_glDepthRangef, "glDepthRangef", glsym_func_void);
   FINDSYM(_sym_glDepthRangef, "glDepthRange", glsym_func_void);
   FALLBAK(_sym_glDepthRangef, glsym_func_void);

   FINDSYM(_sym_glDetachShader, "glDetachShader", glsym_func_void);
   FALLBAK(_sym_glDetachShader, glsym_func_void);

   FINDSYM(_sym_glDisable, "glDisable", glsym_func_void);
   FALLBAK(_sym_glDisable, glsym_func_void);

   FINDSYM(_sym_glDisableVertexAttribArray, "glDisableVertexAttribArray", glsym_func_void);
   FALLBAK(_sym_glDisableVertexAttribArray, glsym_func_void);

   FINDSYM(_sym_glDrawArrays, "glDrawArrays", glsym_func_void);
   FALLBAK(_sym_glDrawArrays, glsym_func_void);

   FINDSYM(_sym_glDrawElements, "glDrawElements", glsym_func_void);
   FALLBAK(_sym_glDrawElements, glsym_func_void);

   FINDSYM(_sym_glEnable, "glEnable", glsym_func_void);
   FALLBAK(_sym_glEnable, glsym_func_void);

   FINDSYM(_sym_glEnableVertexAttribArray, "glEnableVertexAttribArray", glsym_func_void);
   FALLBAK(_sym_glEnableVertexAttribArray, glsym_func_void);

   FINDSYM(_sym_glFinish, "glFinish", glsym_func_void);
   FALLBAK(_sym_glFinish, glsym_func_void);

   FINDSYM(_sym_glFlush, "glFlush", glsym_func_void);
   FALLBAK(_sym_glFlush, glsym_func_void);

   FINDSYM(_sym_glFramebufferRenderbuffer, "glFramebufferRenderbuffer", glsym_func_void);
   FALLBAK(_sym_glFramebufferRenderbuffer, glsym_func_void);

   FINDSYM(_sym_glFramebufferTexture2D, "glFramebufferTexture2D", glsym_func_void);
   FALLBAK(_sym_glFramebufferTexture2D, glsym_func_void);

   FINDSYM(_sym_glFrontFace, "glFrontFace", glsym_func_void);
   FALLBAK(_sym_glFrontFace, glsym_func_void);

   FINDSYM(_sym_glGenBuffers, "glGenBuffers", glsym_func_void);
   FALLBAK(_sym_glGenBuffers, glsym_func_void);

   FINDSYM(_sym_glGenerateMipmap, "glGenerateMipmap", glsym_func_void);
   FALLBAK(_sym_glGenerateMipmap, glsym_func_void);

   FINDSYM(_sym_glGenFramebuffers, "glGenFramebuffers", glsym_func_void);
   FALLBAK(_sym_glGenFramebuffers, glsym_func_void);

   FINDSYM(_sym_glGenRenderbuffers, "glGenRenderbuffers", glsym_func_void);
   FALLBAK(_sym_glGenRenderbuffers, glsym_func_void);

   FINDSYM(_sym_glGenTextures, "glGenTextures", glsym_func_void);
   FALLBAK(_sym_glGenTextures, glsym_func_void);

   FINDSYM(_sym_glGetActiveAttrib, "glGetActiveAttrib", glsym_func_void);
   FALLBAK(_sym_glGetActiveAttrib, glsym_func_void);

   FINDSYM(_sym_glGetActiveUniform, "glGetActiveUniform", glsym_func_void);
   FALLBAK(_sym_glGetActiveUniform, glsym_func_void);

   FINDSYM(_sym_glGetAttachedShaders, "glGetAttachedShaders", glsym_func_void);
   FALLBAK(_sym_glGetAttachedShaders, glsym_func_void);

   FINDSYM(_sym_glGetAttribLocation, "glGetAttribLocation", glsym_func_int);
   FALLBAK(_sym_glGetAttribLocation, glsym_func_int);

   FINDSYM(_sym_glGetBooleanv, "glGetBooleanv", glsym_func_void);
   FALLBAK(_sym_glGetBooleanv, glsym_func_void);

   FINDSYM(_sym_glGetBufferParameteriv, "glGetBufferParameteriv", glsym_func_void);
   FALLBAK(_sym_glGetBufferParameteriv, glsym_func_void);

   FINDSYM(_sym_glGetError, "glGetError", glsym_func_uint);
   FALLBAK(_sym_glGetError, glsym_func_uint);

   FINDSYM(_sym_glGetFloatv, "glGetFloatv", glsym_func_void);
   FALLBAK(_sym_glGetFloatv, glsym_func_void);

   FINDSYM(_sym_glGetFramebufferAttachmentParameteriv, "glGetFramebufferAttachmentParameteriv", glsym_func_void);
   FALLBAK(_sym_glGetFramebufferAttachmentParameteriv, glsym_func_void);

   FINDSYM(_sym_glGetIntegerv, "glGetIntegerv", glsym_func_void);
   FALLBAK(_sym_glGetIntegerv, glsym_func_void);

   FINDSYM(_sym_glGetProgramiv, "glGetProgramiv", glsym_func_void);
   FALLBAK(_sym_glGetProgramiv, glsym_func_void);

   FINDSYM(_sym_glGetProgramInfoLog, "glGetProgramInfoLog", glsym_func_void);
   FALLBAK(_sym_glGetProgramInfoLog, glsym_func_void);

   FINDSYM(_sym_glGetRenderbufferParameteriv, "glGetRenderbufferParameteriv", glsym_func_void);
   FALLBAK(_sym_glGetRenderbufferParameteriv, glsym_func_void);

   FINDSYM(_sym_glGetShaderiv, "glGetShaderiv", glsym_func_void);
   FALLBAK(_sym_glGetShaderiv, glsym_func_void);

   FINDSYM(_sym_glGetShaderInfoLog, "glGetShaderInfoLog", glsym_func_void);
   FALLBAK(_sym_glGetShaderInfoLog, glsym_func_void);

   FINDSYM(_sym_glGetShaderPrecisionFormat, "glGetShaderPrecisionFormat", glsym_func_void);
   FALLBAK(_sym_glGetShaderPrecisionFormat, glsym_func_void);

   FINDSYM(_sym_glGetShaderSource, "glGetShaderSource", glsym_func_void);
   FALLBAK(_sym_glGetShaderSource, glsym_func_void);

   FINDSYM(_sym_glGetString, "glGetString", glsym_func_uchar_ptr);
   FALLBAK(_sym_glGetString, glsym_func_const_uchar_ptr);

   FINDSYM(_sym_glGetTexParameterfv, "glGetTexParameterfv", glsym_func_void);
   FALLBAK(_sym_glGetTexParameterfv, glsym_func_void);

   FINDSYM(_sym_glGetTexParameteriv, "glGetTexParameteriv", glsym_func_void);
   FALLBAK(_sym_glGetTexParameteriv, glsym_func_void);

   FINDSYM(_sym_glGetUniformfv, "glGetUniformfv", glsym_func_void);
   FALLBAK(_sym_glGetUniformfv, glsym_func_void);

   FINDSYM(_sym_glGetUniformiv, "glGetUniformiv", glsym_func_void);
   FALLBAK(_sym_glGetUniformiv, glsym_func_void);

   FINDSYM(_sym_glGetUniformLocation, "glGetUniformLocation", glsym_func_int);
   FALLBAK(_sym_glGetUniformLocation, glsym_func_int);

   FINDSYM(_sym_glGetVertexAttribfv, "glGetVertexAttribfv", glsym_func_void);
   FALLBAK(_sym_glGetVertexAttribfv, glsym_func_void);

   FINDSYM(_sym_glGetVertexAttribiv, "glGetVertexAttribiv", glsym_func_void);
   FALLBAK(_sym_glGetVertexAttribiv, glsym_func_void);

   FINDSYM(_sym_glGetVertexAttribPointerv, "glGetVertexAttribPointerv", glsym_func_void);
   FALLBAK(_sym_glGetVertexAttribPointerv, glsym_func_void);

   FINDSYM(_sym_glHint, "glHint", glsym_func_void);
   FALLBAK(_sym_glHint, glsym_func_void);

   FINDSYM(_sym_glIsBuffer, "glIsBuffer", glsym_func_uchar);
   FALLBAK(_sym_glIsBuffer, glsym_func_uchar);

   FINDSYM(_sym_glIsEnabled, "glIsEnabled", glsym_func_uchar);
   FALLBAK(_sym_glIsEnabled, glsym_func_uchar);

   FINDSYM(_sym_glIsFramebuffer, "glIsFramebuffer", glsym_func_uchar);
   FALLBAK(_sym_glIsFramebuffer, glsym_func_uchar);

   FINDSYM(_sym_glIsProgram, "glIsProgram", glsym_func_uchar);
   FALLBAK(_sym_glIsProgram, glsym_func_uchar);

   FINDSYM(_sym_glIsRenderbuffer, "glIsRenderbuffer", glsym_func_uchar);
   FALLBAK(_sym_glIsRenderbuffer, glsym_func_uchar);

   FINDSYM(_sym_glIsShader, "glIsShader", glsym_func_uchar);
   FALLBAK(_sym_glIsShader, glsym_func_uchar);

   FINDSYM(_sym_glIsTexture, "glIsTexture", glsym_func_uchar);
   FALLBAK(_sym_glIsTexture, glsym_func_uchar);

   FINDSYM(_sym_glLineWidth, "glLineWidth", glsym_func_void);
   FALLBAK(_sym_glLineWidth, glsym_func_void);

   FINDSYM(_sym_glLinkProgram, "glLinkProgram", glsym_func_void);
   FALLBAK(_sym_glLinkProgram, glsym_func_void);

   FINDSYM(_sym_glPixelStorei, "glPixelStorei", glsym_func_void);
   FALLBAK(_sym_glPixelStorei, glsym_func_void);

   FINDSYM(_sym_glPolygonOffset, "glPolygonOffset", glsym_func_void);
   FALLBAK(_sym_glPolygonOffset, glsym_func_void);

   FINDSYM(_sym_glReadPixels, "glReadPixels", glsym_func_void);
   FALLBAK(_sym_glReadPixels, glsym_func_void);

   FINDSYM(_sym_glReleaseShaderCompiler, "glReleaseShaderCompiler", glsym_func_void);
   FALLBAK(_sym_glReleaseShaderCompiler, glsym_func_void);

   FINDSYM(_sym_glRenderbufferStorage, "glRenderbufferStorage", glsym_func_void);
   FALLBAK(_sym_glRenderbufferStorage, glsym_func_void);

   FINDSYM(_sym_glSampleCoverage, "glSampleCoverage", glsym_func_void);
   FALLBAK(_sym_glSampleCoverage, glsym_func_void);

   FINDSYM(_sym_glScissor, "glScissor", glsym_func_void);
   FALLBAK(_sym_glScissor, glsym_func_void);

   FINDSYM(_sym_glShaderBinary, "glShaderBinary", glsym_func_void);
   FALLBAK(_sym_glShaderBinary, glsym_func_void);

   FINDSYM(_sym_glShaderSource, "glShaderSource", glsym_func_void);
   FALLBAK(_sym_glShaderSource, glsym_func_void);

   FINDSYM(_sym_glStencilFunc, "glStencilFunc", glsym_func_void);
   FALLBAK(_sym_glStencilFunc, glsym_func_void);

   FINDSYM(_sym_glStencilFuncSeparate, "glStencilFuncSeparate", glsym_func_void);
   FALLBAK(_sym_glStencilFuncSeparate, glsym_func_void);

   FINDSYM(_sym_glStencilMask, "glStencilMask", glsym_func_void);
   FALLBAK(_sym_glStencilMask, glsym_func_void);

   FINDSYM(_sym_glStencilMaskSeparate, "glStencilMaskSeparate", glsym_func_void);
   FALLBAK(_sym_glStencilMaskSeparate, glsym_func_void);

   FINDSYM(_sym_glStencilOp, "glStencilOp", glsym_func_void);
   FALLBAK(_sym_glStencilOp, glsym_func_void);

   FINDSYM(_sym_glStencilOpSeparate, "glStencilOpSeparate", glsym_func_void);
   FALLBAK(_sym_glStencilOpSeparate, glsym_func_void);

   FINDSYM(_sym_glTexImage2D, "glTexImage2D", glsym_func_void);
   FALLBAK(_sym_glTexImage2D, glsym_func_void);

   FINDSYM(_sym_glTexParameterf, "glTexParameterf", glsym_func_void);
   FALLBAK(_sym_glTexParameterf, glsym_func_void);

   FINDSYM(_sym_glTexParameterfv, "glTexParameterfv", glsym_func_void);
   FALLBAK(_sym_glTexParameterfv, glsym_func_void);

   FINDSYM(_sym_glTexParameteri, "glTexParameteri", glsym_func_void);
   FALLBAK(_sym_glTexParameteri, glsym_func_void);

   FINDSYM(_sym_glTexParameteriv, "glTexParameteriv", glsym_func_void);
   FALLBAK(_sym_glTexParameteriv, glsym_func_void);

   FINDSYM(_sym_glTexSubImage2D, "glTexSubImage2D", glsym_func_void);
   FALLBAK(_sym_glTexSubImage2D, glsym_func_void);

   FINDSYM(_sym_glUniform1f, "glUniform1f", glsym_func_void);
   FALLBAK(_sym_glUniform1f, glsym_func_void);

   FINDSYM(_sym_glUniform1fv, "glUniform1fv", glsym_func_void);
   FALLBAK(_sym_glUniform1fv, glsym_func_void);

   FINDSYM(_sym_glUniform1i, "glUniform1i", glsym_func_void);
   FALLBAK(_sym_glUniform1i, glsym_func_void);

   FINDSYM(_sym_glUniform1iv, "glUniform1iv", glsym_func_void);
   FALLBAK(_sym_glUniform1iv, glsym_func_void);

   FINDSYM(_sym_glUniform2f, "glUniform2f", glsym_func_void);
   FALLBAK(_sym_glUniform2f, glsym_func_void);

   FINDSYM(_sym_glUniform2fv, "glUniform2fv", glsym_func_void);
   FALLBAK(_sym_glUniform2fv, glsym_func_void);

   FINDSYM(_sym_glUniform2i, "glUniform2i", glsym_func_void);
   FALLBAK(_sym_glUniform2i, glsym_func_void);

   FINDSYM(_sym_glUniform2iv, "glUniform2iv", glsym_func_void);
   FALLBAK(_sym_glUniform2iv, glsym_func_void);

   FINDSYM(_sym_glUniform3f, "glUniform3f", glsym_func_void);
   FALLBAK(_sym_glUniform3f, glsym_func_void);

   FINDSYM(_sym_glUniform3fv, "glUniform3fv", glsym_func_void);
   FALLBAK(_sym_glUniform3fv, glsym_func_void);

   FINDSYM(_sym_glUniform3i, "glUniform3i", glsym_func_void);
   FALLBAK(_sym_glUniform3i, glsym_func_void);

   FINDSYM(_sym_glUniform3iv, "glUniform3iv", glsym_func_void);
   FALLBAK(_sym_glUniform3iv, glsym_func_void);

   FINDSYM(_sym_glUniform4f, "glUniform4f", glsym_func_void);
   FALLBAK(_sym_glUniform4f, glsym_func_void);

   FINDSYM(_sym_glUniform4fv, "glUniform4fv", glsym_func_void);
   FALLBAK(_sym_glUniform4fv, glsym_func_void);

   FINDSYM(_sym_glUniform4i, "glUniform4i", glsym_func_void);
   FALLBAK(_sym_glUniform4i, glsym_func_void);

   FINDSYM(_sym_glUniform4iv, "glUniform4iv", glsym_func_void);
   FALLBAK(_sym_glUniform4iv, glsym_func_void);

   FINDSYM(_sym_glUniformMatrix2fv, "glUniformMatrix2fv", glsym_func_void);
   FALLBAK(_sym_glUniformMatrix2fv, glsym_func_void);

   FINDSYM(_sym_glUniformMatrix3fv, "glUniformMatrix3fv", glsym_func_void);
   FALLBAK(_sym_glUniformMatrix3fv, glsym_func_void);

   FINDSYM(_sym_glUniformMatrix4fv, "glUniformMatrix4fv", glsym_func_void);
   FALLBAK(_sym_glUniformMatrix4fv, glsym_func_void);

   FINDSYM(_sym_glUseProgram, "glUseProgram", glsym_func_void);
   FALLBAK(_sym_glUseProgram, glsym_func_void);

   FINDSYM(_sym_glValidateProgram, "glValidateProgram", glsym_func_void);
   FALLBAK(_sym_glValidateProgram, glsym_func_void);

   FINDSYM(_sym_glVertexAttrib1f, "glVertexAttrib1f", glsym_func_void);
   FALLBAK(_sym_glVertexAttrib1f, glsym_func_void);

   FINDSYM(_sym_glVertexAttrib1fv, "glVertexAttrib1fv", glsym_func_void);
   FALLBAK(_sym_glVertexAttrib1fv, glsym_func_void);

   FINDSYM(_sym_glVertexAttrib2f, "glVertexAttrib2f", glsym_func_void);
   FALLBAK(_sym_glVertexAttrib2f, glsym_func_void);

   FINDSYM(_sym_glVertexAttrib2fv, "glVertexAttrib2fv", glsym_func_void);
   FALLBAK(_sym_glVertexAttrib2fv, glsym_func_void);

   FINDSYM(_sym_glVertexAttrib3f, "glVertexAttrib3f", glsym_func_void);
   FALLBAK(_sym_glVertexAttrib3f, glsym_func_void);

   FINDSYM(_sym_glVertexAttrib3fv, "glVertexAttrib3fv", glsym_func_void);
   FALLBAK(_sym_glVertexAttrib3fv, glsym_func_void);

   FINDSYM(_sym_glVertexAttrib4f, "glVertexAttrib4f", glsym_func_void);
   FALLBAK(_sym_glVertexAttrib4f, glsym_func_void);

   FINDSYM(_sym_glVertexAttrib4fv, "glVertexAttrib4fv", glsym_func_void);
   FALLBAK(_sym_glVertexAttrib4fv, glsym_func_void);

   FINDSYM(_sym_glVertexAttribPointer, "glVertexAttribPointer", glsym_func_void);
   FALLBAK(_sym_glVertexAttribPointer, glsym_func_void);

   FINDSYM(_sym_glViewport, "glViewport", glsym_func_void);
   FALLBAK(_sym_glViewport, glsym_func_void);

#undef FINDSYM
#undef FALLBAK

   return 1;
}

static int
gl_lib_init(void)
{

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
   //------------------------------------------------//
   // Open EGL Library as EGL is separate
   egl_lib_handle = dlopen("libEGL.so.1", RTLD_NOW|RTLD_GLOBAL);
   if (!egl_lib_handle)
      egl_lib_handle = dlopen("libEGL.so", RTLD_NOW|RTLD_GLOBAL);
   if (!egl_lib_handle)
     {
        ERR("%s\n", dlerror());
        return 0;
     }

   // use gl_lib handle for GL symbols
   gl_lib_handle = dlopen("libGLESv2.so.1", RTLD_NOW);
   if (!gl_lib_handle)
      gl_lib_handle = dlopen("libGLESv2.so", RTLD_NOW);
   if (!gl_lib_handle)
     {
        ERR("%s\n", dlerror());
        return 0;
     }
   //------------------------------------------------//

#else // GLX


   // use gl_lib handle for both GLX and GL symbols
   //gl_lib_handle = dlopen("/usr/lib/libGL.so", RTLD_NOW);
   gl_lib_handle = dlopen("libGL.so.1", RTLD_NOW);
   if (!gl_lib_handle)
      gl_lib_handle = dlopen("libGL.so", RTLD_NOW);
   if (!gl_lib_handle)
     {
        ERR("%s\n", dlerror());
        return 0;
     }

   //------------------------------------------------//

#endif // defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)

   if (!glue_sym_init()) return 0;
   if (!gl_sym_init()) return 0;

   return 1;
}


//----------------------------------------------------------------//
//                      Override Functions                        //
//----------------------------------------------------------------//
#define EVASGLUE_API_OVERRIDE(func, api_pre, prefix) \
   api_pre##func = prefix##func

#define EVASGL_API_OVERRIDE(func, api, prefix) \
     (api)->func = prefix##func

static void
override_glue_normal_path()
{
#ifdef EVAS_GL_NAME_MANGLE
#  define N_ORD(f) EVASGLUE_API_OVERRIDE(f, glsym_, _sym_) // GL Normal Path
#  define W_ORD(f) EVASGLUE_API_OVERRIDE(f, glsym_, evgl_) // GL Wrapped Path
#else
#  define N_ORD(f) EVASGLUE_API_OVERRIDE(f,, _sym_) // GL Normal Path
#  define W_ORD(f) EVASGLUE_API_OVERRIDE(f,, evgl_) // GL Wrapped Path
#endif

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
   N_ORD(eglGetProcAddress);
   N_ORD(eglGetError);
   N_ORD(eglGetDisplay);
   N_ORD(eglInitialize);
   N_ORD(eglTerminate);
   N_ORD(eglChooseConfig);
   N_ORD(eglCreateWindowSurface);
   N_ORD(eglCreatePixmapSurface);
   N_ORD(eglDestroySurface);
   N_ORD(eglBindAPI);
   N_ORD(eglWaitClient);
   N_ORD(eglSurfaceAttrib);
   N_ORD(eglBindTexImage);
   N_ORD(eglReleaseTexImage);
   N_ORD(eglSwapInterval);
   N_ORD(eglCreateContext);
   N_ORD(eglDestroyContext);
   N_ORD(eglGetCurrentContext);
   N_ORD(eglGetCurrentSurface);
   N_ORD(eglGetCurrentDisplay);
   N_ORD(eglWaitGL);
   N_ORD(eglWaitNative);
   N_ORD(eglSwapBuffers);
   N_ORD(eglCopyBuffers);
   N_ORD(eglQueryString);

   // Extensions
   N_ORD(eglCreateImage);
   N_ORD(eglDestroyImage);
   N_ORD(eglMapImageSEC);
   N_ORD(eglUnmapImageSEC);
   N_ORD(eglGetImageAttribSEC);
   N_ORD(glEGLImageTargetTexture2DOES);
   N_ORD(glEGLImageTargetRenderbufferStorageOES);

   // Wrapped functions for evasgl specific purpose
   W_ORD(eglMakeCurrent);
#else
   N_ORD(glXGetProcAddress);
   N_ORD(glXChooseVisual);
   N_ORD(glXCreateContext);
   N_ORD(glXDestroyContext);
   N_ORD(glXGetCurrentContext);
   N_ORD(glXGetCurrentDrawable);
   N_ORD(glXSwapBuffers);
   N_ORD(glXWaitX);
   N_ORD(glXWaitGL);
   N_ORD(glXQueryExtension);
   N_ORD(glXQueryExtensionsString);
   N_ORD(glXChooseFBConfig);
   N_ORD(glXGetFBConfigs);
   N_ORD(glXGetFBConfigAttrib);
   N_ORD(glXGetVisualFromFBConfig);
   N_ORD(glXDestroyWindow);
   N_ORD(glXMakeContextCurrent);
   N_ORD(glXBindTexImage);
   N_ORD(glXReleaseTexImage);
   N_ORD(glXGetVideoSync);
   N_ORD(glXWaitVideoSync);
   N_ORD(glXCreatePixmap);
   N_ORD(glXDestroyPixmap);
   N_ORD(glXQueryDrawable);
   N_ORD(glXSwapIntervalSGI);
   N_ORD(glXSwapIntervalEXT);

   // Wrapped functions for evasgl specific purpose
   W_ORD(glXMakeCurrent);
#endif

#undef N_ORD
#undef W_ORD
}


static void
override_glue_wrapped_path()
{

#ifdef EVAS_GL_NAME_MANGLE
#  define W_ORD(f) EVASGLUE_API_OVERRIDE(f, glsym_, evgl_) // GL Wrapped Path
#else
#  define W_ORD(f) EVASGLUE_API_OVERRIDE(f,, evgl_) // GL Wrapped Path
#endif

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
   W_ORD(eglGetProcAddress);
   W_ORD(eglGetError);
   W_ORD(eglGetDisplay);
   W_ORD(eglInitialize);
   W_ORD(eglTerminate);
   W_ORD(eglChooseConfig);
   W_ORD(eglCreateWindowSurface);
   W_ORD(eglCreatePixmapSurface);
   W_ORD(eglDestroySurface);
   W_ORD(eglBindAPI);
   W_ORD(eglWaitClient);
   W_ORD(eglSurfaceAttrib);
   W_ORD(eglBindTexImage);
   W_ORD(eglReleaseTexImage);
   W_ORD(eglSwapInterval);
   W_ORD(eglCreateContext);
   W_ORD(eglDestroyContext);
   W_ORD(eglGetCurrentContext);
   W_ORD(eglGetCurrentSurface);
   W_ORD(eglGetCurrentDisplay);
   W_ORD(eglWaitGL);
   W_ORD(eglWaitNative);
   W_ORD(eglSwapBuffers);
   W_ORD(eglCopyBuffers);
   W_ORD(eglQueryString);

   // Extensions
   W_ORD(eglCreateImage);
   W_ORD(eglDestroyImage);
   W_ORD(eglMapImageSEC);
   W_ORD(eglUnmapImageSEC);
   W_ORD(eglGetImageAttribSEC);
   W_ORD(glEGLImageTargetTexture2DOES);
   W_ORD(glEGLImageTargetRenderbufferStorageOES);

   // Wrapped functions for evasgl specific purpose
   W_ORD(eglMakeCurrent);
#else
   W_ORD(glXGetProcAddress);
   W_ORD(glXChooseVisual);
   W_ORD(glXCreateContext);
   W_ORD(glXDestroyContext);
   W_ORD(glXGetCurrentContext);
   W_ORD(glXGetCurrentDrawable);
   W_ORD(glXSwapBuffers);
   W_ORD(glXWaitX);
   W_ORD(glXWaitGL);
   W_ORD(glXQueryExtension);
   W_ORD(glXQueryExtensionsString);
   W_ORD(glXChooseFBConfig);
   W_ORD(glXGetFBConfigs);
   W_ORD(glXGetFBConfigAttrib);
   W_ORD(glXGetVisualFromFBConfig);
   W_ORD(glXDestroyWindow);
   W_ORD(glXMakeContextCurrent);
   W_ORD(glXBindTexImage);
   W_ORD(glXReleaseTexImage);
   W_ORD(glXGetVideoSync);
   W_ORD(glXWaitVideoSync);
   W_ORD(glXCreatePixmap);
   W_ORD(glXDestroyPixmap);
   W_ORD(glXQueryDrawable);
   W_ORD(glXSwapIntervalSGI);
   W_ORD(glXSwapIntervalEXT);

   // Wrapped functions for evasgl specific purpose
   W_ORD(glXMakeCurrent);
#endif

#undef W_ORD
}

static void
override_glue_fast_path()
{
   // Inherit from wrapped path
   override_glue_wrapped_path();

#ifdef EVAS_GL_NAME_MANGLE
#  define F_ORD(f) EVASGLUE_API_OVERRIDE(f, glsym_, fpgl_) // GL Fast Path
#else
#  define F_ORD(f) EVASGLUE_API_OVERRIDE(f,, fpgl_) // GL Fast Path
#endif

#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
   // Fastpath-ed Functions
   F_ORD(eglCreateContext);
   F_ORD(eglDestroyContext);
   F_ORD(eglDestroySurface);
   F_ORD(eglMakeCurrent);
   F_ORD(eglGetCurrentContext);
   F_ORD(eglGetCurrentSurface);

#else
   // Fastpath-ed Functions
   F_ORD(glXCreateContext);
   F_ORD(glXDestroyContext);
   F_ORD(glXMakeCurrent);
   F_ORD(glXGetCurrentContext);
   F_ORD(glXGetCurrentDrawable);

   F_ORD(glXMakeContextCurrent);
#endif

#undef F_ORD
}

static void
override_gl_normal_path()
{
#ifdef EVAS_GL_NAME_MANGLE
#  define N_ORD(f) EVASGLUE_API_OVERRIDE(f, glsym_, _sym_) // GL Normal Path
#  define W_ORD(f) EVASGLUE_API_OVERRIDE(f, glsym_, evgl_) // GL Wrapped Path
#else
#  define N_ORD(f) EVASGLUE_API_OVERRIDE(f,, _sym_) // GL Normal Path
#  define W_ORD(f) EVASGLUE_API_OVERRIDE(f,, evgl_) // GL Wrapped Path
#endif
   N_ORD(glAttachShader);
   N_ORD(glBindAttribLocation);
   N_ORD(glBufferData);
   N_ORD(glBufferSubData);
   N_ORD(glCheckFramebufferStatus);
   N_ORD(glClear);
   N_ORD(glCompileShader);
   N_ORD(glCompressedTexImage2D);
   N_ORD(glCompressedTexSubImage2D);
   N_ORD(glCopyTexImage2D);
   N_ORD(glCopyTexSubImage2D);
   N_ORD(glCreateProgram);
   N_ORD(glCreateShader);
   N_ORD(glDeleteBuffers);
   N_ORD(glDeleteFramebuffers);
   N_ORD(glDeleteProgram);
   N_ORD(glDeleteRenderbuffers);
   N_ORD(glDeleteShader);
   N_ORD(glDeleteTextures);
   N_ORD(glFinish);
   N_ORD(glFlush);
   N_ORD(glFramebufferRenderbuffer);
   N_ORD(glFramebufferTexture2D);
   N_ORD(glGenBuffers);
   N_ORD(glGenerateMipmap);
   N_ORD(glGenFramebuffers);
   N_ORD(glGenRenderbuffers);
   N_ORD(glGenTextures);
   N_ORD(glGetActiveAttrib);
   N_ORD(glGetActiveUniform);
   N_ORD(glGetAttachedShaders);
   N_ORD(glGetAttribLocation);
   N_ORD(glGetBooleanv);
   N_ORD(glGetBufferParameteriv);
   N_ORD(glGetError);
   N_ORD(glGetFloatv);
   N_ORD(glGetFramebufferAttachmentParameteriv);
   N_ORD(glGetIntegerv);
   N_ORD(glGetProgramiv);
   N_ORD(glGetProgramInfoLog);
   N_ORD(glGetRenderbufferParameteriv);
   N_ORD(glGetShaderiv);
   N_ORD(glGetShaderInfoLog);
   N_ORD(glGetShaderSource);
   N_ORD(glGetString);
   N_ORD(glGetTexParameterfv);
   N_ORD(glGetTexParameteriv);
   N_ORD(glGetUniformfv);
   N_ORD(glGetUniformiv);
   N_ORD(glGetUniformLocation);
   N_ORD(glIsBuffer);
   N_ORD(glIsEnabled);
   N_ORD(glIsFramebuffer);
   N_ORD(glIsProgram);
   N_ORD(glIsRenderbuffer);
   N_ORD(glIsShader);
   N_ORD(glIsTexture);
   N_ORD(glLineWidth);
   N_ORD(glLinkProgram);
   N_ORD(glReadPixels);
   N_ORD(glRenderbufferStorage);
   N_ORD(glShaderSource);
   N_ORD(glTexImage2D);
   N_ORD(glTexParameterf);
   N_ORD(glTexParameterfv);
   N_ORD(glTexParameteri);
   N_ORD(glTexParameteriv);
   N_ORD(glTexSubImage2D);
   N_ORD(glUniform1f);
   N_ORD(glUniform1fv);
   N_ORD(glUniform1i);
   N_ORD(glUniform1iv);
   N_ORD(glUniform2f);
   N_ORD(glUniform2fv);
   N_ORD(glUniform2i);
   N_ORD(glUniform2iv);
   N_ORD(glUniform3f);
   N_ORD(glUniform3fv);
   N_ORD(glUniform3i);
   N_ORD(glUniform3iv);
   N_ORD(glUniform4f);
   N_ORD(glUniform4fv);
   N_ORD(glUniform4i);
   N_ORD(glUniform4iv);
   N_ORD(glUniformMatrix2fv);
   N_ORD(glUniformMatrix3fv);
   N_ORD(glUniformMatrix4fv);
   N_ORD(glValidateProgram);

   N_ORD(glActiveTexture);
   N_ORD(glBindBuffer);
   N_ORD(glBindTexture);
   N_ORD(glBlendColor);
   N_ORD(glBlendEquation);
   N_ORD(glBlendEquationSeparate);
   N_ORD(glBlendFunc);
   N_ORD(glBlendFuncSeparate);
   N_ORD(glClearColor);
   N_ORD(glClearDepthf);
   N_ORD(glClearStencil);
   N_ORD(glColorMask);
   N_ORD(glCullFace);
   N_ORD(glDepthFunc);
   N_ORD(glDepthMask);
   N_ORD(glDepthRangef);
   N_ORD(glDetachShader);
   N_ORD(glDisable);
   N_ORD(glDisableVertexAttribArray);
   N_ORD(glDrawArrays);
   N_ORD(glDrawElements);
   N_ORD(glEnable);
   N_ORD(glEnableVertexAttribArray);
   N_ORD(glFrontFace);
   N_ORD(glGetVertexAttribfv);
   N_ORD(glGetVertexAttribiv);
   N_ORD(glGetVertexAttribPointerv);
   N_ORD(glHint);
   N_ORD(glPixelStorei);
   N_ORD(glPolygonOffset);
   N_ORD(glSampleCoverage);
   N_ORD(glScissor);
   N_ORD(glStencilFunc);
   N_ORD(glStencilFuncSeparate);
   N_ORD(glStencilMask);
   N_ORD(glStencilMaskSeparate);
   N_ORD(glStencilOp);
   N_ORD(glStencilOpSeparate);
   N_ORD(glUseProgram);
   N_ORD(glVertexAttrib1f);
   N_ORD(glVertexAttrib1fv);
   N_ORD(glVertexAttrib2f);
   N_ORD(glVertexAttrib2fv);
   N_ORD(glVertexAttrib3f);
   N_ORD(glVertexAttrib3fv);
   N_ORD(glVertexAttrib4f);
   N_ORD(glVertexAttrib4fv);
   N_ORD(glVertexAttribPointer);
   N_ORD(glViewport);

   // Extensions
   N_ORD(glGetProgramBinary);
   N_ORD(glProgramBinary);
   N_ORD(glProgramParameteri);

   //----------------------------------------------------//
   // Functions that need to be overriden for evasgl use
   W_ORD(glBindFramebuffer);
   W_ORD(glBindRenderbuffer);

   // GLES2.0 API compat on top of desktop gl
   W_ORD(glGetShaderPrecisionFormat);
   W_ORD(glReleaseShaderCompiler);
   W_ORD(glShaderBinary);

#undef N_ORD
#undef W_ORD
}

static void
override_gl_wrapped_path()
{

#ifdef EVAS_GL_NAME_MANGLE
#  define N_ORD(f) EVASGLUE_API_OVERRIDE(f, glsym_, _sym_) // GL Normal Path
#  define W_ORD(f) EVASGLUE_API_OVERRIDE(f, glsym_, evgl_) // GL Wrapped Path
#else
#  define N_ORD(f) EVASGLUE_API_OVERRIDE(f,, _sym_) // GL Normal Path
#  define W_ORD(f) EVASGLUE_API_OVERRIDE(f,, evgl_) // GL Wrapped Path
#endif

   W_ORD(glAttachShader);
   W_ORD(glBindAttribLocation);
   W_ORD(glBufferData);
   W_ORD(glBufferSubData);
   W_ORD(glCheckFramebufferStatus);
   W_ORD(glClear);
   W_ORD(glCompileShader);
   W_ORD(glCompressedTexImage2D);
   W_ORD(glCompressedTexSubImage2D);
   W_ORD(glCopyTexImage2D);
   W_ORD(glCopyTexSubImage2D);
   W_ORD(glCreateProgram);
   W_ORD(glCreateShader);
   W_ORD(glDeleteBuffers);
   W_ORD(glDeleteFramebuffers);
   W_ORD(glDeleteProgram);
   W_ORD(glDeleteRenderbuffers);
   W_ORD(glDeleteShader);
   W_ORD(glDeleteTextures);
   W_ORD(glFinish);
   W_ORD(glFlush);
   W_ORD(glFramebufferRenderbuffer);
   W_ORD(glFramebufferTexture2D);
   W_ORD(glGenBuffers);
   W_ORD(glGenerateMipmap);
   W_ORD(glGenFramebuffers);
   W_ORD(glGenRenderbuffers);
   W_ORD(glGenTextures);
   W_ORD(glGetActiveAttrib);
   W_ORD(glGetActiveUniform);
   W_ORD(glGetAttachedShaders);
   W_ORD(glGetAttribLocation);
   W_ORD(glGetBooleanv);
   W_ORD(glGetBufferParameteriv);
   W_ORD(glGetError);
   W_ORD(glGetFloatv);
   W_ORD(glGetFramebufferAttachmentParameteriv);
   W_ORD(glGetIntegerv);
   W_ORD(glGetProgramiv);
   W_ORD(glGetProgramInfoLog);
   W_ORD(glGetRenderbufferParameteriv);
   W_ORD(glGetShaderiv);
   W_ORD(glGetShaderInfoLog);
   W_ORD(glGetShaderSource);
   W_ORD(glGetString);
   W_ORD(glGetTexParameterfv);
   W_ORD(glGetTexParameteriv);
   W_ORD(glGetUniformfv);
   W_ORD(glGetUniformiv);
   W_ORD(glGetUniformLocation);
   W_ORD(glIsBuffer);
   W_ORD(glIsEnabled);
   W_ORD(glIsFramebuffer);
   W_ORD(glIsProgram);
   W_ORD(glIsRenderbuffer);
   W_ORD(glIsShader);
   W_ORD(glIsTexture);
   W_ORD(glLineWidth);
   W_ORD(glLinkProgram);
   W_ORD(glReadPixels);
   W_ORD(glRenderbufferStorage);
   W_ORD(glShaderSource);
   W_ORD(glTexImage2D);
   W_ORD(glTexParameterf);
   W_ORD(glTexParameterfv);
   W_ORD(glTexParameteri);
   W_ORD(glTexParameteriv);
   W_ORD(glTexSubImage2D);
   W_ORD(glUniform1f);
   W_ORD(glUniform1fv);
   W_ORD(glUniform1i);
   W_ORD(glUniform1iv);
   W_ORD(glUniform2f);
   W_ORD(glUniform2fv);
   W_ORD(glUniform2i);
   W_ORD(glUniform2iv);
   W_ORD(glUniform3f);
   W_ORD(glUniform3fv);
   W_ORD(glUniform3i);
   W_ORD(glUniform3iv);
   W_ORD(glUniform4f);
   W_ORD(glUniform4fv);
   W_ORD(glUniform4i);
   W_ORD(glUniform4iv);
   W_ORD(glUniformMatrix2fv);
   W_ORD(glUniformMatrix3fv);
   W_ORD(glUniformMatrix4fv);
   W_ORD(glValidateProgram);

   W_ORD(glActiveTexture);
   W_ORD(glBindBuffer);
   W_ORD(glBindTexture);
   W_ORD(glBlendColor);
   W_ORD(glBlendEquation);
   W_ORD(glBlendEquationSeparate);
   W_ORD(glBlendFunc);
   W_ORD(glBlendFuncSeparate);
   W_ORD(glClearColor);
   W_ORD(glClearDepthf);
   W_ORD(glClearStencil);
   W_ORD(glColorMask);
   W_ORD(glCullFace);
   W_ORD(glDepthFunc);
   W_ORD(glDepthMask);
   W_ORD(glDepthRangef);
   W_ORD(glDetachShader);
   W_ORD(glDisable);
   W_ORD(glDisableVertexAttribArray);
   W_ORD(glDrawArrays);
   W_ORD(glDrawElements);
   W_ORD(glEnable);
   W_ORD(glEnableVertexAttribArray);
   W_ORD(glFrontFace);
   W_ORD(glGetVertexAttribfv);
   W_ORD(glGetVertexAttribiv);
   W_ORD(glGetVertexAttribPointerv);
   W_ORD(glHint);
   W_ORD(glPixelStorei);
   W_ORD(glPolygonOffset);
   W_ORD(glSampleCoverage);
   W_ORD(glScissor);
   W_ORD(glStencilFunc);
   W_ORD(glStencilFuncSeparate);
   W_ORD(glStencilMask);
   W_ORD(glStencilMaskSeparate);
   W_ORD(glStencilOp);
   W_ORD(glStencilOpSeparate);
   W_ORD(glUseProgram);
   W_ORD(glVertexAttrib1f);
   W_ORD(glVertexAttrib1fv);
   W_ORD(glVertexAttrib2f);
   W_ORD(glVertexAttrib2fv);
   W_ORD(glVertexAttrib3f);
   W_ORD(glVertexAttrib3fv);
   W_ORD(glVertexAttrib4f);
   W_ORD(glVertexAttrib4fv);
   W_ORD(glVertexAttribPointer);
   W_ORD(glViewport);

   // Extensions
   W_ORD(glGetProgramBinary);
   W_ORD(glProgramBinary);
   W_ORD(glProgramParameteri);

   //----------------------------------------------------//
   // Functions that need to be overriden for evasgl use
   W_ORD(glBindFramebuffer);
   W_ORD(glBindRenderbuffer);

   // GLES2.0 API compat on top of desktop gl
   W_ORD(glGetShaderPrecisionFormat);
   W_ORD(glReleaseShaderCompiler);
   W_ORD(glShaderBinary);

#undef N_ORD
#undef W_ORD
}

static void
override_gl_fast_path()
{
   // Inherit from wrapped path
   override_gl_wrapped_path();

#ifdef EVAS_GL_NAME_MANGLE
#  define F_ORD(f) EVASGLUE_API_OVERRIDE(f, glsym_, fpgl_) // GL Wrapped Path
#else
#  define F_ORD(f) EVASGLUE_API_OVERRIDE(f,, fpgl_) // GL Wrapped Path
#endif

   // Fast-Path Functions
   F_ORD(glActiveTexture);
   F_ORD(glBindBuffer);
   F_ORD(glBindTexture);
   F_ORD(glBlendColor);
   F_ORD(glBlendEquation);
   F_ORD(glBlendEquationSeparate);
   F_ORD(glBlendFunc);
   F_ORD(glBlendFuncSeparate);
   F_ORD(glClearColor);
   F_ORD(glClearDepthf);
   F_ORD(glClearStencil);
   F_ORD(glColorMask);
   F_ORD(glCullFace);
   F_ORD(glDepthFunc);
   F_ORD(glDepthMask);
   F_ORD(glDepthRangef);
   F_ORD(glDisable);
   F_ORD(glDisableVertexAttribArray);
   F_ORD(glDrawArrays);
   F_ORD(glDrawElements);
   F_ORD(glEnable);
   F_ORD(glEnableVertexAttribArray);
   F_ORD(glFrontFace);
   F_ORD(glGetVertexAttribfv);
   F_ORD(glGetVertexAttribiv);
   F_ORD(glGetVertexAttribPointerv);
   F_ORD(glHint);
   F_ORD(glLineWidth);
   F_ORD(glPixelStorei);
   F_ORD(glPolygonOffset);
   F_ORD(glSampleCoverage);
   F_ORD(glScissor);
   F_ORD(glStencilFunc);
   F_ORD(glStencilFuncSeparate);
   F_ORD(glStencilMask);
   F_ORD(glStencilMaskSeparate);
   F_ORD(glStencilOp);
   F_ORD(glStencilOpSeparate);
   F_ORD(glUseProgram);
   F_ORD(glVertexAttrib1f);
   F_ORD(glVertexAttrib1fv);
   F_ORD(glVertexAttrib2f);
   F_ORD(glVertexAttrib2fv);
   F_ORD(glVertexAttrib3f);
   F_ORD(glVertexAttrib3fv);
   F_ORD(glVertexAttrib4f);
   F_ORD(glVertexAttrib4fv);
   F_ORD(glVertexAttribPointer);
   F_ORD(glViewport);

   // Functions that need to be overriden for evasgl use
   F_ORD(glBindFramebuffer);
   F_ORD(glBindRenderbuffer);

#undef F_ORD
}


static void
override_glue_apis(Evas_GL_Opt_Flag opt)
{
   switch(opt)
     {
      case GL_NORMAL_PATH:
         override_glue_normal_path();
         break;
      case GL_WRAPPED_PATH:
         override_glue_wrapped_path();
         break;
      case GL_FAST_PATH:
         override_glue_fast_path();
         break;
      default:
         ERR("Invalide GL Override Option!!!\n");
     }
}

static void
override_gl_apis(Evas_GL_Opt_Flag opt)
{
   //_gl.version = EVAS_GL_API_VERSION;

   switch(opt)
     {
      case GL_NORMAL_PATH:
         override_gl_normal_path();
         break;
      case GL_WRAPPED_PATH:
         override_gl_wrapped_path();
         break;
      case GL_FAST_PATH:
         override_gl_fast_path();
         break;
      default:
         ERR("Invalide GL Override Option!!!\n");
     }
}


int
init_gl()
{
   char *fp_env;
   int fastpath_opt = 0;
   Evas_GL_Opt_Flag api_opt = GL_NORMAL_PATH;

   fprintf(stderr, "Initializing OpenGL APIs...\n");

   fp_env = getenv("EVAS_GL_FASTPATH");

   if (fp_env) fastpath_opt = atoi(fp_env);
   else fastpath_opt = 0;

   switch(fastpath_opt)
     {
      case 1:
         api_opt = GL_FAST_PATH;
         fprintf(stderr, "API OPT: %d Fastpath enabled...\n", fastpath_opt);
         fprintf(stderr, "#######################################################\n");
         fprintf(stderr, "########### [Evas] Fast Path is enabled ###############\n");
         fprintf(stderr, "#######################################################\n");
         break;
      case 2:
         api_opt = GL_WRAPPED_PATH;
         fprintf(stderr, "API OPT: %d Wrapped API path enabled...\n", fastpath_opt);
         break;
      case 3:
         api_opt = GL_FAST_PATH;
         log_opt = 1;
         fprintf(stderr, "API OPT: %d Fastpath enabled...\n", fastpath_opt);
         fprintf(stderr, "#######################################################\n");
         fprintf(stderr, "########### [Evas] Fast Path is enabled ###############\n");
         fprintf(stderr, "#######################################################\n");
         break;
      case 4:
         api_opt = GL_FAST_PATH;
         log_opt = 2;
         fprintf(stderr, "API OPT: %d Fastpath enabled...\n", fastpath_opt);
         fprintf(stderr, "#######################################################\n");
         fprintf(stderr, "########### [Evas] Fast Path is enabled ###############\n");
         fprintf(stderr, "#######################################################\n");
         break;
      default:
         fprintf(stderr, "API OPT: %d Default API path enabled...\n", fastpath_opt);
         api_opt = GL_NORMAL_PATH;
         break;
     }

   if (!gl_lib_init()) return 0;

   override_glue_apis(api_opt);
   override_gl_apis(api_opt);

   EVAS_GL_INIT_LOG(mc_log);

   return 1;
}

void
free_gl()
{
#if defined (GLES_VARIETY_S3C6410) || defined (GLES_VARIETY_SGX)
   if (global_ctx)
     {
        ERR("Destroying global context...\n");
        _sym_eglMakeCurrent(global_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        _sym_eglDestroyContext(global_dpy, global_ctx);
     }
   if (egl_lib_handle) dlclose(egl_lib_handle);
   if (gl_lib_handle) dlclose (gl_lib_handle);
#else
   if (global_ctx)
     {
        ERR("Destroying global context...\n");
        _sym_glXDestroyContext(global_dpy, global_ctx);
     }
   if (gl_lib_handle) dlclose (gl_lib_handle);
#endif
}

