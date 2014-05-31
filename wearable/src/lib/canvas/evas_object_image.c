#ifdef HAVE_CONFIG_H
# include "config.h"  /* so that EAPI in Evas.h is correctly defined */
#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#include <math.h>

#include "evas_common.h"
#include "evas_private.h"
#include "../engines/common/evas_convert_color.h"
#include "../engines/common/evas_convert_colorspace.h"
#include "../engines/common/evas_convert_yuv.h"

#define VERBOSE_PROXY_ERROR 1

/* private magic number for image objects */
static const char o_type[] = "image";

/* private struct for rectangle object internal data */
typedef struct _Evas_Object_Image      Evas_Object_Image;

struct _Evas_Object_Image
{
   DATA32            magic;

   struct {
      int                  spread;
      Evas_Coord_Rectangle fill;
      struct {
           short         w, h, stride;
      } image;
      struct {
           short         l, r, t, b;
           unsigned char fill;
           double        scale;
      } border;

      Evas_Object   *source;
      Evas_Map      *defmap;
      const char    *file;
      const char    *key;
      int            frame;
      Evas_Colorspace cspace;

      unsigned char  smooth_scale : 1;
      unsigned char  has_alpha :1;
      unsigned char  opaque :1;
      unsigned char  opaque_valid :1;
      Eina_Bool      src_cut_w : 1;
      Eina_Bool      src_cut_h : 1;
   } cur, prev;

   int               pixels_checked_out;
   int               load_error;
   Eina_List        *pixel_updates;

   struct {
      unsigned char  scale_down_by;
      double         dpi;
      short          w, h;
      struct {
         short       x, y, w, h;
      } region;
      Eina_Bool  orientation : 1;
   } load_opts;

   struct {
      Evas_Object_Image_Pixels_Get_Cb  get_pixels;
      void                            *get_pixels_data;
   } func;

   Evas_Video_Surface video;

   const char             *tmpf;
   int                     tmpf_fd;

   Evas_Image_Scale_Hint   scale_hint;
   Evas_Image_Content_Hint content_hint;

   void             *engine_data;

   Eina_Bool         changed : 1;
   Eina_Bool         dirty_pixels : 1;
   Eina_Bool         filled : 1;
   Eina_Bool         preloading : 1;
   Eina_Bool         video_surface : 1;
   Eina_Bool         video_visible : 1;
   Eina_Bool         created : 1;
   Eina_Bool         proxyrendering : 1;
   Eina_Bool         proxy_src_clip : 1;
   Eina_Bool         direct_render : 1;
   Eina_Bool         written : 1;
};

/* private methods for image objects */
static void evas_object_image_unload(Evas_Object *obj, Eina_Bool dirty);
static void evas_object_image_load(Evas_Object *obj);
static Evas_Coord evas_object_image_figure_x_fill(Evas_Object *obj, Evas_Coord start, Evas_Coord size, Eina_Bool src_cut, Evas_Coord *size_ret);
static Evas_Coord evas_object_image_figure_y_fill(Evas_Object *obj, Evas_Coord start, Evas_Coord size, Eina_Bool src_cut, Evas_Coord *size_ret);

static void evas_object_image_init(Evas_Object *obj);
static void *evas_object_image_new(void);
static void evas_object_image_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y);
static void evas_object_image_free(Evas_Object *obj);
static void evas_object_image_render_pre(Evas_Object *obj);
static void evas_object_image_render_post(Evas_Object *obj);

static unsigned int evas_object_image_id_get(Evas_Object *obj);
static unsigned int evas_object_image_visual_id_get(Evas_Object *obj);
static void *evas_object_image_engine_data_get(Evas_Object *obj);

static int evas_object_image_is_opaque(Evas_Object *obj);
static int evas_object_image_was_opaque(Evas_Object *obj);
static int evas_object_image_is_inside(Evas_Object *obj, Evas_Coord x, Evas_Coord y);
static int evas_object_image_has_opaque_rect(Evas_Object *obj);
static int evas_object_image_get_opaque_rect(Evas_Object *obj, Evas_Coord *x, Evas_Coord *y, Evas_Coord *w, Evas_Coord *h);
static int evas_object_image_can_map(Evas_Object *obj);

static void *evas_object_image_data_convert_internal(Evas_Object_Image *o, void *data, Evas_Colorspace to_cspace);
static void evas_object_image_filled_resize_listener(void *data, Evas *e, Evas_Object *obj, void *einfo);

static void _proxy_unset(Evas_Object *proxy);
static void _proxy_set(Evas_Object *proxy, Evas_Object *src);
static void _proxy_error(Evas_Object *proxy, void *context, void *output, void *surface, int x, int y);

static void _cleanup_tmpf(Evas_Object *obj);

static const Evas_Object_Func object_func =
{
   /* methods (compulsory) */
   evas_object_image_free,
     evas_object_image_render,
     evas_object_image_render_pre,
     evas_object_image_render_post,
     evas_object_image_id_get,
     evas_object_image_visual_id_get,
     evas_object_image_engine_data_get,
     /* these are optional. NULL = nothing */
     NULL,
     NULL,
     NULL,
     NULL,
     evas_object_image_is_opaque,
     evas_object_image_was_opaque,
     evas_object_image_is_inside,
     NULL,
     NULL,
     NULL,
     evas_object_image_has_opaque_rect,
     evas_object_image_get_opaque_rect,
     evas_object_image_can_map
};

EVAS_MEMPOOL(_mp_obj);

static void
_evas_object_image_cleanup(Evas_Object *obj, Evas_Object_Image *o)
{
   if ((o->preloading) && (o->engine_data))
     {
        o->preloading = EINA_FALSE;
        obj->layer->evas->engine.func->image_data_preload_cancel(obj->layer->evas->engine.data.output,
                                                                 o->engine_data,
                                                                 obj);
     }
   if (o->tmpf) _cleanup_tmpf(obj);
   if (o->cur.source) _proxy_unset(obj);
}

EAPI Evas_Object *
evas_object_image_add(Evas *e)
{
   Evas_Object *obj;
   Evas_Object_Image *o;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return NULL;
   MAGIC_CHECK_END();
   EINA_SAFETY_ON_NULL_RETURN_VAL(e->engine.func, NULL);
   obj = evas_object_new(e);
   evas_object_image_init(obj);
   evas_object_inject(obj, e);
   o = (Evas_Object_Image *)(obj->object_data);
   o->cur.cspace = obj->layer->evas->engine.func->image_colorspace_get(obj->layer->evas->engine.data.output, o->engine_data);
   return obj;
}

EAPI Evas_Object *
evas_object_image_filled_add(Evas *e)
{
   Evas_Object *obj;
   obj = evas_object_image_add(e);
   evas_object_image_filled_set(obj, 1);
   return obj;
}

static void
_cleanup_tmpf(Evas_Object *obj)
{
#ifdef HAVE_SYS_MMAN_H
   Evas_Object_Image *o;

   o = (Evas_Object_Image *)(obj->object_data);
   if (!o->tmpf) return;
#ifdef __linux__
#else
   unlink(o->tmpf);
#endif
   if (o->tmpf_fd >= 0) close(o->tmpf_fd);
   eina_stringshare_del(o->tmpf);
   o->tmpf_fd = -1;
   o->tmpf = NULL;
#else
   (void) obj;
#endif
}

static void
_create_tmpf(Evas_Object *obj, void *data, int size, char *format __UNUSED__)
{
#ifdef HAVE_SYS_MMAN_H
   Evas_Object_Image *o;
   char buf[PATH_MAX];
   void *dst;
   int fd = -1;
   
   o = (Evas_Object_Image *)(obj->object_data);
#ifdef __linux__
   snprintf(buf, sizeof(buf), "/dev/shm/.evas-tmpf-%i-%p-%i-XXXXXX", 
            (int)getpid(), data, (int)size);
   fd = mkstemp(buf);
#endif   
   if (fd < 0)
     {
        const char *tmpdir = getenv("TMPDIR");

        if (!tmpdir)
          {
             tmpdir = getenv("TMP");
             if (!tmpdir)
               {
                  tmpdir = getenv("TEMP");
                  if (!tmpdir) tmpdir = "/tmp";
               }
          }
        snprintf(buf, sizeof(buf), "%s/.evas-tmpf-%i-%p-%i-XXXXXX",
                 tmpdir, (int)getpid(), data, (int)size);
        fd = mkstemp(buf);
        if (fd < 0) return;
     }
   if (ftruncate(fd, size) < 0)
     {
        unlink(buf);
        close(fd);
        return;
     }
#ifdef __linux__
   unlink(buf);
#endif

   eina_mmap_safety_enabled_set(EINA_TRUE);

   dst = mmap(NULL, size,
              PROT_READ | PROT_WRITE,
              MAP_SHARED,
              fd, 0);
   if (dst == MAP_FAILED)
     {
        close(fd);
        return;
     }
   o->tmpf_fd = fd;
#ifdef __linux__
   snprintf(buf, sizeof(buf), "/proc/%li/fd/%i", (long)getpid(), fd);
#endif
   o->tmpf = eina_stringshare_add(buf);
   memcpy(dst, data, size);
   munmap(dst, size);
#else
   (void) obj;
   (void) data;
   (void) size;
   (void) format;
#endif
}

EAPI void
evas_object_image_memfile_set(Evas_Object *obj, void *data, int size, char *format, char *key)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   _cleanup_tmpf(obj);
   evas_object_image_file_set(obj, NULL, NULL);
   // invalidate the cache effectively
   evas_object_image_alpha_set(obj, !o->cur.has_alpha);
   evas_object_image_alpha_set(obj, o->cur.has_alpha);

   if ((size < 1) || (!data)) return;

   _create_tmpf(obj, data, size, format);
   evas_object_image_file_set(obj, o->tmpf, key);
   if (!o->engine_data)
     {
        ERR("unable to load '%s' from memory", o->tmpf);
        _cleanup_tmpf(obj);
        return;
     }
}

EAPI void
evas_object_image_file_set(Evas_Object *obj, const char *file, const char *key)
{
   Evas_Object_Image *o;
   Evas_Image_Load_Opts lo;
   Eina_Bool resize_call = EINA_FALSE;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if ((o->tmpf) && (file != o->tmpf)) _cleanup_tmpf(obj);
   if ((o->cur.file) && (file) && (!strcmp(o->cur.file, file)))
     {
        if ((!o->cur.key) && (!key))
          return;
        if ((o->cur.key) && (key) && (!strcmp(o->cur.key, key)))
          return;
     }
   /*
    * WTF? why cancel a null image preload? this is just silly (tm)
    if (!o->engine_data)
     obj->layer->evas->engine.func->image_data_preload_cancel(obj->layer->evas->engine.data.output,
							      o->engine_data,
							      obj);
 */
   if (o->cur.source) _proxy_unset(obj);
   if (o->cur.file) eina_stringshare_del(o->cur.file);
   if (o->cur.key) eina_stringshare_del(o->cur.key);
   if (file) o->cur.file = eina_stringshare_add(file);
   else o->cur.file = NULL;
   if (key) o->cur.key = eina_stringshare_add(key);
   else o->cur.key = NULL;
   o->prev.file = NULL;
   o->prev.key = NULL;
   if (o->engine_data)
     {
        if (o->preloading)
          {
             o->preloading = EINA_FALSE;
             obj->layer->evas->engine.func->image_data_preload_cancel(obj->layer->evas->engine.data.output, o->engine_data, obj);
          }
        obj->layer->evas->engine.func->image_free(obj->layer->evas->engine.data.output, o->engine_data);
     }
   o->load_error = EVAS_LOAD_ERROR_NONE;
   lo.scale_down_by = o->load_opts.scale_down_by;
   lo.dpi = o->load_opts.dpi;
   lo.w = o->load_opts.w;
   lo.h = o->load_opts.h;
   lo.region.x = o->load_opts.region.x;
   lo.region.y = o->load_opts.region.y;
   lo.region.w = o->load_opts.region.w;
   lo.region.h = o->load_opts.region.h;
   lo.orientation = o->load_opts.orientation;
   o->engine_data = obj->layer->evas->engine.func->image_load(obj->layer->evas->engine.data.output,
                                                              o->cur.file,
                                                              o->cur.key,
                                                              &o->load_error,
                                                              &lo);
   if (o->engine_data)
     {
        int w, h;
        int stride;

        obj->layer->evas->engine.func->image_size_get(obj->layer->evas->engine.data.output, o->engine_data, &w, &h);
        if (obj->layer->evas->engine.func->image_stride_get)
          obj->layer->evas->engine.func->image_stride_get(obj->layer->evas->engine.data.output, o->engine_data, &stride);
        else
          stride = w * 4;
        o->cur.has_alpha = obj->layer->evas->engine.func->image_alpha_get(obj->layer->evas->engine.data.output, o->engine_data);
        o->cur.cspace = obj->layer->evas->engine.func->image_colorspace_get(obj->layer->evas->engine.data.output, o->engine_data);

        if ((o->cur.image.w != w) || (o->cur.image.h != h))
          resize_call = EINA_TRUE;

        o->cur.image.w = w;
        o->cur.image.h = h;
        o->cur.image.stride = stride;
     }
   else
     {
        if (o->load_error == EVAS_LOAD_ERROR_NONE)
          o->load_error = EVAS_LOAD_ERROR_GENERIC;
        o->cur.has_alpha = 1;
        o->cur.cspace = EVAS_COLORSPACE_ARGB8888;

        if ((o->cur.image.w != 0) || (o->cur.image.h != 0))
          resize_call = EINA_TRUE;

        o->cur.image.w = 0;
        o->cur.image.h = 0;
        o->cur.image.stride = 0;
     }
   o->written = EINA_FALSE;
   o->changed = EINA_TRUE;
   if (resize_call) evas_object_inform_call_image_resize(obj);
   evas_object_change(obj);
}

EAPI void
evas_object_image_file_get(const Evas_Object *obj, const char **file, const char **key)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (file) *file = NULL;
   if (key) *key = NULL;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   if (file) *file = NULL;
   if (key) *key = NULL;
   return;
   MAGIC_CHECK_END();
   if (file) *file = o->cur.file;
   if (key) *key = o->cur.key;
}

EAPI void
evas_object_image_source_visible_set(Evas_Object *obj, Eina_Bool visible)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (!o->cur.source) return;

   visible = !!visible;
   if (o->cur.source->proxy.src_invisible == !visible) return;
   o->cur.source->proxy.src_invisible = !visible;
   o->cur.source->changed_src_visible = EINA_TRUE;
   evas_object_smart_member_cache_invalidate(o->cur.source, EINA_FALSE,
                                             EINA_FALSE, EINA_TRUE);
   evas_object_change(obj);
}

EAPI Eina_Bool
evas_object_image_source_visible_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;
   Eina_Bool visible = EINA_FALSE;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   visible = !o->cur.source->proxy.src_invisible;
   return visible;
}

EAPI Eina_Bool
evas_object_image_source_set(Evas_Object *obj, Evas_Object *src)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   o = obj->object_data;
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   if (obj->delete_me && src)
     {
        WRN("Setting deleted object %p as image source %p", src, obj);
        return EINA_FALSE;
     }
   if (src)
     {
        if (src->delete_me)
          {
             WRN("Setting object %p to deleted image source %p", src, obj);
             return EINA_FALSE;
          }
        if (!src->layer)
          {
             CRIT("No evas surface associated with source object (%p)", obj);
             return EINA_FALSE;
          }
        if ((obj->layer && src->layer) &&
            (obj->layer->evas != src->layer->evas))
          {
             CRIT("Setting object %p from Evas (%p) from another Evas (%p)", src, src->layer->evas, obj->layer->evas);
             return EINA_FALSE;
          }
        if (src == obj)
          {
             CRIT("Setting object %p as a source for itself", obj);
             return EINA_FALSE;
          }
     }
   if (o->cur.source == src) return EINA_TRUE;

   _evas_object_image_cleanup(obj, o);
   /* Kill the image if any */
   if (o->cur.file || o->cur.key)
      evas_object_image_file_set(obj, NULL, NULL);

   if (src) _proxy_set(obj, src);
   else _proxy_unset(obj);

   return EINA_TRUE;
}


