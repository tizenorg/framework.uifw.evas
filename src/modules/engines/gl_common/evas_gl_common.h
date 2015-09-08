#ifndef EVAS_GL_COMMON_H
#define EVAS_GL_COMMON_H

#include "evas_common.h"
#include "evas_private.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <Eet.h>

#define GL_GLEXT_PROTOTYPES

#ifdef BUILD_ENGINE_GL_COCOA
# include <OpenGL/gl.h>
# include <OpenGL/glext.h>
#else
# ifdef _EVAS_ENGINE_SDL_H
#  ifdef GL_GLES
#   include <SDL/SDL_opengles.h>
#  else
#   include <SDL/SDL_opengl.h>
#  endif
# else
#  ifdef GL_GLES
#   include <GLES2/gl2.h>
#   include <GLES2/gl2ext.h>
#  else
#   include <GL/gl.h>
#   include <GL/glext.h>
#  endif
# endif
#endif

#ifndef GL_TEXTURE_RECTANGLE_NV
# define GL_TEXTURE_RECTANGLE_NV 0x84F5
#endif
#ifndef GL_BGRA
# define GL_BGRA 0x80E1
#endif
#ifndef GL_RGBA4
# define GL_RGBA4 0x8056
#endif
#ifndef GL_RGBA8
# define GL_RGBA8 0x8058
#endif
#ifndef GL_RGBA12
# define GL_RGBA12 0x805A
#endif
#ifndef GL_RGBA16
# define GL_RGBA16 0x805B
#endif
#ifndef GL_R3_G3_B2
# define GL_R3_G3_B2 0x2A10
#endif
#ifndef GL_RGB4
# define GL_RGB4 0x804F
#endif
#ifndef GL_RGB5
# define GL_RGB5 0x8050
#endif
#ifndef GL_RGB8
# define GL_RGB8 0x8051
#endif
#ifndef GL_RGB10
# define GL_RGB10 0x8052
#endif
#ifndef GL_RGB12
# define GL_RGB12 0x8053
#endif
#ifndef GL_RGB16
# define GL_RGB16 0x8054
#endif
#ifndef GL_ALPHA4
# define GL_ALPHA4 0x803B
#endif
#ifndef GL_ALPHA8
# define GL_ALPHA8 0x803C
#endif
#ifndef GL_ALPHA12
# define GL_ALPHA12 0x803D
#endif
#ifndef GL_ALPHA16
# define GL_ALPHA16 0x803E
#endif
#ifndef GL_LUMINANCE4
# define GL_LUMINANCE4 0x803F
#endif
#ifndef GL_LUMINANCE8
# define GL_LUMINANCE8 0x8040
#endif
#ifndef GL_LUMINANCE12
# define GL_LUMINANCE12 0x8041
#endif
#ifndef GL_LUMINANCE16
# define GL_LUMINANCE16 0x8042
#endif
#ifndef GL_LUMINANCE4_ALPHA4
# define GL_LUMINANCE4_ALPHA4 0x8043
#endif
#ifndef GL_LUMINANCE8_ALPHA8
# define GL_LUMINANCE8_ALPHA8 0x8045
#endif
#ifndef GL_LUMINANCE12_ALPHA12
# define GL_LUMINANCE12_ALPHA12 0x8047
#endif
#ifndef GL_LUMINANCE16_ALPHA16
# define GL_LUMINANCE16_ALPHA16 0x8048
#endif
#ifndef GL_ETC1_RGB8_OES
# define GL_ETC1_RGB8_OES 0x8D64
#endif
#ifndef GL_COMPRESSED_RGB8_ETC2
# define GL_COMPRESSED_RGB8_ETC2 0x9274
#endif
#ifndef GL_COMPRESSED_RGBA8_ETC2_EAC
# define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#endif

#ifndef GL_UNPACK_ROW_LENGTH_EXT
# define GL_UNPACK_ROW_LENGTH_EXT 0x0cf2
#endif

#ifndef EGL_NO_DISPLAY
# define EGL_NO_DISPLAY 0
#endif

#ifndef EGL_NO_CONTEXT
# define EGL_NO_CONTEXT 0
#endif
#ifndef EGL_NONE
# define EGL_NONE 0x3038
#endif
#ifndef EGL_TRUE
# define EGL_TRUE 1
#endif
#ifndef EGL_FALSE
# define EGL_FALSE 0
#endif

