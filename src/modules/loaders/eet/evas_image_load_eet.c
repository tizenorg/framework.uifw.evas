#ifdef HAVE_CONFIG_H
# include "config.h"  /* so that EAPI in Eet.h is correctly defined */
#endif

#include <Eet.h>

#include "evas_common.h"
#include "evas_private.h"


static Eina_Bool evas_image_load_file_head_eet(Image_Entry *ie, const char *file, const char *key, int *error) EINA_ARG_NONNULL(1, 2, 4);
static Eina_Bool evas_image_load_file_data_eet(Image_Entry *ie, const char *file, const char *key, int *error) EINA_ARG_NONNULL(1, 2, 4);

Evas_Image_Load_Func evas_image_load_eet_func =
{
  EINA_TRUE,
  evas_image_load_file_head_eet,
  evas_image_load_file_data_eet,
  NULL,
  EINA_FALSE
};

static const Evas_Colorspace cspaces_etc1[2] = {
  EVAS_COLORSPACE_ETC1,
  EVAS_COLORSPACE_ARGB8888
};

static const Evas_Colorspace cspaces_etc2_rgb[2] = {
  EVAS_COLORSPACE_RGB8_ETC2,
  EVAS_COLORSPACE_ARGB8888
};

static const Evas_Colorspace cspaces_etc2_rgba[2] = {
  EVAS_COLORSPACE_RGBA8_ETC2_EAC,
  EVAS_COLORSPACE_ARGB8888
};

static const Evas_Colorspace cspaces_etc1_alpha[2] = {
   EVAS_COLORSPACE_ETC1_ALPHA,
   EVAS_COLORSPACE_ARGB8888
};


static Eina_Bool
evas_image_load_file_head_eet(Image_Entry *ie, const char *file, const char *key, int *error)
{
   int                  alpha, compression, quality;
   Eet_Image_Encoding   lossy;
   const Eet_Colorspace *cspaces = NULL;
   unsigned int         w, h;
   Eet_File            *ef;
   int                  ok;
   Eina_Bool		res = EINA_FALSE;

   if (!key)
     {
	*error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
	return EINA_FALSE;
     }

   ef = eet_open((char *)file, EET_FILE_MODE_READ);
   if (!ef)
     {
	*error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
	return EINA_FALSE;
     }
   ok = eet_data_image_header_read(ef, key,
				   &w, &h, &alpha, &compression, &quality, &lossy);
   if (!ok)
     {
	*error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
	goto on_error;
     }
   if (IMG_TOO_BIG(w, h))
     {
	*error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	goto on_error;
     }

   if (eet_data_image_colorspace_get(ef, key, NULL, &cspaces))
     {
        if (cspaces)
          {
             unsigned int i;

             for (i = 0; cspaces[i] != EET_COLORSPACE_ARGB8888; i++)
               {
                  if (cspaces[i] == EET_COLORSPACE_ETC1)
                    {
                       ie->cspaces = cspaces_etc1;
                       break;
                    }
                  else if (cspaces[i] == EET_COLORSPACE_RGB8_ETC2)
                    {
                       ie->cspaces = cspaces_etc2_rgb;
                       break;
                    }
                  else if (cspaces[i] == EET_COLORSPACE_RGBA8_ETC2_EAC)
                    {
                       ie->cspaces = cspaces_etc2_rgba;
                       break;
                    }
                  else if (cspaces[i] == EET_COLORSPACE_ETC1_ALPHA)
                    {
                       ie->cspaces = cspaces_etc1_alpha;
                       break;
                    }
               }
          }
     }

   if (alpha) ie->flags.alpha = 1;
   ie->w = w;
   ie->h = h;
   res = EINA_TRUE;
   *error = EVAS_LOAD_ERROR_NONE;

 on_error:
   eet_close(ef);
   return res;
}