EAPI Evas_Object *
evas_object_image_source_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = obj->object_data;
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return NULL;
   MAGIC_CHECK_END();

   return o->cur.source;
}

EAPI Eina_Bool
evas_object_image_source_unset(Evas_Object *obj)
{
   return evas_object_image_source_set(obj, NULL);
}

EAPI void
evas_object_image_source_clip_set(Evas_Object *obj, Eina_Bool source_clip)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();

   o = obj->object_data;

   source_clip = !!source_clip;
   if (o->proxy_src_clip == source_clip) return;
   o->proxy_src_clip = source_clip;

   if (!o->cur.source) return;

   evas_object_change(o->cur.source);
}

EAPI Eina_Bool
evas_object_image_source_clip_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   o = obj->object_data;

   return o->proxy_src_clip;
}

EAPI void
evas_object_image_border_set(Evas_Object *obj, int l, int r, int t, int b)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (l < 0) l = 0;
   if (r < 0) r = 0;
   if (t < 0) t = 0;
   if (b < 0) b = 0;
   if ((o->cur.border.l == l) &&
       (o->cur.border.r == r) &&
       (o->cur.border.t == t) &&
       (o->cur.border.b == b)) return;
   o->cur.border.l = l;
   o->cur.border.r = r;
   o->cur.border.t = t;
   o->cur.border.b = b;
   o->cur.opaque_valid = 0;
   o->changed = EINA_TRUE;
   evas_object_change(obj);
}

EAPI void
evas_object_image_border_get(const Evas_Object *obj, int *l, int *r, int *t, int *b)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (l) *l = 0;
   if (r) *r = 0;
   if (t) *t = 0;
   if (b) *b = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   if (l) *l = 0;
   if (r) *r = 0;
   if (t) *t = 0;
   if (b) *b = 0;
   return;
   MAGIC_CHECK_END();
   if (l) *l = o->cur.border.l;
   if (r) *r = o->cur.border.r;
   if (t) *t = o->cur.border.t;
   if (b) *b = o->cur.border.b;
}

EAPI void
evas_object_image_border_center_fill_set(Evas_Object *obj, Evas_Border_Fill_Mode fill)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (fill == o->cur.border.fill) return;
   o->cur.border.fill = fill;
   o->changed = EINA_TRUE;
   evas_object_change(obj);
}

EAPI Evas_Border_Fill_Mode
evas_object_image_border_center_fill_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0;
   MAGIC_CHECK_END();
   return o->cur.border.fill;
}

EAPI void
evas_object_image_filled_set(Evas_Object *obj, Eina_Bool setting)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();

   setting = !!setting;
   if (o->filled == setting) return;

   o->filled = setting;
   if (!o->filled)
     evas_object_event_callback_del(obj, EVAS_CALLBACK_RESIZE,
                                    evas_object_image_filled_resize_listener);
   else
     {
        Evas_Coord w, h;

        evas_object_geometry_get(obj, NULL, NULL, &w, &h);
        evas_object_image_fill_set(obj, 0, 0, w, h);

        evas_object_event_callback_add(obj, EVAS_CALLBACK_RESIZE,
                                       evas_object_image_filled_resize_listener,
                                       NULL);
     }
}

EAPI Eina_Bool
evas_object_image_filled_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0;
   MAGIC_CHECK_END();

   return o->filled;
}

EAPI void
evas_object_image_border_scale_set(Evas_Object *obj, double scale)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (scale == o->cur.border.scale) return;
   o->cur.border.scale = scale;
   o->changed = EINA_TRUE;
   evas_object_change(obj);
}

EAPI double
evas_object_image_border_scale_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 1.0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 1.0;
   MAGIC_CHECK_END();
   return o->cur.border.scale;
}

EAPI void
evas_object_image_fill_set(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   Evas_Object_Image *o;

   if (w == 0) return;
   if (h == 0) return;
   if (w < 0) w = -w;
   if (h < 0) h = -h;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();

   if ((o->cur.fill.x == x) &&
       (o->cur.fill.y == y) &&
       (o->cur.fill.w == w) &&
       (o->cur.fill.h == h)) return;
   o->cur.fill.x = x;
   o->cur.fill.y = y;
   o->cur.fill.w = w;
   o->cur.fill.h = h;
   o->cur.opaque_valid = 0;
   if (o->cur.source) o->cur.source->proxy.redraw = EINA_TRUE;
   o->changed = EINA_TRUE;
   evas_object_change(obj);
}

EAPI void
evas_object_image_fill_get(const Evas_Object *obj, Evas_Coord *x, Evas_Coord *y, Evas_Coord *w, Evas_Coord *h)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (x) *x = 0;
   if (y) *y = 0;
   if (w) *w = 0;
   if (h) *h = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   if (x) *x = 0;
   if (y) *y = 0;
   if (w) *w = 0;
   if (h) *h = 0;
   return;
   MAGIC_CHECK_END();
   if (x) *x = o->cur.fill.x;
   if (y) *y = o->cur.fill.y;
   if (w) *w = o->cur.fill.w;
   if (h) *h = o->cur.fill.h;
}


EAPI void
evas_object_image_fill_spread_set(Evas_Object *obj, Evas_Fill_Spread spread)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (spread == (Evas_Fill_Spread)o->cur.spread) return;
   o->cur.spread = spread;
   o->changed = EINA_TRUE;
   evas_object_change(obj);
}

EAPI Evas_Fill_Spread
evas_object_image_fill_spread_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EVAS_TEXTURE_REPEAT;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EVAS_TEXTURE_REPEAT;
   MAGIC_CHECK_END();
   return (Evas_Fill_Spread)o->cur.spread;
}

EAPI void
evas_object_image_size_set(Evas_Object *obj, int w, int h)
{
   Evas_Object_Image *o;
   int stride = 0;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   _evas_object_image_cleanup(obj, o);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   if (w > 32768) return;
   if (h > 32768) return;
   if ((w == o->cur.image.w) &&
       (h == o->cur.image.h)) return;
   o->cur.image.w = w;
   o->cur.image.h = h;
   if (o->engine_data)
      o->engine_data = obj->layer->evas->engine.func->image_size_set(obj->layer->evas->engine.data.output, o->engine_data, w, h);
   else
      o->engine_data = obj->layer->evas->engine.func->image_new_from_copied_data
      (obj->layer->evas->engine.data.output, w, h, NULL, o->cur.has_alpha,
          o->cur.cspace);

   if (o->engine_data)
     {
        if (obj->layer->evas->engine.func->image_scale_hint_set)
           obj->layer->evas->engine.func->image_scale_hint_set
           (obj->layer->evas->engine.data.output,
               o->engine_data, o->scale_hint);
        if (obj->layer->evas->engine.func->image_content_hint_set)
           obj->layer->evas->engine.func->image_content_hint_set
           (obj->layer->evas->engine.data.output,
               o->engine_data, o->content_hint);
        if (obj->layer->evas->engine.func->image_stride_get)
           obj->layer->evas->engine.func->image_stride_get
           (obj->layer->evas->engine.data.output,
               o->engine_data, &stride);
        else
           stride = w * 4;
     }
   else
      stride = w * 4;
   o->written = EINA_TRUE;
   o->cur.image.stride = stride;

/* FIXME - in engine call above
   if (o->engine_data)
     o->engine_data = obj->layer->evas->engine.func->image_alpha_set(obj->layer->evas->engine.data.output,
								     o->engine_data,
								     o->cur.has_alpha);
*/
   EVAS_OBJECT_IMAGE_FREE_FILE_AND_KEY(o);
   o->changed = EINA_TRUE;
   evas_object_inform_call_image_resize(obj);
   evas_object_change(obj);
}

EAPI void
evas_object_image_size_get(const Evas_Object *obj, int *w, int *h)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (w) *w = 0;
   if (h) *h = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   if (w) *w = 0;
   if (h) *h = 0;
   return;
   MAGIC_CHECK_END();
   if (w) *w = o->cur.image.w;
   if (h) *h = o->cur.image.h;
}

EAPI int
evas_object_image_stride_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0;
   MAGIC_CHECK_END();
   return o->cur.image.stride;
}

EAPI Evas_Load_Error
evas_object_image_load_error_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0;
   MAGIC_CHECK_END();
   return o->load_error;
}

EAPI void *
evas_object_image_data_convert(Evas_Object *obj, Evas_Colorspace to_cspace)
{
   Evas_Object_Image *o;
   DATA32 *data;
   void* result = NULL;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return NULL;
   MAGIC_CHECK_END();
   if ((o->preloading) && (o->engine_data))
     {
        o->preloading = EINA_FALSE;
        obj->layer->evas->engine.func->image_data_preload_cancel(obj->layer->evas->engine.data.output, o->engine_data, obj);
     }
   if (!o->engine_data) return NULL;
   if (o->video_surface)
     o->video.update_pixels(o->video.data, obj, &o->video);
   if (o->cur.cspace == to_cspace) return NULL;
   data = NULL;
   o->engine_data = obj->layer->evas->engine.func->image_data_get(obj->layer->evas->engine.data.output, o->engine_data, 0, &data, &o->load_error);
   result = evas_object_image_data_convert_internal(o, data, to_cspace);
   if (o->engine_data)
     {
        o->engine_data = obj->layer->evas->engine.func->image_data_put(obj->layer->evas->engine.data.output, o->engine_data, data);
     }

  return result;
}
EAPI void
evas_object_image_data_set(Evas_Object *obj, void *data)
{
   Evas_Object_Image *o;
   void *p_data;
   Eina_Bool resize_call = EINA_FALSE;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   _evas_object_image_cleanup(obj, o);
   p_data = o->engine_data;
   if (data)
     {
        if (o->engine_data)
          {
             o->engine_data =
               obj->layer->evas->engine.func->image_data_put(obj->layer->evas->engine.data.output,
                                                             o->engine_data,
                                                             data);
          }
        else
          {
             o->engine_data =
                obj->layer->evas->engine.func->image_new_from_data(obj->layer->evas->engine.data.output,
                                                                  o->cur.image.w,
                                                                  o->cur.image.h,
                                                                  data,
                                                                  o->cur.has_alpha,
                                                                  o->cur.cspace);
          }
        if (o->engine_data)
          {
             int stride = 0;

             if (obj->layer->evas->engine.func->image_scale_hint_set)
                obj->layer->evas->engine.func->image_scale_hint_set
                (obj->layer->evas->engine.data.output,
                    o->engine_data, o->scale_hint);
             if (obj->layer->evas->engine.func->image_content_hint_set)
                obj->layer->evas->engine.func->image_content_hint_set
                (obj->layer->evas->engine.data.output,
                    o->engine_data, o->content_hint);
             if (obj->layer->evas->engine.func->image_stride_get)
                obj->layer->evas->engine.func->image_stride_get
                (obj->layer->evas->engine.data.output,
                    o->engine_data, &stride);
             else
                stride = o->cur.image.w * 4;
             o->cur.image.stride = stride;
         }
        o->written = EINA_TRUE;
     }
   else
     {
        if (o->engine_data)
          obj->layer->evas->engine.func->image_free(obj->layer->evas->engine.data.output, o->engine_data);
        o->load_error = EVAS_LOAD_ERROR_NONE;
        if ((o->cur.image.w != 0) || (o->cur.image.h != 0))
          resize_call = EINA_TRUE;
        o->cur.image.w = 0;
        o->cur.image.h = 0;
        o->cur.image.stride = 0;
        o->engine_data = NULL;
     }
/* FIXME - in engine call above
   if (o->engine_data)
     o->engine_data = obj->layer->evas->engine.func->image_alpha_set(obj->layer->evas->engine.data.output,
								     o->engine_data,
								     o->cur.has_alpha);
*/
   if (o->pixels_checked_out > 0) o->pixels_checked_out--;
   if (p_data != o->engine_data)
     {
        EVAS_OBJECT_IMAGE_FREE_FILE_AND_KEY(o);
        o->written = EINA_TRUE;
        o->pixels_checked_out = 0;
     }
   if (resize_call) evas_object_inform_call_image_resize(obj);
}

EAPI void *
evas_object_image_data_get(const Evas_Object *obj, Eina_Bool for_writing)
{
   Evas_Object_Image *o;
   DATA32 *data;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return NULL;
   MAGIC_CHECK_END();
   if (!o->engine_data) return NULL;

   data = NULL;
   if (obj->layer->evas->engine.func->image_scale_hint_set)
      obj->layer->evas->engine.func->image_scale_hint_set
      (obj->layer->evas->engine.data.output,
          o->engine_data, o->scale_hint);
   if (obj->layer->evas->engine.func->image_content_hint_set)
      obj->layer->evas->engine.func->image_content_hint_set
      (obj->layer->evas->engine.data.output,
          o->engine_data, o->content_hint);
   o->engine_data = obj->layer->evas->engine.func->image_data_get(obj->layer->evas->engine.data.output, o->engine_data, for_writing, &data, &o->load_error);

   /* if we fail to get engine_data, we have to return NULL */
   if (!o->engine_data) return NULL;

   if (o->engine_data)
     {
        int stride = 0;

        if (obj->layer->evas->engine.func->image_stride_get)
           obj->layer->evas->engine.func->image_stride_get
           (obj->layer->evas->engine.data.output,
               o->engine_data, &stride);
        else
           stride = o->cur.image.w * 4;
        o->cur.image.stride = stride;
     }
   o->pixels_checked_out++;
   if (for_writing)
     {
        EVAS_OBJECT_IMAGE_FREE_FILE_AND_KEY(o);
        o->written = EINA_TRUE;
     }

   return data;
}

EAPI void
evas_object_image_preload(Evas_Object *obj, Eina_Bool cancel)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return ;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return ;
   MAGIC_CHECK_END();
   if (!o->engine_data)
     {
        o->preloading = EINA_TRUE;
        evas_object_inform_call_image_preloaded(obj);
        return;
     }
   // FIXME: if already busy preloading, then dont request again until
   // preload done
   if (cancel)
     {
        if (o->preloading)
          {
             o->preloading = EINA_FALSE;
             obj->layer->evas->engine.func->image_data_preload_cancel(obj->layer->evas->engine.data.output,
                                                                      o->engine_data,
                                                                      obj);
          }
     }
   else
     {
        if (!o->preloading)
          {
             o->preloading = EINA_TRUE;
             obj->layer->evas->engine.func->image_data_preload_request(obj->layer->evas->engine.data.output,
                                                                       o->engine_data,
                                                                       obj);
          }
     }
}

