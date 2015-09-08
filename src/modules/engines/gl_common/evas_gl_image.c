#include "evas_gl_private.h"

void
evas_gl_common_image_alloc_ensure(Evas_GL_Image *im)
{
   if (!im->im) return;
   im->im = (RGBA_Image *)evas_cache_image_size_set(&im->im->cache_entry,
                                                    im->w, im->h);
}

void
evas_gl_common_image_all_unload(Evas_Engine_GL_Context *gc)
{
   Eina_List *l;
   Evas_GL_Image *im;

   EINA_LIST_FOREACH(gc->shared->images, l, im)
     {
        if (im->im) evas_cache_image_unload_data(&im->im->cache_entry);
        if (im->tex)
          {
             if (!im->tex->pt->dyn.img)
               {
                  evas_gl_common_texture_free(im->tex);
                  im->tex = NULL;
               }
          }
     }
}

static void
_evas_gl_image_cache_trim(Evas_Engine_GL_Context *gc)
{
   int size = evas_common_image_get_cache();

   while (gc->shared->images_size > size)
     {
        Evas_GL_Image *im2;
        Eina_List *l = NULL;

        EINA_LIST_REVERSE_FOREACH(gc->shared->images, l, im2)
          {
             if (im2->references == 0)
               {
                  im2->cached = 0;
                  im2->gc->shared->images =
                     eina_list_remove_list(im2->gc->shared->images, l);
                  im2->gc->shared->images_size -= (im2->csize);
                  evas_gl_common_image_free(im2);
                  l = NULL;
                  break;
               }
          }
        if ((gc->shared->images_size > size) && (l))
          {
//             printf("EEK %i > %i, no 0 ref imgs\n",
//                    gc->shared->images_size, size);
             break;
          }
        if (!gc->shared->images)
          {
//             printf("EEK %i > %i, no imgs\n",
//                    gc->shared->images_size, size);
             break;
          }
     }
}

static Eina_Bool
_evas_gl_image_cache_add(Evas_GL_Image *im)
{
   if (im->references == 0)
     {
        im->csize = im->w * im->h * 4;
        im->gc->shared->images_size += im->csize;
        _evas_gl_image_cache_trim(im->gc);
        return EINA_TRUE;
     }
   else
     {
        im->gc->shared->images = eina_list_remove(im->gc->shared->images, im);
        im->cached = 0;
     }
   return EINA_FALSE;
}

void
evas_gl_common_image_ref(Evas_GL_Image *im)
{
   if (im->references == 0)
     {
        im->gc->shared->images_size -= (im->csize);
     }
   im->references++;
}

void
evas_gl_common_image_unref(Evas_GL_Image *im)
{
   im->references--;
   if (im->references == 0)
     {
        _evas_gl_image_cache_add(im);
     }
}

static void
_evas_gl_cspace_list_fill(Evas_Engine_GL_Context *gc)
{
#define CS_APPEND(cs) gc->shared->info.cspaces = eina_list_append \
   (gc->shared->info.cspaces, (void *) (intptr_t) cs)
   if (gc->shared->info.etc2)
     {
        CS_APPEND(EVAS_COLORSPACE_RGBA8_ETC2_EAC);
        CS_APPEND(EVAS_COLORSPACE_RGB8_ETC2);
        CS_APPEND(EVAS_COLORSPACE_ETC1);
        CS_APPEND(EVAS_COLORSPACE_ETC1_ALPHA);
     }
   else if (gc->shared->info.etc1)
     {
        CS_APPEND(EVAS_COLORSPACE_ETC1);
        CS_APPEND(EVAS_COLORSPACE_ETC1_ALPHA);
     }
   CS_APPEND(EVAS_COLORSPACE_GRY8);
   CS_APPEND(EVAS_COLORSPACE_AGRY88);
   CS_APPEND(EVAS_COLORSPACE_ARGB8888);
}

Evas_GL_Image *
evas_gl_common_image_load(Evas_Engine_GL_Context *gc, const char *file, const char *key, Evas_Image_Load_Opts *lo, int *error)
{
   Evas_GL_Image        *im;
   RGBA_Image           *im_im;
   Eina_List            *l;
   Evas_Colorspace cspace = EVAS_COLORSPACE_ARGB8888;

   im_im = evas_common_load_image_from_file(file, key, lo, error);
   if (!im_im) return NULL;

   /* i'd LOVe to do this, but we can't because we load to load header
    * to get image size to know if its too big or not! so this disallows
    * us to know that - photocam thus suffers
   if (((int)im_im->cache_entry.w > gc->shared->info.max_texture_size) ||
       ((int)im_im->cache_entry.h > gc->shared->info.max_texture_size))
     {
        evas_cache_image_drop(&(im_im->cache_entry));
        *error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
        return NULL;
     }
    */

   // FIXME: keep unreffed shared images around
   EINA_LIST_FOREACH(gc->shared->images, l, im)
     {
	if (im->im == im_im)
	  {
        // drop a reference in  evas_cache as gl_cache is taking a reference.
        evas_cache_image_drop(&(im_im->cache_entry));
	     gc->shared->images = eina_list_remove_list(gc->shared->images, l);
	     gc->shared->images = eina_list_prepend(gc->shared->images, im);
        evas_gl_common_image_ref(im);
	     *error = EVAS_LOAD_ERROR_NONE;
	     return im;
	  }
     }

   im = calloc(1, sizeof(Evas_GL_Image));
   if (!im)
     {
        evas_cache_image_drop(&(im_im->cache_entry));
	*error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	return NULL;
     }
   if (im_im->cache_entry.cspaces)
     {
        Evas_Colorspace cs;
        unsigned int i;
        Eina_List *l2;
        void *ldata;

        if (!gc->shared->info.cspaces)
          _evas_gl_cspace_list_fill(gc);

        cspace = EVAS_COLORSPACE_ARGB8888;
        for (i = 0; im_im->cache_entry.cspaces[i] != EVAS_COLORSPACE_ARGB8888; i++)
          EINA_LIST_FOREACH(gc->shared->info.cspaces, l2, ldata)
            {
               cs = (Evas_Colorspace) (intptr_t) ldata;
               if (cs == im_im->cache_entry.cspaces[i])
                 {
                    cspace = cs;
                    goto found_cspace;
                 }
            }

found_cspace:
        // ETC2 is backwards compatible with ETC1 but we prefer ETC2
        if (cspace == EVAS_COLORSPACE_ETC1 && gc->shared->info.etc2)
          cspace = EVAS_COLORSPACE_RGB8_ETC2;

        im_im->cache_entry.space = cspace;
     }
   im->references = 1;
   im->im = im_im;
   im->gc = gc;
   im->cached = 1;
   im->cs.space = cspace;
   im->alpha = im->im->cache_entry.flags.alpha;
   im->w = im->im->cache_entry.w;
   im->h = im->im->cache_entry.h;
   im->native.target = GL_TEXTURE_2D;
   if (lo) im->load_opts = *lo;
   gc->shared->images = eina_list_prepend(gc->shared->images, im);
   return im;
}

