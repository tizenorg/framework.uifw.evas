#ifdef HAVE_CONFIG_H
# include "config.h"  /* so that EAPI in Eet.h is correctly defined */
#endif

#ifdef BUILD_LOADER_EET
# include <Eet.h>
#endif

#include "evas_common.h"
#include "evas_private.h"
#include "evas_image_private.h"
#include "evas_convert_yuv.h"
#include "evas_cs.h"

#ifdef HAVE_VALGRIND
# include <memcheck.h>
#endif

static Evas_Cache_Image * eci = NULL;
static int                reference = 0;

/* static RGBA_Image *evas_rgba_line_buffer = NULL; */

#define  EVAS_RGBA_LINE_BUFFER_MIN_LEN  256
#define  EVAS_RGBA_LINE_BUFFER_MAX_LEN  2048

/* static RGBA_Image *evas_alpha_line_buffer = NULL; */

#define  EVAS_ALPHA_LINE_BUFFER_MIN_LEN  256
#define  EVAS_ALPHA_LINE_BUFFER_MAX_LEN  2048


static Image_Entry      *_evas_common_rgba_image_new(void);
static void              _evas_common_rgba_image_delete(Image_Entry *ie);

static int               _evas_common_rgba_image_surface_alloc(Image_Entry *ie, unsigned int w, unsigned int h);
static void              _evas_common_rgba_image_surface_delete(Image_Entry *ie);
static DATA32           *_evas_common_rgba_image_surface_pixels(Image_Entry *ie);

static void              _evas_common_rgba_image_unload(Image_Entry *im);

static void              _evas_common_rgba_image_dirty_region(Image_Entry *im, unsigned int x, unsigned int y, unsigned int w, unsigned int h);

static int               _evas_common_rgba_image_ram_usage(Image_Entry *ie);

/* Only called when references > 0. Need to provide a fresh copie of im. */
/* The destination surface does have a surface, but no allocated pixel data. */
static int               _evas_common_rgba_image_dirty(Image_Entry* dst, const Image_Entry* src);

#if 0
static void
_evas_common_rgba_image_debug(const char* context, Image_Entry *eim)
{
  DBG("%p = [%s] {%s,%s} %i [%i|%i]", eim, context, eim->file, eim->key, eim->references, eim->w, eim->h);
}
#endif

static const Evas_Cache_Image_Func      _evas_common_image_func =
{
  _evas_common_rgba_image_new,
  _evas_common_rgba_image_delete,
  _evas_common_rgba_image_surface_alloc,
  _evas_common_rgba_image_surface_delete,
  _evas_common_rgba_image_surface_pixels,
  evas_common_load_rgba_image_module_from_file,
  _evas_common_rgba_image_unload,
  _evas_common_rgba_image_dirty_region,
  _evas_common_rgba_image_dirty,
  evas_common_rgba_image_size_set,
  evas_common_rgba_image_from_copied_data,
  evas_common_rgba_image_from_data,
  evas_common_rgba_image_colorspace_set,
  evas_common_load_rgba_image_data_from_file,
  _evas_common_rgba_image_ram_usage,
/*   _evas_common_rgba_image_debug */
  NULL
};

EAPI void
evas_common_image_init(void)
{
   if (!eci)
     eci = evas_cache_image_init(&_evas_common_image_func);
   reference++;
////   ERR("REF++=%i", reference);

#ifdef BUILD_LOADER_EET
   eet_init();
#endif
   evas_common_scalecache_init();
}