EAPI void
evas_object_image_data_copy_set(Evas_Object *obj, void *data)
{
   Evas_Object_Image *o;

   if (!data) return;
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   _evas_object_image_cleanup(obj, o);
   if ((o->cur.image.w <= 0) ||
       (o->cur.image.h <= 0)) return;
   if (o->engine_data)
     obj->layer->evas->engine.func->image_free(obj->layer->evas->engine.data.output,
                                               o->engine_data);
   o->engine_data =
     obj->layer->evas->engine.func->image_new_from_copied_data(obj->layer->evas->engine.data.output,
                                                               o->cur.image.w,
                                                               o->cur.image.h,
                                                               data,
                                                               o->cur.has_alpha,
                                                               o->cur.cspace);
   if (o->engine_data)
     {
        int stride = 0;

        o->engine_data =
          obj->layer->evas->engine.func->image_alpha_set(obj->layer->evas->engine.data.output,
                                                         o->engine_data,
                                                         o->cur.has_alpha);
        if (obj->layer->evas->engine.func->image_scale_hint_set)
           obj->layer->evas->engine.func->image_scale_hint_set
           (obj->layer->evas->engine.data.output,
               o->engine_data, o->scale_hint);
        if (obj->layer->evas->engine.func->image_content_hint_set)
           obj->layer->evas->engine.func->image_content_hint_set
           (obj->layer->evas->engine.data.output,
               o->engine_data, o->content_hint);
        if (obj->layer->evas->engine.func->image_stride_get)
           obj->layer->evas->engine.func->image_stride_get
           (obj->layer->evas->engine.data.output,
               o->engine_data, &stride);
        else
           stride = o->cur.image.w * 4;
        o->cur.image.stride = stride;
        o->written = EINA_TRUE;
     }
   o->pixels_checked_out = 0;
   EVAS_OBJECT_IMAGE_FREE_FILE_AND_KEY(o);
}

EAPI void
evas_object_image_data_update_add(Evas_Object *obj, int x, int y, int w, int h)
{
   Evas_Object_Image *o;
   Eina_Rectangle *r;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   RECTS_CLIP_TO_RECT(x, y, w, h, 0, 0, o->cur.image.w, o->cur.image.h);
   if ((w <= 0)  || (h <= 0)) return;
   if (!o->written) return;
   NEW_RECT(r, x, y, w, h);
   if (r) o->pixel_updates = eina_list_append(o->pixel_updates, r);

   o->changed = EINA_TRUE;
   evas_object_change(obj);
}

EAPI void
evas_object_image_alpha_set(Evas_Object *obj, Eina_Bool has_alpha)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if ((o->preloading) && (o->engine_data))
     {
        o->preloading = EINA_FALSE;
        obj->layer->evas->engine.func->image_data_preload_cancel(obj->layer->evas->engine.data.output,
                                                                 o->engine_data,
                                                                 obj);
     }
   if (((has_alpha) && (o->cur.has_alpha)) ||
       ((!has_alpha) && (!o->cur.has_alpha)))
     return;
   o->cur.has_alpha = has_alpha;
   if (o->engine_data)
     {
        int stride = 0;

        o->engine_data =
          obj->layer->evas->engine.func->image_alpha_set(obj->layer->evas->engine.data.output,
                                                         o->engine_data,
                                                         o->cur.has_alpha);
        if (obj->layer->evas->engine.func->image_scale_hint_set)
           obj->layer->evas->engine.func->image_scale_hint_set
           (obj->layer->evas->engine.data.output,
               o->engine_data, o->scale_hint);
        if (obj->layer->evas->engine.func->image_content_hint_set)
           obj->layer->evas->engine.func->image_content_hint_set
           (obj->layer->evas->engine.data.output,
               o->engine_data, o->content_hint);
        if (obj->layer->evas->engine.func->image_stride_get)
           obj->layer->evas->engine.func->image_stride_get
           (obj->layer->evas->engine.data.output,
               o->engine_data, &stride);
        else
           stride = o->cur.image.w * 4;
        o->cur.image.stride = stride;
        o->written = EINA_TRUE;
     }
   evas_object_image_data_update_add(obj, 0, 0, o->cur.image.w, o->cur.image.h);
   EVAS_OBJECT_IMAGE_FREE_FILE_AND_KEY(o);
}


EAPI Eina_Bool
evas_object_image_alpha_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0;
   MAGIC_CHECK_END();
   return o->cur.has_alpha;
}

EAPI void
evas_object_image_smooth_scale_set(Evas_Object *obj, Eina_Bool smooth_scale)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (((smooth_scale) && (o->cur.smooth_scale)) ||
       ((!smooth_scale) && (!o->cur.smooth_scale)))
     return;
   o->cur.smooth_scale = smooth_scale;
   o->changed = EINA_TRUE;
   evas_object_change(obj);
}

EAPI Eina_Bool
evas_object_image_smooth_scale_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0;
   MAGIC_CHECK_END();
   return o->cur.smooth_scale;
}

EAPI void
evas_object_image_reload(Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if ((o->preloading) && (o->engine_data))
     {
        o->preloading = EINA_FALSE;
        obj->layer->evas->engine.func->image_data_preload_cancel(obj->layer->evas->engine.data.output,
                                                                 o->engine_data,
                                                                 obj);
     }
   if ((!o->cur.file) ||
       (o->pixels_checked_out > 0)) return;
   if (o->engine_data)
     o->engine_data = obj->layer->evas->engine.func->image_dirty_region(obj->layer->evas->engine.data.output, o->engine_data, 0, 0, o->cur.image.w, o->cur.image.h);
   evas_object_image_unload(obj, 1);
   evas_object_inform_call_image_unloaded(obj);
   evas_object_image_load(obj);
   o->prev.file = NULL;
   o->prev.key = NULL;
   o->changed = EINA_TRUE;
   evas_object_change(obj);
}

EAPI Eina_Bool
evas_object_image_save(const Evas_Object *obj, const char *file, const char *key, const char *flags)
{
   Evas_Object_Image *o;
   DATA32 *data = NULL;
   int quality = 80, compress = 9, ok = 0;
   RGBA_Image *im;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0;
   MAGIC_CHECK_END();

   if (!o->engine_data) return 0;
   o->engine_data = obj->layer->evas->engine.func->image_data_get(obj->layer->evas->engine.data.output, o->engine_data, 0, &data, &o->load_error);
   if (flags)
     {
        char *p, *pp;
        char *tflags;

        tflags = alloca(strlen(flags) + 1);
        strcpy(tflags, flags);
        p = tflags;
        while (p)
          {
             pp = strchr(p, ' ');
             if (pp) *pp = 0;
             sscanf(p, "quality=%i", &quality);
             sscanf(p, "compress=%i", &compress);
             if (pp) p = pp + 1;
             else break;
          }
     }
   im = (RGBA_Image*) evas_cache_image_data(evas_common_image_cache_get(),
                                            o->cur.image.w,
                                            o->cur.image.h,
                                            data,
                                            o->cur.has_alpha,
                                            EVAS_COLORSPACE_ARGB8888);
   if (im)
     {
        if (o->cur.cspace == EVAS_COLORSPACE_ARGB8888)
          im->image.data = data;
        else
          im->image.data = evas_object_image_data_convert_internal(o,
                                                                   data,
                                                                   EVAS_COLORSPACE_ARGB8888);
        if (im->image.data)
          {
             ok = evas_common_save_image_to_file(im, file, key, quality, compress);

             if (o->cur.cspace != EVAS_COLORSPACE_ARGB8888)
               free(im->image.data);
          }

        evas_cache_image_drop(&im->cache_entry);
     }
   o->engine_data = obj->layer->evas->engine.func->image_data_put(obj->layer->evas->engine.data.output,
                                                                  o->engine_data,
                                                                  data);
   return ok;
}

EAPI Eina_Bool
evas_object_image_pixels_import(Evas_Object *obj, Evas_Pixel_Import_Source *pixels)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0;
   MAGIC_CHECK_END();
   _evas_object_image_cleanup(obj, o);
   if ((pixels->w != o->cur.image.w) || (pixels->h != o->cur.image.h)) return 0;
   switch (pixels->format)
     {
#if 0
      case EVAS_PIXEL_FORMAT_ARGB32:
	  {
	     if (o->engine_data)
	       {
		  DATA32 *image_pixels = NULL;

		  o->engine_data =
		    obj->layer->evas->engine.func->image_data_get(obj->layer->evas->engine.data.output,
								  o->engine_data,
								  1,
								  &image_pixels,
                                                                  &o->load_error);
/* FIXME: need to actualyl support this */
/*		  memcpy(image_pixels, pixels->rows, o->cur.image.w * o->cur.image.h * 4);*/
		  if (o->engine_data)
		    o->engine_data =
		    obj->layer->evas->engine.func->image_data_put(obj->layer->evas->engine.data.output,
								  o->engine_data,
								  image_pixels);
		  if (o->engine_data)
		    o->engine_data =
		    obj->layer->evas->engine.func->image_alpha_set(obj->layer->evas->engine.data.output,
								   o->engine_data,
								   o->cur.has_alpha);
		  o->changed = EINA_TRUE;
		  evas_object_change(obj);
	       }
	  }
	break;
#endif
#ifdef BUILD_CONVERT_YUV
      case EVAS_PIXEL_FORMAT_YUV420P_601:
          {
             if (o->engine_data)
               {
                  DATA32 *image_pixels = NULL;

                  o->engine_data =
                     obj->layer->evas->engine.func->image_data_get(obj->layer->evas->engine.data.output, o->engine_data, 1, &image_pixels,&o->load_error);
                  if (image_pixels)
                    evas_common_convert_yuv_420p_601_rgba((DATA8 **) pixels->rows, (DATA8 *) image_pixels, o->cur.image.w, o->cur.image.h);
                  if (o->engine_data)
                    o->engine_data =
                       obj->layer->evas->engine.func->image_data_put(obj->layer->evas->engine.data.output, o->engine_data, image_pixels);
                  if (o->engine_data)
                    o->engine_data =
                       obj->layer->evas->engine.func->image_alpha_set(obj->layer->evas->engine.data.output, o->engine_data, o->cur.has_alpha);
                  o->changed = EINA_TRUE;
                  evas_object_change(obj);
               }
          }
        break;
#endif
      default:
        return 0;
        break;
     }
   return 1;
}

EAPI void
evas_object_image_pixels_get_callback_set(Evas_Object *obj, Evas_Object_Image_Pixels_Get_Cb func, void *data)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   o->func.get_pixels = func;
   o->func.get_pixels_data = data;
}

EAPI void
evas_object_image_pixels_dirty_set(Evas_Object *obj, Eina_Bool dirty)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (dirty) o->dirty_pixels = EINA_TRUE;
   else o->dirty_pixels = EINA_FALSE;
   o->changed = EINA_TRUE;
   evas_object_change(obj);
}

EAPI Eina_Bool
evas_object_image_pixels_dirty_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0;
   MAGIC_CHECK_END();
   if (o->dirty_pixels) return 1;
   return 0;
}

EAPI void
evas_object_image_load_dpi_set(Evas_Object *obj, double dpi)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (dpi == o->load_opts.dpi) return;
   o->load_opts.dpi = dpi;
   if (o->cur.file)
     {
        evas_object_image_unload(obj, 0);
        evas_object_inform_call_image_unloaded(obj);
        evas_object_image_load(obj);
        o->changed = EINA_TRUE;
        evas_object_change(obj);
     }
}

EAPI double
evas_object_image_load_dpi_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0.0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0.0;
   MAGIC_CHECK_END();
   return o->load_opts.dpi;
}

EAPI void
evas_object_image_load_size_set(Evas_Object *obj, int w, int h)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if ((o->load_opts.w == w) && (o->load_opts.h == h)) return;
   o->load_opts.w = w;
   o->load_opts.h = h;
   if (o->cur.file)
     {
        evas_object_image_unload(obj, 0);
        evas_object_inform_call_image_unloaded(obj);
        evas_object_image_load(obj);
        o->changed = EINA_TRUE;
        evas_object_change(obj);
     }
}

EAPI void
evas_object_image_load_size_get(const Evas_Object *obj, int *w, int *h)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (w) *w = o->load_opts.w;
   if (h) *h = o->load_opts.h;
}

EAPI void
evas_object_image_load_scale_down_set(Evas_Object *obj, int scale_down)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (o->load_opts.scale_down_by == scale_down) return;
   o->load_opts.scale_down_by = scale_down;
   if (o->cur.file)
     {
        evas_object_image_unload(obj, 0);
        evas_object_inform_call_image_unloaded(obj);
        evas_object_image_load(obj);
        o->changed = EINA_TRUE;
        evas_object_change(obj);
     }
}

EAPI int
evas_object_image_load_scale_down_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return 0;
   MAGIC_CHECK_END();
   return o->load_opts.scale_down_by;
}

EAPI void
evas_object_image_load_region_set(Evas_Object *obj, int x, int y, int w, int h)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if ((o->load_opts.region.x == x) && (o->load_opts.region.y == y) &&
       (o->load_opts.region.w == w) && (o->load_opts.region.h == h)) return;
   o->load_opts.region.x = x;
   o->load_opts.region.y = y;
   o->load_opts.region.w = w;
   o->load_opts.region.h = h;
   if (o->cur.file)
     {
        evas_object_image_unload(obj, 0);
        evas_object_inform_call_image_unloaded(obj);
        evas_object_image_load(obj);
        o->changed = EINA_TRUE;
        evas_object_change(obj);
     }
}

EAPI void
evas_object_image_load_region_get(const Evas_Object *obj, int *x, int *y, int *w, int *h)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (x) *x = o->load_opts.region.x;
   if (y) *y = o->load_opts.region.y;
   if (w) *w = o->load_opts.region.w;
   if (h) *h = o->load_opts.region.h;
}

EAPI void
evas_object_image_load_orientation_set(Evas_Object *obj, Eina_Bool enable)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   o->load_opts.orientation = !!enable;
}

EAPI Eina_Bool
evas_object_image_load_orientation_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   return o->load_opts.orientation;
}

EAPI void
evas_object_image_colorspace_set(Evas_Object *obj, Evas_Colorspace cspace)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();

   _evas_object_image_cleanup(obj, o);

   o->cur.cspace = cspace;
   if (o->engine_data)
     obj->layer->evas->engine.func->image_colorspace_set(obj->layer->evas->engine.data.output, o->engine_data, cspace);
}

EAPI Evas_Colorspace
evas_object_image_colorspace_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EVAS_COLORSPACE_ARGB8888;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EVAS_COLORSPACE_ARGB8888;
   MAGIC_CHECK_END();
   return o->cur.cspace;
}

EAPI void
evas_object_image_video_surface_set(Evas_Object *obj, Evas_Video_Surface *surf)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   _evas_object_image_cleanup(obj, o);
   if (o->video_surface)
     {
        o->video_surface = EINA_FALSE;
        obj->layer->evas->video_objects = eina_list_remove(obj->layer->evas->video_objects, obj);
     }

   if (surf)
     {
        if (surf->version != EVAS_VIDEO_SURFACE_VERSION) return ;

        if (!surf->update_pixels ||
            !surf->move ||
            !surf->resize ||
            !surf->hide ||
            !surf->show)
          return ;

        o->created = EINA_TRUE;
        o->video_surface = EINA_TRUE;
        o->video = *surf;

        obj->layer->evas->video_objects = eina_list_append(obj->layer->evas->video_objects, obj);
     }
   else
     {
        o->video_surface = EINA_FALSE;
        o->video.update_pixels = NULL;
        o->video.move = NULL;
        o->video.resize = NULL;
        o->video.hide = NULL;
        o->video.show = NULL;
        o->video.data = NULL;
     }
}

EAPI const Evas_Video_Surface *
evas_object_image_video_surface_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return NULL;
   MAGIC_CHECK_END();
   if (!o->video_surface) return NULL;
   return &o->video;
}

EAPI void
evas_object_image_native_surface_set(Evas_Object *obj, Evas_Native_Surface *surf)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   _evas_object_image_cleanup(obj, o);
   if (!obj->layer->evas->engine.func->image_native_set) return;
   if ((surf) &&
       ((surf->version < 2) ||
        (surf->version > EVAS_NATIVE_SURFACE_VERSION))) return;
   o->engine_data = obj->layer->evas->engine.func->image_native_set(obj->layer->evas->engine.data.output, o->engine_data, surf);
}