Evas_GL_Image *
evas_gl_common_image_new_from_data(Evas_Engine_GL_Context *gc, unsigned int w, unsigned int h, DATA32 *data, int alpha, int cspace)
{
   Evas_GL_Image *im;
   Eina_List *l;

   if (((int)w > gc->shared->info.max_texture_size) ||
       ((int)h > gc->shared->info.max_texture_size))
     {
        ERR("Image w :%d h :%d greater than supported value: %d", w, h,
           gc->shared->info.max_texture_size);
        return NULL;
     }

   if (data)
     {
        EINA_LIST_FOREACH(gc->shared->images, l, im)
          {
             if (((void *)(im->im->image.data) == (void *)data) &&
                 (im->im->cache_entry.w == w) &&
                 (im->im->cache_entry.h == h))
               {
                  gc->shared->images = eina_list_remove_list(gc->shared->images, l);
                  gc->shared->images = eina_list_prepend(gc->shared->images, im);
                  evas_gl_common_image_ref(im);
                  return im;
               }
          }
     }
   im = calloc(1, sizeof(Evas_GL_Image));
   if (!im) return NULL;
   im->references = 1;
   im->im = (RGBA_Image *) evas_cache_image_data(evas_common_image_cache_get(),
                                                 w, h, data, alpha, cspace);
   if (!im->im)
     {
	free(im);
	return NULL;
     }
   im->native.target = GL_TEXTURE_2D;
   im->gc = gc;
   im->cs.space = cspace;
   im->alpha = im->im->cache_entry.flags.alpha;
   im->w = im->im->cache_entry.w;
   im->h = im->im->cache_entry.h;
   switch (cspace)
     {
      case EVAS_COLORSPACE_ARGB8888:
      case EVAS_COLORSPACE_GRY8:
      case EVAS_COLORSPACE_AGRY88:
        break;
      case EVAS_COLORSPACE_ETC1:
        if (gc->shared->info.etc1 && !gc->shared->info.etc2) break;
        ERR("We don't know what to do with ETC1 on this hardware. You need to add a software converter here.");
        break;
      case EVAS_COLORSPACE_RGB8_ETC2:
      case EVAS_COLORSPACE_RGBA8_ETC2_EAC:
        if (gc->shared->info.etc2) break;
        ERR("We don't know what to do with ETC2 on this hardware. You need to add a software converter here.");
        break;
      case EVAS_COLORSPACE_YCBCR422P601_PL:
      case EVAS_COLORSPACE_YCBCR422P709_PL:
        if (im->tex) evas_gl_common_texture_free(im->tex);
	im->tex = NULL;
	im->cs.data = data;
	im->cs.no_free = 1;
	break;
      default:
	abort();
	break;
     }
   return im;
}

Evas_GL_Image *
evas_gl_common_image_new_from_copied_data(Evas_Engine_GL_Context *gc, unsigned int w, unsigned int h, DATA32 *data, int alpha, int cspace)
{
   Evas_GL_Image *im;

   if (((int)w > gc->shared->info.max_texture_size) ||
       ((int)h > gc->shared->info.max_texture_size))
   {
      ERR("Image w :%d h :%d greater than supported value: %d", w, h,
         gc->shared->info.max_texture_size);
      return NULL;
   }

   im = calloc(1, sizeof(Evas_GL_Image));
   if (!im) return NULL;
   im->references = 1;
   im->im = (RGBA_Image *) evas_cache_image_copied_data(evas_common_image_cache_get(),
                                                        w, h, data, alpha, cspace);
   if (!im->im)
     {
	free(im);
	return NULL;
     }
   im->native.target = GL_TEXTURE_2D;
   im->gc = gc;
   im->cs.space = cspace;
   im->alpha = im->im->cache_entry.flags.alpha;
   im->w = im->im->cache_entry.w;
   im->h = im->im->cache_entry.h;
   switch (cspace)
     {
      case EVAS_COLORSPACE_ARGB8888:
      case EVAS_COLORSPACE_GRY8:
      case EVAS_COLORSPACE_AGRY88:
        break;
      case EVAS_COLORSPACE_ETC1:
        if (gc->shared->info.etc1 && !gc->shared->info.etc2) break;
        ERR("We don't know what to do with ETC1 on this hardware. You need to add a software converter here.");
        break;
      case EVAS_COLORSPACE_RGB8_ETC2:
      case EVAS_COLORSPACE_RGBA8_ETC2_EAC:
        if (gc->shared->info.etc2) break;
        ERR("We don't know what to do with ETC2 on this hardware. You need to add a software converter here.");
        break;
      case EVAS_COLORSPACE_YCBCR422P601_PL:
      case EVAS_COLORSPACE_YCBCR422P709_PL:
        if (im->tex) evas_gl_common_texture_free(im->tex);
        im->tex = NULL;
	im->cs.no_free = 0;
        if (im->im->cache_entry.h > 0)
          im->cs.data = calloc(1, im->im->cache_entry.h * sizeof(unsigned char *) * 2);
        if ((data) && (im->cs.data))
	  memcpy(im->cs.data, data, im->im->cache_entry.h * sizeof(unsigned char *) * 2);
	break;
      default:
	abort();
	break;
     }
   return im;
}