EAPI void
evas_common_image_shutdown(void)
{
   if (--reference == 0)
     {
////	printf("REF--=%i\n", reference);
// DISABLE for now - something wrong with cache shutdown freeing things
// still in use - rage_thumb segv's now.
//
// actually - i think i see it. cache ref goes to 0 (and thus gets freed)
// because in eng_setup() when a buffer changes size it is FIRST freed
// THEN allocated again - thus brignhjing ref to 0 then back to 1 immediately
// where it should stay at 1. - see evas_engine.c in the buffer enigne for
// example. eng_output_free() is called BEFORE _output_setup(). although this
// is only a SIGNE of the problem. we can patch this up with either freeing
// after the setup (so we just pt a ref of 2 then back to 1), or just
// evas_common_image_init() at the start and evas_common_image_shutdown()
// after it all. really ref 0 should only be reached when no more canvases
// with no more objects exist anywhere.

// ENABLE IT AGAIN, hope it is fixed. Gustavo @ January 22nd, 2009.
       evas_cache_image_shutdown(eci);
       eci = NULL;
     }

#ifdef BUILD_LOADER_EET
   eet_shutdown();
#endif
   evas_common_scalecache_shutdown();
}

EAPI void
evas_common_image_image_all_unload(void)
{
   evas_common_rgba_image_scalecache_dump();
   evas_cache_image_unload_all(eci);
}

static Image_Entry *
_evas_common_rgba_image_new(void)
{
   RGBA_Image *im;

   im = calloc(1, sizeof(RGBA_Image));
   if (!im) return NULL;
   im->flags = RGBA_IMAGE_NOTHING;
   im->ref = 1;
#ifdef EVAS_FRAME_QUEUING
   LKI(im->cache_entry.ref_fq_add);
   LKI(im->cache_entry.ref_fq_del);
   eina_condition_new(&(im->cache_entry.cond_fq_del),
		      &(im->cache_entry.ref_fq_del));
#endif

   evas_common_rgba_image_scalecache_init(&im->cache_entry);

   return &im->cache_entry;
}

static void
_evas_common_rgba_image_delete(Image_Entry *ie)
{
   RGBA_Image *im = (RGBA_Image *)ie;

#ifdef BUILD_PIPE_RENDER
   evas_common_pipe_free(im);
# ifdef EVAS_FRAME_QUEUING
   LKD(im->cache_entry.ref_fq_add);
   LKD(im->cache_entry.ref_fq_del);
   eina_condition_free(&(im->cache_entry.cond_fq_del));
# endif
#endif
   evas_common_rgba_image_scalecache_shutdown(&im->cache_entry);
   if (ie->info.module) evas_module_unref((Evas_Module *)ie->info.module);
   /* memset the image to 0x99 because i recently saw a segv where an
    * seemed to be used BUT its contents were wrong - it looks like it was
    * overwritten by something from efreet - as there was an execute command
    * for a command there and some other signs - but to make sure, I am
    * going to empty this struct out in case this happens again so i know
    * that something else is overwritign this struct - or not */
//   memset(im, 0x99, sizeof(im));
#ifdef EVAS_CSERVE
   if (ie->data1) evas_cserve_image_free(ie);
#endif
/*
 * FIXME: This doesn't seem to be needed... But I'm not sure why.
 *	 -- nash
     {
        Filtered_Image *fi;

        EINA_LIST_FREE(im->filtered, fi)
          {
             free(fi->key);
             _evas_common_rgba_image_delete((Image_Entry *)(fi->image));
             free(fi);
          }
     }
*/
   if (ie->frames)
     {
        Eina_List *l;
        Image_Entry_Frame *frame;
        EINA_LIST_FOREACH(ie->frames, l, frame)
          {
           if (frame)
             {
                if (frame->data) free(frame->data);
                if (frame->info) free(frame->info);
                free (frame);
             }
          }
     }
   free(im);
}

EAPI void
evas_common_rgba_image_free(Image_Entry *ie)
{
   _evas_common_rgba_image_surface_delete(ie);
   _evas_common_rgba_image_delete(ie);
}