EAPI Evas_Native_Surface *
evas_object_image_native_surface_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return NULL;
   MAGIC_CHECK_END();
   if (!obj->layer->evas->engine.func->image_native_get) return NULL;
   return obj->layer->evas->engine.func->image_native_get(obj->layer->evas->engine.data.output, o->engine_data);
}

EAPI void
evas_object_image_scale_hint_set(Evas_Object *obj, Evas_Image_Scale_Hint hint)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (o->scale_hint == hint) return;
   o->scale_hint = hint;
   if (o->engine_data)
     {
        int stride = 0;

        if (obj->layer->evas->engine.func->image_scale_hint_set)
          obj->layer->evas->engine.func->image_scale_hint_set
             (obj->layer->evas->engine.data.output,
               o->engine_data, o->scale_hint);
        if (obj->layer->evas->engine.func->image_stride_get)
           obj->layer->evas->engine.func->image_stride_get
           (obj->layer->evas->engine.data.output,
               o->engine_data, &stride);
        else
           stride = o->cur.image.w * 4;
        o->cur.image.stride = stride;
     }
}

EAPI Evas_Image_Scale_Hint
evas_object_image_scale_hint_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EVAS_IMAGE_SCALE_HINT_NONE;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EVAS_IMAGE_SCALE_HINT_NONE;
   MAGIC_CHECK_END();
   return o->scale_hint;
}

EAPI void
evas_object_image_content_hint_set(Evas_Object *obj, Evas_Image_Content_Hint hint)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   if (o->content_hint == hint) return;
   o->content_hint = hint;
   if (o->engine_data)
     {
        int stride = 0;

        if (obj->layer->evas->engine.func->image_content_hint_set)
           obj->layer->evas->engine.func->image_content_hint_set
           (obj->layer->evas->engine.data.output,
               o->engine_data, o->content_hint);
        if (obj->layer->evas->engine.func->image_stride_get)
           obj->layer->evas->engine.func->image_stride_get
           (obj->layer->evas->engine.data.output,
               o->engine_data, &stride);
        else
           stride = o->cur.image.w * 4;
        o->cur.image.stride = stride;
     }
}

EAPI void
evas_object_image_alpha_mask_set(Evas_Object *obj, Eina_Bool ismask)
{
   Evas_Object_Image *o;

   if (!ismask) return;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();

   /* Convert to A8 if not already */

   /* done */

}

#define FRAME_MAX 1024
EAPI Evas_Image_Content_Hint
evas_object_image_content_hint_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EVAS_IMAGE_CONTENT_HINT_NONE;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EVAS_IMAGE_CONTENT_HINT_NONE;
   MAGIC_CHECK_END();
   return o->content_hint;
}

EAPI Eina_Bool
evas_object_image_region_support_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *) (obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   return obj->layer->evas->engine.func->image_can_region_get(
      obj->layer->evas->engine.data.output,
      o->engine_data);
}

/* animated feature */
EAPI Eina_Bool
evas_object_image_animated_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   if (obj->layer->evas->engine.func->image_animated_get)
     return obj->layer->evas->engine.func->image_animated_get(obj->layer->evas->engine.data.output, o->engine_data);
   return EINA_FALSE;
}

EAPI int
evas_object_image_animated_frame_count_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return -1;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return -1;
   MAGIC_CHECK_END();

   if (!evas_object_image_animated_get(obj)) return -1;
   if (obj->layer->evas->engine.func->image_animated_frame_count_get)
     return obj->layer->evas->engine.func->image_animated_frame_count_get(obj->layer->evas->engine.data.output, o->engine_data);
   return -1;
}

EAPI Evas_Image_Animated_Loop_Hint
evas_object_image_animated_loop_type_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EVAS_IMAGE_ANIMATED_HINT_NONE;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EVAS_IMAGE_ANIMATED_HINT_NONE;
   MAGIC_CHECK_END();

   if (!evas_object_image_animated_get(obj)) return EVAS_IMAGE_ANIMATED_HINT_NONE;

   if (obj->layer->evas->engine.func->image_animated_loop_type_get)
     return obj->layer->evas->engine.func->image_animated_loop_type_get(obj->layer->evas->engine.data.output, o->engine_data);
   return EVAS_IMAGE_ANIMATED_HINT_NONE;
}

EAPI int
evas_object_image_animated_loop_count_get(const Evas_Object *obj)
{
   Evas_Object_Image *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return -1;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return -1;
   MAGIC_CHECK_END();

   if (!evas_object_image_animated_get(obj)) return -1;

   if (obj->layer->evas->engine.func->image_animated_loop_count_get)
     return obj->layer->evas->engine.func->image_animated_loop_count_get(obj->layer->evas->engine.data.output, o->engine_data);
   return -1;
}

EAPI double
evas_object_image_animated_frame_duration_get(const Evas_Object *obj, int start_frame, int frame_num)
{
   Evas_Object_Image *o;
   int frame_count = 0;

   if (start_frame < 1) return -1;
   if (frame_num < 0) return -1;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return -1;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return -1;
   MAGIC_CHECK_END();

   if (!evas_object_image_animated_get(obj)) return -1;

   if (!obj->layer->evas->engine.func->image_animated_frame_count_get) return -1;

   frame_count = obj->layer->evas->engine.func->image_animated_frame_count_get(obj->layer->evas->engine.data.output, o->engine_data);

   if ((start_frame + frame_num) > frame_count) return -1;
   if (obj->layer->evas->engine.func->image_animated_frame_duration_get)
     return obj->layer->evas->engine.func->image_animated_frame_duration_get(obj->layer->evas->engine.data.output, o->engine_data, start_frame, frame_num);
   return -1;
}

EAPI void
evas_object_image_animated_frame_set(Evas_Object *obj, int frame_index)
{
   Evas_Object_Image *o;
   int frame_count = 0;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();

   if (!o->cur.file) return;
   if (o->cur.frame == frame_index) return;

   if (!evas_object_image_animated_get(obj)) return;

   frame_count = evas_object_image_animated_frame_count_get(obj);

   /* limit the size of frame to FRAME_MAX */
   if ((frame_count > FRAME_MAX) || (frame_count < 0) || (frame_index > frame_count))
     return;

   if (!obj->layer->evas->engine.func->image_animated_frame_set) return;
   if (!obj->layer->evas->engine.func->image_animated_frame_set(obj->layer->evas->engine.data.output, o->engine_data, frame_index))
     return;

   o->prev.frame = o->cur.frame;
   o->cur.frame = frame_index;

   o->changed = EINA_TRUE;
   evas_object_change(obj);

}

EAPI void
evas_image_cache_flush(Evas *e)
{
   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return;
   MAGIC_CHECK_END();

   e->engine.func->image_cache_flush(e->engine.data.output);
}

EAPI void
evas_image_cache_reload(Evas *e)
{
   Evas_Layer *layer;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return;
   MAGIC_CHECK_END();

   evas_image_cache_flush(e);
   EINA_INLIST_FOREACH(e->layers, layer)
     {
        Evas_Object *obj;

        EINA_INLIST_FOREACH(layer->objects, obj)
          {
             Evas_Object_Image *o;

             o = (Evas_Object_Image *)(obj->object_data);
             if (o->magic == MAGIC_OBJ_IMAGE)
               {
                  evas_object_image_unload(obj, 1);
                  evas_object_inform_call_image_unloaded(obj);
               }
          }
     }
   evas_image_cache_flush(e);
   EINA_INLIST_FOREACH(e->layers, layer)
     {
        Evas_Object *obj;

        EINA_INLIST_FOREACH(layer->objects, obj)
          {
             Evas_Object_Image *o;

             o = (Evas_Object_Image *)(obj->object_data);
             if (o->magic == MAGIC_OBJ_IMAGE)
               {
                  evas_object_image_load(obj);
                  o->changed = EINA_TRUE;
                  evas_object_change(obj);
               }
          }
     }
   evas_image_cache_flush(e);
}

EAPI void
evas_image_cache_set(Evas *e, int size)
{
   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return;
   MAGIC_CHECK_END();

   if (size < 0) size = 0;
   e->engine.func->image_cache_set(e->engine.data.output, size);
}

EAPI int
evas_image_cache_get(const Evas *e)
{
   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return 0;
   MAGIC_CHECK_END();

   return e->engine.func->image_cache_get(e->engine.data.output);
}

EAPI Eina_Bool
evas_image_max_size_get(const Evas *e, int *maxw, int *maxh)
{
   int w = 0, h = 0;
   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   if (maxw) *maxw = 0xffff;
   if (maxh) *maxh = 0xffff;
   if (!e->engine.func->image_max_size_get) return EINA_FALSE;
   e->engine.func->image_max_size_get(e->engine.data.output, &w, &h);
   if (maxw) *maxw = w;
   if (maxh) *maxh = h;
   return EINA_TRUE;
}

/* all nice and private */
static void
_proxy_unset(Evas_Object *proxy)
{
   Evas_Object_Image *o;

   o = proxy->object_data;
   if (!o->cur.source) return;

   if (o->cur.source->proxy.surface)
     {
        proxy->layer->evas->engine.func->image_map_surface_free(proxy->layer->evas->engine.data.output,
                                                                o->cur.source->proxy.surface);
        o->cur.source->proxy.surface = NULL;
     }

   o->cur.source->proxy.proxies = eina_list_remove(o->cur.source->proxy.proxies, proxy);

   o->cur.source = NULL;
   if (o->cur.defmap)
     {
        evas_map_free(o->cur.defmap);
        o->cur.defmap = NULL;
     }
   o->cur.src_cut_w = EINA_FALSE;
   o->cur.src_cut_h = EINA_FALSE;
}


static void
_proxy_set(Evas_Object *proxy, Evas_Object *src)
{
   Evas_Object_Image *o;

   o = proxy->object_data;

   evas_object_image_file_set(proxy, NULL, NULL);

   o->cur.source = src;
   o->load_error = EVAS_LOAD_ERROR_NONE;

   src->proxy.proxies = eina_list_append(src->proxy.proxies, proxy);
   src->proxy.redraw = EINA_TRUE;
}

/* Some moron just set a proxy on a proxy.
 * Give them some pixels.  A random color
 */
static void
_proxy_error(Evas_Object *proxy, void *context, void *output, void *surface,
             int x, int y)
{
   Evas_Func *func;
   int r = rand() % 255;
   int g = rand() % 255;
   int b = rand() % 255;

   /* XXX: Eina log error or something I'm sure
    * If it bugs you, just fix it.  Don't tell me */
   if (VERBOSE_PROXY_ERROR) printf("Err: Argh! Recursive proxies.\n");

   func = proxy->layer->evas->engine.func;
   func->context_color_set(output, context, r, g, b, 255);
   func->context_multiplier_unset(output, context);
   func->context_render_op_set(output, context, proxy->cur.render_op);
   func->rectangle_draw(output, context, surface, proxy->cur.geometry.x + x,
                        proxy->cur.geometry.y + y,
                        proxy->cur.geometry.w,
                        proxy->cur.geometry.h);
   return;
}

/* Check the proxy just shows a part of the source area.
   If so, proxy doens't need to allocate whole size of the source surface.
   This related code could be removed once the gl texture maximum size
   limitation is unleashed. */
static void
_proxy_area_get(Evas_Object *proxy_obj, Evas_Object_Image *o, Evas_Object *source, Evas_Coord_Point* offset, Evas_Coord_Size* size, Eina_Bool *src_cut_w, Eina_Bool *src_cut_h)
{
   double scale_x = (double) source->cur.geometry.w / (double) o->cur.fill.w;
   double scale_y = (double) source->cur.geometry.h / (double) o->cur.fill.h;
   size->w = ((double) proxy_obj->cur.geometry.w * scale_x);
   size->h = ((double) proxy_obj->cur.geometry.h * scale_y);

   //Need to draw whole area of the source.
   /* Tizen Only: Since Proxy has the texture size limitation problem,
      we set a key value for entry magnifier so that evas does some hackish way.
      This hackish code should be removed once evas supports a mechanism like a
      virtual texture. */
   /* if ((o->cur.fill.x > 0) ||
        ((o->cur.fill.x + o->cur.fill.w) < proxy_obj->cur.geometry.w)) */
   if (!evas_object_data_get(proxy_obj, "tizenonly") &&
       ((o->cur.fill.x > 0) ||
         ((o->cur.fill.x + o->cur.fill.w) < proxy_obj->cur.geometry.w)))
     {
        size->w = source->cur.geometry.w;
        offset->x = -source->cur.geometry.x;
        *src_cut_w = EINA_FALSE;
     }
   //Need to draw only a part of the source.
   else
     {
        offset->x = -(source->cur.geometry.x -
                      ((double) o->cur.fill.x * scale_x));
        *src_cut_w = EINA_TRUE;
     }
   //Need to draw whole area of the source.
   /* Tizen Only: Since Proxy has the texture size limitation problem,
      we set a key value for entry magnifier so that evas does some hackish way.
      This hackish code should be removed once evas supports a mechanism like a
      virtual texture. */
   /* if ((o->cur.fill.y > 0) ||
        ((o->cur.fill.y + o->cur.fill.h) < proxy_obj->cur.geometry.h)) */
   if (!evas_object_data_get(proxy_obj, "tizenonly") &&
       ((o->cur.fill.y > 0) ||
         ((o->cur.fill.y + o->cur.fill.h) < proxy_obj->cur.geometry.h)))
     {
        size->h = source->cur.geometry.h;
        offset->y = -source->cur.geometry.y;
        *src_cut_h = EINA_FALSE;
     }
   //Need to draw only a part of the source.
   else
     {
        offset->y = -(source->cur.geometry.y -
                      ((double) o->cur.fill.y * scale_y));
        *src_cut_h = EINA_TRUE;
     }
}

/**
 * Render the source object when a proxy is set.
 *
 * Used to force a draw if necessary, else just makes sures it's available.
 */
static void
_proxy_subrender(Evas *e, Evas_Object *source, Evas_Object *proxy)
{
   void *ctx;
   int w, h;
   Evas_Object_Image *o;
   Evas_Coord_Point offset;
   Evas_Coord_Size size;
   Eina_Bool src_cut_w, src_cut_h;

   if (!source) return;

   o = proxy->object_data;
   _proxy_area_get(proxy, o, source, &offset, &size, &src_cut_w, &src_cut_h);
   o->cur.src_cut_w = src_cut_w;
   o->cur.src_cut_h = src_cut_h;

   w = source->cur.geometry.w;
   h = source->cur.geometry.h;

   source->proxy.redraw = EINA_FALSE;

   /* We need to redraw surface then */
   if ((source->proxy.surface) &&
       ((source->proxy.w != size.w) || (source->proxy.h != size.h)))
     {
        e->engine.func->image_map_surface_free(e->engine.data.output,
                                               source->proxy.surface);
        source->proxy.surface = NULL;
     }

   /* FIXME: Hardcoded alpha 'on' */
   /* FIXME (cont): Should see if the object has alpha */
   if (!source->proxy.surface)
     {
        source->proxy.surface = e->engine.func->image_map_surface_new
           (e->engine.data.output, size.w, size.h, 1);
        source->proxy.w = size.w;
        source->proxy.h = size.h;
     }

   if (!source->proxy.surface) return;

   ctx = e->engine.func->context_new(e->engine.data.output);
   e->engine.func->context_color_set(e->engine.data.output, ctx, 0, 0, 0,
                                     0);
   e->engine.func->context_render_op_set(e->engine.data.output, ctx,
                                         EVAS_RENDER_COPY);
   e->engine.func->rectangle_draw(e->engine.data.output, ctx,
                                  source->proxy.surface, 0, 0, size.w, size.h);
   e->engine.func->context_free(e->engine.data.output, ctx);

   ctx = e->engine.func->context_new(e->engine.data.output);

   Eina_Bool source_clip = evas_object_image_source_clip_get(proxy);

   Evas_Proxy_Render_Data proxy_render_data = {
        .proxy_obj = proxy,
        .src_obj = source,
        .source_clip = source_clip
   };

   evas_render_mapped(e, source, ctx, source->proxy.surface, offset.x, offset.y,
                      1, 0, 0, e->output.w, e->output.h, &proxy_render_data
#ifdef REND_DBG
                      , 1
#endif
                     );
   e->engine.func->context_free(e->engine.data.output, ctx);

   source->proxy.surface = e->engine.func->image_dirty_region
      (e->engine.data.output, source->proxy.surface, 0, 0, size.w, size.h);
}

