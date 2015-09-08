/**
 * @file Evas_GL_GLES1_Helpers.h
 * @defgroup Evas_GL_GLES1_Helpers Evas GL GLES1 helpers
 * @ingroup Evas_GL
 */

/**
 * @brief Provides a set of helper functions and macros to use GLES 1.1 with
 * @ref Evas_GL "Evas GL".
 *
 * This file redefines all the OpenGL-ES 1.1 functions as follow:
 * @code
#define glFunction  __evas_gl_glapi->glFunction
 * @endcode
 *
 * Extension functions can then be checked for existence simply by writing:
 * @code
if (glExtensionFunction)
  {
     ...
     glExtensionFunction(...);
     ...
  }
 * @endcode
 *
 * When using Elementary @ref GLView, please include the header file
 * @ref Elementary_GL_Helpers "Elementary_GL_Helpers.h" instead.
 *
 * This header file should be included when using @ref Evas_GL "Evas GL"
 * directly at a low level and with an OpenGL-ES 1.1 context only.
 *
 * @note When this file is included, all @c glFunctions are now macros, which
 *       means that the @ref Evas_GL_API struct can't be used anyore.
 *
 * @see @ref elm_opengl_page
 */
#ifndef _EVAS_GL_GLES1_HELPERS_H
#define _EVAS_GL_GLES1_HELPERS_H

#include <Evas_GL.h>


// local convenience macros

/**
 * @addtogroup Evas_GL_GLES1_Helpers
 * @{
 */

/**
 * @brief Macro to place at the beginning of any function using GLES 1.1 APIs
 *
 * Normally, it is necessary to call each function using its pointer as in:
 * @code
glapi->glFunction();
 * @endcode
 *
 * When using this macro, developers can then call all @c glFunctions without
 * changing their code:
 * @code
EVAS_GL_GLES1_USE(evasgl, context); // Add this at the beginning
glFunction(); // All calls 'look' normal
 * @endcode
 *
 * @note Please use @ref ELEMENTARY_GLVIEW_USE() instead, when possible.
 * @since_tizen @if MOBILE 2.3 @elseif WEARABLE 2.3.1 @endif
 */
#define EVAS_GL_GLES1_USE(evasgl, context) \
   Evas_GL_API *__evas_gl_glapi = evas_gl_context_api_get(evasgl, context);

/**
 * @brief Macro to place at the beginning of any function using GLES 1.1 APIs
 *
 * This is similar to @ref EVAS_GL_GLES1_USE except that it will return from
 * the function if the GL API can not be used.
 *
 * @note Please use @ref ELEMENTARY_GLVIEW_USE() instead, when possible.
 * @since_tizen @if MOBILE 2.3 @elseif WEARABLE 2.3.1 @endif
 */
#define EVAS_GL_GLES1_USE_OR_RETURN(evasgl, context, retval) \
   Evas_GL_API *__evas_gl_glapi = evas_gl_context_api_get(evasgl, context); \
   if (!__evas_gl_glapi) return retval;

// End of the convenience functions

// Global convenience macros

/**
 * @brief Convenience macro to use the GL helpers in simple applications: declare
 *
 * @c EVAS_GL_GLOBAL_GLES1_DECLARE should be used in a global header for the
 * application. For example, in a platform-specific compatibility header file.
 *
 * To be used with OpenGL-ES 1.1 contexts.
 *
 * Example of a global header file @c main.h:
 * @code
#include <Evas_GL_GLES1_Helpers.h>
// other includes...

EVAS_GL_GLOBAL_GLES1_DECLARE()

// ...
 * @endcode
 *
 * @note Please use @ref ELEMENTARY_GLVIEW_USE() instead, when possible.
 *
 * @see @ref ELEMENTARY_GLVIEW_GLOBAL_DECLARE
 * @see @ref EVAS_GL_GLOBAL_GLES1_DEFINE
 * @see @ref EVAS_GL_GLOBAL_GLES1_USE
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 */
#define EVAS_GL_GLOBAL_GLES1_DECLARE() \
   extern Evas_GL_API *__evas_gl_glapi;