Evas_GL_Image *
evas_gl_common_image_new(Evas_Engine_GL_Context *gc, unsigned int w, unsigned int h, int alpha, int cspace)
{
   Evas_GL_Image *im;

   if (((int)w > gc->shared->info.max_texture_size) ||
       ((int)h > gc->shared->info.max_texture_size))
   {
      ERR("Image w :%d h :%d greater than supported value: %d", w, h,
         gc->shared->info.max_texture_size);
      return NULL;
   }

   im = calloc(1, sizeof(Evas_GL_Image));
   if (!im) return NULL;
   im->references = 1;
   im->im = (RGBA_Image *)evas_cache_image_empty(evas_common_image_cache_get());
   if (!im->im)
     {
	free(im);
	return NULL;
     }
   im->native.target = GL_TEXTURE_2D;
   im->gc = gc;
   im->im->cache_entry.flags.alpha = alpha ? 1 : 0;
   im->cs.space = cspace;
   im->alpha = im->im->cache_entry.flags.alpha;
   im->im->cache_entry.w = w;
   im->im->cache_entry.h = h;
   im->w = im->im->cache_entry.w;
   im->h = im->im->cache_entry.h;
   evas_cache_image_colorspace(&im->im->cache_entry, cspace);
   im->im = (RGBA_Image *)evas_cache_image_size_set(&im->im->cache_entry, w, h);
   switch (cspace)
     {
      case EVAS_COLORSPACE_ARGB8888:
      case EVAS_COLORSPACE_GRY8:
      case EVAS_COLORSPACE_AGRY88:
        break;
      case EVAS_COLORSPACE_ETC1:
        if (gc->shared->info.etc1 && !gc->shared->info.etc2) break;
        ERR("We don't know what to do with ETC1 on this hardware. You need to add a software converter here.");
        break;
      case EVAS_COLORSPACE_RGB8_ETC2:
      case EVAS_COLORSPACE_RGBA8_ETC2_EAC:
        if (gc->shared->info.etc2) break;
        ERR("We don't know what to do with ETC2 on this hardware. You need to add a software converter here.");
        break;
      case EVAS_COLORSPACE_YCBCR422P601_PL:
      case EVAS_COLORSPACE_YCBCR422P709_PL:
      case EVAS_COLORSPACE_YCBCR422601_PL:
      case EVAS_COLORSPACE_YCBCR420NV12601_PL:
      case EVAS_COLORSPACE_YCBCR420TM12601_PL:
//        if (im->tex) evas_gl_common_texture_free(im->tex);
	im->tex = NULL;
	im->cs.no_free = 0;
        if (im->im->cache_entry.h > 0)
          im->cs.data = calloc(1, im->im->cache_entry.h * sizeof(unsigned char *) * 2);
	break;
      default:
	abort();
	break;
     }
   return im;
}

Evas_GL_Image *
evas_gl_common_image_alpha_set(Evas_GL_Image *im, int alpha)
{
   if (!im) return NULL;
   if (im->alpha == alpha) return im;
   im->alpha = alpha;
   if (!im->im) return im;
   evas_gl_common_image_alloc_ensure(im);
   evas_cache_image_load_data(&im->im->cache_entry);
   im->im->cache_entry.flags.alpha = alpha ? 1 : 0;

   if (im->tex) evas_gl_common_texture_free(im->tex);
   if (im->tex_only)
     {
        im->tex = evas_gl_common_texture_native_new(im->gc, im->w, im->h,
                                                    im->alpha, im);
     }
   else
     {
        im->tex = evas_gl_common_texture_new(im->gc, im->im);
        if (im->tex) evas_gl_common_texture_update(im->tex, im->im);
     }
   return im;
}

void
evas_gl_common_image_native_enable(Evas_GL_Image *im)
{
   if (im->cs.data)
     {
	if (!im->cs.no_free) free(im->cs.data);
        im->cs.data = NULL;
     }
   im->cs.no_free = 0;
   if (im->cached)
     {
        if (im->references == 0)
           im->gc->shared->images_size -= (im->csize);
        im->gc->shared->images = eina_list_remove(im->gc->shared->images, im);
        im->cached = 0;
     }
   if (im->im)
     {
        evas_cache_image_drop(&im->im->cache_entry);
        im->im = NULL;
     }
   if (im->tex)
     {
        evas_gl_common_texture_free(im->tex);
        im->tex = NULL;
     }

   im->cs.space = EVAS_COLORSPACE_ARGB8888;
   im->tex = evas_gl_common_texture_native_new(im->gc, im->w, im->h, im->alpha, im);
   im->tex_only = 1;
}

void
evas_gl_common_image_native_disable(Evas_GL_Image *im)
{
   if (im->im)
     {
        evas_cache_image_drop(&im->im->cache_entry);
        im->im = NULL;
     }
   if (im->tex)
     {
        evas_gl_common_texture_free(im->tex);
        im->tex = NULL;
     }
   im->tex_only = 0;

   im->im = (RGBA_Image *)evas_cache_image_empty(evas_common_image_cache_get());
   im->im->cache_entry.flags.alpha = im->alpha;
   im->cs.space = EVAS_COLORSPACE_ARGB8888;
   evas_cache_image_colorspace(&im->im->cache_entry, im->cs.space);
}

void
evas_gl_common_image_scale_hint_set(Evas_GL_Image *im, int hint)
{
   im->scale_hint = hint;
   // FIXME: take advantage of this even in gl (eg if image is
   // 1600x1200 but we always use it at 800x600 or even less - drop
   // the texture res down for "non dynamic" stuff to save memory)
}