EAPI void
evas_common_rgba_image_unload(Image_Entry *ie)
{
   RGBA_Image   *im = (RGBA_Image *) ie;

   if (!ie->flags.loaded) return;
   if ((!ie->info.module) && (!ie->data1)) return;
   if (!ie->file) return;

   ie->flags.loaded = 0;

   if ((im->cs.data) && (im->image.data))
     {
	if (im->cs.data != im->image.data)
	  {
	     if (!im->cs.no_free) free(im->cs.data);
	  }
     }
   else if (im->cs.data)
     {
	if (!im->cs.no_free) free(im->cs.data);
     }
   im->cs.data = NULL;

#ifdef EVAS_CSERVE
   if (ie->data1)
     {
        evas_cserve_image_useless(ie);
        im->image.data = NULL;
        ie->allocated.w = 0;
        ie->allocated.h = 0;
        ie->flags.loaded = 0;
#ifdef BUILD_ASYNC_PRELOAD
        ie->flags.preload_done = 0;
#endif
        return;
     }
#endif

   if (im->image.data && !im->image.no_free)
     free(im->image.data);
   im->image.data = NULL;
   ie->allocated.w = 0;
   ie->allocated.h = 0;
   ie->flags.loaded = 0;
#ifdef BUILD_ASYNC_PRELOAD
   ie->flags.preload_done = 0;
#endif
}

void
_evas_common_rgba_image_post_surface(Image_Entry *ie)
{
#ifdef HAVE_PIXMAN
# ifdef PIXMAN_IMAGE   
   RGBA_Image *im = (RGBA_Image *)ie;

   if (im->pixman.im) pixman_image_unref(im->pixman.im);
   if (im->cache_entry.flags.alpha)
     {
        im->pixman.im = pixman_image_create_bits
        (
// FIXME: endianess determines this
            PIXMAN_a8r8g8b8,
//            PIXMAN_b8g8r8a8,
            im->cache_entry.w, im->cache_entry.h,
            im->image.data,
            im->cache_entry.w * 4
        );
     }
   else
     {
        im->pixman.im = pixman_image_create_bits
        (
// FIXME: endianess determines this
            PIXMAN_x8r8g8b8,
//            PIXMAN_b8g8r8x8,
            im->cache_entry.w, im->cache_entry.h,
            im->image.data,
            im->cache_entry.w * 4
        );
     }
# else
   (void)ie;
# endif
#else
   (void)ie;
#endif
}

static int
_evas_common_rgba_image_surface_alloc(Image_Entry *ie, unsigned int w, unsigned int h)
{
   RGBA_Image   *im = (RGBA_Image *) ie;
   size_t        siz = 0;

#ifdef EVAS_CSERVE
   if (ie->data1) return 0;
#endif
   if (im->image.no_free) return 0;

   if (im->flags & RGBA_IMAGE_ALPHA_ONLY)
     siz = w * h * sizeof(DATA8);
   else
     siz = w * h * sizeof(DATA32);

   if (im->image.data) free(im->image.data);
   im->image.data = malloc(siz);
   if (!im->image.data) return -1;

#ifdef HAVE_VALGRIND
# ifdef VALGRIND_MAKE_READABLE
   VALGRIND_MAKE_READABLE(im->image.data, siz);
# else
#  ifdef VALGRIND_MAKE_MEM_DEFINED
   VALGRIND_MAKE_MEM_DEFINED(im->image.data, siz);
#  endif
# endif
#endif
   _evas_common_rgba_image_post_surface(ie);

   return 0;
}

static void
_evas_common_rgba_image_surface_delete(Image_Entry *ie)
{
   RGBA_Image   *im = (RGBA_Image *) ie;

#ifdef HAVE_PIXMAN
# ifdef PIXMAN_IMAGE   
   if (im->pixman.im)
     {
        pixman_image_unref(im->pixman.im);
        im->pixman.im = NULL;
     }
# endif   
#endif
   if (ie->file)
     DBG("unload: [%p] %s %s", ie, ie->file, ie->key);
   if ((im->cs.data) && (im->image.data))
     {
	if (im->cs.data != im->image.data)
	  {
	     if (!im->cs.no_free) free(im->cs.data);
	  }
     }
   else if (im->cs.data)
     {
	if (!im->cs.no_free) free(im->cs.data);
     }
   im->cs.data = NULL;

   if (im->image.data && !im->image.no_free)
     free(im->image.data);
#ifdef EVAS_CSERVE
   else if (ie->data1)
     evas_cserve_image_free(ie);
#endif

   im->image.data = NULL;
   ie->allocated.w = 0;
   ie->allocated.h = 0;
#ifdef BUILD_ASYNC_PRELOAD
   ie->flags.preload_done = 0;
#endif
   ie->flags.loaded = 0;
   evas_common_rgba_image_scalecache_dirty(&im->cache_entry);
}