#ifndef EGL_IMAGE_PRESERVED_KHR
# define EGL_IMAGE_PRESERVED_KHR 0x30D2
#endif
#ifndef EGL_NATIVE_BUFFER_TIZEN
# define EGL_NATIVE_BUFFER_TIZEN 0x32A0
#endif
#ifndef EGL_NATIVE_SURFACE_TIZEN
# define EGL_NATIVE_SURFACE_TIZEN 0x32A1
#endif
#ifndef EGL_MAP_GL_TEXTURE_2D_SEC
# define EGL_MAP_GL_TEXTURE_2D_SEC 0x3201
#endif
#ifndef  EGL_MAP_GL_TEXTURE_HEIGHT_SEC
# define EGL_MAP_GL_TEXTURE_HEIGHT_SEC 0x3202
#endif
#ifndef EGL_MAP_GL_TEXTURE_WIDTH_SEC
# define EGL_MAP_GL_TEXTURE_WIDTH_SEC 0x3203
#endif
#ifndef EGL_MAP_GL_TEXTURE_FORMAT_SEC
# define EGL_MAP_GL_TEXTURE_FORMAT_SEC 0x3204
#endif
#ifndef EGL_MAP_GL_TEXTURE_RGB_SEC
# define EGL_MAP_GL_TEXTURE_RGB_SEC 0x3205
#endif
#ifndef EGL_MAP_GL_TEXTURE_RGBA_SEC
# define EGL_MAP_GL_TEXTURE_RGBA_SEC 0x3206
#endif
#ifndef EGL_MAP_GL_TEXTURE_BGRA_SEC
# define EGL_MAP_GL_TEXTURE_BGRA_SEC 0x3207
#endif
#ifndef EGL_MAP_GL_TEXTURE_LUMINANCE_SEC
# define EGL_MAP_GL_TEXTURE_LUMINANCE_SEC 0x3208
#endif
#ifndef EGL_MAP_GL_TEXTURE_LUMINANCE_ALPHA_SEC
# define EGL_MAP_GL_TEXTURE_LUMINANCE_ALPHA_SEC	0x3209
#endif
#ifndef EGL_MAP_GL_TEXTURE_PIXEL_TYPE_SEC
# define EGL_MAP_GL_TEXTURE_PIXEL_TYPE_SEC 0x320a
#endif
#ifndef EGL_MAP_GL_TEXTURE_UNSIGNED_BYTE_SEC
# define EGL_MAP_GL_TEXTURE_UNSIGNED_BYTE_SEC 0x320b
#endif
#ifndef EGL_MAP_GL_TEXTURE_STRIDE_IN_BYTES_SEC
# define EGL_MAP_GL_TEXTURE_STRIDE_IN_BYTES_SEC 0x320c
#endif
#ifndef GL_PROGRAM_BINARY_LENGTH
# define GL_PROGRAM_BINARY_LENGTH 0x8741
#endif
#ifndef GL_NUM_PROGRAM_BINARY_FORMATS
# define GL_NUM_PROGRAM_BINARY_FORMATS 0x87FE
#endif
#ifndef GL_PROGRAM_BINARY_FORMATS
# define GL_PROGRAM_BINARY_FORMATS 0x87FF
#endif
#ifndef GL_PROGRAM_BINARY_RETRIEVABLE_HINT
# define GL_PROGRAM_BINARY_RETRIEVABLE_HINT 0x8257
#endif
#ifndef GL_MAX_SAMPLES_IMG
#define GL_MAX_SAMPLES_IMG 0x9135
#endif
#ifndef GL_MAX_SAMPLES_EXT
#define GL_MAX_SAMPLES_EXT 0x8D57
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif
#ifndef EGL_MAP_GL_TEXTURE_DEVICE_CPU_SEC
#define EGL_MAP_GL_TEXTURE_DEVICE_CPU_SEC 1
#endif
#ifndef EGL_MAP_GL_TEXTURE_DEVICE_G2D_SEC
#define EGL_MAP_GL_TEXTURE_DEVICE_G2D_SEC 2
#endif
#ifndef EGL_MAP_GL_TEXTURE_OPTION_READ_SEC
#define EGL_MAP_GL_TEXTURE_OPTION_READ_SEC (1<<0)
#endif
#ifndef EGL_MAP_GL_TEXTURE_OPTION_WRITE_SEC
#define EGL_MAP_GL_TEXTURE_OPTION_WRITE_SEC (1<<1)
#endif
#ifndef EGL_GL_TEXTURE_2D_KHR
#define EGL_GL_TEXTURE_2D_KHR 0x30B1
#endif
#ifndef EGL_GL_TEXTURE_LEVEL_KHR
#define EGL_GL_TEXTURE_LEVEL_KHR 0x30BC
#endif
#ifndef EGL_IMAGE_PRESERVED_KHR
#define EGL_IMAGE_PRESERVED_KHR 0x30D2
#endif

#ifndef GL_COLOR_BUFFER_BIT0_QCOM
// if GL_COLOR_BUFFER_BIT0_QCOM  just assume the rest arent... saves fluff
#define GL_COLOR_BUFFER_BIT0_QCOM                     0x00000001
#define GL_COLOR_BUFFER_BIT1_QCOM                     0x00000002
#define GL_COLOR_BUFFER_BIT2_QCOM                     0x00000004
#define GL_COLOR_BUFFER_BIT3_QCOM                     0x00000008
#define GL_COLOR_BUFFER_BIT4_QCOM                     0x00000010
#define GL_COLOR_BUFFER_BIT5_QCOM                     0x00000020
#define GL_COLOR_BUFFER_BIT6_QCOM                     0x00000040
#define GL_COLOR_BUFFER_BIT7_QCOM                     0x00000080
#define GL_DEPTH_BUFFER_BIT0_QCOM                     0x00000100
#define GL_DEPTH_BUFFER_BIT1_QCOM                     0x00000200
#define GL_DEPTH_BUFFER_BIT2_QCOM                     0x00000400
#define GL_DEPTH_BUFFER_BIT3_QCOM                     0x00000800
#define GL_DEPTH_BUFFER_BIT4_QCOM                     0x00001000
#define GL_DEPTH_BUFFER_BIT5_QCOM                     0x00002000
#define GL_DEPTH_BUFFER_BIT6_QCOM                     0x00004000
#define GL_DEPTH_BUFFER_BIT7_QCOM                     0x00008000
#define GL_STENCIL_BUFFER_BIT0_QCOM                   0x00010000
#define GL_STENCIL_BUFFER_BIT1_QCOM                   0x00020000
#define GL_STENCIL_BUFFER_BIT2_QCOM                   0x00040000
#define GL_STENCIL_BUFFER_BIT3_QCOM                   0x00080000
#define GL_STENCIL_BUFFER_BIT4_QCOM                   0x00100000
#define GL_STENCIL_BUFFER_BIT5_QCOM                   0x00200000
#define GL_STENCIL_BUFFER_BIT6_QCOM                   0x00400000
#define GL_STENCIL_BUFFER_BIT7_QCOM                   0x00800000
#define GL_MULTISAMPLE_BUFFER_BIT0_QCOM               0x01000000
#define GL_MULTISAMPLE_BUFFER_BIT1_QCOM               0x02000000
#define GL_MULTISAMPLE_BUFFER_BIT2_QCOM               0x04000000
#define GL_MULTISAMPLE_BUFFER_BIT3_QCOM               0x08000000
#define GL_MULTISAMPLE_BUFFER_BIT4_QCOM               0x10000000
#define GL_MULTISAMPLE_BUFFER_BIT5_QCOM               0x20000000
#define GL_MULTISAMPLE_BUFFER_BIT6_QCOM               0x40000000
#define GL_MULTISAMPLE_BUFFER_BIT7_QCOM               0x80000000
#endif

