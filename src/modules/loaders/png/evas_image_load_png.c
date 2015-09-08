#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <png.h>
#include <setjmp.h>

#ifdef HAVE_EVIL
# include <Evil.h>
#endif

#ifdef _WIN32_WCE
# define E_FOPEN(file, mode) evil_fopen_native((file), (mode))
# define E_FREAD(buffer, size, count, stream) evil_fread_native(buffer, size, count, stream)
# define E_FCLOSE(stream) evil_fclose_native(stream)
#else
# define E_FOPEN(file, mode) fopen((file), (mode))
# define E_FREAD(buffer, size, count, stream) fread(buffer, size, count, stream)
# define E_FCLOSE(stream) fclose(stream)
#endif

#include "evas_common.h"
#include "evas_private.h"


#define PNG_BYTES_TO_CHECK 4


static Eina_Bool evas_image_load_file_head_png(Image_Entry *ie, const char *file, const char *key, int *error) EINA_ARG_NONNULL(1, 2, 4);
static Eina_Bool evas_image_load_file_data_png(Image_Entry *ie, const char *file, const char *key, int *error) EINA_ARG_NONNULL(1, 2, 4);

static Evas_Image_Load_Func evas_image_load_png_func =
{
  EINA_TRUE,
  evas_image_load_file_head_png,
  evas_image_load_file_data_png,
  NULL,
  EINA_FALSE
};

static const Evas_Colorspace cspace_grey[2] = {
   EVAS_COLORSPACE_GRY8,
   EVAS_COLORSPACE_ARGB8888
};

static const Evas_Colorspace cspace_grey_alpha[2] = {
   EVAS_COLORSPACE_AGRY88,
   EVAS_COLORSPACE_ARGB8888
};

static Eina_Bool
evas_image_load_file_head_png(Image_Entry *ie, const char *file, const char *key __UNUSED__, int *error)
{
   png_uint_32 w32, h32;
   FILE *f;
   png_structp png_ptr = NULL;
   png_infop info_ptr = NULL;
   int bit_depth, color_type, interlace_type;
   unsigned char buf[PNG_BYTES_TO_CHECK];
   char hasa;

   hasa = 0;
   f = E_FOPEN(file, "rb");
   if (!f)
     {
        ERR("File: '%s' does not exist\n", file);
	*error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
	return EINA_FALSE;
     }

   /* if we havent read the header before, set the header data */
   if (E_FREAD(buf, PNG_BYTES_TO_CHECK, 1, f) != 1)
     {
	*error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
	goto close_file;
     }

   if (png_sig_cmp(buf, 0, PNG_BYTES_TO_CHECK))
     {
	*error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
	goto close_file;
     }

   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (!png_ptr)
     {
	*error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	goto close_file;
     }

   info_ptr = png_create_info_struct(png_ptr);
   if (!info_ptr)
     {
	png_destroy_read_struct(&png_ptr, NULL, NULL);
	*error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	goto close_file;
     }
   if (setjmp(png_jmpbuf(png_ptr)))
     {
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	*error = EVAS_LOAD_ERROR_CORRUPT_FILE;
	goto close_file;
     }
   png_init_io(png_ptr, f);
   png_set_sig_bytes(png_ptr, PNG_BYTES_TO_CHECK);
   png_read_info(png_ptr, info_ptr);
   png_get_IHDR(png_ptr, info_ptr, (png_uint_32 *) (&w32),
		(png_uint_32 *) (&h32), &bit_depth, &color_type,
		&interlace_type, NULL, NULL);
   if ((w32 < 1) || (h32 < 1) || (w32 > IMG_MAX_SIZE) || (h32 > IMG_MAX_SIZE) ||
       IMG_TOO_BIG(w32, h32))
     {
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	if (IMG_TOO_BIG(w32, h32))
	  *error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	else
	  *error = EVAS_LOAD_ERROR_GENERIC;
	goto close_file;
     }
   if (ie->load_opts.scale_down_by > 1)
     {
        ie->w = (int) w32 / ie->load_opts.scale_down_by;
        ie->h = (int) h32 / ie->load_opts.scale_down_by;
        if ((ie->w < 1) || (ie->h < 1))
          {
             *error = EVAS_LOAD_ERROR_GENERIC;
             goto close_file;
          }
     }
   else
     {
        ie->w = (int) w32;
        ie->h = (int) h32;
     }
   if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) hasa = 1;
   switch (color_type)
     {
      case PNG_COLOR_TYPE_RGB_ALPHA:
         hasa = 1;
         break;
      case PNG_COLOR_TYPE_GRAY_ALPHA:
         hasa = 1;
         ie->cspaces = cspace_grey_alpha;
         break;
      case PNG_COLOR_TYPE_GRAY:
         if (!hasa) ie->cspaces = cspace_grey;
         break;
     }
   if (hasa) ie->flags.alpha = 1;
   png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
   E_FCLOSE(f);

   *error = EVAS_LOAD_ERROR_NONE;
   return EINA_TRUE;

 close_file:
   E_FCLOSE(f);
   return EINA_FALSE;
}