static void
_evas_common_rgba_image_unload(Image_Entry *im)
{
//   DBG("unload: [%p] %s %s", im, im->file, im->key);
   evas_common_rgba_image_scalecache_dirty(im);
   evas_common_rgba_image_unload(im);
}

static void
_evas_common_rgba_image_dirty_region(Image_Entry* ie, unsigned int x __UNUSED__, unsigned int y __UNUSED__, unsigned int w __UNUSED__, unsigned int h __UNUSED__)
{
   RGBA_Image   *im = (RGBA_Image *) ie;

#ifdef EVAS_CSERVE
   if (ie->data1) evas_cserve_image_free(ie);
#endif
   im->flags |= RGBA_IMAGE_IS_DIRTY;
   evas_common_rgba_image_scalecache_dirty(&im->cache_entry);
}

/* Only called when references > 0. Need to provide a fresh copie of im. */
static int
_evas_common_rgba_image_dirty(Image_Entry *ie_dst, const Image_Entry *ie_src)
{
   RGBA_Image   *dst = (RGBA_Image *) ie_dst;
   RGBA_Image   *src = (RGBA_Image *) ie_src;

   evas_common_rgba_image_scalecache_dirty((Image_Entry *)ie_src);
   evas_common_rgba_image_scalecache_dirty(ie_dst);
   evas_cache_image_load_data(&src->cache_entry);
   if (_evas_common_rgba_image_surface_alloc(&dst->cache_entry,
                                             src->cache_entry.w, src->cache_entry.h))
     {
#ifdef EVAS_CSERVE
        if (ie_src->data1) evas_cserve_image_free((Image_Entry*) ie_src);
#endif
        return 1;
     }

#ifdef EVAS_CSERVE
   if (ie_src->data1) evas_cserve_image_free((Image_Entry*) ie_src);
#endif
   evas_common_image_colorspace_normalize(src);
   evas_common_image_colorspace_normalize(dst);
/*    evas_common_blit_rectangle(src, dst, 0, 0, src->cache_entry.w, src->cache_entry.h, 0, 0); */
/*    evas_common_cpu_end_opt(); */

   return 0;
}

static int
_evas_common_rgba_image_ram_usage(Image_Entry *ie)
{
   RGBA_Image *im = (RGBA_Image *)ie;
   int size = sizeof(struct _RGBA_Image);

   if (ie->cache_key) size += strlen(ie->cache_key);
   if (ie->file) size += strlen(ie->file);
   if (ie->key) size += strlen(ie->key);

   if (im->image.data)
     {
#ifdef EVAS_CSERVE
        if ((!im->image.no_free) || (ie->data1))
#else
        if ((!im->image.no_free))
#endif
          size += im->cache_entry.w * im->cache_entry.h * sizeof(DATA32);
     }
   size += evas_common_rgba_image_scalecache_usage_get(&im->cache_entry);
   return size;
}

static DATA32 *
_evas_common_rgba_image_surface_pixels(Image_Entry *ie)
{
   RGBA_Image *im = (RGBA_Image *) ie;

   return im->image.data;
}