static void
evas_object_image_unload(Evas_Object *obj, Eina_Bool dirty)
{
   Evas_Object_Image *o;
   Eina_Bool resize_call = EINA_FALSE;

   o = (Evas_Object_Image *)(obj->object_data);

   if ((!o->cur.file) ||
       (o->pixels_checked_out > 0)) return;
   if (dirty)
     {
        if (o->engine_data)
           o->engine_data = obj->layer->evas->engine.func->image_dirty_region
           (obj->layer->evas->engine.data.output,
               o->engine_data,
               0, 0,
               o->cur.image.w, o->cur.image.h);
     }
   if (o->engine_data)
     {
        if (o->preloading)
          {
             o->preloading = EINA_FALSE;
             obj->layer->evas->engine.func->image_data_preload_cancel(obj->layer->evas->engine.data.output,
                                                                      o->engine_data,
                                                                      obj);
          }
        obj->layer->evas->engine.func->image_free(obj->layer->evas->engine.data.output,
                                                  o->engine_data);
     }
   o->engine_data = NULL;
   o->load_error = EVAS_LOAD_ERROR_NONE;
   o->cur.has_alpha = 1;
   o->cur.cspace = EVAS_COLORSPACE_ARGB8888;
   if ((o->cur.image.w != 0) || (o->cur.image.h != 0)) resize_call = EINA_TRUE;
   o->cur.image.w = 0;
   o->cur.image.h = 0;
   o->cur.image.stride = 0;
   if (resize_call) evas_object_inform_call_image_resize(obj);
}

static void
evas_object_image_load(Evas_Object *obj)
{
   Evas_Object_Image *o;
   Evas_Image_Load_Opts lo;

   o = (Evas_Object_Image *)(obj->object_data);
   if (o->engine_data) return;

   lo.scale_down_by = o->load_opts.scale_down_by;
   lo.dpi = o->load_opts.dpi;
   lo.w = o->load_opts.w;
   lo.h = o->load_opts.h;
   lo.region.x = o->load_opts.region.x;
   lo.region.y = o->load_opts.region.y;
   lo.region.w = o->load_opts.region.w;
   lo.region.h = o->load_opts.region.h;
   lo.orientation = o->load_opts.orientation;
   o->engine_data = obj->layer->evas->engine.func->image_load
      (obj->layer->evas->engine.data.output,
          o->cur.file,
          o->cur.key,
          &o->load_error,
          &lo);
   if (o->engine_data)
     {
        int w, h;
        int stride = 0;
        Eina_Bool resize_call = EINA_FALSE;

        obj->layer->evas->engine.func->image_size_get
           (obj->layer->evas->engine.data.output,
            o->engine_data, &w, &h);
        if (obj->layer->evas->engine.func->image_stride_get)
          obj->layer->evas->engine.func->image_stride_get
             (obj->layer->evas->engine.data.output,
              o->engine_data, &stride);
        else
          stride = w * 4;
        o->cur.has_alpha = obj->layer->evas->engine.func->image_alpha_get
           (obj->layer->evas->engine.data.output,
            o->engine_data);
        o->cur.cspace = obj->layer->evas->engine.func->image_colorspace_get
           (obj->layer->evas->engine.data.output,
            o->engine_data);
        if ((o->cur.image.w != w) || (o->cur.image.h != h))
          resize_call = EINA_TRUE;
        o->cur.image.w = w;
        o->cur.image.h = h;
        o->cur.image.stride = stride;
        if (resize_call) evas_object_inform_call_image_resize(obj);
     }
   else
     {
        o->load_error = EVAS_LOAD_ERROR_GENERIC;
     }
}

static Evas_Coord
evas_object_image_figure_x_fill(Evas_Object *obj, Evas_Coord start, Evas_Coord size, Eina_Bool src_cut, Evas_Coord *size_ret)
{
   Evas_Coord w;

   w = ((size * obj->layer->evas->output.w) /
        (Evas_Coord)obj->layer->evas->viewport.w);
   if (size <= 0) size = 1;

   /* Proxy has the surface that's filled with the filled area.
      So it doesn't need to figure out the filled area again */
   if (src_cut)
     {
        start = 0;
        size = obj->cur.geometry.w;
     }

   if (start > 0)
     {
        while (start - size > 0) start -= size;
     }
   else if (start < 0)
     {
        while (start < 0) start += size;
     }
   start = ((start * obj->layer->evas->output.w) /
            (Evas_Coord)obj->layer->evas->viewport.w);
   *size_ret = w;
   return start;
}

static Evas_Coord
evas_object_image_figure_y_fill(Evas_Object *obj, Evas_Coord start, Evas_Coord size, Eina_Bool src_cut, Evas_Coord *size_ret)
{
   Evas_Coord h;

   h = ((size * obj->layer->evas->output.h) /
        (Evas_Coord)obj->layer->evas->viewport.h);
   if (size <= 0) size = 1;

   /* Proxy has the surface that's filled with the filled area.
      So it doesn't need to figure out the filled area again */
   if (src_cut)
     {
        start = 0;
        size = obj->cur.geometry.h;
     }
   if (start > 0)
     {
        while (start - size > 0) start -= size;
     }
   else if (start < 0)
     {
        while (start < 0) start += size;
     }
   start = ((start * obj->layer->evas->output.h) /
            (Evas_Coord)obj->layer->evas->viewport.h);
   *size_ret = h;
   return start;
}

static void
evas_object_image_init(Evas_Object *obj)
{
   /* alloc image ob, setup methods and default values */
   obj->object_data = evas_object_image_new();
   /* set up default settings for this kind of object */
   obj->cur.color.r = 255;
   obj->cur.color.g = 255;
   obj->cur.color.b = 255;
   obj->cur.color.a = 255;
   obj->cur.geometry.x = 0;
   obj->cur.geometry.y = 0;
   obj->cur.geometry.w = 0;
   obj->cur.geometry.h = 0;
   obj->cur.layer = 0;
   obj->cur.anti_alias = 0;
   obj->cur.render_op = EVAS_RENDER_BLEND;
   /* set up object-specific settings */
   obj->prev = obj->cur;
   /* set up methods (compulsory) */
   obj->func = &object_func;
   obj->type = o_type;
}

static void *
evas_object_image_new(void)
{
   Evas_Object_Image *o;

   /* alloc obj private data */
   EVAS_MEMPOOL_INIT(_mp_obj, "evas_object_image", Evas_Object_Image, 16, NULL);
   o = EVAS_MEMPOOL_ALLOC(_mp_obj, Evas_Object_Image);
   if (!o) return NULL;
   EVAS_MEMPOOL_PREP(_mp_obj, o, Evas_Object_Image);
   o->magic = MAGIC_OBJ_IMAGE;
   o->cur.fill.w = 0;
   o->cur.fill.h = 0;
   o->cur.smooth_scale = 1;
   o->cur.border.fill = 1;
   o->cur.border.scale = 1.0;
   o->cur.cspace = EVAS_COLORSPACE_ARGB8888;
   o->cur.spread = EVAS_TEXTURE_REPEAT;
   o->cur.opaque_valid = 0;
   o->cur.source = NULL;
   o->cur.src_cut_w = EINA_FALSE;
   o->cur.src_cut_h = EINA_FALSE;
   o->prev = o->cur;
   o->tmpf_fd = -1;
   o->proxy_src_clip = EINA_TRUE;
   return o;
}

