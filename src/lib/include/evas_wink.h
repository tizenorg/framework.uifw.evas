#ifndef EVAS_WINK_H
#define EVAS_WINK_H

#ifndef EVAS_WINK_MODULE_NAME
#error "EVAS_WINK_MODULE_NAME (bmp/wbmp/png/jpeg/gif) should be defined before include evas_wink.h"
#endif

#ifdef USE_WINK_CODEC

#define _LOAD_FILE_HEAD_WINK(module) evas_image_load_file_head_##module##_wink
#define _LOAD_FILE_DATA_WINK(module) evas_image_load_file_data_##module##_wink
#define LOAD_FILE_HEAD_WINK(module) _LOAD_FILE_HEAD_WINK(module)
#define LOAD_FILE_DATA_WINK(module) _LOAD_FILE_DATA_WINK(module)

#define _WINK_DEC_FROM_FILE(module) wink_dec_##module##_from_file
#define WINK_DEC_FROM_FILE(module) _WINK_DEC_FROM_FILE(module)

#define WK_IMG_TYPE(module) _WK_IMG_TYPE(module)
#define _WK_IMG_TYPE(module) _WK_IMG_TYPE_##module

#define _WK_IMG_TYPE_bmp   WK_IMG_TYPE_BMP
#define _WK_IMG_TYPE_wbmp  WK_IMG_TYPE_WBMP
#define _WK_IMG_TYPE_png   WK_IMG_TYPE_PNG
#define _WK_IMG_TYPE_jpeg  WK_IMG_TYPE_JPEG
#define _WK_IMG_TYPE_gif   WK_IMG_TYPE_GIF

#include "wink_codec_api.h"

static Eina_Bool LOAD_FILE_HEAD_WINK(EVAS_WINK_MODULE_NAME)(Image_Entry *ie, const char *file, const char *key __UNUSED__, int *error) EINA_ARG_NONNULL(1, 2, 4);
static Eina_Bool LOAD_FILE_DATA_WINK(EVAS_WINK_MODULE_NAME)(Image_Entry *ie, const char *file, const char *key __UNUSED__, int *error) EINA_ARG_NONNULL(1, 2, 4);

static Eina_Bool
LOAD_FILE_HEAD_WINK(EVAS_WINK_MODULE_NAME)(Image_Entry *ie, const char *file, const char *key __UNUSED__, int *error)
{
   WINKImageType type;
   WINKImageInfo image_info;

   type = wink_parse_from_file((WKUCHAR *) file, &image_info);

   if (type != WK_IMG_TYPE(EVAS_WINK_MODULE_NAME))
   {
   	*error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
   	return EINA_FALSE;
   }

   ie->w = image_info.width;
   ie->h = image_info.height;

   *error = EVAS_LOAD_ERROR_NONE;

   return EINA_TRUE;
}

static Eina_Bool
LOAD_FILE_DATA_WINK(EVAS_WINK_MODULE_NAME)(Image_Entry *ie, const char *file, const char *key __UNUSED__, int *error)
{
   WINKRawInfo * rawinfo;
   WINKDecOption option = 0;

   WINKResult result;

   DATA32 * pixels;

   rawinfo = wink_create_raw_info(ie->w, ie->h, WK_RAW_BGRA8888);
   if (rawinfo == NULL)
   {
      *error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
   	return EINA_FALSE;
   }

   result = WINK_DEC_FROM_FILE(EVAS_WINK_MODULE_NAME)((WKUCHAR *) file, rawinfo, option);

   if (result != WK_RESULT_SUCC)
   {
#ifdef WINK_SUPPORT_GET_LAST_ERROR
      WINKError errno = wink_get_last_error();
      switch(errno)
      {
      case WINK_ERR_NO_ERROR:
            *error = EVAS_LOAD_ERROR_NONE;
            break;
      case WINK_ERR_INVALID_INPUTADDRESS:
            *error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
            break;
      case WINK_ERR_CODEC_PARSE_FAIL:
      case WINK_ERR_CODEC_DEC_FAIL:
      case WINK_ERR_CODEC_ENC_FAIL:
            *error = EVAS_LOAD_ERROR_CORRUPT_FILE;
            break;
      case WINK_ERR_UNSUPPORTED_COLORSPACE:
      case WINK_ERR_UNSUPPORTED_TYPE:
            *error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
            break;
      case WINK_ERR_INVALID_INPUTSIZE:
      case WINK_ERR_INVALID_IMAGESIZE:
      case WINK_ERR_OUTOF_MEMORY:
      case WINK_ERR_NOT_ENOUGH_DISKSPACE:
            *error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
            break;
      default:
            *error = EVAS_LOAD_ERROR_GENERIC;
      }
#else
      *error = EVAS_LOAD_ERROR_GENERIC;
#endif
      wink_destroy_raw_info(rawinfo);
      return EINA_FALSE;
   }
   *error = EVAS_LOAD_ERROR_NONE;

   evas_cache_image_surface_alloc(ie, ie->w, ie->h);
   pixels = evas_cache_image_pixels(ie);
   if (!pixels)
   {
      *error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
      wink_destroy_raw_info(rawinfo);
      return EINA_FALSE;
   }

   memcpy(pixels, rawinfo->raw_stream, ie->w * ie->h * 4);

   wink_destroy_raw_info(rawinfo);
   return EINA_TRUE;
}
#endif /* USE_WINK_CODEC */

#endif /* EVAS_WINK_H */