#if 0
void
evas_common_image_surface_alpha_tiles_calc(RGBA_Surface *is, int tsize)
{
   int x, y;
   DATA32 *ptr;

   if (is->spans) return;
   if (!is->im->cache_entry.flags.alpha) return;
   /* FIXME: dont handle alpha only images yet */
   if ((is->im->flags & RGBA_IMAGE_ALPHA_ONLY)) return;
   if (tsize < 0) tsize = 0;
   is->spans = calloc(1, sizeof(RGBA_Image_Span *) * is->h);
   if (!is->spans) return;
   ptr = is->data;
   for (y = 0; y < is->h; y++)
     {
	RGBA_Image_Span *sp;

	sp = NULL;
	for (x = 0; x < is->w; x++)
	  {
	     DATA8 a;

	     a = A_VAL(ptr);
	     if (sp)
	       {
		  if (a == 0)
		    {
		       is->spans[y] = eina_inlist_append(is->spans[y], sp);
		       sp = NULL;
		    }
		  else
		    {
		       sp->w++;
		       if ((sp->v == 2) && (a != 255)) sp->v = 1;
		    }
	       }
	     else
	       {
		  if (a == 255)
		    {
		       sp = calloc(1, sizeof(RGBA_Image_Span));
		       sp->x = x;
		       sp->w = 1;
		       sp->v = 2;
		    }
		  else if (a > 0)
		    {
		       sp = calloc(1, sizeof(RGBA_Image_Span));
		       sp->x = x;
		       sp->w = 1;
		       sp->v = 1;
		    }
	       }
	     ptr++;
	  }
	if (sp)
	  {
	     is->spans[y] = eina_inlist_append(is->spans[y], sp);
	     sp = NULL;
	  }
     }
}
#endif

/* EAPI void */
/* evas_common_image_surface_dealloc(RGBA_Surface *is) */
/* { */
/*    if ((is->data) && (!is->no_free)) */
/*      { */
/*	free(is->data); */
/*	is->data = NULL; */
/*      } */
/* } */

static RGBA_Image *
evas_common_image_create(unsigned int w, unsigned int h)
{
   RGBA_Image *im;

   im = (RGBA_Image *) _evas_common_rgba_image_new();
   if (!im) return NULL;
   im->cache_entry.w = w;
   im->cache_entry.h = h;
   if (_evas_common_rgba_image_surface_alloc(&im->cache_entry, w, h))
     {
        _evas_common_rgba_image_delete(&im->cache_entry);
        return NULL;
     }
   im->cache_entry.flags.cached = 0;
   return im;
}

EAPI RGBA_Image *
evas_common_image_alpha_create(unsigned int w, unsigned int h)
{
   RGBA_Image   *im;

   im = (RGBA_Image *) _evas_common_rgba_image_new();
   if (!im) return NULL;
   im->cache_entry.w = w;
   im->cache_entry.h = h;
   im->cache_entry.flags.alpha = 1;
   if (_evas_common_rgba_image_surface_alloc(&im->cache_entry, w, h))
     {
        _evas_common_rgba_image_delete(&im->cache_entry);
        return NULL;
     }
   im->cache_entry.flags.cached = 0;
   return im;
}

EAPI RGBA_Image *
evas_common_image_new(unsigned int w, unsigned int h, unsigned int alpha)
{
   if (alpha)
     return evas_common_image_alpha_create(w, h);
   return evas_common_image_create(w, h);
}