void
evas_gl_common_image_content_hint_set(Evas_GL_Image *im, int hint)
{
   if (im->content_hint == hint) return;
   im->content_hint = hint;
   if (!im->gc) return;
   if ((!im->gc->shared->info.sec_image_map)
       && (!im->gc->shared->info.sec_tbm_surface)) return;
   if (!im->gc->shared->info.bgra) return;
   // does not handle yuv yet.
   if ((im->cs.space != EVAS_COLORSPACE_ARGB8888)
       && (im->cs.space != EVAS_COLORSPACE_GRY8)
       && (im->cs.space != EVAS_COLORSPACE_AGRY88)
       && (im->cs.space != EVAS_COLORSPACE_ETC1)
       && (im->cs.space != EVAS_COLORSPACE_RGB8_ETC2)
       && (im->cs.space != EVAS_COLORSPACE_RGBA8_ETC2_EAC)) return;
   if (im->content_hint == EVAS_IMAGE_CONTENT_HINT_DYNAMIC)
     {
        if (im->cs.data)
          {
             if (!im->cs.no_free) free(im->cs.data);
             im->cs.data = NULL;
          }
        im->cs.no_free = 0;
        if (im->cached)
          {
             if (im->references == 0)
                im->gc->shared->images_size -= im->csize;
             im->gc->shared->images = eina_list_remove(im->gc->shared->images, im);
             im->cached = 0;
          }
        if (im->im)
          {
             evas_cache_image_drop(&im->im->cache_entry);
             im->im = NULL;
          }
        if (im->tex)
          {
             evas_gl_common_texture_free(im->tex);
             im->tex = NULL;
          }
        im->tex = evas_gl_common_texture_dynamic_new(im->gc, im);
        im->tex_only = 1;
     }
   else
     {
        if (im->im)
          {
             evas_cache_image_drop(&im->im->cache_entry);
             im->im = NULL;
          }
        if (im->tex)
          {
             evas_gl_common_texture_free(im->tex);
             im->tex = NULL;
          }
        im->tex_only = 0;

        im->im = (RGBA_Image *)evas_cache_image_empty(evas_common_image_cache_get());
        im->im->cache_entry.flags.alpha = im->alpha;
        im->im->cache_entry.space = im->cs.space;
        evas_cache_image_colorspace(&im->im->cache_entry, im->cs.space);
        im->im = (RGBA_Image *)evas_cache_image_size_set(&im->im->cache_entry, im->w, im->h);
        if (!im->tex)
           im->tex = evas_gl_common_texture_new(im->gc, im->im);
     }
}

void
evas_gl_common_image_cache_flush(Evas_Engine_GL_Context *gc)
{
   _evas_gl_image_cache_trim(gc);
}

void
evas_gl_common_image_free(Evas_GL_Image *im)
{
#if 0 // filtering disabled
   Filtered_Image *fi;
#endif

   evas_gl_common_context_flush(im->gc);
   im->references--;
   if (im->references > 0) return;

   if (im->scaled.origin)
     {
        evas_gl_common_image_free(im->scaled.origin);
        im->scaled.origin = NULL;
     }

   if (im->native.func.free)
     im->native.func.free(im->native.func.data, im);

   if (im->cs.data)
     {
	if (!im->cs.no_free) free(im->cs.data);
     }
   if (im->cached)
     {
        if (_evas_gl_image_cache_add(im)) return;
     }
   if (im->im) evas_cache_image_drop(&im->im->cache_entry);
   if (im->tex) evas_gl_common_texture_free(im->tex);

#if 0 // filtering disabled
   EINA_LIST_FREE(im->filtered, fi)
     {
	free(fi->key);
	evas_gl_common_image_free((Evas_GL_Image *)fi->image);
	free(fi);
     }
#endif

   free(im);
}

Evas_GL_Image *
evas_gl_common_image_surface_new(Evas_Engine_GL_Context *gc, unsigned int w, unsigned int h, int alpha)
{
   Evas_GL_Image *im;

   if (((int)w > gc->shared->info.max_texture_size) ||
       ((int)h > gc->shared->info.max_texture_size))
   {
      ERR("Image w :%d h :%d greater than supported value: %d", w, h,
         gc->shared->info.max_texture_size);
      return NULL;
   }

   im = calloc(1, sizeof(Evas_GL_Image));
   if (!im) return NULL;
   im->native.target = GL_TEXTURE_2D;
   im->references = 1;
   im->gc = gc;
   im->cs.space = EVAS_COLORSPACE_ARGB8888;
   im->alpha = alpha;
   im->w = w;
   im->h = h;
   im->tex = evas_gl_common_texture_render_new(gc, w, h, alpha);
   im->tex_only = 1;
   return im;
}

void
evas_gl_common_image_dirty(Evas_GL_Image *im, unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
   if ((w == 0) && (h == 0) && (x == 0) && (y == 0))
     {
        w = im->w;
        h = im->h;
     }
   if (im->im)
     {
        evas_gl_common_image_alloc_ensure(im);
        im->im = (RGBA_Image *)evas_cache_image_dirty(&im->im->cache_entry, x, y, w, h);
     }
   im->dirty = 1;
}

