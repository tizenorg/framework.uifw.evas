#include "evas_common.h"
#include "evas_xlib_image.h"
#include "evas_private.h"

#include "Evas_Engine_Software_X11.h"
#include "evas_engine.h"

#ifdef HAVE_DLSYM
# include <dlfcn.h>      /* dlopen,dlclose,etc */
#else
# error evas_native_buffer should not get compiled if dlsym is not found on the system!
#endif

#define EVAS_ROUND_UP_4(num) (((num)+3) & ~3)
#define EVAS_ROUND_UP_8(num) (((num)+7) & ~7)

typedef struct native_buffer native_buffer_t;

typedef enum {
    STATUS_SUCCESS = 0,             /**< Success */
    STATUS_ERROR,                   /**< Error */
    STATUS_NO_MEMORY,               /**< Error: no memory */
    STATUS_NULL_POINTER,            /**< Error: NULL pointer */
    STATUS_EMPTY_POOL,              /**< Error: empty pool */
    STATUS_EGL_ERROR,               /**< Error: EGL error */
    STATUS_INVALID_BUFFER_STATE,    /**< Error: invalid buffer state */
} status_t;

typedef enum {
    NATIVE_BUFFER_FORMAT_INVALID,   /**< Invalid buffer format */
    NATIVE_BUFFER_FORMAT_RGBA_8888, /**< RGBA8888 */
    NATIVE_BUFFER_FORMAT_RGBX_8888, /**< RGBX8888 */
    NATIVE_BUFFER_FORMAT_RGB_888,   /**< RGB888 */
    NATIVE_BUFFER_FORMAT_RGB_565,   /**< RGB565 */
    NATIVE_BUFFER_FORMAT_BGRA_8888, /**< BGRA8888 */
    NATIVE_BUFFER_FORMAT_A_8,       /**< A8 */
    NATIVE_BUFFER_FORMAT_YV12,      /**< YV12 - 8bit Y plane followed by 8bit 2x2 subsampled V, U planes */
    NATIVE_BUFFER_FORMAT_I420,      /**< I420 - 8bit Y plane followed by 8bit 2x2 subsampled U, V planes */
    NATIVE_BUFFER_FORMAT_NV12,      /**< NV12 - 8bit Y plane followed by an interleaved U/V plane with 2x2 subsampling */
    NATIVE_BUFFER_FORMAT_NV21,      /**< NV21 - 8bit Y plane followed by an interleaved V/U plane with 2x2 subsampling */
    NATIVE_BUFFER_FORMAT_NV12T,     /**< NV12T - NV12, but use specified tile size */
    /* ... these formats will be extended over time from this point on */
} native_buffer_format_t;

typedef enum {
    NATIVE_BUFFER_USAGE_DEFAULT     = 0x00000000, /**< To get default handle */
    NATIVE_BUFFER_USAGE_CPU         = 0x00000001, /**< Can be read from to or written by the CPU */
    NATIVE_BUFFER_USAGE_2D          = 0x00000002, /**< Can be accessed by the 2D accelerator as either a source or a destination */
    NATIVE_BUFFER_USAGE_3D_TEXTURE  = 0x00000004, /**< Can be accessed by the 3D accelerator as a source texture buffer */
    NATIVE_BUFFER_USAGE_3D_RENDER   = 0x00000008, /**< Can be accessed by the 3D accelerator as a destination buffer */
    NATIVE_BUFFER_USAGE_MM          = 0x00000010, /**< Can be accessed by video codec decode or encode hardware */
    NATIVE_BUFFER_USAGE_DISPLAY     = 0x00000020, /**< Can be scanned out by the display */
    /* ... these usages will be extended over time from this point on */
} native_buffer_usage_t;

typedef enum {
    NATIVE_BUFFER_ACCESS_OPTION_READ  = (1 << 0),   /**< Buffer can be locked for reading */
    NATIVE_BUFFER_ACCESS_OPTION_WRITE = (1 << 1)    /**< Buffer can be locked for writing */
} native_buffer_access_option_t;

static status_t (*sym_native_buffer_lock) (native_buffer_t *buffer, int usage, int option, void** addr) = NULL;
static status_t (*sym_native_buffer_unlock) (native_buffer_t *buffer) = NULL;
static int (*sym_native_buffer_get_width) (const native_buffer_t *buffer) = NULL;
static int (*sym_native_buffer_get_height) (const native_buffer_t *buffer) = NULL;
static int (*sym_native_buffer_get_stride) (const native_buffer_t *buffer) = NULL;
static native_buffer_format_t (*sym_native_buffer_get_format) (const native_buffer_t *buffer) = NULL;

