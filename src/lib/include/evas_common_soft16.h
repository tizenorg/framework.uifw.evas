#ifndef EVAS_COMMON_SOFT16_H
#define EVAS_COMMON_SOFT16_H

#include "evas_common.h"
#include "evas_private.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RGB_565_UNPACKED_MASK 0x07e0f81f
#define RGB_565_UNPACK(rgb)                                             \
   (((rgb) | ((rgb) << 16)) & RGB_565_UNPACKED_MASK)
#define RGB_565_PACK(rgb)                                               \
  ((((rgb) & RGB_565_UNPACKED_MASK) |                                   \
   ((rgb) & RGB_565_UNPACKED_MASK) >> 16) & 0xffff)
#define RGB_565_UNPACKED_BLEND(a, b, alpha)                             \
   ((b) + (a) - ((((b) * (alpha)) >> 5) & RGB_565_UNPACKED_MASK))
#define RGB_565_UNPACKED_BLEND_UNMUL(a, b, alpha)                       \
   ((b) + ((((a) - (b)) * (alpha)) >> 5))

#define RGB_565_FROM_COMPONENTS(r, g, b)                                \
  (((((r) >> 3) & 0x1f) << 11) |                                        \
   ((((g) >> 2) & 0x3f) << 5) |                                         \
   (((b) >> 3) & 0x1f))

static inline unsigned int
_calc_stride(unsigned int w)
{
   unsigned int pad = w % 4;
   if (!pad)  return w;
   else return w + 4 - pad;
}

#define IMG_BYTE_SIZE(stride, height, has_alpha)                       \
   ((stride) * (height) * (!(has_alpha) ? 2 : 3))

typedef struct _Soft16_Image Soft16_Image;
struct _Soft16_Image
{
   Image_Entry    cache_entry;

   RGBA_Image    *source;

   int            stride;     // pixel stride - likely a multiple of 2
   DATA16        *pixels;     // 16bpp pixels rgb565
   DATA8         *alpha;      // 8bit alpha mask - optional. points into pixels

   struct
   {
/*      unsigned char  have_alpha  : 1; // 1 if we have halpha */
     unsigned char  free_pixels : 1; // 1 if pixels should be freed
     unsigned char  free_alpha  : 1; // 1 if alpha mask should be freed
   } flags;
};

/**
 * Image (evas_soft16_main.c)
 */
EAPI void                evas_common_soft16_image_init(void);
EAPI void                evas_common_soft16_image_shutdown(void);
EAPI Evas_Cache_Image   *evas_common_soft16_image_cache_get(void);

EAPI void                evas_common_soft16_image_draw(Soft16_Image *src, Soft16_Image *dst, RGBA_Draw_Context *dc, int src_region_x, int src_region_y, int src_region_w, int src_region_h, int dst_region_x, int dst_region_y, int dst_region_w, int dst_region_h, int smooth);
EAPI Soft16_Image       *evas_common_soft16_image_alpha_set(Soft16_Image *im, int have_alpha);

void                     evas_common_soft16_image_draw_unscaled(Soft16_Image *src, Soft16_Image *dst, RGBA_Draw_Context *dc, const Eina_Rectangle sr, const Eina_Rectangle dr, const Eina_Rectangle cr);
void                     evas_common_soft16_image_draw_scaled_sampled(Soft16_Image *src, Soft16_Image *dst, RGBA_Draw_Context *dc, const Eina_Rectangle sr, const Eina_Rectangle dr, const Eina_Rectangle cr);

/* convert/dither functions */
void                     evas_common_soft16_image_convert_from_rgb(Soft16_Image *im, const DATA32 *src);
void                     evas_common_soft16_image_convert_from_rgba(Soft16_Image *im, const DATA32 *src);

/**
 * Rectangle (evas_soft16_rectangle.c)
 */
EAPI void                evas_common_soft16_rectangle_draw(Soft16_Image *dst, RGBA_Draw_Context *dc, int x, int y, int w, int h);

/**
 * Polygon (evas_soft16_polygon.c)
 */
  EAPI void                evas_common_soft16_polygon_draw(Soft16_Image *dst, RGBA_Draw_Context *dc, RGBA_Polygon_Point *points, int x, int y);

/**
 * Line (evas_soft16_line.c)
 */
EAPI void                evas_common_soft16_line_draw(Soft16_Image *dst, RGBA_Draw_Context *dc, int x0, int y0, int x1, int y1);

/**
 * Font (evas_soft16_font.c)
 */
EAPI void               *evas_common_soft16_font_glyph_new(void *data, RGBA_Font_Glyph *fg);
EAPI void                evas_common_soft16_font_glyph_free(void *ext_dat);
EAPI void                evas_common_soft16_font_glyph_draw(void *data, void *dest, void *context, RGBA_Font_Glyph *fg, int x, int y);

#ifdef __cplusplus
}
#endif

#endif