#define SHAD_VERTEX 0
#define SHAD_COLOR  1
#define SHAD_TEXUV  2
#define SHAD_TEXUV2 3
#define SHAD_TEXUV3 4
#define SHAD_TEXM   5
#define SHAD_TEXSAM 6

typedef struct _Evas_GL_Program                      Evas_GL_Program;
typedef struct _Evas_GL_Program_Source               Evas_GL_Program_Source;
typedef struct _Evas_GL_Shared                       Evas_GL_Shared;
typedef struct _Evas_Engine_GL_Context               Evas_Engine_GL_Context;
typedef struct _Evas_GL_Texture_Pool                 Evas_GL_Texture_Pool;
typedef struct _Evas_GL_Texture                      Evas_GL_Texture;
typedef struct _Evas_GL_Image                        Evas_GL_Image;
typedef struct _Evas_GL_Font_Texture                 Evas_GL_Font_Texture;
typedef struct _Evas_GL_Polygon                      Evas_GL_Polygon;
typedef struct _Evas_GL_Polygon_Point                Evas_GL_Polygon_Point;

typedef enum {
  SHADER_RECT,
  SHADER_FONT,

  SHADER_IMG_MASK,

  SHADER_IMG,
  SHADER_IMG_NOMUL,
  SHADER_IMG_AFILL,
  SHADER_IMG_NOMUL_AFILL,
  SHADER_IMG_BGRA,
  SHADER_IMG_BGRA_NOMUL,
  SHADER_IMG_BGRA_AFILL,
  SHADER_IMG_BGRA_NOMUL_AFILL,
  SHADER_TEX,
  SHADER_TEX_NOMUL,
  SHADER_TEX_AFILL,
  SHADER_TEX_NOMUL_AFILL,

  SHADER_IMG_21,
  SHADER_IMG_21_NOMUL,
  SHADER_IMG_21_AFILL,
  SHADER_IMG_21_NOMUL_AFILL,
  SHADER_IMG_21_BGRA,
  SHADER_IMG_21_BGRA_NOMUL,
  SHADER_IMG_21_BGRA_AFILL,
  SHADER_IMG_21_BGRA_NOMUL_AFILL,
  SHADER_TEX_21,
  SHADER_TEX_21_NOMUL,
  SHADER_TEX_21_AFILL,
  SHADER_TEX_21_NOMUL_AFILL,

  SHADER_IMG_12,
  SHADER_IMG_12_NOMUL,
  SHADER_IMG_12_AFILL,
  SHADER_IMG_12_NOMUL_AFILL,
  SHADER_IMG_12_BGRA,
  SHADER_IMG_12_BGRA_NOMUL,
  SHADER_IMG_12_BGRA_AFILL,
  SHADER_IMG_12_BGRA_NOMUL_AFILL,
  SHADER_TEX_12,
  SHADER_TEX_12_NOMUL,
  SHADER_TEX_12_AFILL,
  SHADER_TEX_12_NOMUL_AFILL,

  SHADER_IMG_22,
  SHADER_IMG_22_NOMUL,
  SHADER_IMG_22_AFILL,
  SHADER_IMG_22_NOMUL_AFILL,
  SHADER_IMG_22_BGRA,
  SHADER_IMG_22_BGRA_NOMUL,
  SHADER_IMG_22_BGRA_AFILL,
  SHADER_IMG_22_BGRA_NOMUL_AFILL,
  SHADER_TEX_22,
  SHADER_TEX_22_NOMUL,
  SHADER_TEX_22_AFILL,
  SHADER_TEX_22_NOMUL_AFILL,

  SHADER_YUV,
  SHADER_YUV_NOMUL,
  SHADER_YUY2,
  SHADER_YUY2_NOMUL,
  SHADER_NV12,
  SHADER_NV12_NOMUL,

  SHADER_FILTER_INVERT,
  SHADER_FILTER_INVERT_NOMUL,
  SHADER_FILTER_INVERT_BGRA,
  SHADER_FILTER_INVERT_BGRA_NOMUL,
  SHADER_FILTER_GREYSCALE,
  SHADER_FILTER_GREYSCALE_NOMUL,
  SHADER_FILTER_GREYSCALE_BGRA,
  SHADER_FILTER_GREYSCALE_BGRA_NOMUL,
  SHADER_FILTER_SEPIA,
  SHADER_FILTER_SEPIA_NOMUL,
  SHADER_FILTER_SEPIA_BGRA,
  SHADER_FILTER_SEPIA_BGRA_NOMUL,

  /* SHADER_FILTER_BLUR, */
  /* SHADER_FILTER_BLUR_NOMUL, */
  /* SHADER_FILTER_BLUR_BGRA, */
  /* SHADER_FILTER_BLUR_BGRA_NOMUL, */
  SHADER_TIZEN,
  SHADER_TIZEN_MASK,
  SHADER_TIZEN_NOMUL,
  SHADER_TIZEN_MASK_NOMUL,
  SHADER_RGB_A_PAIR,
  SHADER_RGB_A_PAIR_NOMUL,

  SHADER_FONT_MASK,
  SHADER_IMG_MASK_BGRA,
  SHADER_IMG_MASK_BGRA_NOMUL,
  SHADER_IMG_MASK_NOMUL,
  SHADER_NV12_MASK,
  SHADER_RECT_MASK,
  /* SHADER_RGB_A_PAIR_MASK, */
  SHADER_YUV_MASK,
  SHADER_YUY2_MASK,
  SHADER_MAP_MASK,
  SHADER_MAP_MASK_BGRA,
  SHADER_MAP_MASK_BGRA_NOMUL,
  SHADER_MAP_MASK_NOMUL,

  SHADER_LAST
} Evas_GL_Shader;