void
evas_gl_common_image_update(Evas_Engine_GL_Context *gc, Evas_GL_Image *im)
{
   Image_Entry *ie;
   if (!im->im) return;
   ie = (Image_Entry *)(im->im);
   evas_gl_common_image_alloc_ensure(im);
/*
   if ((im->cs.space == EVAS_COLORSPACE_YCBCR422P601_PL) ||
       (im->cs.space == EVAS_COLORSPACE_YCBCR422P709_PL))
     {
        // SOFTWARE convert. do multi texture later
        if ((im->cs.data) && (*((unsigned char **)im->cs.data)))
          {
             if (im->dirty || !im->im->image.data)
               {
                  free(im->im->image.data);
                  im->im->image.data = malloc(im->im->cache_entry.w * im->im->cache_entry.h * sizeof(DATA32));
                  if (im->im->image.data)
                    evas_common_convert_yuv_420p_601_rgba(im->cs.data,
                                                          (void *)im->im->image.data,
                                                          im->im->cache_entry.w, im->im->cache_entry.h);
               }
          }
        space = EVAS_COLORSPACE_ARGB8888;
     }
   else
 */
   switch (im->cs.space)
     {
      case EVAS_COLORSPACE_ARGB8888:
      case EVAS_COLORSPACE_GRY8:
      case EVAS_COLORSPACE_AGRY88:
      case EVAS_COLORSPACE_ETC1:
      case EVAS_COLORSPACE_RGB8_ETC2:
      case EVAS_COLORSPACE_RGBA8_ETC2_EAC:
         if ((im->tex) &&
             ((im->dirty) || (ie->flags.animated)))
          {
             evas_cache_image_load_data(&im->im->cache_entry);
             evas_gl_common_texture_update(im->tex, im->im);
             evas_cache_image_unload_data(&im->im->cache_entry);
          }
	if ((!im->tex) && (ie->load_error == 0))
          {
             evas_cache_image_load_data(&im->im->cache_entry);
             im->tex = evas_gl_common_texture_new(gc, im->im);
             evas_cache_image_unload_data(&im->im->cache_entry);
          }
        im->dirty = 0;
        if (!im->tex) return;
	break;
      case EVAS_COLORSPACE_ETC1_ALPHA:
        if ((im->tex) && (im->dirty))
          {
             evas_cache_image_load_data(&im->im->cache_entry);
             evas_gl_common_texture_rgb_a_pair_update(im->tex, im->im);
             evas_cache_image_unload_data(&im->im->cache_entry);
          }
        else if ((!im->tex))
          {
             evas_cache_image_load_data(&im->im->cache_entry);
             im->tex = evas_gl_common_texture_rgb_a_pair_new(gc, im->im);
             evas_cache_image_unload_data(&im->im->cache_entry);
          }
        im->dirty = 0;
        if (!im->tex) return;
        break;
      case EVAS_COLORSPACE_YCBCR422P601_PL:
      case EVAS_COLORSPACE_YCBCR422P709_PL:
        if ((im->tex) && (im->dirty))
          {
             evas_gl_common_texture_yuv_update(im->tex, im->cs.data,
                                               im->im->cache_entry.w,
                                               im->im->cache_entry.h);
             im->dirty = 0;
          }
        if ((!im->tex) && (im->cs.data) && (*((unsigned char **)im->cs.data)))
          {
             im->tex = evas_gl_common_texture_yuv_new(gc, im->cs.data,
                                                      im->im->cache_entry.w,
                                                      im->im->cache_entry.h);
             im->dirty = 0;
          }
        if (!im->tex) return;
        break;
      case EVAS_COLORSPACE_YCBCR422601_PL:
        if ((im->tex) && (im->dirty))
          {
             evas_gl_common_texture_yuy2_update(im->tex, im->cs.data,
                                                im->im->cache_entry.w,
                                                im->im->cache_entry.h);
             im->dirty = 0;
          }
        if ((!im->tex) && (im->cs.data) && (*((unsigned char **)im->cs.data)))
          {
             im->tex = evas_gl_common_texture_yuy2_new(gc, im->cs.data,
                                                       im->im->cache_entry.w,
                                                       im->im->cache_entry.h);
             im->dirty = 0;
          }
        if (!im->tex) return;
        break;
      case EVAS_COLORSPACE_YCBCR420NV12601_PL:
        if ((im->tex) && (im->dirty))
          {
             evas_gl_common_texture_nv12_update(im->tex, im->cs.data,
                                                im->im->cache_entry.w,
                                                im->im->cache_entry.h);
             im->dirty = 0;
          }
        if ((!im->tex) && (im->cs.data) && (*((unsigned char **)im->cs.data)))
          {
             im->tex = evas_gl_common_texture_nv12_new(gc, im->cs.data,
                                                       im->im->cache_entry.w,
                                                       im->im->cache_entry.h);
             im->dirty = 0;
          }
        if (!im->tex) return;
        break;
      case EVAS_COLORSPACE_YCBCR420TM12601_PL:
        if ((im->tex) && (im->dirty))
          {
             evas_gl_common_texture_nv12tiled_update(im->tex, im->cs.data,
                                                     im->im->cache_entry.w,
                                                     im->im->cache_entry.h);
             im->dirty = 0;
          }
        if ((!im->tex) && (im->cs.data) && (*((unsigned char **)im->cs.data)))
          {
             im->tex = evas_gl_common_texture_nv12tiled_new(gc, im->cs.data,
                                                            im->im->cache_entry.w,
                                                            im->im->cache_entry.h);
             im->dirty = 0;
          }
        if (!im->tex) return;
        break;
      default:
        ERR("unhandled img format colorspace=%d", im->cs.space);
        break;
    }
}