static Eina_Bool
evas_image_load_file_data_png(Image_Entry *ie, const char *file, const char *key __UNUSED__, int *error)
{
   unsigned char *surface;
   png_uint_32 w32, h32;
   int w, h;
   FILE *f;
   png_structp png_ptr = NULL;
   png_infop info_ptr = NULL;
   int bit_depth, color_type, interlace_type;
   unsigned char buf[PNG_BYTES_TO_CHECK];
   char hasa, passes;
   int i, j, p, k;
   int scale_ratio = 1, image_w = 0, image_h = 0;
   unsigned char *tmp_line;
   unsigned int pack_offset;

   hasa = 0;
   f = E_FOPEN(file, "rb");
   if (!f)
     {
	*error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
	return EINA_FALSE;
     }

   /* if we havent read the header before, set the header data */
   if (E_FREAD(buf, PNG_BYTES_TO_CHECK, 1, f) != 1)
     {
	*error = EVAS_LOAD_ERROR_CORRUPT_FILE;
        goto close_file;
     }
   if (png_sig_cmp(buf, 0, PNG_BYTES_TO_CHECK))
     {
	*error = EVAS_LOAD_ERROR_CORRUPT_FILE;
	goto close_file;
     }
   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (!png_ptr)
     {
	*error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	goto close_file;
     }

   info_ptr = png_create_info_struct(png_ptr);
   if (!info_ptr)
     {
	png_destroy_read_struct(&png_ptr, NULL, NULL);
	*error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	goto close_file;
     }
   if (setjmp(png_jmpbuf(png_ptr)))
     {
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	*error = EVAS_LOAD_ERROR_CORRUPT_FILE;
	goto close_file;
     }
   png_init_io(png_ptr, f);
   png_set_sig_bytes(png_ptr, PNG_BYTES_TO_CHECK);
   png_read_info(png_ptr, info_ptr);
   png_get_IHDR(png_ptr, info_ptr, (png_uint_32 *) (&w32),
		(png_uint_32 *) (&h32), &bit_depth, &color_type,
		&interlace_type, NULL, NULL);
   image_w = w32;
   image_h = h32;
   if (ie->load_opts.scale_down_by > 1)
     {
        scale_ratio = ie->load_opts.scale_down_by;
        w32 /= scale_ratio;
        h32 /= scale_ratio;
     }
   evas_cache_image_surface_alloc(ie, w32, h32);
   surface = (unsigned char *) evas_cache_image_pixels(ie);
   if (!surface)
     {
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	*error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	goto close_file;
     }
   if ((w32 != ie->w) || (h32 != ie->h))
     {
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	*error = EVAS_LOAD_ERROR_GENERIC;
	goto close_file;
     }
   if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
     {
        /* expand transparency entry -> alpha channel if present */
        png_set_tRNS_to_alpha(png_ptr);
        hasa = 1;
     }
   if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) hasa = 1;
   if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA) hasa = 1;
   if (hasa) ie->flags.alpha = 1;

   /* Prep for transformations...  ultimately we want ARGB */
   /* expand palette -> RGB if necessary */
   if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
   /* expand gray (w/reduced bits) -> 8-bit RGB if necessary */
   if ((color_type == PNG_COLOR_TYPE_GRAY) ||
       (color_type == PNG_COLOR_TYPE_GRAY_ALPHA))
     {
        if (ie->space == EVAS_COLORSPACE_ARGB8888)
          png_set_gray_to_rgb(png_ptr);
        if (bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
     }
   /* reduce 16bit color -> 8bit color if necessary */
   if (bit_depth > 8) png_set_strip_16(png_ptr);
   /* pack all pixels to byte boundaries */
   png_set_packing(png_ptr);

   w = w32;
   h = h32;

   switch (ie->space)
     {
      case EVAS_COLORSPACE_ARGB8888:
         /* we want ARGB */
#ifdef WORDS_BIGENDIAN
         png_set_swap_alpha(png_ptr);
         if (!hasa) png_set_filler(png_ptr, 0xff, PNG_FILLER_BEFORE);
#else
         png_set_bgr(png_ptr);
         if (!hasa) png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
#endif
         pack_offset = sizeof(DATA32);
         break;
      case EVAS_COLORSPACE_AGRY88:
         /* we want AGRY */
#ifdef WORDS_BIGENDIAN
         png_set_swap_alpha(png_ptr);
         if (!hasa) png_set_filler(png_ptr, 0xff, PNG_FILLER_BEFORE);
#else
         if (!hasa) png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
#endif
         pack_offset = sizeof(DATA16);
         break;
      case EVAS_COLORSPACE_GRY8: pack_offset = sizeof(DATA8); break;
      default: abort();
     }

   passes = png_set_interlace_handling(png_ptr);

   /* we read image line by line if scale down was set */
   if (scale_ratio == 1)
     {
        for (p = 0; p < passes; p++)
          {
             for (i = 0; i < h; i++)
               png_read_row(png_ptr, surface + (i * w * pack_offset), NULL);
          }
        png_read_end(png_ptr, info_ptr);
     }
   else
     {
        unsigned char *src_ptr, *dst_ptr;

        dst_ptr = surface;
        if (passes == 1)
          {
             tmp_line = (unsigned char *) alloca(image_w * pack_offset);
             for (i = 0; i < h; i++)
               {
                  png_read_row(png_ptr, tmp_line, NULL);
                  src_ptr = tmp_line;
                  for (j = 0; j < w; j++)
                    {
                       for (k = 0; k < (int)pack_offset; k++)
                         dst_ptr[k] = src_ptr[k];
                       dst_ptr += pack_offset;
                       src_ptr += scale_ratio * pack_offset;
                    }
                  for (j = 0; j < (scale_ratio - 1); j++)
                    png_read_row(png_ptr, tmp_line, NULL);
               }
          }
        else
          {
             unsigned char *pixels2 = malloc(image_w * image_h * pack_offset);

             if (pixels2)
               {
                  for (p = 0; p < passes; p++)
                    {
                       for (i = 0; i < image_h; i++)
                         png_read_row(png_ptr, pixels2 + (i * image_w * pack_offset), NULL);
                    }

                  for (i = 0; i < h; i++)
                    {
                       src_ptr = pixels2 + (i * scale_ratio * image_w * pack_offset);
                       for (j = 0; j < w; j++)
                         {
                            for (k = 0; k < (int)pack_offset; k++)
                              dst_ptr[k] = src_ptr[k];
                            src_ptr += scale_ratio * pack_offset;
                            dst_ptr += pack_offset;
                         }
                    }
                  free(pixels2);
               }
          }
     }

   png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
   E_FCLOSE(f);
   evas_common_image_premul(ie);

   *error = EVAS_LOAD_ERROR_NONE;
   return EINA_TRUE;

 close_file:
   E_FCLOSE(f);
   return EINA_FALSE;
}

static int
module_open(Evas_Module *em)
{
   if (!em) return 0;
   em->functions = (void *)(&evas_image_load_png_func);
   return 1;
}

static void
module_close(Evas_Module *em __UNUSED__)
{
}

static Evas_Module_Api evas_modapi =
{
   EVAS_MODULE_API_VERSION,
   "png",
   "none",
   {
     module_open,
     module_close
   }
};

EVAS_MODULE_DEFINE(EVAS_MODULE_TYPE_IMAGE_LOADER, image_loader, png);

#ifndef EVAS_STATIC_BUILD_PNG
EVAS_EINA_MODULE_DEFINE(image_loader, png);
#endif