struct _Evas_GL_Program
{
   GLuint vert, frag, prog;

   int tex_count;

   Eina_Bool init;
};

struct _Evas_GL_Program_Source
{
   const char *src;
   const unsigned int *bin;
   int bin_size;
};

struct _Evas_GL_Shared
{
   Eina_List          *images;

   int                 images_size;

   struct {
      GLint max_texture_units;
      GLint max_texture_size;
      GLint max_vertex_elements;
      GLfloat anisotropic;
      Eina_Bool rgb : 1;
      Eina_Bool bgra : 1;
      Eina_Bool tex_npo2 : 1;
      Eina_Bool tex_rect : 1;
      Eina_Bool sec_image_map : 1;
      Eina_Bool sec_tbm_surface : 1;
      Eina_Bool bin_program : 1;
      Eina_Bool unpack_row_length : 1;
      Eina_Bool etc1 : 1;
      Eina_Bool etc2 : 1;
      Eina_Bool etc1_subimage : 1; // may be set to false at runtime if op fails
      // tuning params - per gpu/cpu combo?
#define MAX_CUTOUT             512
#define DEF_CUTOUT                  512

// FIXME bug with pipes > 1 right now, should default to 32
#define MAX_PIPES              128
#define DEF_PIPES                    32
#define DEF_PIPES_SGX_540            32
#define DEF_PIPES_TEGRA_2             1

#define MIN_ATLAS_ALLOC         16
#define MAX_ATLAS_ALLOC       1024
#define DEF_ATLAS_ALLOC       1024
#define DEF_ATLAS_RECT_ALLOC   256

#define MIN_ATLAS_ALLOC_ALPHA   16
#define MAX_ATLAS_ALLOC_ALPHA 4096
#define DEF_ATLAS_ALLOC_ALPHA 4096
#define DEF_ATLAS_RECT_ALLOC_ALPHA 512

#define MAX_ATLAS_W            512
#define DEF_ATLAS_W                 512

#define MAX_ATLAS_H            512
#define DEF_ATLAS_H                 512

#define ATLAS_FORMATS_COUNT    7
#define MIN_ATLAS_SLOT          16
#define MAX_ATLAS_SLOT         512
#define DEF_ATLAS_SLOT               16

      Eina_List *cspaces; // depend on the values of etc1, etc2

      struct {
         struct {
            int max;
         } cutout;
         struct {
            int max;
         } pipes;
         struct {
            int max_alloc_size;
            int max_alloc_alpha_size;
            int max_w;
            int max_h;
            int slot_size;
         } atlas;
      } tune;
   } info;

   struct {
      Eina_List       *whole;
      Eina_List       *atlas[33][ATLAS_FORMATS_COUNT];
   } tex;

   Eina_Hash          *native_pm_hash;
   Eina_Hash          *native_tex_hash;
   Eina_Hash          *native_buffer_hash;
   Eina_Hash          *native_tbm_hash;
   Eina_Hash          *native_evasgl_hash;

   Evas_GL_Program     shader[SHADER_LAST];

   int references;
   int w, h;
   int rot;
   int mflip;
   // persp map
   int foc, z0, px, py;
   int ax, ay;
};

#define RTYPE_RECT  1
#define RTYPE_IMAGE 2
#define RTYPE_FONT  3
#define RTYPE_YUV   4
#define RTYPE_MAP   5 /* need to merge with image */
#define RTYPE_IMASK 6
#define RTYPE_YUY2  7
#define RTYPE_NV12  8
#define ARRAY_BUFFER_USE 500
#define ARRAY_BUFFER_USE_SHIFT 100

struct _Evas_Engine_GL_Context
{
   int                references;
   int                w, h;
   int                rot;
   int                foc, z0, px, py;
   RGBA_Draw_Context *dc;

   Evas_GL_Shared     *shared;

   int flushnum;
   struct {
      int                top_pipe;
      struct {
         GLuint          cur_prog;
         GLuint          cur_tex, cur_texu, cur_texv;
         GLuint          cur_texm, cur_texmu, cur_texmv;
         int             render_op;
         int             cx, cy, cw, ch;
         int             smooth;
         int             blend;
         int             clip;
      } current;
   } state;

   struct {
      int                x, y, w, h;
      Eina_Bool          enabled : 1;
      Eina_Bool          used : 1;
   } master_clip;

   struct {
      struct {
         int             x, y, w, h;
         int             type;
      } region;
      struct {
         int             x, y, w, h;
         Eina_Bool       active : 1;
      } clip;
      struct {
         Evas_GL_Image  *surface;
         GLuint          cur_prog;
         GLuint          cur_tex, cur_texu, cur_texv, cur_texm;
         void           *cur_tex_dyn, *cur_texu_dyn, *cur_texv_dyn;
         int             render_op;
         int             cx, cy, cw, ch;
         int             smooth;
         int             blend;
         int             clip;
      } shader;
      struct {
         int num, alloc;
         GLshort *vertex;
         GLubyte *color;
         GLfloat *texuv;
         GLfloat *texuv2;
         GLfloat *texuv3;
         GLfloat *texm;
         GLfloat *texsam;
         Eina_Bool line: 1;
         Eina_Bool use_vertex : 1;
         Eina_Bool use_color : 1;
         Eina_Bool use_texuv : 1;
         Eina_Bool use_texuv2 : 1;
         Eina_Bool use_texuv3 : 1;
         Eina_Bool use_texm : 1;
         Eina_Bool use_texsam : 1;
         Eina_Bool mask_smooth : 1;
         Evas_GL_Image *im;
         GLuint buffer;
         int buffer_alloc;
         int buffer_use;
      } array;
   } pipe[MAX_PIPES];

   struct {
      Eina_Bool size : 1;
   } change;

   Eina_List *font_glyph_textures;

   Evas_GL_Image *def_surface;

#ifdef GL_GLES
// FIXME: hack. expose egl display to gl core for egl image sec extn.
   void *egldisp;
#endif

   GLuint preserve_bit;
   int gles_version;