void
evas_gl_common_image_map_draw(Evas_Engine_GL_Context *gc, Evas_GL_Image *im,
                              int npoints, RGBA_Map_Point *p, int smooth, int level EINA_UNUSED)
{
   int mx = 0, my = 0, mw = 0, mh = 0;
   RGBA_Draw_Context *dc = gc->dc;
   Eina_Bool mask_smooth = EINA_FALSE;
   Evas_GL_Image *mask = dc->clip.mask;
   Evas_GL_Texture *mtex = mask ? mask->tex : NULL;
   int r, g, b, a;
   int c, cx, cy, cw, ch;

   if (dc->mul.use)
     {
        a = (dc->mul.col >> 24) & 0xff;
        r = (dc->mul.col >> 16) & 0xff;
        g = (dc->mul.col >> 8 ) & 0xff;
        b = (dc->mul.col      ) & 0xff;
     }
   else
     {
        r = g = b = a = 255;
     }

   evas_gl_common_image_update(gc, im);

   c = dc->clip.use;
   cx = dc->clip.x;   cy = dc->clip.y;
   cw = dc->clip.w;   ch = dc->clip.h;
   if (!im->tex) return;
   im->tex->im = im;

   if (mtex && mtex->pt && mtex->pt->w && mtex->pt->h)
     {
        // canvas coords
        mx = dc->clip.mask_x;
        my = dc->clip.mask_y;
        if (mask->scaled.origin)
          {
             mw = mask->scaled.w;
             mh = mask->scaled.h;
             //scalex = mask->w / (double)mask->scaled.w;
             //scaley = mask->h / (double)mask->scaled.h;
             mask_smooth = mask->scaled.smooth;
          }
        else
          {
             mw = mask->w;
             mh = mask->h;
          }
        //if (c) RECTS_CLIP_TO_RECT(mx, my, mw, mh, cx, cy, cw, ch);
        //mx = mx - dc->clip.mask_x;
        //my = my - dc->clip.mask_y;
     }
   else mtex = NULL;

   evas_gl_common_context_image_map_push(gc, im->tex, npoints, p,
                                         c, cx, cy, cw, ch,
                                         mtex, mx, my, mw, mh, mask_smooth,
                                         r, g, b, a, smooth, im->tex_only,
                                         im->cs.space);
}