static void
evas_object_image_free(Evas_Object *obj)
{
   Evas_Object_Image *o;
   Eina_Rectangle *r;

   /* frees private object data. very simple here */
   o = (Evas_Object_Image *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return;
   MAGIC_CHECK_END();
   /* free obj */
   _cleanup_tmpf(obj);
   if (o->cur.file) eina_stringshare_del(o->cur.file);
   if (o->cur.key) eina_stringshare_del(o->cur.key);
   if (o->cur.source) _proxy_unset(obj);
   if (o->engine_data)
     {
        if (o->preloading)
          {
             o->preloading = EINA_FALSE;
             obj->layer->evas->engine.func->image_data_preload_cancel(obj->layer->evas->engine.data.output,
                                                                      o->engine_data,
                                                                      obj);
          }
        obj->layer->evas->engine.func->image_free(obj->layer->evas->engine.data.output,
                                                  o->engine_data);
     }
   if (o->video_surface)
     {
        o->video_surface = EINA_FALSE;
        obj->layer->evas->video_objects = eina_list_remove(obj->layer->evas->video_objects, obj);
     }
   o->engine_data = NULL;
   o->magic = 0;
   EINA_LIST_FREE(o->pixel_updates, r)
     eina_rectangle_free(r);
   EVAS_MEMPOOL_FREE(_mp_obj, o);
}

static void
evas_object_image_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y)
{
   Evas_Object_Image *o;
   int imagew, imageh, uvw, uvh;
   void *pixels;

   /* render object to surface with context, and offset by x,y */
   o = (Evas_Object_Image *)(obj->object_data);

   if ((o->cur.fill.w < 1) || (o->cur.fill.h < 1))
     return; /* no error message, already printed in pre_render */

   /* Proxy sanity */
   if (o->proxyrendering)
     {
        _proxy_error(obj, context, output, surface, x, y);
        return;
     }

   /* We are displaying the overlay */
   if (o->video_visible)
     {
        /* Create a transparent rectangle */
        obj->layer->evas->engine.func->context_color_set(output,
                                                         context,
                                                         0, 0, 0, 0);
        obj->layer->evas->engine.func->context_multiplier_unset(output,
                                                                context);
        obj->layer->evas->engine.func->context_render_op_set(output, context,
                                                             EVAS_RENDER_COPY);
        obj->layer->evas->engine.func->rectangle_draw(output,
                                                      context,
                                                      surface,
                                                      obj->cur.geometry.x + x,
                                                      obj->cur.geometry.y + y,
                                                      obj->cur.geometry.w,
                                                      obj->cur.geometry.h);

        return ;
     }

   if (o->pixel_updates)
     {
        Eina_Rectangle *rect;
        Evas *e = obj->layer->evas;

        if ((o->cur.border.l == 0) &&
            (o->cur.border.r == 0) &&
            (o->cur.border.t == 0) &&
            (o->cur.border.b == 0) &&
            (o->cur.image.w > 0) &&
            (o->cur.image.h > 0) &&
            (!((obj->cur.map) && (obj->cur.usemap))))
          {
             EINA_LIST_FREE(o->pixel_updates, rect)
               {
                  e->engine.func->image_dirty_region(e->engine.data.output, o->engine_data,
                                                     rect->x, rect->y, rect->w, rect->h);
                  eina_rectangle_free(rect);
               }
          }
        else
          {
             EINA_LIST_FREE(o->pixel_updates, rect) eina_rectangle_free(rect);
             e->engine.func->image_dirty_region(e->engine.data.output, o->engine_data,
                                                0, 0, o->cur.image.w, o->cur.image.h);
          }
     }

   obj->layer->evas->engine.func->context_color_set(output,
                                                    context,
                                                    255, 255, 255, 255);

   if ((obj->cur.cache.clip.r == 255) &&
       (obj->cur.cache.clip.g == 255) &&
       (obj->cur.cache.clip.b == 255) &&
       (obj->cur.cache.clip.a == 255))
     {
        obj->layer->evas->engine.func->context_multiplier_unset(output,
                                                                context);
     }
   else
     obj->layer->evas->engine.func->context_multiplier_set(output,
                                                           context,
                                                           obj->cur.cache.clip.r,
                                                           obj->cur.cache.clip.g,
                                                           obj->cur.cache.clip.b,
                                                           obj->cur.cache.clip.a);

   obj->layer->evas->engine.func->context_render_op_set(output, context,
                                                        obj->cur.render_op);

   if (!o->cur.source)
     {
        pixels = o->engine_data;
        imagew = o->cur.image.w;
        imageh = o->cur.image.h;
        uvw = imagew;
        uvh = imageh;
     }
   else if (o->cur.source->proxy.surface && !o->cur.source->proxy.redraw)
     {
        pixels = o->cur.source->proxy.surface;
        imagew = o->cur.source->proxy.w;
        imageh = o->cur.source->proxy.h;
        uvw = imagew;
        uvh = imageh;
     }
   else if (o->cur.source->type == o_type &&
            ((Evas_Object_Image *)o->cur.source->object_data)->engine_data)
     {
        Evas_Object_Image *oi;
        oi = o->cur.source->object_data;
        pixels = oi->engine_data;
        imagew = oi->cur.image.w;
        imageh = oi->cur.image.h;
        uvw = o->cur.source->cur.geometry.w;
        uvh = o->cur.source->cur.geometry.h;
     }
   else
     {
        o->proxyrendering = EINA_TRUE;
        _proxy_subrender(obj->layer->evas, o->cur.source, obj);
        pixels = o->cur.source->proxy.surface;
        imagew = o->cur.source->proxy.w;
        imageh = o->cur.source->proxy.h;
        uvw = imagew;
        uvh = imageh;
        o->proxyrendering = EINA_FALSE;
     }

   ///Jiyoun: This code will be modified after opensource fix lockup issue
   evas_object_inform_call_render_pre(obj);

   // Clear out the pixel get stuff..
   if (obj->layer->evas->engine.func->gl_get_pixels_set)
     {
        obj->layer->evas->engine.func->gl_get_pixels_set(output, NULL, NULL, NULL);
     }

   if (pixels)
     {
        Evas_Coord idw, idh, idx, idy;
        int ix, iy, iw, ih;
        //int direct_render = 0;
        int direct_override = 0;
        int direct_force_off = 0;

        if (o->dirty_pixels)
          {
             if (o->func.get_pixels)
               {

                  if (obj->layer->evas->engine.func->image_native_get)
                    {
                       Evas_Native_Surface *ns;
                       ns = obj->layer->evas->engine.func->image_native_get(obj->layer->evas->engine.data.output, o->engine_data);
                       if ( (ns) &&
                            (ns->type == EVAS_NATIVE_SURFACE_OPENGL) &&
                            (ns->data.opengl.texture_id) &&
                            (!ns->data.opengl.framebuffer_id) )
                         {

                            // Check if we can do direct rendering...
                            if (obj->layer->evas->engine.func->gl_direct_override_get)
                               obj->layer->evas->engine.func->gl_direct_override_get(output, &direct_override, &direct_force_off);
                            if ( (((obj->cur.geometry.w == o->cur.image.w) &&
                                   (obj->cur.geometry.h == o->cur.image.h) &&
                                   (obj->cur.color.r == 255) &&
                                   (obj->cur.color.g == 255) &&
                                   (obj->cur.color.b == 255) &&
                                   (obj->cur.color.a == 255) &&
                                   (!obj->cur.map) &&
                                   (!o->cur.has_alpha)) ||
                                  (direct_override)) &&
                                 (!direct_force_off) )
                              {

                                 if (obj->layer->evas->engine.func->gl_get_pixels_set)
                                   {
                                      obj->layer->evas->engine.func->gl_get_pixels_set(output, o->func.get_pixels, o->func.get_pixels_data, obj);
                                   }

                                 o->direct_render = EINA_TRUE;
                              }
                            else
                               o->direct_render = EINA_FALSE;

                         }

                       if ( (ns) &&
                            ((ns->type == EVAS_NATIVE_SURFACE_X11) ||
                            (ns->type == EVAS_NATIVE_SURFACE_TIZEN)))
                         {
                            if (obj->layer->evas->engine.func->context_flush)
                               obj->layer->evas->engine.func->context_flush(output);
                         }
                    }

                  if (!o->direct_render)
                     o->func.get_pixels(o->func.get_pixels_data, obj);

                  if (o->engine_data != pixels)
                     pixels = o->engine_data;
                  o->engine_data = obj->layer->evas->engine.func->image_dirty_region
                     (obj->layer->evas->engine.data.output, o->engine_data,
                      0, 0, o->cur.image.w, o->cur.image.h);
               }
             o->dirty_pixels = EINA_FALSE;
          }
        else
          {
             // Check if the it's not dirty but it has direct rendering
             if (o->direct_render)
               {
                  obj->layer->evas->engine.func->gl_get_pixels_set(output, o->func.get_pixels, o->func.get_pixels_data, obj);
               }

          }

        if ((obj->cur.map) && (obj->cur.map->count > 3) && (obj->cur.usemap))
          {
             evas_object_map_update(obj, x, y, imagew, imageh, uvw, uvh);

             obj->layer->evas->engine.func->image_map_draw
                (output, context, surface, pixels, obj->spans,
                 o->cur.smooth_scale | obj->cur.map->smooth, 0);
          }
        else
          {
             obj->layer->evas->engine.func->image_scale_hint_set(output,
                                                                 pixels,
                                                                 o->scale_hint);
             /* This is technically a bug here: If the value is recreated
              * (which is returned)it may be a new object, however exactly 0
              * of all the evas engines do this. */
             obj->layer->evas->engine.func->image_border_set(output, pixels,
                                                             o->cur.border.l,
                                                             o->cur.border.r,
                                                             o->cur.border.t,
                                                             o->cur.border.b);
             idx = evas_object_image_figure_x_fill(obj,
                                                   o->cur.fill.x,
                                                   o->cur.fill.w,
                                                   o->cur.src_cut_w, &idw);
             idy = evas_object_image_figure_y_fill(obj,
                                                   o->cur.fill.y,
                                                   o->cur.fill.h,
                                                   o->cur.src_cut_h, &idh);
             if (idw < 1) idw = 1;
             if (idh < 1) idh = 1;
             if (idx > 0) idx -= idw;
             if (idy > 0) idy -= idh;
             while ((int)idx < obj->cur.geometry.w)
               {
                  Evas_Coord ydy;
                  int dobreak_w = 0;

                  ydy = idy;
                  ix = idx;
                  if (((o->cur.fill.w == obj->cur.geometry.w) &&
                      (o->cur.fill.x == 0)) || o->cur.src_cut_w)
                    {
                       dobreak_w = 1;
                       iw = obj->cur.geometry.w;
                    }
                  else
                    iw = ((int)(idx + idw)) - ix;
                  while ((int)idy < obj->cur.geometry.h)
                    {
                       int dobreak_h = 0;

                       iy = idy;
                       if (((o->cur.fill.h == obj->cur.geometry.h) &&
                           (o->cur.fill.y == 0)) || o->cur.src_cut_h)
                         {
                            ih = obj->cur.geometry.h;
                            dobreak_h = 1;
                         }
                       else
                         ih = ((int)(idy + idh)) - iy;
                       if ((o->cur.border.l == 0) &&
                           (o->cur.border.r == 0) &&
                           (o->cur.border.t == 0) &&
                           (o->cur.border.b == 0) &&
                           (o->cur.border.fill != 0))
                         obj->layer->evas->engine.func->image_draw(output,
                                                                   context,
                                                                   surface,
                                                                   pixels,
                                                                   0, 0,
                                                                   imagew,
                                                                   imageh,
                                                                   obj->cur.geometry.x + ix + x,
                                                                   obj->cur.geometry.y + iy + y,
                                                                   iw, ih,
                                                                   o->cur.smooth_scale);
                       else
                         {
                            int inx, iny, inw, inh, outx, outy, outw, outh;
                            int bl, br, bt, bb, bsl, bsr, bst, bsb;
                            int imw, imh, ox, oy;

                            ox = obj->cur.geometry.x + ix + x;
                            oy = obj->cur.geometry.y + iy + y;
                            imw = imagew;
                            imh = imageh;
                            bl = o->cur.border.l;
                            br = o->cur.border.r;
                            bt = o->cur.border.t;
                            bb = o->cur.border.b;
                            if ((bl + br) > iw)
                              {
                                 bl = iw / 2;
                                 br = iw - bl;
                              }
                            if ((bl + br) > imw)
                              {
                                 bl = imw / 2;
                                 br = imw - bl;
                              }
                            if ((bt + bb) > ih)
                              {
                                 bt = ih / 2;
                                 bb = ih - bt;
                              }
                            if ((bt + bb) > imh)
                              {
                                 bt = imh / 2;
                                 bb = imh - bt;
                              }
                            if (o->cur.border.scale != 1.0)
                              {
                                 bsl = ((double)bl * o->cur.border.scale);
                                 bsr = ((double)br * o->cur.border.scale);
                                 bst = ((double)bt * o->cur.border.scale);
                                 bsb = ((double)bb * o->cur.border.scale);
                              }
                            else
                              {
                                  bsl = bl; bsr = br; bst = bt; bsb = bb;
                              }
                            // #--
                            // |
                            inx = 0; iny = 0;
                            inw = bl; inh = bt;
                            outx = ox; outy = oy;
                            outw = bsl; outh = bst;
                            obj->layer->evas->engine.func->image_draw(output, context, surface, pixels, inx, iny, inw, inh, outx, outy, outw, outh, o->cur.smooth_scale);
                            // .##
                            // |
                            inx = bl; iny = 0;
                            inw = imw - bl - br; inh = bt;
                            outx = ox + bsl; outy = oy;
                            outw = iw - bsl - bsr; outh = bst;
                            obj->layer->evas->engine.func->image_draw(output, context, surface, pixels, inx, iny, inw, inh, outx, outy, outw, outh, o->cur.smooth_scale);
                            // --#
                            //   |
                            inx = imw - br; iny = 0;
                            inw = br; inh = bt;
                            outx = ox + iw - bsr; outy = oy;
                            outw = bsr; outh = bst;
                            obj->layer->evas->engine.func->image_draw(output, context, surface, pixels, inx, iny, inw, inh, outx, outy, outw, outh, o->cur.smooth_scale);
                            // .--
                            // #  
                            inx = 0; iny = bt;
                            inw = bl; inh = imh - bt - bb;
                            outx = ox; outy = oy + bst;
                            outw = bsl; outh = ih - bst - bsb;
                            obj->layer->evas->engine.func->image_draw(output, context, surface, pixels, inx, iny, inw, inh, outx, outy, outw, outh, o->cur.smooth_scale);
                            // .--.
                            // |##|
                            if (o->cur.border.fill > EVAS_BORDER_FILL_NONE)
                              {
                                 inx = bl; iny = bt;
                                 inw = imw - bl - br; inh = imh - bt - bb;
                                 outx = ox + bsl; outy = oy + bst;
                                 outw = iw - bsl - bsr; outh = ih - bst - bsb;
                                 if ((o->cur.border.fill == EVAS_BORDER_FILL_SOLID) &&
                                     (obj->cur.cache.clip.a == 255) &&
                                     (obj->cur.render_op == EVAS_RENDER_BLEND))
                                   {
                                      obj->layer->evas->engine.func->context_render_op_set(output, context,
                                                                                           EVAS_RENDER_COPY);
                                      obj->layer->evas->engine.func->image_draw(output, context, surface, pixels, inx, iny, inw, inh, outx, outy, outw, outh, o->cur.smooth_scale);
                                      obj->layer->evas->engine.func->context_render_op_set(output, context,
                                                                                           obj->cur.render_op);
                                   }
                                 else
                                   obj->layer->evas->engine.func->image_draw(output, context, surface, pixels, inx, iny, inw, inh, outx, outy, outw, outh, o->cur.smooth_scale);
                              }
                            // --.
                            //   #
                            inx = imw - br; iny = bt;
                            inw = br; inh = imh - bt - bb;
                            outx = ox + iw - bsr; outy = oy + bst;
                            outw = bsr; outh = ih - bst - bsb;
                            obj->layer->evas->engine.func->image_draw(output, context, surface, pixels, inx, iny, inw, inh, outx, outy, outw, outh, o->cur.smooth_scale);
                            // |
                            // #--
                            inx = 0; iny = imh - bb;
                            inw = bl; inh = bb;
                            outx = ox; outy = oy + ih - bsb;
                            outw = bsl; outh = bsb;
                            obj->layer->evas->engine.func->image_draw(output, context, surface, pixels, inx, iny, inw, inh, outx, outy, outw, outh, o->cur.smooth_scale);
                            // |
                            // .## 
                            inx = bl; iny = imh - bb;
                            inw = imw - bl - br; inh = bb;
                            outx = ox + bsl; outy = oy + ih - bsb;
                            outw = iw - bsl - bsr; outh = bsb;
                            obj->layer->evas->engine.func->image_draw(output, context, surface, pixels, inx, iny, inw, inh, outx, outy, outw, outh, o->cur.smooth_scale);
                            //   |
                            // --#
                            inx = imw - br; iny = imh - bb;
                            inw = br; inh = bb;
                            outx = ox + iw - bsr; outy = oy + ih - bsb;
                            outw = bsr; outh = bsb;
                            obj->layer->evas->engine.func->image_draw(output, context, surface, pixels, inx, iny, inw, inh, outx, outy, outw, outh, o->cur.smooth_scale);
                         }
                       idy += idh;
                       if (dobreak_h) break;
                    }
                  idx += idw;
                  idy = ydy;
                  if (dobreak_w) break;
               }
          }
     }
   ///Jiyoun: This code will be modified after opensource fix lockup issue
   evas_object_inform_call_render_post(obj);
}

static void
evas_object_image_render_pre(Evas_Object *obj)
{
   Evas_Object_Image *o;
   int is_v = 0, was_v = 0;
   Evas *e;

   /* dont pre-render the obj twice! */
   if (obj->pre_render_done) return;
   obj->pre_render_done = 1;
   /* pre-render phase. this does anything an object needs to do just before */
   /* rendering. this could mean loading the image data, retrieving it from */
   /* elsewhere, decoding video etc. */
   /* then when this is done the object needs to figure if it changed and */
   /* if so what and where and add the appropriate redraw rectangles */
   o = (Evas_Object_Image *)(obj->object_data);
   e = obj->layer->evas;

   if ((o->cur.fill.w < 1) || (o->cur.fill.h < 1))
     {
        ERR("%p has invalid fill size: %dx%d. Ignored",
            obj, o->cur.fill.w, o->cur.fill.h);
        return;
     }

   /* if someone is clipping this obj - go calculate the clipper */
   if (obj->cur.clipper)
     {
        if (obj->cur.cache.clip.dirty)
          evas_object_clip_recalc(obj->cur.clipper);
        obj->cur.clipper->func->render_pre(obj->cur.clipper);
     }
   /* Proxy: Do it early */
   if (o->cur.source &&
       (o->cur.source->proxy.redraw || o->cur.source->changed))
     {
        /* XXX: Do I need to sort out the map here? */
        evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
        goto done;
     }

   /* now figure what changed and add draw rects */
   /* if it just became visible or invisible */
   is_v = evas_object_is_visible(obj);
   was_v = evas_object_was_visible(obj);
   if (is_v != was_v)
     {
        evas_object_render_pre_visible_change(&e->clip_changes, obj, is_v, was_v);
        if (!o->pixel_updates) goto done;
     }
   if (obj->changed_map)
     {
        evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
        goto done;
     }
   /* it's not visible - we accounted for it appearing or not so just abort */
   if (!is_v) goto done;
   /* clipper changed this is in addition to anything else for obj */
   if (was_v)
     evas_object_render_pre_clipper_change(&e->clip_changes, obj);
   /* if we restacked (layer or just within a layer) and don't clip anyone */
   if (obj->restack)
     {
        evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
        if (!o->pixel_updates) goto done;
     }
   /* if it changed color */
   if ((obj->cur.color.r != obj->prev.color.r) ||
       (obj->cur.color.g != obj->prev.color.g) ||
       (obj->cur.color.b != obj->prev.color.b) ||
       (obj->cur.color.a != obj->prev.color.a))
     {
        evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
        if (!o->pixel_updates) goto done;
     }
   /* if it changed render op */
   if (obj->cur.render_op != obj->prev.render_op)
     {
        evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
        if (!o->pixel_updates) goto done;
     }
   /* if it changed anti_alias */
   if (obj->cur.anti_alias != obj->prev.anti_alias)
     {
        evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
        if (!o->pixel_updates) goto done;
     }
   if (o->changed)
     {
        if (((o->cur.file) && (!o->prev.file)) ||
            ((!o->cur.file) && (o->prev.file)) ||
            ((o->cur.key) && (!o->prev.key)) ||
            ((!o->cur.key) && (o->prev.key))
           )
          {
             evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
             if (!o->pixel_updates) goto done;
          }
        if ((o->cur.image.w != o->prev.image.w) ||
            (o->cur.image.h != o->prev.image.h) ||
            (o->cur.has_alpha != o->prev.has_alpha) ||
            (o->cur.cspace != o->prev.cspace) ||
            (o->cur.smooth_scale != o->prev.smooth_scale))
          {
             evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
             if (!o->pixel_updates) goto done;
          }
        if ((o->cur.border.l != o->prev.border.l) ||
            (o->cur.border.r != o->prev.border.r) ||
            (o->cur.border.t != o->prev.border.t) ||
            (o->cur.border.b != o->prev.border.b) ||
            (o->cur.border.fill != o->prev.border.fill) ||
            (o->cur.border.scale != o->prev.border.scale))
          {
             evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
             if (!o->pixel_updates) goto done;
          }
        if (o->dirty_pixels)
          {
             evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
             if (!o->pixel_updates) goto done;
          }
        if (o->cur.frame != o->prev.frame)
          {
             evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
             if (!o->pixel_updates) goto done;
          }

     }
   /* if it changed geometry - and obviously not visibility or color */
   /* calculate differences since we have a constant color fill */
   /* we really only need to update the differences */
#if 0 // XXX: maybe buggy?
   if (((obj->cur.geometry.x != obj->prev.geometry.x) ||
	(obj->cur.geometry.y != obj->prev.geometry.y) ||
	(obj->cur.geometry.w != obj->prev.geometry.w) ||
	(obj->cur.geometry.h != obj->prev.geometry.h)) &&
       (o->cur.fill.w == o->prev.fill.w) &&
       (o->cur.fill.h == o->prev.fill.h) &&
       ((o->cur.fill.x + obj->cur.geometry.x) == (o->prev.fill.x + obj->prev.geometry.x)) &&
       ((o->cur.fill.y + obj->cur.geometry.y) == (o->prev.fill.y + obj->prev.geometry.y)) &&
       (!o->pixel_updates)
       )
     {
	evas_rects_return_difference_rects(&e->clip_changes,
					   obj->cur.geometry.x,
					   obj->cur.geometry.y,
					   obj->cur.geometry.w,
					   obj->cur.geometry.h,
					   obj->prev.geometry.x,
					   obj->prev.geometry.y,
					   obj->prev.geometry.w,
					   obj->prev.geometry.h);
	if (!o->pixel_updates) goto done;
     }
#endif
   if (((obj->cur.geometry.x != obj->prev.geometry.x) ||
        (obj->cur.geometry.y != obj->prev.geometry.y) ||
        (obj->cur.geometry.w != obj->prev.geometry.w) ||
        (obj->cur.geometry.h != obj->prev.geometry.h))
      )
     {
        evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
        if (!o->pixel_updates) goto done;
     }
   if (o->changed)
     {
        if ((o->cur.fill.x != o->prev.fill.x) ||
            (o->cur.fill.y != o->prev.fill.y) ||
            (o->cur.fill.w != o->prev.fill.w) ||
            (o->cur.fill.h != o->prev.fill.h))
          {
             evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
             if (!o->pixel_updates) goto done;
          }
        if (o->pixel_updates)
          {
             if ((o->cur.border.l == 0) &&
                 (o->cur.border.r == 0) &&
                 (o->cur.border.t == 0) &&
                 (o->cur.border.b == 0) &&
                 (o->cur.image.w > 0) &&
                 (o->cur.image.h > 0) &&
                 (!((obj->cur.map) && (obj->cur.usemap))))
               {
                  Eina_Rectangle *rr;
                  Eina_List *l;

                  EINA_LIST_FOREACH(o->pixel_updates, l, rr)
                    {
                       Evas_Coord idw, idh, idx, idy;
                       int x, y, w, h;

                       idx = evas_object_image_figure_x_fill(obj,
                                                             o->cur.fill.x,
                                                             o->cur.fill.w,
                                                             o->cur.src_cut_w,
                                                             &idw);
                       idy = evas_object_image_figure_y_fill(obj,
                                                             o->cur.fill.y,
                                                             o->cur.fill.h,
                                                             o->cur.src_cut_h,
                                                             &idh);
                       if (idw < 1) idw = 1;
                       if (idh < 1) idh = 1;
                       if (idx > 0) idx -= idw;
                       if (idy > 0) idy -= idh;
                       while (idx < obj->cur.geometry.w)
                         {
                            Evas_Coord ydy;

                            ydy = idy;
                            x = idx;
                            w = ((int)(idx + idw)) - x;
                            while (idy < obj->cur.geometry.h)
                              {
                                 Eina_Rectangle r;

                                 y = idy;
                                 h = ((int)(idy + idh)) - y;

                                 r.x = (rr->x * w) / o->cur.image.w;
                                 r.y = (rr->y * h) / o->cur.image.h;
                                 r.w = ((rr->w * w) + (o->cur.image.w * 2) - 1) / o->cur.image.w;
                                 r.h = ((rr->h * h) + (o->cur.image.h * 2) - 1) / o->cur.image.h;
                                 r.x += obj->cur.geometry.x + x;
                                 r.y += obj->cur.geometry.y + y;
                                 RECTS_CLIP_TO_RECT(r.x, r.y, r.w, r.h,
                                                    obj->cur.cache.clip.x, obj->cur.cache.clip.y,
                                                    obj->cur.cache.clip.w, obj->cur.cache.clip.h);
                                 evas_add_rect(&e->clip_changes, r.x, r.y, r.w, r.h);
                                 idy += h;
                              }
                            idx += idw;
                            idy = ydy;
                         }
                    }
                  goto done;
               }
             else
               {
                  evas_object_render_pre_prev_cur_add(&e->clip_changes, obj);
                  goto done;
               }
          }
     }
   /* it obviously didn't change - add a NO obscure - this "unupdates"  this */
   /* area so if there were updates for it they get wiped. don't do it if we */
   /* aren't fully opaque and we are visible */
   if (evas_object_is_visible(obj) &&
       evas_object_is_opaque(obj))
     {
        Evas_Coord x, y, w, h;

        x = obj->cur.cache.clip.x;
        y = obj->cur.cache.clip.y;
        w = obj->cur.cache.clip.w;
        h = obj->cur.cache.clip.h;
        if (obj->cur.clipper)
          {
             RECTS_CLIP_TO_RECT(x, y, w, h,
                                obj->cur.clipper->cur.cache.clip.x,
                                obj->cur.clipper->cur.cache.clip.y,
                                obj->cur.clipper->cur.cache.clip.w,
                                obj->cur.clipper->cur.cache.clip.h);
          }
        e->engine.func->output_redraws_rect_del(e->engine.data.output,
                                                x, y, w, h);
     }
   done:
   evas_object_render_pre_effect_updates(&e->clip_changes, obj, is_v, was_v);
}

