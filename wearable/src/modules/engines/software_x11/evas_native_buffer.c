#include "evas_common.h"
#include "evas_xlib_image.h"
#include "evas_private.h"

#include "Evas_Engine_Software_X11.h"
#include "evas_engine.h"

#ifdef HAVE_NATIVE_BUFFER
# include "native-buffer.h"

#define EVAS_ROUND_UP_4(num) (((num)+3) & ~3)
#define EVAS_ROUND_UP_8(num) (((num)+7) & ~7)

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
   im->image.data         = NULL;

   free(n);
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

   w = native_buffer_get_width(ns->data.tizen.buffer);
   h = native_buffer_get_height(ns->data.tizen.buffer);
   stride = native_buffer_get_stride(ns->data.tizen.buffer);
   format = native_buffer_get_format(ns->data.tizen.buffer);

   if (native_buffer_lock(ns->data.tizen.buffer, NATIVE_BUFFER_USAGE_CPU,
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
         evas_common_image_colorspace_dirty(im);
         break;
      case NATIVE_BUFFER_FORMAT_I420: /* EVAS_COLORSPACE_YCBCR422P601_PL */
         evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_YCBCR422P601_PL);
         _evas_video_i420(im->cs.data, pixels_data, w, h, h);
         evas_common_image_colorspace_dirty(im);
         break;
      case NATIVE_BUFFER_FORMAT_NV12: /* EVAS_COLORSPACE_YCBCR420NV12601_PL */
         evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_YCBCR420NV12601_PL);
         _evas_video_nv12(im->cs.data, pixels_data, w, h, h);
         evas_common_image_colorspace_dirty(im);
         break;
      case NATIVE_BUFFER_FORMAT_NV12T: /* EVAS_COLORSPACE_YCBCR420TM12601_PL */
         evas_cache_image_colorspace(&im->cache_entry, EVAS_COLORSPACE_YCBCR420TM12601_PL);
         /* How to figure out plan ?!? */
         evas_common_image_colorspace_dirty(im);
         break;

         /* Not planning to handle those in software */
      case NATIVE_BUFFER_FORMAT_NV21: /* Same as NATIVE_BUFFER_FORMAT_NV12, but with U/V reversed */
      case NATIVE_BUFFER_FORMAT_RGB_888:
      case NATIVE_BUFFER_FORMAT_RGB_565:
      case NATIVE_BUFFER_FORMAT_A_8:
      default:
         native_buffer_unlock(ns->data.tizen.buffer);
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

   native_buffer_unlock(ns->data.tizen.buffer);
   return im;
}
#endif