/**
 * @brief Convenience macro to use the GL helpers in simple applications: define
 *
 * To be used with OpenGL-ES 1.1 contexts.
 *
 * Example of a file @c glview.c:
 *
 * @code
#include "main.h"
EVAS_GL_GLOBAL_GLES1_DEFINE()

// ...

static inline void
evgl_init(...)
{
   // ...
   evasgl = evas_gl_new(canvas);
   // ...
   ctx = evas_gl_context_version_create(evasgl, NULL, EVAS_GL_GLES_1_X);
   EVAS_GL_GLOBAL_GLES1_USE(evasgl, ctx);
   // ...
}

// ...
 * @endcode
 *
 * @see @ref ELEMENTARY_GLVIEW_GLOBAL_DEFINE
 * @see @ref EVAS_GL_GLOBAL_GLES1_DECLARE
 * @see @ref EVAS_GL_GLOBAL_GLES1_USE
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 */
#define EVAS_GL_GLOBAL_GLES1_DEFINE() \
   Evas_GL_API *__evas_gl_glapi = NULL;

/**
 * @brief Convenience macro to use the GL helpers in simple applications: use
 *
 * This macro will set the global variable holding the GL API so that it's
 * available to the application.
 *
 * It should be used right after setting up the GL context object.
 *
 * @see @ref ELEMENTARY_GLVIEW_GLOBAL_USE
 * @see @ref EVAS_GL_GLOBAL_GLES1_DECLARE
 * @see @ref EVAS_GL_GLOBAL_GLES1_DEFINE
 *
 * @if MOBILE @since_tizen 2.3
 * @elseif WEARABLE @since_tizen 2.3.1
 * @endif
 */
#define EVAS_GL_GLOBAL_GLES1_USE(evgl, ctx) \
   do { __evas_gl_glapi = evas_gl_context_api_get(evgl, ctx); } while (0)


// End of the convenience functions (global)
/** @} */