static void
evas_object_image_render_post(Evas_Object *obj)
{
   Evas_Object_Image *o;
   Eina_Rectangle *r;

   /* this moves the current data to the previous state parts of the object */
   /* in whatever way is safest for the object. also if we don't need object */
   /* data anymore we can free it if the object deems this is a good idea */
   o = (Evas_Object_Image *)(obj->object_data);
   /* remove those pesky changes */
   evas_object_clip_changes_clean(obj);
   EINA_LIST_FREE(o->pixel_updates, r)
     eina_rectangle_free(r);
   /* move cur to prev safely for object data */
   evas_object_cur_prev(obj);
   o->prev = o->cur;
   o->changed = EINA_FALSE;
   /* FIXME: copy strings across */
}

static unsigned int evas_object_image_id_get(Evas_Object *obj)
{
   Evas_Object_Image *o;

   o = (Evas_Object_Image *)(obj->object_data);
   if (!o) return 0;
   return MAGIC_OBJ_IMAGE;
}

static unsigned int evas_object_image_visual_id_get(Evas_Object *obj)
{
   Evas_Object_Image *o;

   o = (Evas_Object_Image *)(obj->object_data);
   if (!o) return 0;
   return MAGIC_OBJ_IMAGE;
}

static void *evas_object_image_engine_data_get(Evas_Object *obj)
{
   Evas_Object_Image *o;

   o = (Evas_Object_Image *)(obj->object_data);
   if (!o) return NULL;
   return o->engine_data;
}

static int
evas_object_image_is_opaque(Evas_Object *obj)
{
   Evas_Object_Image *o;

   /* this returns 1 if the internal object data implies that the object is */
   /* currently fully opaque over the entire rectangle it occupies */
   o = (Evas_Object_Image *)(obj->object_data);
/*  disable caching due tyo maps screwing with this
   o->cur.opaque_valid = 0;
   if (o->cur.opaque_valid)
     {
        if (!o->cur.opaque) return 0;
     }
   else
*/
     {
        o->cur.opaque = 0;
/* disable caching */
/*        o->cur.opaque_valid = 1; */
        if ((o->cur.fill.w < 1) || (o->cur.fill.h < 1))
           return o->cur.opaque;
        if (((o->cur.border.l != 0) ||
             (o->cur.border.r != 0) ||
             (o->cur.border.t != 0) ||
             (o->cur.border.b != 0)) &&
            (!o->cur.border.fill)) return o->cur.opaque;
        if (!o->engine_data) return o->cur.opaque;
        o->cur.opaque = 1;
     }
   // FIXME: use proxy
   if (o->cur.source)
     {
        o->cur.opaque = evas_object_is_opaque(o->cur.source);
        return o->cur.opaque; /* FIXME: Should go poke at the object */
     }
   if (o->cur.has_alpha)
     {
        o->cur.opaque = 0;
        return o->cur.opaque;
     }
   if ((obj->cur.map) && (obj->cur.usemap))
     {
        Evas_Map *m = obj->cur.map;

        if ((m->points[0].a == 255) &&
            (m->points[1].a == 255) &&
            (m->points[2].a == 255) &&
            (m->points[3].a == 255))
          {
             if (
                 ((m->points[0].x == m->points[3].x) &&
                     (m->points[1].x == m->points[2].x) &&
                     (m->points[0].y == m->points[1].y) &&
                     (m->points[2].y == m->points[3].y))
                 ||
                 ((m->points[0].x == m->points[1].x) &&
                     (m->points[2].x == m->points[3].x) &&
                     (m->points[0].y == m->points[3].y) &&
                     (m->points[1].y == m->points[2].y))
                )
               {
                  if ((m->points[0].x == obj->cur.geometry.x) &&
                      (m->points[0].y == obj->cur.geometry.y) &&
                      (m->points[2].x == (obj->cur.geometry.x + obj->cur.geometry.w)) &&
                      (m->points[2].y == (obj->cur.geometry.y + obj->cur.geometry.h)))
                    return o->cur.opaque;
               }
          }
        o->cur.opaque = 0;
        return o->cur.opaque;
     }
   if (obj->cur.render_op == EVAS_RENDER_COPY) return o->cur.opaque;
   return o->cur.opaque;
}

static int
evas_object_image_was_opaque(Evas_Object *obj)
{
   Evas_Object_Image *o;

   /* this returns 1 if the internal object data implies that the object was */
   /* previously fully opaque over the entire rectangle it occupies */
   o = (Evas_Object_Image *)(obj->object_data);
   if (o->prev.opaque_valid)
     {
        if (!o->prev.opaque) return 0;
     }
   else
     {
        o->prev.opaque = 0;
        o->prev.opaque_valid = 1;
        if ((o->prev.fill.w < 1) || (o->prev.fill.h < 1))
           return 0;
        if (((o->prev.border.l != 0) ||
             (o->prev.border.r != 0) ||
             (o->prev.border.t != 0) ||
             (o->prev.border.b != 0)) &&
            (!o->prev.border.fill)) return 0;
        if (!o->engine_data) return 0;
        o->prev.opaque = 1;
     }
   // FIXME: use proxy
   if (o->prev.source) return 0; /* FIXME: Should go poke at the object */
   if (obj->prev.usemap) return 0;
   if (obj->prev.render_op == EVAS_RENDER_COPY) return 1;
   if (o->prev.has_alpha) return 0;
   if (obj->prev.render_op != EVAS_RENDER_BLEND) return 0;
   return 1;
}

static inline Eina_Bool
_pixel_alpha_get(RGBA_Image *im, int x, int y, DATA8 *alpha,
                 int src_region_x, int src_region_y, int src_region_w, int src_region_h,
                 int dst_region_x, int dst_region_y, int dst_region_w, int dst_region_h)
{
   int px, py, dx, dy, sx, sy, src_w, src_h;
   double scale_w, scale_h;

   if ((dst_region_x > x) || (x >= (dst_region_x + dst_region_w)) ||
       (dst_region_y > y) || (y >= (dst_region_y + dst_region_h)))
     {
        *alpha = 0;
        return EINA_FALSE;
     }

   src_w = im->cache_entry.w;
   src_h = im->cache_entry.h;
   if ((src_w == 0) || (src_h == 0))
     {
        *alpha = 0;
        return EINA_TRUE;
     }

   EINA_SAFETY_ON_TRUE_GOTO(src_region_x < 0, error_oob);
   EINA_SAFETY_ON_TRUE_GOTO(src_region_y < 0, error_oob);
   EINA_SAFETY_ON_TRUE_GOTO(src_region_x + src_region_w > src_w, error_oob);
   EINA_SAFETY_ON_TRUE_GOTO(src_region_y + src_region_h > src_h, error_oob);

   scale_w = (double)dst_region_w / (double)src_region_w;
   scale_h = (double)dst_region_h / (double)src_region_h;

   /* point at destination */
   dx = x - dst_region_x;
   dy = y - dst_region_y;

   /* point at source */
   sx = dx / scale_w;
   sy = dy / scale_h;

   /* pixel point (translated) */
   px = src_region_x + sx;
   py = src_region_y + sy;
   EINA_SAFETY_ON_TRUE_GOTO(px >= src_w, error_oob);
   EINA_SAFETY_ON_TRUE_GOTO(py >= src_h, error_oob);

   switch (im->cache_entry.space)
     {
     case EVAS_COLORSPACE_ARGB8888:
       {
          DATA32 *pixel = im->image.data;
          pixel += ((py * src_w) + px);
          *alpha = ((*pixel) >> 24) & 0xff;
       }
       break;

     default:
        ERR("Colorspace %d not supported.", im->cache_entry.space);
        *alpha = 0;
     }

   return EINA_TRUE;

 error_oob:
   ERR("Invalid region src=(%d, %d, %d, %d), dst=(%d, %d, %d, %d), image=%dx%d",
       src_region_x, src_region_y, src_region_w, src_region_h,
       dst_region_x, dst_region_y, dst_region_w, dst_region_h,
       src_w, src_h);
   *alpha = 0;
   return EINA_TRUE;
}