Eina_Bool
evas_image_load_file_data_eet(Image_Entry *ie, const char *file, const char *key, int *error)
{
   unsigned int         w, h;
   int                  alpha, compression, quality, ok;
   Eet_File            *ef;
   DATA32              *body, *p, *end, *data;
   DATA32               nas = 0;
   Eina_Bool		res = EINA_FALSE;
   Eet_Colorspace       cspace;
   Eet_Image_Encoding   lossy;

   switch (ie->space)
     {
      case EVAS_COLORSPACE_ETC1: cspace = EET_COLORSPACE_ETC1; break;
      case EVAS_COLORSPACE_ETC1_ALPHA: cspace = EET_COLORSPACE_ETC1_ALPHA; break;
      case EVAS_COLORSPACE_RGB8_ETC2: cspace = EET_COLORSPACE_RGB8_ETC2; break;
      case EVAS_COLORSPACE_RGBA8_ETC2_EAC: cspace = EET_COLORSPACE_RGBA8_ETC2_EAC; break;
      case EVAS_COLORSPACE_ARGB8888: cspace = EET_COLORSPACE_ARGB8888; break;
      default:
        *error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
        return EINA_FALSE;
     }

   if (!key)
     {
	*error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
	return EINA_FALSE;
     }
   if (ie->flags.loaded)
     {
	*error = EVAS_LOAD_ERROR_NONE;
	return EINA_TRUE;
     }
   ef = eet_open(file, EET_FILE_MODE_READ);
   if (!ef)
     {
	*error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
	return EINA_FALSE;
     }
   ok = eet_data_image_header_read(ef, key,
				   &w, &h, &alpha, &compression, &quality, &lossy);
   if (IMG_TOO_BIG(w, h))
     {
	*error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	goto on_error;
     }
   if (!ok)
     {
	*error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
	goto on_error;
     }
   evas_cache_image_surface_alloc(ie, w, h);
   data = evas_cache_image_pixels(ie);
   if (!data)
     {
	*error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	goto on_error;
     }

   ok = eet_data_image_read_to_cspace_surface_cipher(ef, key, NULL, 0, 0,
                                                     data, w, h, w * 4,
                                                     cspace,
                                                     &alpha, &compression, &quality, &lossy);
   if (!ok)
     {
	*error = EVAS_LOAD_ERROR_GENERIC;
	goto on_error;
     }
   if (alpha && (cspace == EET_COLORSPACE_ARGB8888))
     {
	ie->flags.alpha = 1;

	body = evas_cache_image_pixels(ie);

	end = body +(w * h);
	for (p = body; p < end; p++)
	  {
	     DATA32 r, g, b, a;

	     a = A_VAL(p);
	     r = R_VAL(p);
	     g = G_VAL(p);
	     b = B_VAL(p);
	     if ((a == 0) || (a == 255)) nas++;
	     if (r > a) r = a;
	     if (g > a) g = a;
	     if (b > a) b = a;
	     *p = ARGB_JOIN(a, r, g, b);
	  }
	if ((ALPHA_SPARSE_INV_FRACTION * nas) >= (ie->w * ie->h))
	  ie->flags.alpha_sparse = 1;
     }
// result is already premultiplied now if u compile with edje
//   evas_common_image_premul(im);
   *error = EVAS_LOAD_ERROR_NONE;
   res = EINA_TRUE;

 on_error:
   eet_close(ef);
   return res;
}

static int
module_open(Evas_Module *em)
{
   if (!em) return 0;
   em->functions = (void *)(&evas_image_load_eet_func);
   return 1;
}

static void
module_close(Evas_Module *em __UNUSED__)
{
}

static Evas_Module_Api evas_modapi =
{
   EVAS_MODULE_API_VERSION,
   "eet",
   "none",
   {
     module_open,
     module_close
   }
};

EVAS_MODULE_DEFINE(EVAS_MODULE_TYPE_IMAGE_LOADER, image_loader, eet);

#ifndef EVAS_STATIC_BUILD_EET
EVAS_EINA_MODULE_DEFINE(image_loader, eet);
#endif