   int depth;

   /* ----- Bitfields come here ----- */

   Eina_Bool havestuff : 1;

   /* If set, the driver will rotate the buffer itself based on an X Window property */
   Eina_Bool pre_rotated : 1;
};

struct _Evas_GL_Texture_Pool
{
   Evas_Engine_GL_Context *gc;
   GLuint           texture, fb;
   GLuint           intformat, format, dataformat;
   int              w, h;
   int              references;
   int              slot, fslot;
   struct {
      void         *img;
      void         *buffer;
      unsigned int *data;
      int           w, h;
      int           stride;
      int           checked_out;
   } dyn;

   Eina_List       *allocations;
   Eina_Rectangle_Pool *eina_pool;
   Eina_Bool        whole : 1;
   Eina_Bool        render : 1;
   Eina_Bool        native : 1;
   Eina_Bool        dynamic : 1;
};

struct _Evas_GL_Texture
{
   Evas_Engine_GL_Context *gc;
   Evas_GL_Image   *im;
   Evas_GL_Texture_Pool *pt, *ptu, *ptv, *ptt;
   union {
      Evas_GL_Texture_Pool *ptuv;
      Evas_GL_Texture_Pool *pta;
   };
   RGBA_Font_Glyph *fglyph;
   int              x, y, w, h;
   double           sx1, sy1, sx2, sy2;
   int              references;

   struct
   {
      Evas_GL_Texture_Pool *pt[2], *ptuv[2];
      int              source;
   } double_buffer;

   Eina_Bool        alpha : 1;
   Eina_Bool        dyn : 1;
   Eina_Rectangle   *rect;
};

struct _Evas_GL_Image
{
   Evas_Engine_GL_Context *gc;
   RGBA_Image      *im;
   Evas_GL_Texture *tex;
   RGBA_Image_Loadopts load_opts;
   int              references;
   // if im->im == NULL, it's a render-surface so these here are used
   int              w, h;
   struct {
      Evas_Colorspace space;
      void         *data;
      unsigned char no_free : 1;
   } cs;

   struct {
      void         *data;
      struct {
         void     (*bind)   (void *data, void *image);
         void     (*unbind) (void *data, void *image);
         void     (*free)   (void *data, void *image);
         int      (*yinvert)(void *data, void *image);
         void      *data;
      } func;
      int           yinvert;
      int           target;
      int           mipmap;
      int           rot;
      float         ratio;
      int           flip;
      unsigned char loose : 1;
   } native;

   struct {
      Evas_GL_Image *origin;
      int            w, h;
      Eina_Bool      smooth : 1;
   } scaled;

   int scale_hint, content_hint;
   int csize;

   Eina_List       *filtered;

   unsigned char    dirty : 1;
   unsigned char    cached : 1;
   unsigned char    alpha : 1;
   unsigned char    tex_only : 1;
   unsigned char    locked : 1; // gl_surface_lock/unlock
   unsigned char    direct : 1; // evas gl direct renderable
};

struct _Evas_GL_Font_Texture
{
   Evas_GL_Texture *tex;
};

struct _Evas_GL_Polygon
{
   Eina_List *points;
   Eina_Bool  changed : 1;
};

struct _Evas_GL_Polygon_Point
{
   int x, y;
};

#if 0
extern Evas_GL_Program_Source shader_rect_frag_src;
extern Evas_GL_Program_Source shader_rect_vert_src;
extern Evas_GL_Program_Source shader_font_frag_src;
extern Evas_GL_Program_Source shader_font_vert_src;

extern Evas_GL_Program_Source shader_img_frag_src;
extern Evas_GL_Program_Source shader_img_vert_src;
extern Evas_GL_Program_Source shader_img_nomul_frag_src;
extern Evas_GL_Program_Source shader_img_nomul_vert_src;
extern Evas_GL_Program_Source shader_img_bgra_frag_src;
extern Evas_GL_Program_Source shader_img_bgra_vert_src;
extern Evas_GL_Program_Source shader_img_bgra_nomul_frag_src;
extern Evas_GL_Program_Source shader_img_bgra_nomul_vert_src;
extern Evas_GL_Program_Source shader_img_mask_frag_src;
extern Evas_GL_Program_Source shader_img_mask_vert_src;

extern Evas_GL_Program_Source shader_yuv_frag_src;
extern Evas_GL_Program_Source shader_yuv_vert_src;
extern Evas_GL_Program_Source shader_yuv_nomul_frag_src;
extern Evas_GL_Program_Source shader_yuv_nomul_vert_src;

extern Evas_GL_Program_Source shader_yuy2_frag_src;
extern Evas_GL_Program_Source shader_yuy2_vert_src;
extern Evas_GL_Program_Source shader_yuy2_nomul_frag_src;
extern Evas_GL_Program_Source shader_yuy2_nomul_vert_src;

extern Evas_GL_Program_Source shader_tex_frag_src;
extern Evas_GL_Program_Source shader_tex_vert_src;
extern Evas_GL_Program_Source shader_tex_nomul_frag_src;
extern Evas_GL_Program_Source shader_tex_nomul_vert_src;

extern Evas_GL_Program_Source shader_filter_invert_frag_src;
extern Evas_GL_Program_Source shader_filter_invert_nomul_frag_src;
extern Evas_GL_Program_Source shader_filter_invert_bgra_frag_src;
extern Evas_GL_Program_Source shader_filter_invert_bgra_nomul_frag_src;
extern Evas_GL_Program_Source shader_filter_sepia_frag_src;
extern Evas_GL_Program_Source shader_filter_sepia_nomul_frag_src;
extern Evas_GL_Program_Source shader_filter_sepia_bgra_frag_src;
extern Evas_GL_Program_Source shader_filter_sepia_bgra_nomul_frag_src;
extern Evas_GL_Program_Source shader_filter_greyscale_frag_src;
extern Evas_GL_Program_Source shader_filter_greyscale_nomul_frag_src;
extern Evas_GL_Program_Source shader_filter_greyscale_bgra_frag_src;
extern Evas_GL_Program_Source shader_filter_greyscale_bgra_nomul_frag_src;
/* blur (annoyingly) needs (aka is faster with) a vertex shader */
extern Evas_GL_Program_Source shader_filter_blur_vert_src;
extern Evas_GL_Program_Source shader_filter_blur_frag_src;
extern Evas_GL_Program_Source shader_filter_blur_nomul_frag_src;
extern Evas_GL_Program_Source shader_filter_blur_bgra_frag_src;
extern Evas_GL_Program_Source shader_filter_blur_bgra_nomul_frag_src;
#endif