static int
evas_object_image_is_inside(Evas_Object *obj, Evas_Coord px, Evas_Coord py)
{
   Evas_Object_Image *o;
   int imagew, imageh, uvw, uvh;
   void *pixels;
   int is_inside = 0;

   /* the following code is similar to evas_object_image_render(), but doesn't
    * draw, just get the pixels so we can check the transparency.
    */
   o = (Evas_Object_Image *)(obj->object_data);
   if (!o->cur.source)
     {
        pixels = o->engine_data;
        imagew = o->cur.image.w;
        imageh = o->cur.image.h;
        uvw = imagew;
        uvh = imageh;
     }
   else if (o->cur.source->proxy.surface && !o->cur.source->proxy.redraw)
     {
        pixels = o->cur.source->proxy.surface;
        imagew = o->cur.source->proxy.w;
        imageh = o->cur.source->proxy.h;
        uvw = imagew;
        uvh = imageh;
     }
   else if (o->cur.source->type == o_type &&
            ((Evas_Object_Image *)o->cur.source->object_data)->engine_data)
     {
        Evas_Object_Image *oi;
        oi = o->cur.source->object_data;
        pixels = oi->engine_data;
        imagew = oi->cur.image.w;
        imageh = oi->cur.image.h;
        uvw = o->cur.source->cur.geometry.w;
        uvh = o->cur.source->cur.geometry.h;
     }
   else
     {
        o->proxyrendering = EINA_TRUE;
        _proxy_subrender(obj->layer->evas, o->cur.source, obj);
        pixels = o->cur.source->proxy.surface;
        imagew = o->cur.source->proxy.w;
        imageh = o->cur.source->proxy.h;
        uvw = imagew;
        uvh = imageh;
        o->proxyrendering = EINA_FALSE;
     }

   if (pixels)
     {
        Evas_Coord idw, idh, idx, idy;
        int ix, iy, iw, ih;

        /* TODO: not handling o->dirty_pixels && o->func.get_pixels,
         * should we handle it now or believe they were done in the last render?
         */
        if (o->dirty_pixels)
          {
             if (o->func.get_pixels)
               {
                  ERR("dirty_pixels && get_pixels not supported");
               }
          }

        /* TODO: not handling map, need to apply map to point */
        if ((obj->cur.map) && (obj->cur.map->count > 3) && (obj->cur.usemap))
          {
             evas_object_map_update(obj, 0, 0, imagew, imageh, uvw, uvh);

             ERR("map not supported");
          }
        else
          {
             RGBA_Image *im;
             DATA32 *data = NULL;
             int err = 0;

             im = obj->layer->evas->engine.func->image_data_get
               (obj->layer->evas->engine.data.output, pixels, 0, &data, &err);
             if ((!im) || (!data) || (err))
               {
                  ERR("Couldn't get image pixels RGBA_Image %p: im=%p, data=%p, err=%d", pixels, im, data, err);
                  goto end;
               }

             idx = evas_object_image_figure_x_fill(obj,
                                                   o->cur.fill.x, o->cur.fill.w,
                                                   o->cur.src_cut_w, &idw);
             idy = evas_object_image_figure_y_fill(obj,
                                                   o->cur.fill.y, o->cur.fill.h,
                                                   o->cur.src_cut_h, &idh);
             if (idw < 1) idw = 1;
             if (idh < 1) idh = 1;
             if (idx > 0) idx -= idw;
             if (idy > 0) idy -= idh;
             while ((int)idx < obj->cur.geometry.w)
               {
                  Evas_Coord ydy;
                  int dobreak_w = 0;
                  ydy = idy;
                  ix = idx;
                  if ((o->cur.fill.w == obj->cur.geometry.w) &&
                      (o->cur.fill.x == 0))
                    {
                       dobreak_w = 1;
                       iw = obj->cur.geometry.w;
                    }
                  else
                    iw = ((int)(idx + idw)) - ix;
                  while ((int)idy < obj->cur.geometry.h)
                    {
                       int dobreak_h = 0;

                       iy = idy;
                       if ((o->cur.fill.h == obj->cur.geometry.h) &&
                           (o->cur.fill.y == 0))
                         {
                            ih = obj->cur.geometry.h;
                            dobreak_h = 1;
                         }
                       else
                         ih = ((int)(idy + idh)) - iy;
                       if ((o->cur.border.l == 0) &&
                           (o->cur.border.r == 0) &&
                           (o->cur.border.t == 0) &&
                           (o->cur.border.b == 0) &&
                           (o->cur.border.fill != 0))
                         {
                            /* NOTE: render handles cserve2 here,
                             * we don't need to
                             */
                              {
                                 DATA8 alpha = 0;
                                 if (_pixel_alpha_get(pixels, px, py, &alpha, 0, 0, imagew, imageh, obj->cur.geometry.x + ix, obj->cur.geometry.y + iy, iw, ih))
                                   {
                                      is_inside = alpha > 0;
                                      dobreak_h = 1;
                                      dobreak_w = 1;
                                      break;
                                   }
                              }
                         }
                       else
                         {
                            int inx, iny, inw, inh, outx, outy, outw, outh;
                            int bl, br, bt, bb, bsl, bsr, bst, bsb;
                            int imw, imh, ox, oy;
                            DATA8 alpha = 0;

                            ox = obj->cur.geometry.x + ix;
                            oy = obj->cur.geometry.y + iy;
                            imw = imagew;
                            imh = imageh;
                            bl = o->cur.border.l;
                            br = o->cur.border.r;
                            bt = o->cur.border.t;
                            bb = o->cur.border.b;
                            if ((bl + br) > iw)
                              {
                                 bl = iw / 2;
                                 br = iw - bl;
                              }
                            if ((bl + br) > imw)
                              {
                                 bl = imw / 2;
                                 br = imw - bl;
                              }
                            if ((bt + bb) > ih)
                              {
                                 bt = ih / 2;
                                 bb = ih - bt;
                              }
                            if ((bt + bb) > imh)
                              {
                                 bt = imh / 2;
                                 bb = imh - bt;
                              }
                            if (o->cur.border.scale != 1.0)
                              {
                                 bsl = ((double)bl * o->cur.border.scale);
                                 bsr = ((double)br * o->cur.border.scale);
                                 bst = ((double)bt * o->cur.border.scale);
                                 bsb = ((double)bb * o->cur.border.scale);
                              }
                            else
                              {
                                  bsl = bl; bsr = br; bst = bt; bsb = bb;
                              }
                            // #--
                            // |
                            inx = 0; iny = 0;
                            inw = bl; inh = bt;
                            outx = ox; outy = oy;
                            outw = bsl; outh = bst;
                            if (_pixel_alpha_get(pixels, px, py, &alpha, inx, iny, inw, inh, outx, outy, outw, outh))
                              {
                                 is_inside = alpha > 0;
                                 dobreak_h = 1;
                                 dobreak_w = 1;
                                 break;
                              }

                            // .##
                            // |
                            inx = bl; iny = 0;
                            inw = imw - bl - br; inh = bt;
                            outx = ox + bsl; outy = oy;
                            outw = iw - bsl - bsr; outh = bst;
                            if (_pixel_alpha_get(pixels, px, py, &alpha, inx, iny, inw, inh, outx, outy, outw, outh))
                              {
                                 is_inside = alpha > 0;
                                 dobreak_h = 1;
                                 dobreak_w = 1;
                                 break;
                              }
                            // --#
                            //   |
                            inx = imw - br; iny = 0;
                            inw = br; inh = bt;
                            outx = ox + iw - bsr; outy = oy;
                            outw = bsr; outh = bst;
                            if (_pixel_alpha_get(pixels, px, py, &alpha, inx, iny, inw, inh, outx, outy, outw, outh))
                              {
                                 is_inside = alpha > 0;
                                 dobreak_h = 1;
                                 dobreak_w = 1;
                                 break;
                              }
                            // .--
                            // #  
                            inx = 0; iny = bt;
                            inw = bl; inh = imh - bt - bb;
                            outx = ox; outy = oy + bst;
                            outw = bsl; outh = ih - bst - bsb;
                            if (_pixel_alpha_get(pixels, px, py, &alpha, inx, iny, inw, inh, outx, outy, outw, outh))
                              {
                                 is_inside = alpha > 0;
                                 dobreak_h = 1;
                                 dobreak_w = 1;
                                 break;
                              }
                            // .--.
                            // |##|
                            if (o->cur.border.fill > EVAS_BORDER_FILL_NONE)
                              {
                                 inx = bl; iny = bt;
                                 inw = imw - bl - br; inh = imh - bt - bb;
                                 outx = ox + bsl; outy = oy + bst;
                                 outw = iw - bsl - bsr; outh = ih - bst - bsb;
                                 if (_pixel_alpha_get(pixels, px, py, &alpha, inx, iny, inw, inh, outx, outy, outw, outh))
                                   {
                                      is_inside = alpha > 0;
                                      dobreak_h = 1;
                                      dobreak_w = 1;
                                      break;
                                   }
                              }
                            // --.
                            //   #
                            inx = imw - br; iny = bt;
                            inw = br; inh = imh - bt - bb;
                            outx = ox + iw - bsr; outy = oy + bst;
                            outw = bsr; outh = ih - bst - bsb;
                            if (_pixel_alpha_get(pixels, px, py, &alpha, inx, iny, inw, inh, outx, outy, outw, outh))
                              {
                                 is_inside = alpha > 0;
                                 dobreak_h = 1;
                                 dobreak_w = 1;
                                 break;
                              }
                            // |
                            // #--
                            inx = 0; iny = imh - bb;
                            inw = bl; inh = bb;
                            outx = ox; outy = oy + ih - bsb;
                            outw = bsl; outh = bsb;
                            if (_pixel_alpha_get(pixels, px, py, &alpha, inx, iny, inw, inh, outx, outy, outw, outh))
                              {
                                 is_inside = alpha > 0;
                                 dobreak_h = 1;
                                 dobreak_w = 1;
                                 break;
                              }
                            // |
                            // .## 
                            inx = bl; iny = imh - bb;
                            inw = imw - bl - br; inh = bb;
                            outx = ox + bsl; outy = oy + ih - bsb;
                            outw = iw - bsl - bsr; outh = bsb;
                            if (_pixel_alpha_get(pixels, px, py, &alpha, inx, iny, inw, inh, outx, outy, outw, outh))
                              {
                                 is_inside = alpha > 0;
                                 dobreak_h = 1;
                                 dobreak_w = 1;
                                 break;
                              }
                            //   |
                            // --#
                            inx = imw - br; iny = imh - bb;
                            inw = br; inh = bb;
                            outx = ox + iw - bsr; outy = oy + ih - bsb;
                            outw = bsr; outh = bsb;
                            if (_pixel_alpha_get(pixels, px, py, &alpha, inx, iny, inw, inh, outx, outy, outw, outh))
                              {
                                 is_inside = alpha > 0;
                                 dobreak_h = 1;
                                 dobreak_w = 1;
                                 break;
                              }
                         }
                       idy += idh;
                       if (dobreak_h) break;
                    }
                  idx += idw;
                  idy = ydy;
                  if (dobreak_w) break;
               }
          }
     }

 end:
   return is_inside;
}

static int
evas_object_image_has_opaque_rect(Evas_Object *obj)
{
   Evas_Object_Image *o;

   o = (Evas_Object_Image *)(obj->object_data);
   if ((obj->cur.map) && (obj->cur.usemap)) return 0;
   if (((o->cur.border.l | o->cur.border.r | o->cur.border.t | o->cur.border.b) != 0) &&
       (o->cur.border.fill == EVAS_BORDER_FILL_SOLID) &&
       (obj->cur.render_op == EVAS_RENDER_BLEND) &&
       (obj->cur.cache.clip.a == 255) &&
       (o->cur.fill.x == 0) &&
       (o->cur.fill.y == 0) &&
       (o->cur.fill.w == obj->cur.geometry.w) &&
       (o->cur.fill.h == obj->cur.geometry.h)
       ) return 1;
   return 0;
}

static int
evas_object_image_get_opaque_rect(Evas_Object *obj, Evas_Coord *x, Evas_Coord *y, Evas_Coord *w, Evas_Coord *h)
{
   Evas_Object_Image *o;

   o = (Evas_Object_Image *)(obj->object_data);
   if (o->cur.border.scale == 1.0)
     {
        *x = obj->cur.geometry.x + o->cur.border.l;
        *y = obj->cur.geometry.y + o->cur.border.t;
        *w = obj->cur.geometry.w - (o->cur.border.l + o->cur.border.r);
        if (*w < 0) *w = 0;
        *h = obj->cur.geometry.h - (o->cur.border.t + o->cur.border.b);
        if (*h < 0) *h = 0;
     }
   else
     {
        *x = obj->cur.geometry.x + (o->cur.border.l * o->cur.border.scale);
        *y = obj->cur.geometry.y + (o->cur.border.t * o->cur.border.scale);
        *w = obj->cur.geometry.w - ((o->cur.border.l * o->cur.border.scale) + (o->cur.border.r * o->cur.border.scale));
        if (*w < 0) *w = 0;
        *h = obj->cur.geometry.h - ((o->cur.border.t * o->cur.border.scale) + (o->cur.border.b * o->cur.border.scale));
        if (*h < 0) *h = 0;
     }
   return 1;
}

static int
evas_object_image_can_map(Evas_Object *obj __UNUSED__)
{
   return 1;
}

static void *
evas_object_image_data_convert_internal(Evas_Object_Image *o, void *data, Evas_Colorspace to_cspace)
{
   void *out = NULL;

   if (!data)
     return NULL;

   switch (o->cur.cspace)
     {
      case EVAS_COLORSPACE_ARGB8888:
         out = evas_common_convert_argb8888_to(data,
                                               o->cur.image.w,
                                               o->cur.image.h,
                                               o->cur.image.stride >> 2,
                                               o->cur.has_alpha,
                                               to_cspace);
         break;
      case EVAS_COLORSPACE_RGB565_A5P:
         out = evas_common_convert_rgb565_a5p_to(data,
                                                 o->cur.image.w,
                                                 o->cur.image.h,
                                                 o->cur.image.stride >> 1,
                                                 o->cur.has_alpha,
                                                 to_cspace);
         break;
      case EVAS_COLORSPACE_YCBCR422601_PL:
         out = evas_common_convert_yuv_422_601_to(data,
                                                  o->cur.image.w,
                                                   o->cur.image.h,
                                                   to_cspace);
          break;
        case EVAS_COLORSPACE_YCBCR422P601_PL:
          out = evas_common_convert_yuv_422P_601_to(data,
                                                    o->cur.image.w,
                                                    o->cur.image.h,
                                                    to_cspace);
          break;
        case EVAS_COLORSPACE_YCBCR420NV12601_PL:
          out = evas_common_convert_yuv_420_601_to(data,
                                                   o->cur.image.w,
                                                   o->cur.image.h,
                                                   to_cspace);
          break;
        case EVAS_COLORSPACE_YCBCR420TM12601_PL:
          out = evas_common_convert_yuv_420T_601_to(data,
                                                    o->cur.image.w,
                                                    o->cur.image.h,
                                                    to_cspace);
          break;
        default:
          WRN("unknow colorspace: %i\n", o->cur.cspace);
          break;
     }

   return out;
}

static void
evas_object_image_filled_resize_listener(void *data __UNUSED__, Evas *e __UNUSED__, Evas_Object *obj, void *einfo __UNUSED__)
{
   Evas_Coord w, h;

   evas_object_geometry_get(obj, NULL, NULL, &w, &h);
   evas_object_image_fill_set(obj, 0, 0, w, h);
}


Eina_Bool
_evas_object_image_preloading_get(const Evas_Object *obj)
{
   Evas_Object_Image *o = (Evas_Object_Image *)(obj->object_data);
   if (!o) return EINA_FALSE;
   MAGIC_CHECK(o, Evas_Object_Image, MAGIC_OBJ_IMAGE);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   return o->preloading;
}

void
_evas_object_image_preloading_set(Evas_Object *obj, Eina_Bool preloading)
{
   Evas_Object_Image *o = (Evas_Object_Image *)(obj->object_data);
   o->preloading = preloading;
}

void
_evas_object_image_preloading_check(Evas_Object *obj)
{
   Evas_Object_Image *o = (Evas_Object_Image *)(obj->object_data);
   if (obj->layer->evas->engine.func->image_load_error_get)
      o->load_error = obj->layer->evas->engine.func->image_load_error_get
      (obj->layer->evas->engine.data.output, o->engine_data);
}

Evas_Object *
_evas_object_image_video_parent_get(Evas_Object *obj)
{
   Evas_Object_Image *o = (Evas_Object_Image *)(obj->object_data);

   return o->video_surface ? o->video.parent : NULL;
}

void
_evas_object_image_video_overlay_show(Evas_Object *obj)
{
   Evas_Object_Image *o = (Evas_Object_Image *)(obj->object_data);

   if (obj->cur.cache.clip.x != obj->prev.cache.clip.x ||
       obj->cur.cache.clip.y != obj->prev.cache.clip.y ||
       o->created || !o->video_visible)
     o->video.move(o->video.data, obj, &o->video, obj->cur.cache.clip.x, obj->cur.cache.clip.y);
   if (obj->cur.cache.clip.w != obj->prev.cache.clip.w ||
       obj->cur.cache.clip.h != obj->prev.cache.clip.h ||
       o->created || !o->video_visible)
     o->video.resize(o->video.data, obj, &o->video, obj->cur.cache.clip.w, obj->cur.cache.clip.h);
   if (!o->video_visible || o->created)
     {
        o->video.show(o->video.data, obj, &o->video);
     }
   else
     {
        /* Cancel dirty on the image */
        Eina_Rectangle *r;

        o->dirty_pixels = EINA_FALSE;
        EINA_LIST_FREE(o->pixel_updates, r)
          eina_rectangle_free(r);
     }
   o->video_visible = EINA_TRUE;
   o->created = EINA_FALSE;
}

void
_evas_object_image_video_overlay_hide(Evas_Object *obj)
{
   Evas_Object_Image *o = (Evas_Object_Image *)(obj->object_data);

   if (o->video_visible || o->created)
     o->video.hide(o->video.data, obj, &o->video);
   if (evas_object_is_visible(obj))
     o->video.update_pixels(o->video.data, obj, &o->video);
   o->video_visible = EINA_FALSE;
   o->created = EINA_FALSE;
}

/* vim:set ts=8 sw=3 sts=3 expandtab cino=>5n-2f0^-2{2(0W1st0 :*/