void
evas_common_image_colorspace_normalize(RGBA_Image *im)
{
   if ((!im->cs.data) ||
       ((!im->cs.dirty) && (!(im->flags & RGBA_IMAGE_IS_DIRTY)))) return;
   switch (im->cache_entry.space)
     {
      case EVAS_COLORSPACE_ARGB8888:
	if (im->image.data != im->cs.data)
	  {
#ifdef EVAS_CSERVE
             if (((Image_Entry *)im)->data1) evas_cserve_image_free(&im->cache_entry);
#endif
	     if (!im->image.no_free) free(im->image.data);
	     im->image.data = im->cs.data;
	     im->cs.no_free = im->image.no_free;
	  }
	break;
      case EVAS_COLORSPACE_YCBCR422P601_PL:
#ifdef BUILD_CONVERT_YUV
	if ((im->image.data) && (*((unsigned char **)im->cs.data)))
	  evas_common_convert_yuv_420p_601_rgba(im->cs.data, (DATA8*) im->image.data,
						im->cache_entry.w, im->cache_entry.h);
#endif
	break;
      case EVAS_COLORSPACE_YCBCR422601_PL:
#ifdef BUILD_CONVERT_YUV
        if ((im->image.data) && (*((unsigned char **)im->cs.data)))
          evas_common_convert_yuv_422_601_rgba(im->cs.data, (DATA8*) im->image.data,
                                               im->cache_entry.w, im->cache_entry.h);
#endif
        break;
      case EVAS_COLORSPACE_YCBCR420NV12601_PL:
#ifdef BUILD_CONVERT_YUV
        if ((im->image.data) && (*((unsigned char **)im->cs.data)))
          evas_common_convert_yuv_420_601_rgba(im->cs.data, (DATA8*) im->image.data,
                                               im->cache_entry.w, im->cache_entry.h);
#endif
        break;
      case EVAS_COLORSPACE_YCBCR420TM12601_PL:
#ifdef BUILD_CONVERT_YUV
        if ((im->image.data) && (*((unsigned char **)im->cs.data)))
          evas_common_convert_yuv_420T_601_rgba(im->cs.data, (DATA8*) im->image.data,
                                                im->cache_entry.w, im->cache_entry.h);
#endif
         break;
      default:
	break;
     }
   im->cs.dirty = 0;
}

EAPI void
evas_common_image_colorspace_dirty(RGBA_Image *im)
{
   im->cs.dirty = 1;
   evas_common_rgba_image_scalecache_dirty(&im->cache_entry);
}

EAPI void
evas_common_image_set_cache(unsigned int size)
{
   if (eci)
     evas_cache_image_set(eci, size);
}

EAPI int
evas_common_image_get_cache(void)
{
   return evas_cache_image_get(eci);
}

EAPI RGBA_Image *
evas_common_load_image_from_file(const char *file, const char *key, RGBA_Image_Loadopts *lo, int *error)
{
   if (!file)
     {
	*error = EVAS_LOAD_ERROR_GENERIC;
	return NULL;
     }
   return (RGBA_Image *) evas_cache_image_request(eci, file, key, lo, error);
}

EAPI void
evas_common_image_cache_free(void)
{
   evas_common_image_set_cache(0);
}

EAPI Evas_Cache_Image*
evas_common_image_cache_get(void)
{
   return eci;
}

EAPI RGBA_Image *
evas_common_image_line_buffer_obtain(int len)
{
   if (len < 1) return NULL;
   if (len < EVAS_RGBA_LINE_BUFFER_MIN_LEN)
	len = EVAS_RGBA_LINE_BUFFER_MIN_LEN;
   return evas_common_image_create(len, 1);
/*
   if (evas_rgba_line_buffer)
     {
	if (evas_rgba_line_buffer->image->w >= len)
	   return evas_rgba_line_buffer;
	evas_rgba_line_buffer->image->data = (DATA32 *)realloc(evas_rgba_line_buffer->image->data, len * sizeof(DATA32));
	if (!evas_rgba_line_buffer->image->data)
	  {
	   evas_common_image_free(evas_rgba_line_buffer);
	   evas_rgba_line_buffer = NULL;
	   return NULL;
	  }
	evas_rgba_line_buffer->image->w = len;
	return evas_rgba_line_buffer;
     }
   evas_rgba_line_buffer = evas_common_image_create(len, 1);
   if (!evas_rgba_line_buffer) return NULL;
   return evas_rgba_line_buffer;
 */
}