void glerr(int err, const char *file, const char *func, int line, const char *op);

Evas_Engine_GL_Context  *evas_gl_common_context_new(void);
void              evas_gl_common_context_free(Evas_Engine_GL_Context *gc);
void              evas_gl_common_context_use(Evas_Engine_GL_Context *gc);
void              evas_gl_common_context_newframe(Evas_Engine_GL_Context *gc);
void              evas_gl_common_context_resize(Evas_Engine_GL_Context *gc, int w, int h, int rot);
void              evas_gl_common_tiling_start(Evas_Engine_GL_Context *gc,
                                              int rot, int gw, int gh,
                                              int cx, int cy, int cw, int ch,
                                              int bitmask);
void              evas_gl_common_context_done(Evas_Engine_GL_Context *gc);
void              evas_gl_common_tiling_done(Evas_Engine_GL_Context *gc);
void              evas_gl_common_context_target_surface_set(Evas_Engine_GL_Context *gc, Evas_GL_Image *surface);

void              evas_gl_common_context_line_push(Evas_Engine_GL_Context *gc,
                                                   int x1, int y1, int x2, int y2,
                                                   int clip, int cx, int cy, int cw, int ch,
                                                   int r, int g, int b, int a);
void              evas_gl_common_context_rectangle_push(Evas_Engine_GL_Context *gc,
                                                        int x, int y, int w, int h,
                                                        int r, int g, int b, int a,
                                                        Evas_GL_Texture *mtex, double mx, double my, double mw, double mh, Eina_Bool mask_smooth);
void              evas_gl_common_context_image_push(Evas_Engine_GL_Context *gc,
                                                    Evas_GL_Texture *tex,
                                                    double sx, double sy, double sw, double sh,
                                                    int x, int y, int w, int h,
                                                    Evas_GL_Texture *mtex, double mx, double my, double mw, double mh, Eina_Bool mask_smooth,
                                                    int r, int g, int b, int a,
                                                    Eina_Bool smooth, Eina_Bool tex_only);
void              evas_gl_common_context_font_push(Evas_Engine_GL_Context *gc,
                                                   Evas_GL_Texture *tex,
                                                   double sx, double sy, double sw, double sh,
                                                   int x, int y, int w, int h,
                                                   Evas_GL_Texture *mtex, double mx, double my, double mw, double mh, Eina_Bool mask_smooth,
                                                   int r, int g, int b, int a);
void             evas_gl_common_context_yuv_push(Evas_Engine_GL_Context *gc,
                                                 Evas_GL_Texture *tex,
                                                 double sx, double sy, double sw, double sh,
                                                 int x, int y, int w, int h,
                                                 Evas_GL_Texture *mtex, double mx, double my, double mw, double mh, Eina_Bool mask_smooth,
                                                 int r, int g, int b, int a,
                                                 Eina_Bool smooth);
void             evas_gl_common_context_yuy2_push(Evas_Engine_GL_Context *gc,
                                                  Evas_GL_Texture *tex,
                                                  double sx, double sy, double sw, double sh,
                                                  int x, int y, int w, int h,
                                                  Evas_GL_Texture *mtex, double mx, double my, double mw, double mh, Eina_Bool mask_smooth,
                                                  int r, int g, int b, int a,
                                                  Eina_Bool smooth);
void             evas_gl_common_context_nv12_push(Evas_Engine_GL_Context *gc,
                                                  Evas_GL_Texture *tex,
                                                  double sx, double sy, double sw, double sh,
                                                  int x, int y, int w, int h,
                                                  Evas_GL_Texture *mtex, double mx, double my, double mw, double mh, Eina_Bool mask_smooth,
                                                  int r, int g, int b, int a,
                                                  Eina_Bool smooth);
void             evas_gl_common_context_rgb_a_pair_push(Evas_Engine_GL_Context *gc,
                                                        Evas_GL_Texture *tex,
                                                        double sx, double sy, double sw, double sh,
                                                        int x, int y, int w, int h,
                                                        int r, int g, int b, int a,
                                                        Eina_Bool smooth);
void             evas_gl_common_context_image_map_push(Evas_Engine_GL_Context *gc,
                                                       Evas_GL_Texture *tex,
                                                       int npoints,
                                                       RGBA_Map_Point *p,
                                                       int clip, int cx, int cy, int cw, int ch,
                                                       Evas_GL_Texture *mtex, int mx, int my, int mw, int mh, Eina_Bool mask_smooth,
                                                       int r, int g, int b, int a,
                                                       Eina_Bool smooth,
                                                       Eina_Bool tex_only,
                                                       Evas_Colorspace cspace);
void              evas_gl_common_context_flush(Evas_Engine_GL_Context *gc);

int               evas_gl_common_shader_program_init(Evas_GL_Shared *shared);
void              evas_gl_common_shader_program_init_done(void);
void              evas_gl_common_shader_program_shutdown(Evas_GL_Program *p);

Eina_Bool         evas_gl_common_file_cache_is_dir(const char *file);
Eina_Bool         evas_gl_common_file_cache_mkdir(const char *dir);
Eina_Bool         evas_gl_common_file_cache_file_exists(const char *file);
Eina_Bool         evas_gl_common_file_cache_mkpath_if_not_exists(const char *path);
Eina_Bool         evas_gl_common_file_cache_mkpath(const char *path);
int               evas_gl_common_file_cache_dir_check(char *cache_dir, int num);
int               evas_gl_common_file_cache_file_check(const char *cache_dir, const char *cache_name, char *cache_file, int dir_num);
int               evas_gl_common_file_cache_save(Evas_GL_Shared *shared);