#define glAlphaFunc                                __evas_gl_glapi->glAlphaFunc
#define glClearColor                               __evas_gl_glapi->glClearColor
#define glClearDepthf                              __evas_gl_glapi->glClearDepthf
#define glClipPlanef                               __evas_gl_glapi->glClipPlanef
#define glColor4f                                  __evas_gl_glapi->glColor4f
#define glDepthRangef                              __evas_gl_glapi->glDepthRangef
#define glFogf                                     __evas_gl_glapi->glFogf
#define glFogfv                                    __evas_gl_glapi->glFogfv
#define glFrustumf                                 __evas_gl_glapi->glFrustumf
#define glGetClipPlanef                            __evas_gl_glapi->glGetClipPlanef
#define glGetFloatv                                __evas_gl_glapi->glGetFloatv
#define glGetLightfv                               __evas_gl_glapi->glGetLightfv
#define glGetMaterialfv                            __evas_gl_glapi->glGetMaterialfv
#define glGetTexEnvfv                              __evas_gl_glapi->glGetTexEnvfv
#define glGetTexParameterfv                        __evas_gl_glapi->glGetTexParameterfv
#define glLightModelf                              __evas_gl_glapi->glLightModelf
#define glLightModelfv                             __evas_gl_glapi->glLightModelfv
#define glLightf                                   __evas_gl_glapi->glLightf
#define glLightfv                                  __evas_gl_glapi->glLightfv
#define glLineWidth                                __evas_gl_glapi->glLineWidth
#define glLoadMatrixf                              __evas_gl_glapi->glLoadMatrixf
#define glMaterialf                                __evas_gl_glapi->glMaterialf
#define glMaterialfv                               __evas_gl_glapi->glMaterialfv
#define glMultMatrixf                              __evas_gl_glapi->glMultMatrixf
#define glMultiTexCoord4f                          __evas_gl_glapi->glMultiTexCoord4f
#define glNormal3f                                 __evas_gl_glapi->glNormal3f
#define glOrthof                                   __evas_gl_glapi->glOrthof
#define glPointParameterf                          __evas_gl_glapi->glPointParameterf
#define glPointParameterfv                         __evas_gl_glapi->glPointParameterfv
#define glPointSize                                __evas_gl_glapi->glPointSize
#define glPolygonOffset                            __evas_gl_glapi->glPolygonOffset
#define glRotatef                                  __evas_gl_glapi->glRotatef
#define glScalef                                   __evas_gl_glapi->glScalef
#define glTexEnvf                                  __evas_gl_glapi->glTexEnvf
#define glTexEnvfv                                 __evas_gl_glapi->glTexEnvfv
#define glTexParameterf                            __evas_gl_glapi->glTexParameterf
#define glTexParameterfv                           __evas_gl_glapi->glTexParameterfv
#define glTranslatef                               __evas_gl_glapi->glTranslatef
#define glActiveTexture                            __evas_gl_glapi->glActiveTexture
#define glAlphaFuncx                               __evas_gl_glapi->glAlphaFuncx
#define glBindBuffer                               __evas_gl_glapi->glBindBuffer
#define glBindTexture                              __evas_gl_glapi->glBindTexture
#define glBlendFunc                                __evas_gl_glapi->glBlendFunc
#define glBufferData                               __evas_gl_glapi->glBufferData
#define glBufferSubData                            __evas_gl_glapi->glBufferSubData
#define glClear                                    __evas_gl_glapi->glClear
#define glClearColorx                              __evas_gl_glapi->glClearColorx
#define glClearDepthx                              __evas_gl_glapi->glClearDepthx
#define glClearStencil                             __evas_gl_glapi->glClearStencil
#define glClientActiveTexture                      __evas_gl_glapi->glClientActiveTexture
#define glClipPlanex                               __evas_gl_glapi->glClipPlanex
#define glColor4ub                                 __evas_gl_glapi->glColor4ub
#define glColor4x                                  __evas_gl_glapi->glColor4x
#define glColorMask                                __evas_gl_glapi->glColorMask
#define glColorPointer                             __evas_gl_glapi->glColorPointer
#define glCompressedTexImage2D                     __evas_gl_glapi->glCompressedTexImage2D
#define glCompressedTexSubImage2D                  __evas_gl_glapi->glCompressedTexSubImage2D
#define glCopyTexImage2D                           __evas_gl_glapi->glCopyTexImage2D
#define glCopyTexSubImage2D                        __evas_gl_glapi->glCopyTexSubImage2D
#define glCullFace                                 __evas_gl_glapi->glCullFace
#define glDeleteBuffers                            __evas_gl_glapi->glDeleteBuffers
#define glDeleteTextures                           __evas_gl_glapi->glDeleteTextures
#define glDepthFunc                                __evas_gl_glapi->glDepthFunc
#define glDepthMask                                __evas_gl_glapi->glDepthMask
#define glDepthRangex                              __evas_gl_glapi->glDepthRangex
#define glDisable                                  __evas_gl_glapi->glDisable
#define glDisableClientState                       __evas_gl_glapi->glDisableClientState
#define glDrawArrays                               __evas_gl_glapi->glDrawArrays
#define glDrawElements                             __evas_gl_glapi->glDrawElements
#define glEnable                                   __evas_gl_glapi->glEnable
#define glEnableClientState                        __evas_gl_glapi->glEnableClientState
#define glFinish                                   __evas_gl_glapi->glFinish
#define glFlush                                    __evas_gl_glapi->glFlush
#define glFogx                                     __evas_gl_glapi->glFogx
#define glFogxv                                    __evas_gl_glapi->glFogxv
#define glFrontFace                                __evas_gl_glapi->glFrontFace
#define glFrustumx                                 __evas_gl_glapi->glFrustumx
#define glGetBooleanv                              __evas_gl_glapi->glGetBooleanv
#define glGetBufferParameteriv                     __evas_gl_glapi->glGetBufferParameteriv
#define glGetClipPlanex                            __evas_gl_glapi->glGetClipPlanex
#define glGenBuffers                               __evas_gl_glapi->glGenBuffers
#define glGenTextures                              __evas_gl_glapi->glGenTextures
#define glGetError                                 __evas_gl_glapi->glGetError
#define glGetFixedv                                __evas_gl_glapi->glGetFixedv
#define glGetIntegerv                              __evas_gl_glapi->glGetIntegerv
#define glGetLightxv                               __evas_gl_glapi->glGetLightxv
#define glGetMaterialxv                            __evas_gl_glapi->glGetMaterialxv
#define glGetPointerv                              __evas_gl_glapi->glGetPointerv
#define glGetString                                __evas_gl_glapi->glGetString
#define glGetTexEnviv                              __evas_gl_glapi->glGetTexEnviv
#define glGetTexEnvxv                              __evas_gl_glapi->glGetTexEnvxv
#define glGetTexParameteriv                        __evas_gl_glapi->glGetTexParameteriv
#define glGetTexParameterxv                        __evas_gl_glapi->glGetTexParameterxv
#define glHint                                     __evas_gl_glapi->glHint
#define glIsBuffer                                 __evas_gl_glapi->glIsBuffer
#define glIsEnabled                                __evas_gl_glapi->glIsEnabled
#define glIsTexture                                __evas_gl_glapi->glIsTexture
#define glLightModelx                              __evas_gl_glapi->glLightModelx
#define glLightModelxv                             __evas_gl_glapi->glLightModelxv
#define glLightx                                   __evas_gl_glapi->glLightx
#define glLightxv                                  __evas_gl_glapi->glLightxv
#define glLineWidthx                               __evas_gl_glapi->glLineWidthx
#define glLoadIdentity                             __evas_gl_glapi->glLoadIdentity
#define glLoadMatrixx                              __evas_gl_glapi->glLoadMatrixx
#define glLogicOp                                  __evas_gl_glapi->glLogicOp
#define glMaterialx                                __evas_gl_glapi->glMaterialx
#define glMaterialxv                               __evas_gl_glapi->glMaterialxv
#define glMatrixMode                               __evas_gl_glapi->glMatrixMode
#define glMultMatrixx                              __evas_gl_glapi->glMultMatrixx
#define glMultiTexCoord4x                          __evas_gl_glapi->glMultiTexCoord4x
#define glNormal3x                                 __evas_gl_glapi->glNormal3x
#define glNormalPointer                            __evas_gl_glapi->glNormalPointer
#define glOrthox                                   __evas_gl_glapi->glOrthox
#define glPixelStorei                              __evas_gl_glapi->glPixelStorei
#define glPointParameterx                          __evas_gl_glapi->glPointParameterx
#define glPointParameterxv                         __evas_gl_glapi->glPointParameterxv
#define glPointSizex                               __evas_gl_glapi->glPointSizex
#define glPolygonOffsetx                           __evas_gl_glapi->glPolygonOffsetx
#define glPopMatrix                                __evas_gl_glapi->glPopMatrix
#define glPushMatrix                               __evas_gl_glapi->glPushMatrix
#define glReadPixels                               __evas_gl_glapi->glReadPixels
#define glRotatex                                  __evas_gl_glapi->glRotatex
#define glSampleCoverage                           __evas_gl_glapi->glSampleCoverage
#define glSampleCoveragex                          __evas_gl_glapi->glSampleCoveragex
#define glScalex                                   __evas_gl_glapi->glScalex
#define glScissor                                  __evas_gl_glapi->glScissor
#define glShadeModel                               __evas_gl_glapi->glShadeModel
#define glStencilFunc                              __evas_gl_glapi->glStencilFunc
#define glStencilMask                              __evas_gl_glapi->glStencilMask
#define glStencilOp                                __evas_gl_glapi->glStencilOp
#define glTexCoordPointer                          __evas_gl_glapi->glTexCoordPointer
#define glTexEnvi                                  __evas_gl_glapi->glTexEnvi
#define glTexEnvx                                  __evas_gl_glapi->glTexEnvx
#define glTexEnviv                                 __evas_gl_glapi->glTexEnviv
#define glTexEnvxv                                 __evas_gl_glapi->glTexEnvxv
#define glTexImage2D                               __evas_gl_glapi->glTexImage2D
#define glTexParameteri                            __evas_gl_glapi->glTexParameteri
#define glTexParameterx                            __evas_gl_glapi->glTexParameterx
#define glTexParameteriv                           __evas_gl_glapi->glTexParameteriv
#define glTexParameterxv                           __evas_gl_glapi->glTexParameterxv
#define glTexSubImage2D                            __evas_gl_glapi->glTexSubImage2D
#define glTranslatex                               __evas_gl_glapi->glTranslatex
#define glVertexPointer                            __evas_gl_glapi->glVertexPointer
#define glViewport                                 __evas_gl_glapi->glViewport