static void *native_buffer_lib = NULL;

static Eina_Bool
native_buffer_init(void)
{
   static int done = 0;

   if (done) return EINA_TRUE;

   const char *native_buffer_libs[] =
   {
      "libnative-buffer.so.0.1.0",
      "libnative-buffer.so.0.0.0",
      NULL,
   };
   int i, fail;
#define SYM(lib, xx)                            \
  do {                                          \
       sym_ ## xx = dlsym(lib, #xx);            \
       if (!(sym_ ## xx)) {                     \
            ERR("%s", dlerror()); \
            fail = 1;                           \
         }                                      \
    } while (0)

   if (native_buffer_lib) return EINA_TRUE;
   for (i = 0; native_buffer_libs[i]; i++)
     {
        native_buffer_lib = dlopen(native_buffer_libs[i], RTLD_LOCAL | RTLD_LAZY);
        if (native_buffer_lib)
          {
             fail = 0;
             SYM(native_buffer_lib, native_buffer_lock);
             SYM(native_buffer_lib, native_buffer_unlock);
             SYM(native_buffer_lib, native_buffer_get_width);
             SYM(native_buffer_lib, native_buffer_get_height);
             SYM(native_buffer_lib, native_buffer_get_stride);
             SYM(native_buffer_lib, native_buffer_get_format);
             if (fail)
               {
                  dlclose(native_buffer_lib);
                  native_buffer_lib = NULL;
               }
             else break;
          }
     }
   if (!native_buffer_lib) return EINA_FALSE;

   done = 1;
   return EINA_TRUE;
}

static void
native_buffer_shutdown(void)
{
   if (native_buffer_lib)
   {
      dlclose(native_buffer_lib);
      native_buffer_lib = NULL;
   }
}

static void
_evas_video_yv12(unsigned char *evas_data, const unsigned char *source_data, unsigned int w, unsigned int h, unsigned int output_height)
{
   const unsigned char **rows;
   unsigned int i, j;
   unsigned int rh;
   unsigned int stride_y, stride_uv;

   rh = output_height;

   rows = (const unsigned char **)evas_data;

   stride_y = EVAS_ROUND_UP_4(w);
   stride_uv = EVAS_ROUND_UP_8(w) / 2;

   for (i = 0; i < rh; i++)
     rows[i] = &source_data[i * stride_y];

   for (j = 0; j < (rh / 2); j++, i++)
     rows[i] = &source_data[h * stride_y +
                            (rh / 2) * stride_uv +
                            j * stride_uv];

   for (j = 0; j < (rh / 2); j++, i++)
     rows[i] = &source_data[h * stride_y + j * stride_uv];
}

static void
_evas_video_i420(unsigned char *evas_data, const unsigned char *source_data, unsigned int w, unsigned int h, unsigned int output_height)
{
   const unsigned char **rows;
   unsigned int i, j;
   unsigned int rh;
   unsigned int stride_y, stride_uv;

   rh = output_height;

   rows = (const unsigned char **)evas_data;

   stride_y = EVAS_ROUND_UP_4(w);
   stride_uv = EVAS_ROUND_UP_8(w) / 2;

   for (i = 0; i < rh; i++)
     rows[i] = &source_data[i * stride_y];

   for (j = 0; j < (rh / 2); j++, i++)
     rows[i] = &source_data[h * stride_y + j * stride_uv];

   for (j = 0; j < (rh / 2); j++, i++)
     rows[i] = &source_data[h * stride_y +
                            (rh / 2) * stride_uv +
                            j * stride_uv];
}

static void
_evas_video_nv12(unsigned char *evas_data, const unsigned char *source_data, unsigned int w, unsigned int h EINA_UNUSED, unsigned int output_height)
{
   const unsigned char **rows;
   unsigned int i, j;
   unsigned int rh;

   rh = output_height;

   rows = (const unsigned char **)evas_data;

   for (i = 0; i < rh; i++)
     rows[i] = &source_data[i * w];

   for (j = 0; j < (rh / 2); j++, i++)
     rows[i] = &source_data[rh * w + j * w];
}

static void
_native_free_cb(void *data EINA_UNUSED, void *image)
{
   RGBA_Image *im = image;
   Evas_Native_Surface *n = im->native.data;

   im->native.data        = NULL;
   im->native.func.data   = NULL;
   im->native.func.free   = NULL;

   free(n);

   native_buffer_shutdown();
}

static void
_evas_rgba_image_data_alloc(RGBA_Image *im, unsigned int w, unsigned int h)
{
   if (im->image.data) free(im->image.data);

   im->image.data = calloc(1, w * h * sizeof(DATA32));
   if (!im->image.data) return;

   ((Image_Entry *)im)->allocated.w = w;
   ((Image_Entry *)im)->allocated.h = h;
}

void *
evas_native_buffer_image_set(void *data EINA_UNUSED, void *image, void *native)
{
   Evas_Native_Surface *ns = native;
   RGBA_Image *im = image;
   /* Outbuf *ob = (Outbuf *)data; */

   void *pixels_data;
   int w, h, stride;
   native_buffer_format_t format;

   if (!native_buffer_init())
     {
        ERR("Could not initialize native buffer!");
        return NULL;
     }

   w = sym_native_buffer_get_width(ns->data.tizen.buffer);
   h = sym_native_buffer_get_height(ns->data.tizen.buffer);
   stride = sym_native_buffer_get_stride(ns->data.tizen.buffer);
   format = sym_native_buffer_get_format(ns->data.tizen.buffer);

   if (sym_native_buffer_lock(ns->data.tizen.buffer, NATIVE_BUFFER_USAGE_CPU,
                          NATIVE_BUFFER_ACCESS_OPTION_READ, &pixels_data) != STATUS_SUCCESS)
     return im;

   im->cache_entry.w = stride;
   im->cache_entry.h = h;

   // Handle all possible format here :"(
   switch (format)
     {
      case NATIVE_BUFFER_FORMAT_RGBA_8888:
      case NATIVE_BUFFER_FORMAT_RGBX_8888:
      case NATIVE_BUFFER_FORMAT_BGRA_8888:
         im->cache_entry.w /= 4;
         evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_ARGB8888);
         im->cache_entry.flags.alpha = format == NATIVE_BUFFER_FORMAT_RGBX_8888 ? 0 : 1;
         im->image.data = pixels_data;
         break;
         /* borrowing code from emotion here */
      case NATIVE_BUFFER_FORMAT_YV12: /* EVAS_COLORSPACE_YCBCR422P601_PL */
         evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_YCBCR422P601_PL);
         _evas_video_yv12(im->cs.data, pixels_data, w, h, h);
         _evas_rgba_image_data_alloc(im, w, h);
         evas_common_image_colorspace_dirty(im);
         break;
      case NATIVE_BUFFER_FORMAT_I420: /* EVAS_COLORSPACE_YCBCR422P601_PL */
         evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_YCBCR422P601_PL);
         _evas_video_i420(im->cs.data, pixels_data, w, h, h);
         _evas_rgba_image_data_alloc(im, w, h);
         evas_common_image_colorspace_dirty(im);
         break;
      case NATIVE_BUFFER_FORMAT_NV12: /* EVAS_COLORSPACE_YCBCR420NV12601_PL */
         evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_YCBCR420NV12601_PL);
         _evas_video_nv12(im->cs.data, pixels_data, w, h, h);
         _evas_rgba_image_data_alloc(im, w, h);
         evas_common_image_colorspace_dirty(im);
         break;
      case NATIVE_BUFFER_FORMAT_NV12T: /* EVAS_COLORSPACE_YCBCR420TM12601_PL */
         evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_YCBCR420TM12601_PL);
         /* How to figure out plan ?!? */
         _evas_rgba_image_data_alloc(im, w, h);
         evas_common_image_colorspace_dirty(im);
         break;

         /* Not planning to handle those in software */
      case NATIVE_BUFFER_FORMAT_NV21: /* Same as NATIVE_BUFFER_FORMAT_NV12, but with U/V reversed */
      case NATIVE_BUFFER_FORMAT_RGB_888:
      case NATIVE_BUFFER_FORMAT_RGB_565:
      case NATIVE_BUFFER_FORMAT_A_8:
      default:
         sym_native_buffer_unlock(ns->data.tizen.buffer);
         return im;
     }

   if (ns)
     {
        Native *n;

        n = calloc(1, sizeof(Native));
        if (n)
          {
             memcpy(n, ns, sizeof(Evas_Native_Surface));
             im->native.data = n;
             im->native.func.data = pixels_data;
             im->native.func.free = _native_free_cb;
          }
     }

   sym_native_buffer_unlock(ns->data.tizen.buffer);
   return im;
}