void
evas_gl_common_image_draw(Evas_Engine_GL_Context *gc, Evas_GL_Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, int smooth)
{
   static Cutout_Rects *rects = NULL;
   RGBA_Draw_Context *dc;
   int r, g, b, a;
   double ssx, ssy, ssw, ssh;
   Cutout_Rect  *rct;
   int c, cx, cy, cw, ch;
   int i;
   int yuv = 0;
   int yuy2 = 0;
   int nv12 = 0;
   int rgb_a_pair = 0;
   Evas_GL_Image *mask;
   int mask_x, mask_y;
   double mx = 0, my = 0, mw = 0, mh = 0;
   Evas_GL_Texture *mtex = NULL;
   Eina_Bool mask_smooth = EINA_FALSE;

   if (sw < 1) sw = 1;
   if (sh < 1) sh = 1;
   dc = gc->dc;
   if (dc->mul.use)
     {
	a = (dc->mul.col >> 24) & 0xff;
	r = (dc->mul.col >> 16) & 0xff;
	g = (dc->mul.col >> 8 ) & 0xff;
	b = (dc->mul.col      ) & 0xff;
     }
   else
     {
	r = g = b = a = 255;
     }

   // Prepare mask image, if there is one
   mask = dc->clip.mask;
   mask_x = dc->clip.mask_x;
   mask_y = dc->clip.mask_y;
   if (mask)
     {
        evas_gl_common_image_update(gc, mask);
        if (!mask->tex)
          {
             ERR("Failed to apply mask image");
             mask = NULL;
             mask_x = 0;
             mask_y = 0;
          }
     }

   evas_gl_common_image_update(gc, im);
   if (!im->tex)
     {
        evas_gl_common_rect_draw(gc, dx, dy, dw, dh);
        return;
     }

   if ((im->cs.space == EVAS_COLORSPACE_YCBCR422P601_PL) ||
       (im->cs.space == EVAS_COLORSPACE_YCBCR422P709_PL))
     yuv = 1;
   if (im->cs.space == EVAS_COLORSPACE_YCBCR422601_PL)
     yuy2 = 1;
   if ((im->cs.space == EVAS_COLORSPACE_YCBCR420NV12601_PL) ||
       (im->cs.space == EVAS_COLORSPACE_YCBCR420TM12601_PL))
     nv12 = 1;
   if (im->cs.space == EVAS_COLORSPACE_ETC1_ALPHA)
     rgb_a_pair = 1;

   im->tex->im = im;
   if ((!gc->dc->cutout.rects) ||
       ((gc->shared->info.tune.cutout.max > 0) &&
           (gc->dc->cutout.active > gc->shared->info.tune.cutout.max)) ||
       (im->native.ratio > 0))
     {
        mtex = mask ? mask->tex : NULL;
        if (mtex && mtex->pt && mtex->pt->w && mtex->pt->h)
          {
             const double tmw = mtex->pt->w;
             const double tmh = mtex->pt->h;
             double scalex = 1.0;
             double scaley = 1.0;

             // canvas coords
             mx = mask_x; my = mask_y;
             if (mask->scaled.origin)
               {
                  mw = mask->scaled.w;
                  mh = mask->scaled.h;
                  scalex = mask->w / (double)mask->scaled.w;
                  scaley = mask->h / (double)mask->scaled.h;
                  mask_smooth = mask->scaled.smooth;
               }
             else
               {
                  mw = mask->w;
                  mh = mask->h;
               }
             // WARNING NOTE: The below code works upstream because
             // the function calling _evas_gl_common_image_push sets cx,cy,cw,ch
             // based on the destination geometry if clip is not used.
             //RECTS_CLIP_TO_RECT(mx, my, mw, mh, cx, cy, cw, ch);
             //RECTS_CLIP_TO_RECT(mx, my, mw, mh, dx, dy, dw, dh);
             c = gc->dc->clip.use; cx = gc->dc->clip.x; cy = gc->dc->clip.y; cw = gc->dc->clip.w; ch = gc->dc->clip.h;
             if (c) RECTS_CLIP_TO_RECT(mx, my, mw, mh, cx, cy, cw, ch);
             RECTS_CLIP_TO_RECT(mx, my, mw, mh, dx, dy, dw, dh);

             // convert to tex coords
             mx = (mtex->x / tmw) + ((mx - mask_x) * scalex / tmw);
             my = (mtex->y / tmh) + ((my - mask_y) * scaley / tmh);
             mw = mw * scalex / tmw;
             mh = mh * scaley / tmh;
          }
        else mtex = NULL;

        if ((gc->dc->clip.use) && (im->native.ratio == 0))
          {
             int nx, ny, nw, nh;

             nx = dx; ny = dy; nw = dw; nh = dh;
             RECTS_CLIP_TO_RECT(nx, ny, nw, nh,
                                gc->dc->clip.x, gc->dc->clip.y,
                                gc->dc->clip.w, gc->dc->clip.h);
             if ((nw < 1) || (nh < 1)) return;
             if ((nx == dx) && (ny == dy) && (nw == dw) && (nh == dh))
               {
                  if (yuv)
                    evas_gl_common_context_yuv_push(gc,
                                                    im->tex,
                                                    sx, sy, sw, sh,
                                                    dx, dy, dw, dh,
                                                    mtex, mx, my, mw, mh, mask_smooth,
                                                    r, g, b, a,
                                                    smooth);
                  else if (yuy2)
                    evas_gl_common_context_yuy2_push(gc,
						     im->tex,
						     sx, sy, sw, sh,
                                                     dx, dy, dw, dh,
                                                     mtex, mx, my, mw, mh, mask_smooth,
						     r, g, b, a,
						     smooth);
                  else if (nv12)
                    evas_gl_common_context_nv12_push(gc,
						     im->tex,
						     sx, sy, sw, sh,
                                                     dx, dy, dw, dh,
                                                     mtex, mx, my, mw, mh, mask_smooth,
						     r, g, b, a,
						     smooth);
                  else if (rgb_a_pair)
                    evas_gl_common_context_rgb_a_pair_push(gc,
                                                           im->tex,
                                                           sx, sy, sw, sh,
                                                           dx, dy, dw, dh,
                                                           /* mtex, mmx, mmy, mmw, mmh, */
                                                           r, g, b, a,
                                                           smooth);
                  else
                    evas_gl_common_context_image_push(gc,
                                                      im->tex,
                                                      sx, sy, sw, sh,
                                                      dx, dy, dw, dh,
                                                      mtex, mx, my, mw, mh, mask_smooth,
                                                      r, g, b, a,
                                                      smooth, im->tex_only);
                  return;
               }

             ssx = (double)sx + ((double)(sw * (nx - dx)) / (double)(dw));
             ssy = (double)sy + ((double)(sh * (ny - dy)) / (double)(dh));
             ssw = ((double)sw * (double)(nw)) / (double)(dw);
             ssh = ((double)sh * (double)(nh)) / (double)(dh);
             if (yuv)
               evas_gl_common_context_yuv_push(gc,
                                               im->tex,
                                               ssx, ssy, ssw, ssh,
                                               nx, ny, nw, nh,
                                               mtex, mx, my, mw, mh, mask_smooth,
                                               r, g, b, a,
                                               smooth);
             else if (yuy2)
               evas_gl_common_context_yuy2_push(gc,
						im->tex,
						ssx, ssy, ssw, ssh,
                                                nx, ny, nw, nh,
                                                mtex, mx, my, mw, mh, mask_smooth,
						r, g, b, a,
						smooth);
             else if (nv12)
               evas_gl_common_context_nv12_push(gc,
						im->tex,
						ssx, ssy, ssw, ssh,
                                                nx, ny, nw, nh,
                                                mtex, mx, my, mw, mh, mask_smooth,
						r, g, b, a,
						smooth);
             else if (rgb_a_pair)
               evas_gl_common_context_rgb_a_pair_push(gc,
                                                      im->tex,
                                                      ssx, ssy, ssw, ssh,
                                                      nx, ny, nw, nh,
                                                      /* mtex, mmx, mmy, mmw, mmh, */
                                                      r, g, b, a,
                                                      smooth);
             else
               evas_gl_common_context_image_push(gc,
                                                 im->tex,
                                                 ssx, ssy, ssw, ssh,
                                                 nx, ny, nw, nh,
                                                 mtex, mx, my, mw, mh, mask_smooth,
                                                 r, g, b, a,
                                                 smooth, im->tex_only);
          }
        else
          {
             if (yuv)
               evas_gl_common_context_yuv_push(gc,
                                               im->tex,
                                               sx, sy, sw, sh,
                                               dx, dy, dw, dh,
                                               mtex, mx, my, mw, mh, mask_smooth,
                                               r, g, b, a,
                                               smooth);
             else if (yuy2)
               evas_gl_common_context_yuy2_push(gc,
						im->tex,
						sx, sy, sw, sh,
                                                dx, dy, dw, dh,
                                                mtex, mx, my, mw, mh, mask_smooth,
						r, g, b, a,
                                                smooth);
             else if (nv12)
               evas_gl_common_context_nv12_push(gc,
                                                im->tex,
                                                sx, sy, sw, sh,
                                                dx, dy, dw, dh,
                                                mtex, mx, my, mw, mh, mask_smooth,
                                                r, g, b, a,
                                                smooth);
             else if (rgb_a_pair)
               evas_gl_common_context_rgb_a_pair_push(gc,
                                                      im->tex,
                                                      sx, sy, sw, sh,
                                                      dx, dy, dw, dh,
                                                      /* mtex, mmx, mmy, mmw, mmh, */
                                                      r, g, b, a,
                                                      smooth);
             else
               evas_gl_common_context_image_push(gc,
                                                 im->tex,
                                                 sx, sy, sw, sh,
                                                 dx, dy, dw, dh,
                                                 mtex, mx, my, mw, mh, mask_smooth,
                                                 r, g, b, a,
                                                 smooth, im->tex_only);
          }
        return;
     }

   /* save out clip info */
   c = gc->dc->clip.use; cx = gc->dc->clip.x; cy = gc->dc->clip.y; cw = gc->dc->clip.w; ch = gc->dc->clip.h;
   evas_common_draw_context_clip_clip(gc->dc, 0, 0, gc->shared->w, gc->shared->h);
   evas_common_draw_context_clip_clip(gc->dc, dx, dy, dw, dh);
   /* our clip is 0 size.. abort */
   if ((gc->dc->clip.w <= 0) || (gc->dc->clip.h <= 0))
     {
        gc->dc->clip.use = c; gc->dc->clip.x = cx; gc->dc->clip.y = cy; gc->dc->clip.w = cw; gc->dc->clip.h = ch;
        return;
     }
   rects = evas_common_draw_context_apply_cutouts(dc, rects);
   for (i = 0; i < rects->active; ++i)
     {
        int nx, ny, nw, nh;

        rct = rects->rects + i;
        nx = dx; ny = dy; nw = dw; nh = dh;
        RECTS_CLIP_TO_RECT(nx, ny, nw, nh, rct->x, rct->y, rct->w, rct->h);

        mtex = mask ? mask->tex : NULL;
        if (mtex && mtex->pt && mtex->pt->w && mtex->pt->h)
          {
             const double tmw = mtex->pt->w;
             const double tmh = mtex->pt->h;
             double scalex = 1.0;
             double scaley = 1.0;

             // canvas coords
             mx = mask_x; my = mask_y;
             if (mask->scaled.origin)
               {
                  mw = mask->scaled.w;
                  mh = mask->scaled.h;
                  scalex = mask->w / (double)mask->scaled.w;
                  scaley = mask->h / (double)mask->scaled.h;
                  mask_smooth = mask->scaled.smooth;
               }
             else
               {
                  mw = mask->w;
                  mh = mask->h;
               }
             // WARNING NOTE: The below code works upstream because
             // the function calling _evas_gl_common_image_push sets cx,cy,cw,ch
             // based on the destination geometry if clip is not used.
             //RECTS_CLIP_TO_RECT(mx, my, mw, mh, cx, cy, cw, ch);
             //RECTS_CLIP_TO_RECT(mx, my, mw, mh, dx, dy, dw, dh);
             if (c) RECTS_CLIP_TO_RECT(mx, my, mw, mh, cx, cy, cw, ch);
             RECTS_CLIP_TO_RECT(mx, my, mw, mh, nx, ny, nw, nh);

             // convert to tex coords
             mx = (mtex->x / tmw) + ((mx - mask_x) * scalex / tmw);
             my = (mtex->y / tmh) + ((my - mask_y) * scaley / tmh);
             mw = mw * scalex / tmw;
             mh = mh * scaley / tmh;
          }

        if ((nw < 1) || (nh < 1)) continue;
        if ((nx == dx) && (ny == dy) && (nw == dw) && (nh == dh))
          {
             if (yuv)
               evas_gl_common_context_yuv_push(gc,
                                               im->tex,
                                               sx, sy, sw, sh,
                                               dx, dy, dw, dh,
                                               mtex, mx, my, mw, mh, mask_smooth,
                                               r, g, b, a,
                                               smooth);
             else if (yuy2)
               evas_gl_common_context_yuy2_push(gc,
						im->tex,
						sx, sy, sw, sh,
                                                dx, dy, dw, dh,
                                                mtex, mx, my, mw, mh, mask_smooth,
						r, g, b, a,
                                                smooth);
             else if (nv12)
               evas_gl_common_context_nv12_push(gc,
                                                im->tex,
                                                sx, sy, sw, sh,
                                                dx, dy, dw, dh,
                                                mtex, mx, my, mw, mh, mask_smooth,
                                                r, g, b, a,
                                                smooth);
             else if (rgb_a_pair)
               evas_gl_common_context_rgb_a_pair_push(gc,
                                                      im->tex,
                                                      sx, sy, sw, sh,
                                                      dx, dy, dw, dh,
                                                      /* mtex, mmx, mmy, mmw, mmh, */
                                                      r, g, b, a,
                                                      smooth);
             else
               evas_gl_common_context_image_push(gc,
                                                 im->tex,
                                                 sx, sy, sw, sh,
                                                 dx, dy, dw, dh,
                                                 mtex, mx, my, mw, mh, mask_smooth,
                                                 r, g, b, a,
                                                 smooth, im->tex_only);
             continue;
          }
        ssx = (double)sx + ((double)(sw * (nx - dx)) / (double)(dw));
        ssy = (double)sy + ((double)(sh * (ny - dy)) / (double)(dh));
        ssw = ((double)sw * (double)(nw)) / (double)(dw);
        ssh = ((double)sh * (double)(nh)) / (double)(dh);
        if (yuv)
          evas_gl_common_context_yuv_push(gc,
                                          im->tex,
                                          ssx, ssy, ssw, ssh,
                                          nx, ny, nw, nh,
                                          mtex, mx, my, mw, mh, mask_smooth,
                                          r, g, b, a,
                                          smooth);
        else if (yuy2)
          evas_gl_common_context_yuy2_push(gc,
					   im->tex,
					   ssx, ssy, ssw, ssh,
                                           nx, ny, nw, nh,
                                           mtex, mx, my, mw, mh, mask_smooth,
					   r, g, b, a,
                                           smooth);
        else if (nv12)
          evas_gl_common_context_nv12_push(gc,
                                           im->tex,
                                           ssx, ssy, ssw, ssh,
                                           nx, ny, nw, nh,
                                           mtex, mx, my, mw, mh, mask_smooth,
                                           r, g, b, a,
                                           smooth);
        else if (rgb_a_pair)
          evas_gl_common_context_rgb_a_pair_push(gc,
                                                 im->tex,
                                                 ssx, ssy, ssw, ssh,
                                                 nx, ny, nw, nh,
                                                 /* mtex, mmx, mmy, mmw, mmh, */
                                                 r, g, b, a,
                                                 smooth);
        else
          evas_gl_common_context_image_push(gc,
                                            im->tex,
                                            ssx, ssy, ssw, ssh,
                                            nx, ny, nw, nh,
                                            mtex, mx, my, mw, mh, mask_smooth,
                                            r, g, b, a,
                                            smooth, im->tex_only);
     }
   /* restore clip info */
   gc->dc->clip.use = c; gc->dc->clip.x = cx; gc->dc->clip.y = cy; gc->dc->clip.w = cw; gc->dc->clip.h = ch;
}