// GLES 1.0 extensions
#define glPointSizePointerOES                      __evas_gl_glapi->glPointSizePointerOES

// GLES 1.1 extensions
#define glBlendEquationSeparateOES                 __evas_gl_glapi->glBlendEquationSeparateOES
#define glBlendFuncSeparateOES                     __evas_gl_glapi->glBlendFuncSeparateOES
#define glBlendEquationOES                         __evas_gl_glapi->glBlendEquationOES
#define glDrawTexsOES                              __evas_gl_glapi->glDrawTexsOES
#define glDrawTexiOES                              __evas_gl_glapi->glDrawTexiOES
#define glDrawTexxOES                              __evas_gl_glapi->glDrawTexxOES
#define glDrawTexsvOES                             __evas_gl_glapi->glDrawTexsvOES
#define glDrawTexivOES                             __evas_gl_glapi->glDrawTexivOES
#define glDrawTexxvOES                             __evas_gl_glapi->glDrawTexxvOES
#define glDrawTexfOES                              __evas_gl_glapi->glDrawTexfOES
#define glDrawTexfvOES                             __evas_gl_glapi->glDrawTexfvOES
#define glEGLImageTargetTexture2DOES               __evas_gl_glapi->glEGLImageTargetTexture2DOES
#define glEGLImageTargetRenderbufferStorageOES     __evas_gl_glapi->glEGLImageTargetRenderbufferStorageOES
#define glAlphaFuncxOES                            __evas_gl_glapi->glAlphaFuncxOES
#define glClearColorxOES                           __evas_gl_glapi->glClearColorxOES
#define glClearDepthxOES                           __evas_gl_glapi->glClearDepthxOES
#define glClipPlanexOES                            __evas_gl_glapi->glClipPlanexOES
#define glColor4xOES                               __evas_gl_glapi->glColor4xOES
#define glDepthRangexOES                           __evas_gl_glapi->glDepthRangexOES
#define glFogxOES                                  __evas_gl_glapi->glFogxOES
#define glFogxvOES                                 __evas_gl_glapi->glFogxvOES
#define glFrustumxOES                              __evas_gl_glapi->glFrustumxOES
#define glGetClipPlanexOES                         __evas_gl_glapi->glGetClipPlanexOES
#define glGetFixedvOES                             __evas_gl_glapi->glGetFixedvOES
#define glGetLightxvOES                            __evas_gl_glapi->glGetLightxvOES
#define glGetMaterialxvOES                         __evas_gl_glapi->glGetMaterialxvOES
#define glGetTexEnvxvOES                           __evas_gl_glapi->glGetTexEnvxvOES
#define glGetTexParameterxvOES                     __evas_gl_glapi->glGetTexParameterxvOES
#define glLightModelxOES                           __evas_gl_glapi->glLightModelxOES
#define glLightModelxvOES                          __evas_gl_glapi->glLightModelxvOES
#define glLightxOES                                __evas_gl_glapi->glLightxOES
#define glLightxvOES                               __evas_gl_glapi->glLightxvOES
#define glLineWidthxOES                            __evas_gl_glapi->glLineWidthxOES
#define glLoadMatrixxOES                           __evas_gl_glapi->glLoadMatrixxOES
#define glMaterialxOES                             __evas_gl_glapi->glMaterialxOES
#define glMaterialxvOES                            __evas_gl_glapi->glMaterialxvOES
#define glMultMatrixxOES                           __evas_gl_glapi->glMultMatrixxOES
#define glMultiTexCoord4xOES                       __evas_gl_glapi->glMultiTexCoord4xOES
#define glNormal3xOES                              __evas_gl_glapi->glNormal3xOES
#define glOrthoxOES                                __evas_gl_glapi->glOrthoxOES
#define glPointParameterxOES                       __evas_gl_glapi->glPointParameterxOES
#define glPointParameterxvOES                      __evas_gl_glapi->glPointParameterxvOES
#define glPointSizexOES                            __evas_gl_glapi->glPointSizexOES
#define glPolygonOffsetxOES                        __evas_gl_glapi->glPolygonOffsetxOES
#define glRotatexOES                               __evas_gl_glapi->glRotatexOES
#define glSampleCoveragexOES                       __evas_gl_glapi->glSampleCoveragexOES
#define glScalexOES                                __evas_gl_glapi->glScalexOES
#define glTexEnvxOES                               __evas_gl_glapi->glTexEnvxOES
#define glTexEnvxvOES                              __evas_gl_glapi->glTexEnvxvOES
#define glTexParameterxOES                         __evas_gl_glapi->glTexParameterxOES
#define glTexParameterxvOES                        __evas_gl_glapi->glTexParameterxvOES
#define glTranslatexOES                            __evas_gl_glapi->glTranslatexOES
#define glIsRenderbufferOES                        __evas_gl_glapi->glIsRenderbufferOES
#define glBindRenderbufferOES                      __evas_gl_glapi->glBindRenderbufferOES
#define glDeleteRenderbuffersOES                   __evas_gl_glapi->glDeleteRenderbuffersOES
#define glGenRenderbuffersOES                      __evas_gl_glapi->glGenRenderbuffersOES
#define glRenderbufferStorageOES                   __evas_gl_glapi->glRenderbufferStorageOES
#define glGetRenderbufferParameterivOES            __evas_gl_glapi->glGetRenderbufferParameterivOES
#define glIsFramebufferOES                         __evas_gl_glapi->glIsFramebufferOES
#define glBindFramebufferOES                       __evas_gl_glapi->glBindFramebufferOES
#define glDeleteFramebuffersOES                    __evas_gl_glapi->glDeleteFramebuffersOES
#define glGenFramebuffersOES                       __evas_gl_glapi->glGenFramebuffersOES
#define glCheckFramebufferStatusOES                __evas_gl_glapi->glCheckFramebufferStatusOES
#define glFramebufferRenderbufferOES               __evas_gl_glapi->glFramebufferRenderbufferOES
#define glFramebufferTexture2DOES                  __evas_gl_glapi->glFramebufferTexture2DOES
#define glGetFramebufferAttachmentParameterivOES   __evas_gl_glapi->glGetFramebufferAttachmentParameterivOES
#define glGenerateMipmapOES                        __evas_gl_glapi->glGenerateMipmapOES
#define glMapBufferOES                             __evas_gl_glapi->glMapBufferOES
#define glUnmapBufferOES                           __evas_gl_glapi->glUnmapBufferOES
#define glGetBufferPointervOES                     __evas_gl_glapi->glGetBufferPointervOES
#define glCurrentPaletteMatrixOES                  __evas_gl_glapi->glCurrentPaletteMatrixOES
#define glLoadPaletteFromModelViewMatrixOES        __evas_gl_glapi->glLoadPaletteFromModelViewMatrixOES
#define glMatrixIndexPointerOES                    __evas_gl_glapi->glMatrixIndexPointerOES
#define glWeightPointerOES                         __evas_gl_glapi->glWeightPointerOES
#define glQueryMatrixxOES                          __evas_gl_glapi->glQueryMatrixxOES
#define glDepthRangefOES                           __evas_gl_glapi->glDepthRangefOES
#define glFrustumfOES                              __evas_gl_glapi->glFrustumfOES
#define glOrthofOES                                __evas_gl_glapi->glOrthofOES
#define glClipPlanefOES                            __evas_gl_glapi->glClipPlanefOES
#define glGetClipPlanefOES                         __evas_gl_glapi->glGetClipPlanefOES
#define glClearDepthfOES                           __evas_gl_glapi->glClearDepthfOES
#define glTexGenfOES                               __evas_gl_glapi->glTexGenfOES
#define glTexGenfvOES                              __evas_gl_glapi->glTexGenfvOES
#define glTexGeniOES                               __evas_gl_glapi->glTexGeniOES
#define glTexGenivOES                              __evas_gl_glapi->glTexGenivOES
#define glTexGenxOES                               __evas_gl_glapi->glTexGenxOES
#define glTexGenxvOES                              __evas_gl_glapi->glTexGenxvOES
#define glGetTexGenfvOES                           __evas_gl_glapi->glGetTexGenfvOES
#define glGetTexGenivOES                           __evas_gl_glapi->glGetTexGenivOES
#define glGetTexGenxvOES                           __evas_gl_glapi->glGetTexGenxvOES
#define glBindVertexArrayOES                       __evas_gl_glapi->glBindVertexArrayOES
#define glDeleteVertexArraysOES                    __evas_gl_glapi->glDeleteVertexArraysOES
#define glGenVertexArraysOES                       __evas_gl_glapi->glGenVertexArraysOES
#define glIsVertexArrayOES                         __evas_gl_glapi->glIsVertexArrayOES
#define glCopyTextureLevelsAPPLE                   __evas_gl_glapi->glCopyTextureLevelsAPPLE
#define glRenderbufferStorageMultisampleAPPLE      __evas_gl_glapi->glRenderbufferStorageMultisampleAPPLE
#define glResolveMultisampleFramebufferAPPLE       __evas_gl_glapi->glResolveMultisampleFramebufferAPPLE
#define glFenceSyncAPPLE                           __evas_gl_glapi->glFenceSyncAPPLE
#define glIsSyncAPPLE                              __evas_gl_glapi->glIsSyncAPPLE
#define glDeleteSyncAPPLE                          __evas_gl_glapi->glDeleteSyncAPPLE
#define glClientWaitSyncAPPLE                      __evas_gl_glapi->glClientWaitSyncAPPLE
#define glWaitSyncAPPLE                            __evas_gl_glapi->glWaitSyncAPPLE
#define glGetInteger64vAPPLE                       __evas_gl_glapi->glGetInteger64vAPPLE
#define glGetSyncivAPPLE                           __evas_gl_glapi->glGetSyncivAPPLE
#define glDiscardFramebufferEXT                    __evas_gl_glapi->glDiscardFramebufferEXT
#define glMapBufferRangeEXT                        __evas_gl_glapi->glMapBufferRangeEXT
#define glFlushMappedBufferRangeEXT                __evas_gl_glapi->glFlushMappedBufferRangeEXT
#define glRenderbufferStorageMultisampleEXT        __evas_gl_glapi->glRenderbufferStorageMultisampleEXT
#define glFramebufferTexture2DMultisampleEXT       __evas_gl_glapi->glFramebufferTexture2DMultisampleEXT
#define glMultiDrawArraysEXT                       __evas_gl_glapi->glMultiDrawArraysEXT
#define glMultiDrawElementsEXT                     __evas_gl_glapi->glMultiDrawElementsEXT
#define glGetGraphicsResetStatusEXT                __evas_gl_glapi->glGetGraphicsResetStatusEXT
#define glReadnPixelsEXT                           __evas_gl_glapi->glReadnPixelsEXT
#define glGetnUniformfvEXT                         __evas_gl_glapi->glGetnUniformfvEXT
#define glGetnUniformivEXT                         __evas_gl_glapi->glGetnUniformivEXT
#define glTexStorage1DEXT                          __evas_gl_glapi->glTexStorage1DEXT
#define glTexStorage2DEXT                          __evas_gl_glapi->glTexStorage2DEXT
#define glTexStorage3DEXT                          __evas_gl_glapi->glTexStorage3DEXT
#define glTextureStorage1DEXT                      __evas_gl_glapi->glTextureStorage1DEXT
#define glTextureStorage2DEXT                      __evas_gl_glapi->glTextureStorage2DEXT
#define glTextureStorage3DEXT                      __evas_gl_glapi->glTextureStorage3DEXT
#define glClipPlanefIMG                            __evas_gl_glapi->glClipPlanefIMG
#define glClipPlanexIMG                            __evas_gl_glapi->glClipPlanexIMG
#define glRenderbufferStorageMultisampleIMG        __evas_gl_glapi->glRenderbufferStorageMultisampleIMG
#define glFramebufferTexture2DMultisampleIMG       __evas_gl_glapi->glFramebufferTexture2DMultisampleIMG
#define glDeleteFencesNV                           __evas_gl_glapi->glDeleteFencesNV
#define glGenFencesNV                              __evas_gl_glapi->glGenFencesNV
#define glIsFenceNV                                __evas_gl_glapi->glIsFenceNV
#define glTestFenceNV                              __evas_gl_glapi->glTestFenceNV
#define glGetFenceivNV                             __evas_gl_glapi->glGetFenceivNV
#define glFinishFenceNV                            __evas_gl_glapi->glFinishFenceNV
#define glSetFenceNV                               __evas_gl_glapi->glSetFenceNV
#define glGetDriverControlsQCOM                    __evas_gl_glapi->glGetDriverControlsQCOM
#define glGetDriverControlStringQCOM               __evas_gl_glapi->glGetDriverControlStringQCOM
#define glEnableDriverControlQCOM                  __evas_gl_glapi->glEnableDriverControlQCOM
#define glDisableDriverControlQCOM                 __evas_gl_glapi->glDisableDriverControlQCOM
#define glExtGetTexturesQCOM                       __evas_gl_glapi->glExtGetTexturesQCOM
#define glExtGetBuffersQCOM                        __evas_gl_glapi->glExtGetBuffersQCOM
#define glExtGetRenderbuffersQCOM                  __evas_gl_glapi->glExtGetRenderbuffersQCOM
#define glExtGetFramebuffersQCOM                   __evas_gl_glapi->glExtGetFramebuffersQCOM
#define glExtGetTexLevelParameterivQCOM            __evas_gl_glapi->glExtGetTexLevelParameterivQCOM
#define glExtTexObjectStateOverrideiQCOM           __evas_gl_glapi->glExtTexObjectStateOverrideiQCOM
#define glExtGetTexSubImageQCOM                    __evas_gl_glapi->glExtGetTexSubImageQCOM
#define glExtGetBufferPointervQCOM                 __evas_gl_glapi->glExtGetBufferPointervQCOM
#define glExtGetShadersQCOM                        __evas_gl_glapi->glExtGetShadersQCOM
#define glExtGetProgramsQCOM                       __evas_gl_glapi->glExtGetProgramsQCOM
#define glExtIsProgramBinaryQCOM                   __evas_gl_glapi->glExtIsProgramBinaryQCOM
#define glExtGetProgramBinarySourceQCOM            __evas_gl_glapi->glExtGetProgramBinarySourceQCOM
#define glStartTilingQCOM                          __evas_gl_glapi->glStartTilingQCOM
#define glEndTilingQCOM                            __evas_gl_glapi->glEndTilingQCOM

