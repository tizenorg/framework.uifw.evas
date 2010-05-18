#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_EVIL
# include <Evil.h>
#endif

#include <stdio.h>
#include <sys/stat.h>

#include "evas_common.h"
#include "evas_private.h"

#define EVAS_WINK_MODULE_NAME wbmp
#include "evas_wink.h"

static Eina_Bool evas_image_load_file_head_wbmp(Image_Entry *ie, const char *file, const char *key, int *error) EINA_ARG_NONNULL(1, 2, 4);
static Eina_Bool evas_image_load_file_data_wbmp(Image_Entry *ie, const char *file, const char *key, int *error) EINA_ARG_NONNULL(1, 2, 4);

static Evas_Image_Load_Func evas_image_load_wbmp_func =
{
  EINA_TRUE,
  evas_image_load_file_head_wbmp,
  evas_image_load_file_data_wbmp
};


static int read_mb(unsigned int * data, FILE * f) {
   int ac = 0;
   int ct = 0;
   unsigned char buf;

   while (1) {
      if ((ct++) == 6)
         return -1;

      if ((fread(&buf, 1, 1, f)) < 1)
         return -1;

      ac = (ac << 7) | (buf & 0x7f);
      if ((buf & 0x80) == 0)
         break;
   }
   *data = ac;
   return 0;
}

static Eina_Bool
evas_image_load_file_head_wbmp(Image_Entry *ie, const char *file, const char *key __UNUSED__, int *error)
{
#ifdef USE_WINK_CODEC
   return evas_image_load_file_head_wbmp_wink(ie, file, key, error);
#endif

   FILE          * f;
   unsigned int       type;
   unsigned char      fixed_header;
   unsigned int       width, height;

   struct stat    statbuf;

   *error = EVAS_LOAD_ERROR_GENERIC;

   f = fopen(file, "rb");
   if (!f)
   {
      *error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
      return EINA_FALSE;
   }

   if (stat(file, &statbuf) == -1)
   	goto bail0;
 
   if (read_mb(&type, f) < 0)
      goto bail0;

   if (type != 0)
   {
   	*error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
	goto bail0;
   }
 
   if (fread(&fixed_header, 1, 1, f) != 1)
      goto bail0;
 
   if (read_mb(&width, f) < 0)
      goto bail0;
 
   if (read_mb(&height, f) < 0)
      goto bail0;

   fclose(f);

   ie->w = width;
   ie->h = height;

   *error = EVAS_LOAD_ERROR_NONE;

   return EINA_TRUE;

bail0:
   fclose(f);

   return EINA_FALSE;
}

static Eina_Bool
evas_image_load_file_data_wbmp(Image_Entry *ie, const char *file, const char *key __UNUSED__, int *error)
{
#ifdef USE_WINK_CODEC
   return evas_image_load_file_data_wbmp_wink(ie, file, key, error);
#endif

   FILE          * f;
   unsigned int       width, height;
   unsigned int       dummy;
   unsigned char      * line = NULL;
   unsigned int       line_length;
   int           cur = 0;
   int           i, j;
   DATA32        * dst_data;

   *error = EVAS_LOAD_ERROR_GENERIC;
   
   f = fopen(file, "rb");
   if (!f)
   {
      *error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
      return EINA_FALSE;
   }

   if (read_mb(&dummy, f) < 0)
      goto bail0;
   if (fread(&dummy, 1, 1, f) != 1)
      goto bail0;
   if (read_mb(&dummy, f) < 0)
      goto bail0;
   if (read_mb(&dummy, f) < 0)
      goto bail0;

   width = ie->w;
   height = ie->h;

   evas_cache_image_surface_alloc(ie, ie->w, ie->h);
   dst_data = evas_cache_image_pixels(ie);
   if (!dst_data)
   {
      *error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
      goto bail0;
   }
   
   line_length = (width + 7) >> 3;
   line = (unsigned char *) malloc(line_length);

   for(i=0;i<height;i++)
   {
      if (fread(line, 1, line_length, f) != line_length)
         goto bail0;

      for(j=0;j<width;j++)
      {
         int idx = j >> 3;
         int offset = 1 << (0x07 - (j & 0x07));
         if (line[idx] & offset)
            dst_data[cur] = 0xFFFFFFFF;
         else
            dst_data[cur] = 0xFF000000;
         cur++;
      }
   }

   if (line)
      free(line);

   fclose(f);

   *error = EVAS_LOAD_ERROR_NONE;

   return EINA_TRUE;

bail0:
   fclose(f);

   if (line)
      free(line);

   return EINA_FALSE;
}

static int
module_open(Evas_Module *em)
{
   if (!em) return 0;
   em->functions = (void *)(&evas_image_load_wbmp_func);
   return 1;
}

static void
module_close(Evas_Module *em)
{
}

static Evas_Module_Api evas_modapi =
{
  EVAS_MODULE_API_VERSION,
  "wbmp",
  "none",
  {
    module_open,
    module_close
  }
};

EVAS_MODULE_DEFINE(EVAS_MODULE_TYPE_IMAGE_LOADER, image_loader, wbmp);

#ifndef EVAS_STATIC_BUILD_WBMP
EVAS_EINA_MODULE_DEFINE(image_loader, wbmp);
#endif