void              evas_gl_common_rect_draw(Evas_Engine_GL_Context *gc, int x, int y, int w, int h);

void              evas_gl_texture_pool_empty(Evas_GL_Texture_Pool *pt);
Evas_GL_Texture  *evas_gl_common_texture_new(Evas_Engine_GL_Context *gc, RGBA_Image *im);
Evas_GL_Texture  *evas_gl_common_texture_native_new(Evas_Engine_GL_Context *gc, unsigned int w, unsigned int h, int alpha, Evas_GL_Image *im);
Evas_GL_Texture  *evas_gl_common_texture_render_new(Evas_Engine_GL_Context *gc, unsigned int w, unsigned int h, int alpha);
Evas_GL_Texture  *evas_gl_common_texture_dynamic_new(Evas_Engine_GL_Context *gc, Evas_GL_Image *im);
void              evas_gl_common_texture_update(Evas_GL_Texture *tex, RGBA_Image *im);
void              evas_gl_common_texture_free(Evas_GL_Texture *tex);
Evas_GL_Texture  *evas_gl_common_texture_alpha_new(Evas_Engine_GL_Context *gc, DATA8 *pixels, unsigned int w, unsigned int h, int fh);
void              evas_gl_common_texture_alpha_update(Evas_GL_Texture *tex, DATA8 *pixels, unsigned int w, unsigned int h, int fh);
Evas_GL_Texture  *evas_gl_common_texture_yuv_new(Evas_Engine_GL_Context *gc, DATA8 **rows, unsigned int w, unsigned int h);
void              evas_gl_common_texture_yuv_update(Evas_GL_Texture *tex, DATA8 **rows, unsigned int w, unsigned int h);
Evas_GL_Texture  *evas_gl_common_texture_yuy2_new(Evas_Engine_GL_Context *gc, DATA8 **rows, unsigned int w, unsigned int h);
void              evas_gl_common_texture_yuy2_update(Evas_GL_Texture *tex, DATA8 **rows, unsigned int w, unsigned int h);
Evas_GL_Texture  *evas_gl_common_texture_nv12_new(Evas_Engine_GL_Context *gc, DATA8 **rows, unsigned int w, unsigned int h);
void              evas_gl_common_texture_nv12_update(Evas_GL_Texture *tex, DATA8 **row, unsigned int w, unsigned int h);
Evas_GL_Texture  *evas_gl_common_texture_nv12tiled_new(Evas_Engine_GL_Context *gc, DATA8 **rows, unsigned int w, unsigned int h);
void              evas_gl_common_texture_nv12tiled_update(Evas_GL_Texture *tex, DATA8 **row, unsigned int w, unsigned int h);
Evas_GL_Texture  *evas_gl_common_texture_rgb_a_pair_new(Evas_Engine_GL_Context *gc, RGBA_Image *im);
void              evas_gl_common_texture_rgb_a_pair_update(Evas_GL_Texture *tex, RGBA_Image *im);

void              evas_gl_common_image_alloc_ensure(Evas_GL_Image *im);
void              evas_gl_common_image_all_unload(Evas_Engine_GL_Context *gc);

void              evas_gl_common_image_ref(Evas_GL_Image *im);
void              evas_gl_common_image_unref(Evas_GL_Image *im);
Evas_GL_Image    *evas_gl_common_image_load(Evas_Engine_GL_Context *gc, const char *file, const char *key, Evas_Image_Load_Opts *lo, int *error);
Evas_GL_Image    *evas_gl_common_image_new_from_data(Evas_Engine_GL_Context *gc, unsigned int w, unsigned int h, DATA32 *data, int alpha, int cspace);
Evas_GL_Image    *evas_gl_common_image_new_from_copied_data(Evas_Engine_GL_Context *gc, unsigned int w, unsigned int h, DATA32 *data, int alpha, int cspace);
Evas_GL_Image    *evas_gl_common_image_new(Evas_Engine_GL_Context *gc, unsigned int w, unsigned int h, int alpha, int cspace);
Evas_GL_Image    *evas_gl_common_image_alpha_set(Evas_GL_Image *im, int alpha);
void              evas_gl_common_image_native_enable(Evas_GL_Image *im);
void              evas_gl_common_image_native_disable(Evas_GL_Image *im);
void              evas_gl_common_image_scale_hint_set(Evas_GL_Image *im, int hint);
void              evas_gl_common_image_content_hint_set(Evas_GL_Image *im, int hint);
void              evas_gl_common_image_cache_flush(Evas_Engine_GL_Context *gc);
void              evas_gl_common_image_free(Evas_GL_Image *im);
Evas_GL_Image    *evas_gl_common_image_surface_new(Evas_Engine_GL_Context *gc, unsigned int w, unsigned int h, int alpha);
void              evas_gl_common_image_dirty(Evas_GL_Image *im, unsigned int x, unsigned int y, unsigned int w, unsigned int h);
void              evas_gl_common_image_update(Evas_Engine_GL_Context *gc, Evas_GL_Image *im);
void              evas_gl_common_image_map_draw(Evas_Engine_GL_Context *gc, Evas_GL_Image *im, int npoints, RGBA_Map_Point *p, int smooth, int level);
void              evas_gl_common_image_draw(Evas_Engine_GL_Context *gc, Evas_GL_Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, int smooth);

void             *evas_gl_font_texture_new(void *gc, RGBA_Font_Glyph *fg);
void              evas_gl_font_texture_free(void *);
void              evas_gl_font_texture_draw(void *gc, void *surface, void *dc, RGBA_Font_Glyph *fg, int x, int y);