// glEvasGL functions
#define glEvasGLImageTargetTexture2DOES            __evas_gl_glapi->glEvasGLImageTargetTexture2DOES
#define glEvasGLImageTargetRenderbufferStorageOES  __evas_gl_glapi->glEvasGLImageTargetRenderbufferStorageOES

// Evas GL glue layer
#define evasglCreateImage                          __evas_gl_glapi->evasglCreateImage
#define evasglCreateImageForContext                __evas_gl_glapi->evasglCreateImageForContext
#define evasglDestroyImage                         __evas_gl_glapi->evasglDestroyImage
#define evasglCreateSync                           __evas_gl_glapi->evasglCreateSync
#define evasglDestroySync                          __evas_gl_glapi->evasglDestroySync
#define evasglClientWaitSync                       __evas_gl_glapi->evasglClientWaitSync
#define evasglSignalSync                           __evas_gl_glapi->evasglSignalSync
#define evasglGetSyncAttrib                        __evas_gl_glapi->evasglGetSyncAttrib
#define evasglWaitSync                             __evas_gl_glapi->evasglWaitSync

/**
 * @ingroup Evas_GL_GLES1_Helpers
 * @brief Macro to check that the GL APIs are properly set (GLES 1.1)
 * @since_tizen @if MOBILE 2.3 @elseif WEARABLE 2.3.1 @endif
 */
#define EVAS_GL_GLES1_API_CHECK() \
   ((__evas_gl_glapi != NULL) && (__evas_gl_glapi->version == EVAS_GL_API_VERSION) && (glAlphaFunc))

/**
 * @}
 */

#endif