EAPI void
evas_common_image_line_buffer_release(RGBA_Image *im)
{
    _evas_common_rgba_image_delete(&im->cache_entry);
/*
   if (!evas_rgba_line_buffer) return;
   if (EVAS_RGBA_LINE_BUFFER_MAX_LEN < evas_rgba_line_buffer->image->w)
     {
	evas_rgba_line_buffer->image->w = EVAS_RGBA_LINE_BUFFER_MAX_LEN;
	evas_rgba_line_buffer->image->data = (DATA32 *)realloc(evas_rgba_line_buffer->image->data,
	                         evas_rgba_line_buffer->image->w * sizeof(DATA32));
	if (!evas_rgba_line_buffer->image->data)
	  {
	   evas_common_image_free(evas_rgba_line_buffer);
	   evas_rgba_line_buffer = NULL;
	  }
     }
 */
}

EAPI void
evas_common_image_line_buffer_free(RGBA_Image *im)
{
    _evas_common_rgba_image_delete(&im->cache_entry);
/*
   if (!evas_rgba_line_buffer) return;
   evas_common_image_free(evas_rgba_line_buffer);
   evas_rgba_line_buffer = NULL;
 */
}

EAPI RGBA_Image *
evas_common_image_alpha_line_buffer_obtain(int len)
{
   if (len < 1) return NULL;
   if (len < EVAS_ALPHA_LINE_BUFFER_MIN_LEN)
	len = EVAS_ALPHA_LINE_BUFFER_MIN_LEN;
   return evas_common_image_alpha_create(len, 1);
/*
   if (evas_alpha_line_buffer)
     {
	if (evas_alpha_line_buffer->image->w >= len)
	   return evas_alpha_line_buffer;
	evas_alpha_line_buffer->image->data = realloc(evas_alpha_line_buffer->image->data, len * sizeof(DATA8));
	if (!evas_alpha_line_buffer->image->data)
	  {
	   evas_common_image_free(evas_alpha_line_buffer);
	   evas_alpha_line_buffer = NULL;
	   return NULL;
	  }
	evas_alpha_line_buffer->image->w = len;
	return evas_alpha_line_buffer;
     }
   evas_alpha_line_buffer = evas_common_image_alpha_create(len, 1);
   return evas_alpha_line_buffer;
 */
}

EAPI void
evas_common_image_alpha_line_buffer_release(RGBA_Image *im)
{
    _evas_common_rgba_image_delete(&im->cache_entry);
/*
   if (!evas_alpha_line_buffer) return;
   if (EVAS_ALPHA_LINE_BUFFER_MAX_LEN < evas_alpha_line_buffer->image->w)
     {
	evas_alpha_line_buffer->image->w = EVAS_ALPHA_LINE_BUFFER_MAX_LEN;
	evas_alpha_line_buffer->image->data = realloc(evas_alpha_line_buffer->image->data,
	                         evas_alpha_line_buffer->image->w * sizeof(DATA8));
	if (!evas_alpha_line_buffer->image->data)
	  {
	   evas_common_image_free(evas_alpha_line_buffer);
	   evas_alpha_line_buffer = NULL;
	  }
     }
 */
}

EAPI void
evas_common_image_premul(Image_Entry *ie)
{
   DATA32  nas = 0;

   if (!ie) return ;
   if (!evas_cache_image_pixels(ie)) return ;
   if (!ie->flags.alpha) return;

   nas = evas_common_convert_argb_premul(evas_cache_image_pixels(ie), ie->w * ie->h);
   if ((ALPHA_SPARSE_INV_FRACTION * nas) >= (ie->w * ie->h))
     ie->flags.alpha_sparse = 1;
}

EAPI void
evas_common_image_set_alpha_sparse(Image_Entry *ie)
{
   DATA32  *s, *se;
   DATA32  nas = 0;

   if (!ie) return;
   if (!evas_cache_image_pixels(ie)) return ;
   if (!ie->flags.alpha) return;

   s = evas_cache_image_pixels(ie);
   se = s + (ie->w * ie->h);
   while (s < se)
     {
	DATA32  p = *s & 0xff000000;

	if (!p || (p == 0xff000000))
	   nas++;
	s++;
     }
   if ((ALPHA_SPARSE_INV_FRACTION * nas) >= (ie->w * ie->h))
     ie->flags.alpha_sparse = 1;
}