Evas_GL_Polygon  *evas_gl_common_poly_point_add(Evas_GL_Polygon *poly, int x, int y);
Evas_GL_Polygon  *evas_gl_common_poly_points_clear(Evas_GL_Polygon *poly);
void              evas_gl_common_poly_draw(Evas_Engine_GL_Context *gc, Evas_GL_Polygon *poly, int x, int y);

void              evas_gl_common_line_draw(Evas_Engine_GL_Context *gc, int x1, int y1, int x2, int y2);

int               evas_gl_common_buffer_dump(Evas_Engine_GL_Context *gc, const char* dname, const char* fname, int frame, const char* suffix);

void evas_gl_common_texture_shared_specific(Evas_Engine_GL_Context *gc, Evas_GL_Texture_Pool *pt, int i);
void evas_gl_common_texture_shared_back(Evas_Engine_GL_Context *gc, Evas_GL_Texture_Pool *pt);

#if 0 // filtering disabled
void              evas_gl_common_filter_draw(Evas_Engine_GL_Context *context, Evas_GL_Image *im, Evas_Filter_Info *filter);
Filtered_Image   *evas_gl_common_image_filtered_get(Evas_GL_Image *im, uint8_t *key, size_t keylen);
Filtered_Image   *evas_gl_common_image_filtered_save(Evas_GL_Image *im, Evas_GL_Image *fimage, uint8_t *key, size_t keylen);
void              evas_gl_common_image_filtered_free(Evas_GL_Image *im, Filtered_Image *);
#endif

extern void       (*glsym_glGenFramebuffers)      (GLsizei a, GLuint *b);
extern void       (*glsym_glBindFramebuffer)      (GLenum a, GLuint b);
extern void       (*glsym_glFramebufferTexture2D) (GLenum a, GLenum b, GLenum c, GLuint d, GLint e);
extern void       (*glsym_glDeleteFramebuffers)   (GLsizei a, const GLuint *b);
extern void       (*glsym_glGetProgramBinary)     (GLuint a, GLsizei b, GLsizei *c, GLenum *d, void *e);
extern void       (*glsym_glProgramBinary)        (GLuint a, GLenum b, const void *c, GLint d);
extern void       (*glsym_glProgramParameteri)    (GLuint a, GLuint b, GLint d);
extern void       (*glsym_glReleaseShaderCompiler)(void);
extern void      *(*glsym_glMapBuffer)            (GLenum a, GLenum b);
extern GLboolean  (*glsym_glUnmapBuffer)          (GLenum a);

#ifdef GL_GLES
extern void          *(*secsym_eglCreateImage)               (void *a, void *b, GLenum c, void *d, const int *e);
extern unsigned int   (*secsym_eglDestroyImage)              (void *a, void *b);
extern void           (*secsym_glEGLImageTargetTexture2DOES) (int a, void *b);
extern void          *(*secsym_eglMapImageSEC)               (void *a, void *b, int c, int d);
extern unsigned int   (*secsym_eglUnmapImageSEC)             (void *a, void *b, int c);
extern unsigned int   (*secsym_eglGetImageAttribSEC)         (void *a, void *b, int c, int *d);


#define TBM_SURF_PLANE_MAX 4 /**< maximum number of the planes  */

/* option to map the tbm_surface */
#define TBM_SURF_OPTION_READ      (1 << 0) /**< access option to read  */
#define TBM_SURF_OPTION_WRITE     (1 << 1) /**< access option to write */

#define __tbm_fourcc_code(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
			      ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define TBM_FORMAT_C8       __tbm_fourcc_code('C', '8', ' ', ' ')
#define TBM_FORMAT_RGBA8888 __tbm_fourcc_code('R', 'A', '2', '4')
#define TBM_FORMAT_BGRA8888 __tbm_fourcc_code('B', 'A', '2', '4')
#define TBM_FORMAT_RGB888   __tbm_fourcc_code('R', 'G', '2', '4')

typedef struct _tbm_surface * tbm_surface_h;
typedef uint32_t tbm_format;
typedef struct _tbm_surface_plane
{
    unsigned char *ptr;   /**< Plane pointer */
    uint32_t size;        /**< Plane size */
    uint32_t offset;      /**< Plane offset */
    uint32_t stride;      /**< Plane stride */

    void *reserved1;      /**< Reserved pointer1 */
    void *reserved2;      /**< Reserved pointer2 */
    void *reserved3;      /**< Reserved pointer3 */
} tbm_surface_plane_s;

typedef struct _tbm_surface_info
{
    uint32_t width;      /**< TBM surface width */
    uint32_t height;     /**< TBM surface height */
    tbm_format format;   /**< TBM surface format*/
    uint32_t bpp;        /**< TBM surface bbp */
    uint32_t size;       /**< TBM surface size */

    uint32_t num_planes;                            /**< The number of planes */
    tbm_surface_plane_s planes[TBM_SURF_PLANE_MAX]; /**< Array of planes */

    void *reserved4;   /**< Reserved pointer4 */
    void *reserved5;   /**< Reserved pointer5 */
    void *reserved6;   /**< Reserved pointer6 */
} tbm_surface_info_s;


extern void *(*secsym_tbm_surface_create) (int width, int height, unsigned int format);
extern int   (*secsym_tbm_surface_destroy) (void *surface);
extern int   (*secsym_tbm_surface_map) (void *surface, int opt, void *info);
extern int   (*secsym_tbm_surface_unmap) (void *surface);
extern int   (*secsym_tbm_surface_get_info) (void *surface, void *info);
#endif

//#define GL_ERRORS 1

#ifdef GL_ERRORS
# define GLERR(fn, fl, ln, op) \
   { \
      int __gl_err = glGetError(); \
      if (__gl_err != GL_NO_ERROR) glerr(__gl_err, fl, fn, ln, op); \
   }
# define GLERRLOG() GLERR(__FUNCTION__, __FILE__, __LINE__, "")
#else
# define GLERR(fn, fl, ln, op)
# define GLERRLOG()
#endif

Eina_Bool evas_gl_common_module_open(void);
void      evas_gl_common_module_close(void);

#endif
