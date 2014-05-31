#include "evas_common.h" /* Includes evas_bidi_utils stuff. */
#include "evas_private.h"
// TIZEN ONLY (20131106) : Font effect for tizen.
#include <math.h>

/* save typing */
#define ENFN obj->layer->evas->engine.func
#define ENDT obj->layer->evas->engine.data.output

/* private magic number for text objects */
static const char o_type[] = "text";

/* private struct for text object internal data */
typedef struct _Evas_Object_Text Evas_Object_Text;
typedef struct _Evas_Object_Text_Item Evas_Object_Text_Item;
// HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
typedef struct _Evas_Object_Text_Emoticon_Item Evas_Object_Text_Emoticon_Item;
//

struct _Evas_Object_Text
{
   DATA32               magic;

   // HAVE_UNICODE_EMOTICON(2013.10.07): Basic implementation for supporting unicode 6.0 emoticon.
   Eina_List                  *emoticons;
   //

   struct {
      const char          *utf8_text; /* The text exposed to the API */
      const char          *font;
      Evas_Font_Description *fdesc;
      const char          *source;
      Evas_Font_Size       size;
      struct {
	 unsigned char  r, g, b, a;
      } outline, shadow, glow, glow2;

      unsigned char        style;
      double               ellipsis;
      Eina_Unicode        *text;
   } cur, prev;

   float                       ascent, descent;
   float                       max_ascent, max_descent;
   Evas_BiDi_Paragraph_Props  *bidi_par_props;
   const char                 *bidi_delimiters;
   Evas_Object_Text_Item      *items;

   Evas_Font_Set              *font;

   struct {
      Evas_Object_Text_Item    *ellipsis_start;
      Evas_Object_Text_Item    *ellipsis_end;
      Evas_Coord                w, h;
      int                       advance;
      Eina_Bool                 ellipsis;
   } last_computed;

   char                        changed : 1;
};

// HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
struct _Evas_Object_Text_Emoticon_Item
{
   Evas                            *e;
   void                            *engine_data;
   int                              img_w, img_h;
   Evas_Coord                       x, y, w, h;
   Eina_Bool                        visible : 1;
};
//

struct _Evas_Object_Text_Item
{
   EINA_INLIST;

   // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
   Evas_Object_Text_Emoticon_Item *emoticon_item;
   //

   size_t               text_pos;
   size_t               visual_pos;
   Evas_Text_Props      text_props;
   Evas_Coord           x, w, h, adv;
};

/* private methods for text objects */
static void evas_object_text_init(Evas_Object *obj);
static void *evas_object_text_new(void);
static void evas_object_text_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y);
static void evas_object_text_free(Evas_Object *obj);
static void evas_object_text_render_pre(Evas_Object *obj);
static void evas_object_text_render_post(Evas_Object *obj);

static unsigned int evas_object_text_id_get(Evas_Object *obj);
static unsigned int evas_object_text_visual_id_get(Evas_Object *obj);
static void *evas_object_text_engine_data_get(Evas_Object *obj);

static int evas_object_text_is_opaque(Evas_Object *obj);
static int evas_object_text_was_opaque(Evas_Object *obj);

static void evas_object_text_scale_update(Evas_Object *obj);
static void _evas_object_text_recalc(Evas_Object *obj, Eina_Unicode *text);

// HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
static void _evas_object_text_emoticon_update_geometry(Evas_Object *obj);
static void _evas_object_text_emoticon_update_size(Evas_Object_Text *o, Evas_Object_Text_Item *it);
//
// TIZEN ONLY (20131106) : Font effect for tizen.
static int evas_object_text_effect_outerglow_alpha_get(int x, int y);
static int evas_object_text_effect_soft_outerglow_alpha_get(int x, int y);
static int evas_object_text_effect_shadow_alpha_get(int x, int y, double opacity, int size);
//
static const Evas_Object_Func object_func =
{
   /* methods (compulsory) */
   evas_object_text_free,
     evas_object_text_render,
     evas_object_text_render_pre,
     evas_object_text_render_post,
     evas_object_text_id_get,
     evas_object_text_visual_id_get,
     evas_object_text_engine_data_get,
   /* these are optional. NULL = nothing */
     NULL,
     NULL,
     NULL,
     NULL,
     evas_object_text_is_opaque,
     evas_object_text_was_opaque,
     NULL,
     NULL,
     NULL,
     evas_object_text_scale_update,
     NULL,
     NULL,
     NULL
};

/* the actual api call to add a rect */
/* it has no other api calls as all properties are standard */

EVAS_MEMPOOL(_mp_obj);

// TIZEN ONLY (20140209) : Font effect for tizen.
//dropshdow30_outerglow20
static int tizen_glow_shadow_outerglow[9]={127,116,24,0,0,0,0,0,0};
static int tizen_glow_shadow_shadow[19]={12,8,5,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static int tizen_shadow_shadow[19]={21,16,11,0,5,3,0,0,0,0,0,0,0,0,0,0,0,0,0};

//dropshdow25_outerglow15
static int tizen_soft_glow_shadow_outerglow[9]={127,116,22,0,0,0,0,0,0};
static int tizen_soft_glow_shadow_shadow[19]={10,6,4,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
Evas_Object_Text_Emoticon_Item *
_text_emoticon_img_new(Evas *e, Evas_Object *obj, Eina_Unicode str)
{
   const char *value;
   char unicode_str[15];
   char image_file[127];
   int load_error = EVAS_LOAD_ERROR_NONE;
   Evas_Image_Load_Opts opts = { EINA_FALSE, 0, 0, 0, 0, { 0, 0, 0, 0 }, 0 };
   Evas_Object_Text_Emoticon_Item *item;

   value = getenv("EVAS_UNICODE_EMOTICON_IMAGE_DISABLE");
   if (value && !strcmp(value, "1"))
     return NULL;

   item = calloc(1, sizeof(Evas_Object_Text_Emoticon_Item));
   if (!item)
     {
        ERR("Failed to allocate emoticon item, textb(%p)", obj);
        return NULL;
     }
   eina_convert_xtoa(str, unicode_str);
   if (strlen(unicode_str) <= 2)
     {
        snprintf(image_file, sizeof(image_file),
                 "%s/u00%s.png", UNICODE_EMOTICON_FOLDER, unicode_str);
     }
   else
     {
        snprintf(image_file, sizeof(image_file),
                 "%s/u%s.png", UNICODE_EMOTICON_FOLDER, unicode_str);
     }

   item->engine_data = e->engine.func->image_load(e->engine.data.output,
                                                  image_file,
                                                  NULL,
                                                  &load_error,
                                                  &opts);
   if (!item->engine_data || (load_error != EVAS_LOAD_ERROR_NONE))
     {
        DBG("Failed to emoticon image, textblock(%p) error(%d)", obj, load_error);
        free(item);
        return NULL;
     }
   e->engine.func->image_size_get(e->engine.data.output, item->engine_data,
                                  &item->img_w, &item->img_h);
   return item;
}

// HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
static void
_text_emoticon_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y)
{
   Evas_Object_Text *o;
   Evas_Object_Text_Item *it;
   Eina_List *l;
   Evas *e = obj->layer->evas;
   Evas_Object_Text_Emoticon_Item *eitem;

   o = (Evas_Object_Text *)(obj->object_data);

   e->engine.func->context_color_set(output, context, 255, 255, 255, 255);
   if ((obj->cur.cache.clip.r == 255) &&
       (obj->cur.cache.clip.g == 255) &&
       (obj->cur.cache.clip.b == 255) &&
       (obj->cur.cache.clip.a == 255))
        e->engine.func->context_multiplier_unset(output, context);
   else if (obj->cur.clipper)
     {
        e->engine.func->context_multiplier_set(output, context,
                                            obj->cur.clipper->cur.cache.clip.r,
                                            obj->cur.clipper->cur.cache.clip.g,
                                            obj->cur.clipper->cur.cache.clip.b,
                                            obj->cur.clipper->cur.cache.clip.a);
     }

   EINA_LIST_FOREACH(o->emoticons, l, it)
     {
        eitem = it->emoticon_item;
        if (!eitem->visible) continue;
        e->engine.func->image_draw(output, context, surface, eitem->engine_data,
                                   0, 0, eitem->img_w, eitem->img_h,
                                   (eitem->x + x), (eitem->y + y),
                                   eitem->w, eitem->h,
                                   EINA_TRUE);
     }
}

// HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
static void
_evas_object_text_emoticon_update_geometry(Evas_Object *obj)
{
   Evas_Object_Text *o;
   Evas_Object_Text_Item *it;
   Eina_List *l;
   int ex, ey, center_offset = 0;

   o = (Evas_Object_Text *)(obj->object_data);
   if (o->emoticons)
     {
        EINA_LIST_FOREACH(o->emoticons, l, it)
          {
             if (!it->emoticon_item) continue;

             //Compute emoticon position
             if (it->h < o->max_ascent + o->max_descent)
               center_offset = (o->max_ascent + o->max_descent - it->h) / 2;

             ex = obj->cur.geometry.x + it->x;
             ey = obj->cur.geometry.y + center_offset;

             it->emoticon_item->x = ex;
             it->emoticon_item->y = ey;

             //Check the visibility
             if (evas_object_is_in_output_rect(obj,
                                               it->emoticon_item->x,
                                               it->emoticon_item->y,
                                               it->emoticon_item->w,
                                               it->emoticon_item->h))
               it->emoticon_item->visible = EINA_TRUE;
             else
               it->emoticon_item->visible = EINA_FALSE;
          }
     }
}

// HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
static void
_evas_object_text_emoticon_update_size(Evas_Object_Text *o, Evas_Object_Text_Item *it)
{
   float emoticon_size;
   float aspect = 1;
   int emo_w, emo_h;

   if (!it->emoticon_item) return;

   emoticon_size = (float)(o->cur.size) * 72 / 62;
   if (it->emoticon_item->img_w != 0 && it->emoticon_item->img_h != 0)
     aspect = (float)(it->emoticon_item->img_w / it->emoticon_item->img_h);

   emo_w = (int)emoticon_size * aspect;
   emo_h = (int)emoticon_size;
   it->w = emo_w;
   it->h = emo_h;
   it->adv = emo_w;
   it->emoticon_item->w = emo_w;
   it->emoticon_item->h = emo_h;
}

static int
_evas_object_text_char_coords_get(const Evas_Object *obj,
      const Evas_Object_Text *o,
      size_t pos, Evas_Coord *x, Evas_Coord *y, Evas_Coord *w, Evas_Coord *h)
{
   Evas_Object_Text_Item *it;

   EINA_INLIST_FOREACH(EINA_INLIST_GET(o->items), it)
     {
        if ((it->text_pos <= pos) &&
              (pos < (it->text_pos + it->text_props.text_len)))
          {
             int ret;
             ret = ENFN->font_char_coords_get(ENDT, o->font,
                   &it->text_props, pos - it->text_pos, x, y, w, h);

             // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
             if (it->emoticon_item)
               {
                  *w = it->w;
                  *h = it->h;
               }
             //

             if (x) *x += it->x;
             return ret;
          }
     }
   return 0;
}

static void
_evas_object_text_item_clean(Evas_Object_Text_Item *it)
{
   evas_common_text_props_content_unref(&it->text_props);
}

static void
_evas_object_text_item_del(Evas_Object_Text *o, Evas_Object_Text_Item *it)
{
   if (!o || !it) return;
   // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
   if (it->emoticon_item)
     {
        o->emoticons = eina_list_remove(o->emoticons, it);

        if (it->emoticon_item->engine_data)
          {
             Evas *e = it->emoticon_item->e;
             e->engine.func->image_free(e->engine.data.output,
                                        it->emoticon_item->engine_data);
          }
        free(it->emoticon_item);
        it->emoticon_item = NULL;
     }
   //

   if (o->last_computed.ellipsis_start == it)
     o->last_computed.ellipsis_start = NULL;
   else if (o->last_computed.ellipsis_end == it)
     o->last_computed.ellipsis_end = NULL;

   if ((EINA_INLIST_GET(it)->next) ||
       (EINA_INLIST_GET(it)->prev) ||
       (EINA_INLIST_GET(o->items) == (EINA_INLIST_GET(it))))
   o->items = (Evas_Object_Text_Item *) eina_inlist_remove(
         EINA_INLIST_GET(o->items),
         EINA_INLIST_GET(it));
   _evas_object_text_item_clean(it);
   free(it);
}

static void
_evas_object_text_items_clean(Evas_Object *obj, Evas_Object_Text *o)
{
   /* FIXME: also preserve item */
   if ((o->cur.font == o->prev.font) &&
       (o->cur.fdesc == o->prev.fdesc) &&
       (o->cur.size == o->prev.size) &&
       (!memcmp(&o->cur.outline, &o->prev.outline, sizeof (o->cur.outline))) &&
       (!memcmp(&o->cur.shadow, &o->prev.shadow, sizeof (o->cur.shadow))) &&
       (!memcmp(&o->cur.glow, &o->prev.glow, sizeof (o->cur.glow))) &&
       (!memcmp(&o->cur.glow2, &o->prev.glow2, sizeof (o->cur.glow2))) &&
       (o->cur.style == o->prev.style) &&
       (obj->cur.scale == obj->prev.scale))
     {
        if ((o->last_computed.ellipsis_start) &&
            (o->last_computed.ellipsis_start == o->items))
          o->items = (Evas_Object_Text_Item *) eina_inlist_remove(EINA_INLIST_GET(o->items),
                                                                  EINA_INLIST_GET(o->last_computed.ellipsis_start));
        if ((o->last_computed.ellipsis_end) &&
            (EINA_INLIST_GET(o->last_computed.ellipsis_end) == EINA_INLIST_GET(o->items)->last))
          o->items = (Evas_Object_Text_Item *) eina_inlist_remove(EINA_INLIST_GET(o->items),
                                                                  EINA_INLIST_GET(o->last_computed.ellipsis_end));
     }
   else
     {
        _evas_object_text_item_del(o, o->last_computed.ellipsis_start);
        o->last_computed.ellipsis_start = NULL;
        _evas_object_text_item_del(o, o->last_computed.ellipsis_end);
        o->last_computed.ellipsis_end = NULL;
     }
   while (o->items)
     {
        _evas_object_text_item_del(o, o->items);
     }
}

static void
_evas_object_text_items_clear(Evas_Object_Text *o)
{
   if ((o->last_computed.ellipsis_start) &&
       (o->last_computed.ellipsis_start != o->items))
     {
        _evas_object_text_item_del(o, o->last_computed.ellipsis_start);
     }
   o->last_computed.ellipsis_start = NULL;
   if ((o->last_computed.ellipsis_end) &&
       (EINA_INLIST_GET(o->last_computed.ellipsis_end) != EINA_INLIST_GET(o->items)->last))
     {
        _evas_object_text_item_del(o, o->last_computed.ellipsis_end);
     }
   o->last_computed.ellipsis_end = NULL;
   while (o->items)
     {
        _evas_object_text_item_del(o, o->items);
     }
}

#ifdef BIDI_SUPPORT
static int
_evas_object_text_it_compare_logical(const void *_it1, const void *_it2)
{
   const Evas_Object_Text_Item *it1 = _it1, *it2 = _it2;
   if (it1->text_pos < it2->text_pos)
     return -1;
   else if (it1->text_pos == it2->text_pos)
     return 0;
   else
     return 1;

}
#endif

static int
_evas_object_text_last_up_to_pos(const Evas_Object *obj,
      const Evas_Object_Text *o, Evas_Coord cx, Evas_Coord cy)
{
   Evas_Object_Text_Item *it;

#ifdef BIDI_SUPPORT
   /*FIXME: not very efficient, sort the items arrays. */
   /* Reorder if it's a bidi text */
   if (o->bidi_par_props)
     {
        Eina_List *logical_it = NULL;
        Evas_Object_Text_Item *i;
        Eina_List *itr;
        Evas_Coord x = 0;
        /* Insert all to the logical list */
        EINA_INLIST_FOREACH(o->items, i)
          {
             logical_it = eina_list_sorted_insert(logical_it,
                   _evas_object_text_it_compare_logical, i);
          }
        EINA_LIST_FOREACH(logical_it, itr, it)
          {
             if ((x <= cx) && (cx < x + it->adv))
               {
                  return it->text_pos + ENFN->font_last_up_to_pos(ENDT,
                        o->font,
                        &it->text_props,
                        cx - x,
                        cy);
               }
             x += it->adv;
          }
        eina_list_free(logical_it);
     }
   else
#endif
     {
        EINA_INLIST_FOREACH(EINA_INLIST_GET(o->items), it)
          {
             if ((it->x <= cx) && (cx < it->x + it->adv))
               {
                  return it->text_pos + ENFN->font_last_up_to_pos(ENDT,
                        o->font,
                        &it->text_props,
                        cx - it->x,
                        cy);
               }
          }
     }
   return -1;
}

static int
_evas_object_text_char_at_coords(const Evas_Object *obj,
      const Evas_Object_Text *o, Evas_Coord cx, Evas_Coord cy,
      Evas_Coord *rx, Evas_Coord *ry, Evas_Coord *rw, Evas_Coord *rh)
{
   Evas_Object_Text_Item *it;

   EINA_INLIST_FOREACH(EINA_INLIST_GET(o->items), it)
     {
        if ((it->x <= cx) && (cx < it->x + it->adv))
          {
             // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
             if (it->emoticon_item)
               return it->text_pos;
             //

             return it->text_pos + ENFN->font_char_at_coords_get(ENDT,
                   o->font,
                   &it->text_props,
                   cx - it->x,
                   cy,
                   rx, ry,
                   rw, rh);
          }
     }
   return -1;
}

static Evas_Coord
_evas_object_text_horiz_advance_get(const Evas_Object_Text *o)
{
   return o->last_computed.advance;
}

static Evas_Coord
_evas_object_text_vert_advance_get(const Evas_Object *obj __UNUSED__,
      const Evas_Object_Text *o)
{
   return o->max_ascent + o->max_descent;
}

EAPI Evas_Object *
evas_object_text_add(Evas *e)
{
   Evas_Object *obj;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return NULL;
   MAGIC_CHECK_END();
   obj = evas_object_new(e);
   evas_object_text_init(obj);
   evas_object_inject(obj, e);
   return obj;
}

EAPI void
evas_object_text_font_source_set(Evas_Object *obj, const char *font_source)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();

   if ((o->cur.source) && (font_source) &&
       (!strcmp(o->cur.source, font_source)))
     return;
   /*
   if (o->cur.source) eina_stringshare_del(o->cur.source);
   if (font_source) o->cur.source = eina_stringshare_add(font_source);
   else o->cur.source = NULL;
    */
   eina_stringshare_replace(&o->cur.source, font_source);
}

EAPI const char *
evas_object_text_font_source_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return NULL;
   MAGIC_CHECK_END();
   return o->cur.source;
}

EAPI void
evas_object_text_font_set(Evas_Object *obj, const char *font, Evas_Font_Size size)
{
   Evas_Object_Text *o;
   int is, was = 0, pass = 0, freeze = 0;
   Evas_Font_Description *fdesc;
   Eina_Bool source_invisible = EINA_FALSE;

   if ((!font) || (size <= 0)) return;
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();

   if (!(o->cur.font && !strcmp(font, o->cur.font)))
     {
        fdesc = evas_font_desc_new();
        evas_font_name_parse(fdesc, font);
     }
   else
     {
        fdesc = evas_font_desc_ref(o->cur.fdesc);
     }

   if (o->cur.fdesc && !evas_font_desc_cmp(fdesc, o->cur.fdesc) &&
       (size == o->cur.size))
     {
        evas_font_desc_unref(fdesc);
        return;
     }

   if (o->cur.fdesc) evas_font_desc_unref(o->cur.fdesc);
   o->cur.fdesc = fdesc;

   o->cur.size = size;
   eina_stringshare_replace(&o->cur.font, font);
   o->prev.font = NULL;

   if (obj->layer->evas->events_frozen <= 0)
     {
        pass = evas_event_passes_through(obj);
        freeze = evas_event_freezes_through(obj);
        source_invisible = evas_object_is_source_invisible(obj);
        if ((!pass) && (!freeze) && (!source_invisible))
          was = evas_object_is_in_output_rect(obj,
                                              obj->layer->evas->pointer.x,
                                              obj->layer->evas->pointer.y, 1, 1);
     }

   /* DO IT */
   if (o->font)
     {
        evas_font_free(obj->layer->evas, o->font);
        o->font = NULL;
     }

   o->font = evas_font_load(obj->layer->evas, o->cur.fdesc, o->cur.source,
         (int)(((double) o->cur.size) * obj->cur.scale));
   if (o->font)
     {
        o->ascent = ENFN->font_ascent_get(ENDT, o->font);
        o->descent = ENFN->font_descent_get(ENDT, o->font);
        o->max_ascent = ENFN->font_max_ascent_get(ENDT, o->font);
        o->max_descent = ENFN->font_max_descent_get(ENDT, o->font);
     }
   else
     {
        o->ascent = 0;
        o->descent = 0;
        o->max_ascent = 0;
        o->max_descent = 0;
     }
   _evas_object_text_recalc(obj, o->cur.text);
   o->changed = 1;
   evas_object_change(obj);
   evas_object_clip_dirty(obj);
   evas_object_coords_recalc(obj);
   if (obj->layer->evas->events_frozen <= 0)
     {
        if ((!pass) && (!freeze))
          {
             is = evas_object_is_in_output_rect(obj,
                                                obj->layer->evas->pointer.x,
                                                obj->layer->evas->pointer.y,
                                                1, 1);
             if ((is ^ was) && obj->cur.visible)
               evas_event_feed_mouse_move(obj->layer->evas,
                                          obj->layer->evas->pointer.x,
                                          obj->layer->evas->pointer.y,
                                          obj->layer->evas->last_timestamp,
                                          NULL);
          }
     }
   evas_object_inform_call_resize(obj);
}

EAPI void
evas_object_text_font_get(const Evas_Object *obj, const char **font, Evas_Font_Size *size)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (font) *font = "";
   if (size) *size = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   if (font) *font = "";
   if (size) *size = 0;
   return;
   MAGIC_CHECK_END();
   if (font) *font = o->cur.font;
   if (size) *size = o->cur.size;
}

static void
_evas_object_text_item_update_sizes(Evas_Object *obj, Evas_Object_Text *o, Evas_Object_Text_Item *it)
{
   ENFN->font_string_size_get(ENDT,
         o->font,
         &it->text_props,
         &it->w, &it->h);
   it->adv = ENFN->font_h_advance_get(ENDT, o->font,
         &it->text_props);
}

/**
 * @internal
 * Create a new text layout item from the string and the format.
 *
 * @param c the context to work on - Not NULL.
 * @param fmt the format to use.
 * @param str the string to use.
 */
static Evas_Object_Text_Item *
_evas_object_text_item_new(Evas_Object *obj, Evas_Object_Text *o,
      Evas_Font_Instance *fi, const Eina_Unicode *str, Evas_Script_Type script,
      size_t pos, size_t visual_pos, size_t len)
{
   Evas_Object_Text_Item *it;

   it = calloc(1, sizeof(Evas_Object_Text_Item));
   it->text_pos = pos;
   it->visual_pos = visual_pos;
   evas_common_text_props_bidi_set(&it->text_props, o->bidi_par_props,
         it->text_pos);
   evas_common_text_props_script_set(&it->text_props, script);

   if (fi)
     {
        ENFN->font_text_props_info_create(ENDT,
              fi, str + pos, &it->text_props,
              o->bidi_par_props, it->text_pos, len, EVAS_TEXT_PROPS_MODE_SHAPE);
        _evas_object_text_item_update_sizes(obj, o, it);
     }
   o->items = (Evas_Object_Text_Item *)
      eina_inlist_append(EINA_INLIST_GET(o->items), EINA_INLIST_GET(it));
   return it;
}

/**
 * @internal
 * Orders o->items according to the visual position.
 *
 * @param obj the evas object
 * @param o the text object
 */
static void
_evas_object_text_item_order(Evas_Object *obj, Evas_Object_Text *o)
{
   (void) obj;
#ifdef BIDI_SUPPORT
   /*FIXME: not very efficient, sort the items arrays. */
   /* Reorder if it's a bidi text */
   if (o->bidi_par_props)
     {
        Evas_Object_Text_Item *i, *j, *min;
        i = o->items;
        while (i)
          {
             min = i;
             EINA_INLIST_FOREACH(i, j)
               {
                  if (j->visual_pos < min->visual_pos)
                    {
                       min = j;
                    }
               }
             if (min != i)
               {
                  o->items = (Evas_Object_Text_Item *) eina_inlist_remove(EINA_INLIST_GET(o->items), EINA_INLIST_GET(min));
                  o->items = (Evas_Object_Text_Item *) eina_inlist_prepend_relative(EINA_INLIST_GET(o->items), EINA_INLIST_GET(min), EINA_INLIST_GET(i));
               }

             i = (Evas_Object_Text_Item *) EINA_INLIST_GET(min)->next;
          }
     }
#endif

   /* calculate the positions according to the order. */
     {
        Evas_Object_Text_Item *it = o->items;
        Evas_Coord x = 0;

        while (it)
          {
             it->x = x;
             x += it->adv;
             it = (Evas_Object_Text_Item *) EINA_INLIST_GET(it)->next;
          }
     }
}

/**
 * Create ellipsis.
 */
static const Eina_Unicode _ellip_str[2] = { 0x2026, '\0' };

/* FIXME: We currently leak ellipsis items. */
static Evas_Object_Text_Item *
_layout_ellipsis_item_new(Evas_Object *obj, Evas_Object_Text *o)
{
   Evas_Object_Text_Item *ellip_ti;
   Evas_Script_Type script;
   Evas_Font_Instance *script_fi = NULL, *cur_fi;
   size_t len = 1; /* The length of _ellip_str */

   script = evas_common_language_script_type_get(_ellip_str, 1);

   if (o->font)
     {
        (void) ENFN->font_run_end_get(ENDT, o->font, &script_fi, &cur_fi,
                                      script, _ellip_str, 1);
     }

   ellip_ti = _evas_object_text_item_new(obj, o, cur_fi,
                                         _ellip_str, script, 0, 0, len);

   return ellip_ti;
}

/* EINA_TRUE if this item is ok and should be included, false if should be
 * discarded. */
static Eina_Bool
_layout_text_item_trim(Evas_Object *obj, Evas_Object_Text *o, Evas_Object_Text_Item *ti, int idx, Eina_Bool want_start)
{
   Evas_Text_Props new_text_props;
   if (idx >= (int) ti->text_props.len)
      return EINA_FALSE;

   memset(&new_text_props, 0, sizeof (new_text_props));

   while (!evas_common_text_props_split(&ti->text_props, &new_text_props, idx))
     idx--;
   if (want_start)
     {
        evas_common_text_props_content_unref(&new_text_props);
     }
   else
     {
        evas_common_text_props_content_unref(&ti->text_props);
        memcpy(&ti->text_props, &new_text_props, sizeof(ti->text_props));
        ti->text_pos += idx;
        ti->visual_pos += idx;
     }
   _evas_object_text_item_update_sizes(obj, o, ti);

   return EINA_TRUE;
}

/**
 * @internal
 * Populates o->items with the items of the text according to text
 *
 * @param obj the evas object
 * @param o the text object
 * @param text the text to layout
 */
static void
_evas_object_text_layout(Evas_Object *obj, Evas_Object_Text *o, Eina_Unicode *text)
{
   EvasBiDiStrIndex *v_to_l = NULL;
   Evas_Coord advance = 0;
   size_t pos, visual_pos;
   int len = eina_unicode_strlen(text);
   int l = 0, r = 0;
#ifdef BIDI_SUPPORT
   int par_len = len;
   int *segment_idxs = NULL;
#endif

   if (o->items &&
       !memcmp(&o->cur, &o->prev, sizeof (o->cur)) &&
       o->cur.text == text &&
       obj->cur.scale == obj->prev.scale &&
       ((o->last_computed.advance <= obj->cur.geometry.w && !o->last_computed.ellipsis) ||
        o->last_computed.w == obj->cur.geometry.w))
     return ;

   o->last_computed.ellipsis = EINA_FALSE;
   if (o->items) _evas_object_text_items_clean(obj, o);

#ifdef BIDI_SUPPORT
   if (o->bidi_delimiters)
      segment_idxs = evas_bidi_segment_idxs_get(text, o->bidi_delimiters);
   evas_bidi_paragraph_props_unref(o->bidi_par_props);
   o->bidi_par_props = evas_bidi_paragraph_props_get(text, len, segment_idxs);
   evas_bidi_props_reorder_line(NULL, 0, len, o->bidi_par_props, &v_to_l);
   if (segment_idxs) free(segment_idxs);
#endif
   visual_pos = pos = 0;

   while (len > 0)
     {
        Evas_Font_Instance *script_fi = NULL;
        int script_len = len, tmp_cut;
        Evas_Script_Type script;
        tmp_cut = evas_common_language_script_end_of_run_get(
              text + pos,
              o->bidi_par_props,
              pos, len);
        if (tmp_cut > 0)
           script_len = tmp_cut;

        script = evas_common_language_script_type_get(text + pos, script_len);

        while (script_len > 0)
          {
             Evas_Object_Text_Item *it;
             Evas_Font_Instance *cur_fi = NULL;
             int run_len = script_len;
             if (o->font)
               {
                  run_len = ENFN->font_run_end_get(ENDT,
                        o->font, &script_fi, &cur_fi,
                        script, text + pos, script_len);
               }
#ifdef BIDI_SUPPORT
             visual_pos = evas_bidi_position_logical_to_visual(
                   v_to_l, par_len, pos);
#else
             visual_pos = pos;
#endif
             it = _evas_object_text_item_new(obj, o, cur_fi, text, script,
                                             pos, visual_pos, run_len);

             // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
             UNICODE_EMOTICON_CHECK(*(text + pos))
               {
                  it->emoticon_item = _text_emoticon_img_new(obj->layer->evas, obj, *(text+pos));
                  if (it->emoticon_item)
                    {
                       it->emoticon_item->e = obj->layer->evas;
                       o->emoticons = eina_list_append(o->emoticons, it);
                       _evas_object_text_emoticon_update_size(o, it);
                    }
               }
             //

             advance += it->adv;
             pos += run_len;
             script_len -= run_len;
             len -= run_len;
          }
     }

   /* Handle ellipsis */
  if (pos && (o->cur.ellipsis >= 0.0) && (advance + l + r > obj->cur.geometry.w) && (obj->cur.geometry.w > 0))
     {
        Evas_Coord ellip_frame = obj->cur.geometry.w;
        Evas_Object_Text_Item *start_ellip_it = NULL, *end_ellip_it = NULL;

        o->last_computed.ellipsis = EINA_TRUE;

        /* Account of the ellipsis item width. As long as ellipsis != 0
         * we have a left ellipsis. And the same with 1 and right. */
        if (o->cur.ellipsis != 0)
          {
             if (o->last_computed.ellipsis_start)
               {
                  start_ellip_it = o->last_computed.ellipsis_start;
                  o->items = (Evas_Object_Text_Item *)
                    eina_inlist_append(EINA_INLIST_GET(o->items), EINA_INLIST_GET(start_ellip_it));
               }
             else
               {
                  start_ellip_it = _layout_ellipsis_item_new(obj, o);
               }
             o->last_computed.ellipsis_start = start_ellip_it;
             ellip_frame -= start_ellip_it->adv;
          }
        if (o->cur.ellipsis != 1)
          {
             /* FIXME: Should take the last item's font and style and etc. *//* weird it's a text, should always have the same style/font */
             if (o->last_computed.ellipsis_end)
               {
                  end_ellip_it = o->last_computed.ellipsis_end;
                  o->items = (Evas_Object_Text_Item *)
                    eina_inlist_append(EINA_INLIST_GET(o->items), EINA_INLIST_GET(end_ellip_it));
               }
             else
               {
                  end_ellip_it = _layout_ellipsis_item_new(obj, o);
               }
             o->last_computed.ellipsis_end = end_ellip_it;
             ellip_frame -= end_ellip_it->adv;
          }

        /* The point where we should start from, going for the full
         * ellip frame. */
        Evas_Coord ellipsis_coord = o->cur.ellipsis * (advance - ellip_frame);
        if (start_ellip_it)
          {
             Evas_Object_Text_Item *itr = o->items;
             advance = 0;

             while (itr && (advance + l + r + itr->adv < ellipsis_coord))
               {
                  Eina_Inlist *itrn = EINA_INLIST_GET(itr)->next;
                  if ((itr != start_ellip_it) && (itr != end_ellip_it))
                    {
                       advance += itr->adv;
                       _evas_object_text_item_del(o, itr);
                    }
                  itr = (Evas_Object_Text_Item *) itrn;
               }
             if (itr && (itr != start_ellip_it))
               {
                  int cut = 1 + ENFN->font_last_up_to_pos(ENDT,
                        o->font,
                        &itr->text_props,
                        ellipsis_coord - (advance + l + r),
                        0);
                  if (cut > 0)
                    {
                       start_ellip_it->text_pos = itr->text_pos;
                       start_ellip_it->visual_pos = itr->visual_pos;
                       if (!_layout_text_item_trim(obj, o, itr, cut, EINA_FALSE))
                         {
                            _evas_object_text_item_del(o, itr);
                         }
                    }
               }

	     o->items = (Evas_Object_Text_Item *) eina_inlist_remove(EINA_INLIST_GET(o->items), EINA_INLIST_GET(start_ellip_it));
             o->items = (Evas_Object_Text_Item *) eina_inlist_prepend(EINA_INLIST_GET(o->items), EINA_INLIST_GET(start_ellip_it));
          }

        if (end_ellip_it)
          {
             Evas_Object_Text_Item *itr = o->items;
             int idx = 0;
             advance = 0;

             while (itr)
               {
                  if (itr != end_ellip_it) /* was start_ellip_it */
                    {
                       if (advance + l + r + itr->adv >= ellip_frame)
                         {
                            break;
                         }
                       advance += itr->adv;
                    }
                  idx++;
                  itr = (Evas_Object_Text_Item *) EINA_INLIST_GET(itr)->next;
               }

             if (itr == end_ellip_it)
               {
                  /* FIXME: We shouldn't do anything. */
               }

             int cut = ENFN->font_last_up_to_pos(ENDT,
                   o->font,
                   &itr->text_props,
                   ellip_frame - (advance + l + r),
                   0);

             if (cut >= 0)
               {
                  end_ellip_it->text_pos = itr->text_pos + cut;
                  end_ellip_it->visual_pos = itr->visual_pos + cut;
                  // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
                  if (itr->emoticon_item)
                    {
                       o->emoticons = eina_list_remove(o->emoticons, itr);

                       if (itr->emoticon_item->engine_data)
                         {
                            Evas *e = obj->layer->evas;
                            e->engine.func->image_free(e->engine.data.output,
                                                       itr->emoticon_item->engine_data);
                         }
                       free(itr->emoticon_item);
                       itr->emoticon_item = NULL;
                    }
                  //
                  if (_layout_text_item_trim(obj, o, itr, cut, EINA_TRUE))
                    {
                       itr = (Evas_Object_Text_Item *) EINA_INLIST_GET(itr)->next;
                    }
               }

             /* Remove the rest of the items */
             while (itr)
               {
                  Eina_Inlist *itrn = EINA_INLIST_GET(itr)->next;
                  if ((itr != start_ellip_it) && (itr != end_ellip_it))
                     _evas_object_text_item_del(o, itr);
                  itr = (Evas_Object_Text_Item *) itrn;
               }
          }
     }
   if (o->cur.text != text) free(o->cur.text);
   o->cur.text = text;
   o->prev = o->cur;

   {
      Evas_Object_Text_Item *itr = o->items;
      advance = 0;

      while (itr)
        {
           advance += itr->adv;
           itr = (Evas_Object_Text_Item *) EINA_INLIST_GET(itr)->next;
        }
      o->last_computed.advance = advance;
   }

   _evas_object_text_item_order(obj, o);

   // HAVE_UNICODE_EMOTICON(2013.10.07): Basic implementation for supporting unicode 6.0 emoticon.
   _evas_object_text_emoticon_update_geometry(obj);
   //

   if (v_to_l) free(v_to_l);
}

static void
_text_resize(void *data,
             Evas *e EINA_UNUSED,
             Evas_Object *obj,
             void *event_info EINA_UNUSED)
{
   Evas_Object_Text *o = data;

   _evas_object_text_recalc(obj, o->cur.text);
}

EAPI void
evas_object_text_ellipsis_set(Evas_Object *obj, double ellipsis)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();

   if (o->cur.ellipsis == ellipsis) return ;

   o->cur.ellipsis = ellipsis;
   o->changed = 1;
   evas_object_change(obj);
   evas_object_clip_dirty(obj);

   evas_object_event_callback_del_full(obj, EVAS_CALLBACK_RESIZE,
                                       _text_resize, o);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_RESIZE,
                                  _text_resize, o);
}

EAPI double
evas_object_text_ellipsis_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return -1;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return -1;
   MAGIC_CHECK_END();

   return o->cur.ellipsis;
}

EAPI void
evas_object_text_text_set(Evas_Object *obj, const char *_text)
{
   Evas_Object_Text *o;
   int is, was, len;
   Eina_Unicode *text;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();

   if ((o->cur.utf8_text) && (_text) && (!strcmp(o->cur.utf8_text, _text)))
      return;
   text = eina_unicode_utf8_to_unicode(_text, &len);

   if (!text) text = eina_unicode_strdup(EINA_UNICODE_EMPTY_STRING);
   was = evas_object_is_in_output_rect(obj,
				       obj->layer->evas->pointer.x,
				       obj->layer->evas->pointer.y, 1, 1);
   /* DO II */
   /*Update bidi_props*/

   if (o->items) _evas_object_text_items_clear(o);

   _evas_object_text_recalc(obj, text);
   eina_stringshare_replace(&o->cur.utf8_text, _text);
   o->prev.utf8_text = NULL;

   if (o->cur.text != text) free(text);

   o->changed = 1;
   evas_object_change(obj);
   evas_object_clip_dirty(obj);
   evas_object_coords_recalc(obj);
   is = evas_object_is_in_output_rect(obj,
				      obj->layer->evas->pointer.x,
				      obj->layer->evas->pointer.y, 1, 1);
   if ((is || was) && obj->cur.visible)
     evas_event_feed_mouse_move(obj->layer->evas,
				obj->layer->evas->pointer.x,
				obj->layer->evas->pointer.y,
				obj->layer->evas->last_timestamp,
				NULL);
   evas_object_inform_call_resize(obj);
}

EAPI void
evas_object_text_bidi_delimiters_set(Evas_Object *obj, const char *delim)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();

   eina_stringshare_replace(&o->bidi_delimiters, delim);
}

EAPI const char *
evas_object_text_bidi_delimiters_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return NULL;
   MAGIC_CHECK_END();

   return o->bidi_delimiters;
}


EAPI const char *
evas_object_text_text_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return NULL;
   MAGIC_CHECK_END();
   return o->cur.utf8_text;
}

EAPI Evas_BiDi_Direction
evas_object_text_direction_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EVAS_BIDI_DIRECTION_NEUTRAL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return EVAS_BIDI_DIRECTION_NEUTRAL;
   MAGIC_CHECK_END();
   if (o->items)
     {
        return o->items->text_props.bidi.dir;
     }
   return EVAS_BIDI_DIRECTION_NEUTRAL;
}

EAPI Evas_Coord
evas_object_text_ascent_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return 0;
   MAGIC_CHECK_END();
   return o->ascent;
}

EAPI Evas_Coord
evas_object_text_descent_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return 0;
   MAGIC_CHECK_END();
   return o->descent;
}

EAPI Evas_Coord
evas_object_text_max_ascent_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return 0;
   MAGIC_CHECK_END();
   return o->max_ascent;
}

EAPI Evas_Coord
evas_object_text_max_descent_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return 0;
   MAGIC_CHECK_END();
   return o->max_descent;
}

EAPI Evas_Coord
evas_object_text_inset_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return 0;
   MAGIC_CHECK_END();
   if (!o->font) return 0;
   if (!o->items) return 0;
   return ENFN->font_inset_get(ENDT, o->font, &o->items->text_props);
}

EAPI Evas_Coord
evas_object_text_horiz_advance_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return 0;
   MAGIC_CHECK_END();
   if (!o->font) return 0;
   if (!o->items) return 0;
   return _evas_object_text_horiz_advance_get(o);
}

EAPI Evas_Coord
evas_object_text_vert_advance_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return 0;
   MAGIC_CHECK_END();
   if (!o->font) return 0;
   if (!o->items) return o->ascent + o->descent;
   return _evas_object_text_vert_advance_get(obj, o);
}

EAPI Eina_Bool
evas_object_text_char_pos_get(const Evas_Object *obj, int pos, Evas_Coord *cx, Evas_Coord *cy, Evas_Coord *cw, Evas_Coord *ch)
{
   Evas_Object_Text *o;
   int l = 0, r = 0, t = 0, b = 0;
   int ret, x = 0, y = 0, w = 0, h = 0;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   if (!o->font) return EINA_FALSE;
   if (!o->items || (pos < 0)) return EINA_FALSE;
   ret = _evas_object_text_char_coords_get(obj, o, (size_t) pos,
            &x, &y, &w, &h);
   evas_text_style_pad_get(o->cur.style, &l, &r, &t, &b);
   y += o->max_ascent - t;
   x -= l;
   if (x < 0)
     {
	w += x;
	x = 0;
     }
   if ((x + w) > obj->cur.geometry.w) w = obj->cur.geometry.w - x;
   if (w < 0) w = 0;
   if (y < 0)
     {
	h += y;
	y = 0;
     }
   if ((y + h) > obj->cur.geometry.h) h = obj->cur.geometry.h - y;
   if (h < 0) h = 0;
   if (cx) *cx = x;
   if (cy) *cy = y;
   if (cw) *cw = w + l + r;
   if (ch) *ch = h + t + b;
   return ret;
}


EAPI int
evas_object_text_last_up_to_pos(const Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return -1;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return -1;
   MAGIC_CHECK_END();
   if (!o->font) return -1;
   if (!o->items) return -1;
   return _evas_object_text_last_up_to_pos(obj, o, x, y - o->max_ascent);
}

EAPI int
evas_object_text_char_coords_get(const Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord *cx, Evas_Coord *cy, Evas_Coord *cw, Evas_Coord *ch)
{
   Evas_Object_Text *o;
   int l = 0, r = 0, t = 0, b = 0;
   int ret, rx = 0, ry = 0, rw = 0, rh = 0;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return -1;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return -1;
   MAGIC_CHECK_END();
   if (!o->font) return -1;
   if (!o->items) return -1;
   ret = _evas_object_text_char_at_coords(obj, o, x, y - o->max_ascent,
         &rx, &ry, &rw, &rh);
   evas_text_style_pad_get(o->cur.style, &l, &r, &t, &b);
   ry += o->max_ascent - t;
   rx -= l;
   if (rx < 0)
     {
	rw += rx;
	rx = 0;
     }
   if ((rx + rw) > obj->cur.geometry.w) rw = obj->cur.geometry.w - rx;
   if (rw < 0) rw = 0;
   if (ry < 0)
     {
	rh += ry;
	ry = 0;
     }
   if ((ry + rh) > obj->cur.geometry.h) rh = obj->cur.geometry.h - ry;
   if (rh < 0) rh = 0;
   if (cx) *cx = rx;
   if (cy) *cy = ry;
   if (cw) *cw = rw + l + r;
   if (ch) *ch = rh + t + b;
   return ret;
}

EAPI void
evas_object_text_style_set(Evas_Object *obj, Evas_Text_Style_Type style)
{
   Evas_Object_Text *o;
   int pl = 0, pr = 0, pt = 0, pb = 0, l = 0, r = 0, t = 0, b = 0;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();
   if (o->cur.style == style) return;
   evas_text_style_pad_get(o->cur.style, &pl, &pr, &pt, &pb);
   o->cur.style = style;
   evas_text_style_pad_get(o->cur.style, &l, &r, &t, &b);
   if (o->items)
     obj->cur.geometry.w += (l - pl) + (r - pr);
   else
     obj->cur.geometry.w = 0;
   obj->cur.geometry.h += (t - pt) + (b - pb);
   evas_object_change(obj);
   evas_object_clip_dirty(obj);
}

EAPI Evas_Text_Style_Type
evas_object_text_style_get(const Evas_Object *obj)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EVAS_TEXT_STYLE_PLAIN;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return EVAS_TEXT_STYLE_PLAIN;
   MAGIC_CHECK_END();
   return o->cur.style;
}

EAPI void
evas_object_text_shadow_color_set(Evas_Object *obj, int r, int g, int b, int a)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();
   if ((o->cur.shadow.r == r) && (o->cur.shadow.g == g) &&
       (o->cur.shadow.b == b) && (o->cur.shadow.a == a))
     return;
   o->cur.shadow.r = r;
   o->cur.shadow.g = g;
   o->cur.shadow.b = b;
   o->cur.shadow.a = a;
   o->changed = 1;
   evas_object_change(obj);
}

EAPI void
evas_object_text_shadow_color_get(const Evas_Object *obj, int *r, int *g, int *b, int *a)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (r) *r = 0;
   if (g) *g = 0;
   if (b) *b = 0;
   if (a) *a = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   if (r) *r = 0;
   if (g) *g = 0;
   if (b) *b = 0;
   if (a) *a = 0;
   return;
   MAGIC_CHECK_END();
   if (r) *r = o->cur.shadow.r;
   if (g) *g = o->cur.shadow.g;
   if (b) *b = o->cur.shadow.b;
   if (a) *a = o->cur.shadow.a;
}

EAPI void
evas_object_text_glow_color_set(Evas_Object *obj, int r, int g, int b, int a)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();
   if ((o->cur.glow.r == r) && (o->cur.glow.g == g) &&
       (o->cur.glow.b == b) && (o->cur.glow.a == a))
     return;
   o->cur.glow.r = r;
   o->cur.glow.g = g;
   o->cur.glow.b = b;
   o->cur.glow.a = a;
   o->changed = 1;
   evas_object_change(obj);
}

EAPI void
evas_object_text_glow_color_get(const Evas_Object *obj, int *r, int *g, int *b, int *a)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (r) *r = 0;
   if (g) *g = 0;
   if (b) *b = 0;
   if (a) *a = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   if (r) *r = 0;
   if (g) *g = 0;
   if (b) *b = 0;
   if (a) *a = 0;
   return;
   MAGIC_CHECK_END();
   if (r) *r = o->cur.glow.r;
   if (g) *g = o->cur.glow.g;
   if (b) *b = o->cur.glow.b;
   if (a) *a = o->cur.glow.a;
}

EAPI void
evas_object_text_glow2_color_set(Evas_Object *obj, int r, int g, int b, int a)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();
   if ((o->cur.glow2.r == r) && (o->cur.glow2.g == g) &&
       (o->cur.glow2.b == b) && (o->cur.glow2.a == a))
     return;
   o->cur.glow2.r = r;
   o->cur.glow2.g = g;
   o->cur.glow2.b = b;
   o->cur.glow2.a = a;
   o->changed = 1;
   evas_object_change(obj);
}

EAPI void
evas_object_text_glow2_color_get(const Evas_Object *obj, int *r, int *g, int *b, int *a)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (r) *r = 0;
   if (g) *g = 0;
   if (b) *b = 0;
   if (a) *a = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   if (r) *r = 0;
   if (g) *g = 0;
   if (b) *b = 0;
   if (a) *a = 0;
   return;
   MAGIC_CHECK_END();
   if (r) *r = o->cur.glow2.r;
   if (g) *g = o->cur.glow2.g;
   if (b) *b = o->cur.glow2.b;
   if (a) *a = o->cur.glow2.a;
}

EAPI void
evas_object_text_outline_color_set(Evas_Object *obj, int r, int g, int b, int a)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();
   if ((o->cur.outline.r == r) && (o->cur.outline.g == g) &&
       (o->cur.outline.b == b) && (o->cur.outline.a == a))
     return;
   o->cur.outline.r = r;
   o->cur.outline.g = g;
   o->cur.outline.b = b;
   o->cur.outline.a = a;
   o->changed = 1;
   evas_object_change(obj);
}

EAPI void
evas_object_text_outline_color_get(const Evas_Object *obj, int *r, int *g, int *b, int *a)
{
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (r) *r = 0;
   if (g) *g = 0;
   if (b) *b = 0;
   if (a) *a = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   if (r) *r = 0;
   if (g) *g = 0;
   if (b) *b = 0;
   if (a) *a = 0;
   return;
   MAGIC_CHECK_END();
   if (r) *r = o->cur.outline.r;
   if (g) *g = o->cur.outline.g;
   if (b) *b = o->cur.outline.b;
   if (a) *a = o->cur.outline.a;
}

EAPI void
evas_object_text_style_pad_get(const Evas_Object *obj, int *l, int *r, int *t, int *b)
{
   int sl = 0, sr = 0, st = 0, sb = 0;
   Evas_Object_Text *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (l) *l = 0;
   if (r) *r = 0;
   if (t) *t = 0;
   if (b) *b = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   if (l) *l = 0;
   if (r) *r = 0;
   if (t) *t = 0;
   if (b) *b = 0;
   return;
   MAGIC_CHECK_END();
   /* use temps to be certain we have initialized values */
   evas_text_style_pad_get(o->cur.style, &sl, &sr, &st, &sb);
   if (l) *l = sl;
   if (r) *r = sr;
   if (t) *t = st;
   if (b) *b = sb;
}




EAPI int
evas_string_char_next_get(const char *str, int pos, int *decoded)
{
   int p, d;

   if ((!str) || (pos < 0))
     {
        if (decoded) *decoded = 0;
        return 0;
     }
   p = pos;
   d = eina_unicode_utf8_get_next(str, &p);
   if (decoded) *decoded = d;
   return p;
}

EAPI int
evas_string_char_prev_get(const char *str, int pos, int *decoded)
{
   int p, d;

   if (decoded) *decoded = 0;
   if ((!str) || (pos < 1)) return 0;
   p = pos;
   d = eina_unicode_utf8_get_prev(str, &p);
   if (decoded) *decoded = d;
   return p;
}

EAPI int
evas_string_char_len_get(const char *str)
{
   if (!str) return 0;
   return eina_unicode_utf8_get_len(str);
}

void
evas_text_style_pad_get(Evas_Text_Style_Type style, int *l, int *r, int *t, int *b)
{
   int sl = 0, sr = 0, st = 0, sb = 0;

   /* Don't calc anything if there's no style. */
   if (style != EVAS_TEXT_STYLE_PLAIN)
     {
        int shad_sz = 0, shad_dst = 0, out_sz = 0;
        int dx = 0, minx = 0, maxx = 0;
        int dy = 0, miny = 0, maxy = 0;
        Eina_Bool have_shadow = EINA_FALSE;

        switch (style & EVAS_TEXT_STYLE_MASK_BASIC)
          {
           case EVAS_TEXT_STYLE_SHADOW:
              shad_dst = 1;
              have_shadow = EINA_TRUE;
              break;
           case EVAS_TEXT_STYLE_OUTLINE_SHADOW:
           case EVAS_TEXT_STYLE_FAR_SHADOW:
              shad_dst = 2;
              out_sz = 1;
              have_shadow = EINA_TRUE;
              break;
           case EVAS_TEXT_STYLE_OUTLINE_SOFT_SHADOW:
              shad_dst = 1;
              shad_sz = 2;
              out_sz = 1;
              have_shadow = EINA_TRUE;
              break;
           case EVAS_TEXT_STYLE_FAR_SOFT_SHADOW:
              shad_dst = 2;
              shad_sz = 2;
              have_shadow = EINA_TRUE;
              break;
           case EVAS_TEXT_STYLE_SOFT_SHADOW:
              shad_dst = 1;
              shad_sz = 2;
              have_shadow = EINA_TRUE;
              break;
           case EVAS_TEXT_STYLE_GLOW:
           case EVAS_TEXT_STYLE_SOFT_OUTLINE:
              out_sz = 2;
              break;
           case EVAS_TEXT_STYLE_OUTLINE:
              out_sz = 1;
              break;
	   case EVAS_TEXT_STYLE_TIZEN_GLOW_SHADOW:
           case EVAS_TEXT_STYLE_TIZEN_SOFT_GLOW_SHADOW:
           case EVAS_TEXT_STYLE_TIZEN_SHADOW:
              out_sz = 1;
              break;

           default:
              break;
          }

        minx = -out_sz;
        maxx = out_sz;
        miny = -out_sz;
        maxy = out_sz;
        if (have_shadow)
          {
             int shx1, shx2, shy1, shy2;
             switch (style & EVAS_TEXT_STYLE_MASK_SHADOW_DIRECTION)
               {
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_BOTTOM_RIGHT:
                   dx = 1;
                   dy = 1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_BOTTOM:
                   dx = 0;
                   dy = 1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_BOTTOM_LEFT:
                   dx = -1;
                   dy = 1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_LEFT:
                   dx = -1;
                   dy = 0;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_TOP_LEFT:
                   dx = -1;
                   dy = -1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_TOP:
                   dx = 0;
                   dy = -1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_TOP_RIGHT:
                   dx = 1;
                   dy = -1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_RIGHT:
                   dx = 1;
                   dy = 0;
                default:
                   break;
               }
             shx1 = dx * shad_dst;
             shx1 -= shad_sz;
             shx2 = dx * shad_dst;
             shx2 += shad_sz;
             if (shx1 < minx) minx = shx1;
             if (shx2 > maxx) maxx = shx2;

             shy1 = dy * shad_dst;
             shy1 -= shad_sz;
             shy2 = dy * shad_dst;
             shy2 += shad_sz;
             if (shy1 < miny) miny = shy1;
             if (shy2 > maxy) maxy = shy2;
          }

        if (l) sl = *l;
        if (r) sr = *r;
        if (t) st = *t;
        if (b) sb = *b;

        if (sr < maxx) sr = maxx;
        if (sl < -minx) sl = -minx;
        if (sb < maxy) sb = maxy;
        if (st < -miny) st = -miny;
     }

   if (l) *l = sl;
   if (r) *r = sr;
   if (t) *t = st;
   if (b) *b = sb;
}

/* all nice and private */
static void
evas_object_text_init(Evas_Object *obj)
{
   // HAVE_UNICODE_EMOTICON(2013.10.07): Basic implementation for supporting unicode 6.0 emoticon.
   Evas_Object_Text *o;
   o = (Evas_Object_Text *)(obj->object_data);
   //

   /* alloc text ob, setup methods and default values */
   obj->object_data = evas_object_text_new();
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
   /* set up object-specific settings */
   obj->prev = obj->cur;
   /* set up methods (compulsory) */
   obj->func = &object_func;
   obj->type = o_type;
}

static void *
evas_object_text_new(void)
{
   Evas_Object_Text *o;

   /* alloc obj private data */
   EVAS_MEMPOOL_INIT(_mp_obj, "evas_object_text", Evas_Object_Text, 8, NULL);
   o = EVAS_MEMPOOL_ALLOC(_mp_obj, Evas_Object_Text);
   if (!o) return NULL;
   EVAS_MEMPOOL_PREP(_mp_obj, o, Evas_Object_Text);
   o->magic = MAGIC_OBJ_TEXT;
   o->cur.ellipsis = -1.0;
   o->prev = o->cur;
#ifdef BIDI_SUPPORT
   o->bidi_par_props = evas_bidi_paragraph_props_new();
#endif
   return o;
}

static void
evas_object_text_free(Evas_Object *obj)
{
   Evas_Object_Text *o;

   /* frees private object data. very simple here */
   o = (Evas_Object_Text *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Text, MAGIC_OBJ_TEXT);
   return;
   MAGIC_CHECK_END();
   /* free obj */
   _evas_object_text_items_clear(o);
   // HAVE_UNICODE_EMOTICON(2013.12.06): Refactoring draw.
   o->emoticons = eina_list_free(o->emoticons);
   //
   if (o->cur.utf8_text) eina_stringshare_del(o->cur.utf8_text);
   if (o->cur.font) eina_stringshare_del(o->cur.font);
   if (o->cur.fdesc) evas_font_desc_unref(o->cur.fdesc);
   if (o->cur.source) eina_stringshare_del(o->cur.source);
   if (o->cur.text) free(o->cur.text);
   if (o->font) evas_font_free(obj->layer->evas, o->font);
#ifdef BIDI_SUPPORT
   evas_bidi_paragraph_props_unref(o->bidi_par_props);
#endif
   o->magic = 0;
   EVAS_MEMPOOL_FREE(_mp_obj, o);
}

///////////////////// TIZEN ONLY (20131106) : Font effect for tizen. /////////////////////
static int __attribute__((optimize("O0")))
evas_object_text_effect_outerglow_alpha_get(int x, int y)
{
   int alpha;
   double intensity = sqrt((x * x) + (y * y));
   intensity = (2.0 * 1.12 - intensity) / 2.0;
   if (intensity > 0.0)
     {
        intensity = (intensity >= 1.0) ? 1.0 : intensity;
        intensity = sin(intensity * 3.141592 / 2.0);
        alpha = (int)(255.0 * 0.3 * intensity + 0.5);
        return alpha;
     }
   else
     return 0;
}

static int __attribute__((optimize("O0")))
evas_object_text_effect_soft_outerglow_alpha_get(int x, int y)
{
   int alpha;
   double intensity = sqrt((x * x) + (y * y));
   intensity = (1.3 * 1.5 - intensity) / 1.3;
   if (intensity > 0.0)
     {
        intensity = (intensity >= 1.0) ? 1.0 : intensity;
        intensity = sin(intensity * 3.141592 / 2.0);
        alpha =(int)(255.0 * 0.5 * intensity + 0.5);
        return alpha;
     }
   else
     return 0;
}

static int __attribute__((optimize("O0")))
evas_object_text_effect_shadow_alpha_get(int x, int y, double opacity, int size)
{
   int alpha;
   double intensity = sqrt((x * x) + (y * y));
   intensity = (size - intensity) / size;
   if (intensity > 0.0)
     {
        intensity = sin((intensity * 3.141592) / 2.0);
        intensity *= intensity;
        alpha =(255.0 * opacity * intensity + 0.5) / (size * 2);
        return alpha;
     }
   else
     return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////

static void
evas_object_text_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y)
{
   int i, j;
   Evas_Object_Text *o;
   Evas_Object_Text_Item *it;
   const char vals[5][5] =
     {
	{0, 1, 2, 1, 0},
	{1, 3, 4, 3, 1},
	{2, 4, 5, 4, 2},
	{1, 3, 4, 3, 1},
	{0, 1, 2, 1, 0}
     };
   int sl = 0, st = 0;
   int shad_dst, shad_sz, dx, dy, haveshad;

   /* render object to surface with context, and offxet by x,y */
   o = (Evas_Object_Text *)(obj->object_data);
   evas_text_style_pad_get(o->cur.style, &sl, NULL, &st, NULL);
   ENFN->context_multiplier_unset(output, context);
   ENFN->context_render_op_set(output, context, obj->cur.render_op);
   /* FIXME: This clipping is just until we fix inset handling correctly. */
   ENFN->context_clip_clip(output, context,
                              obj->cur.geometry.x + x,
                              obj->cur.geometry.y + y,
                              obj->cur.geometry.w,
                              obj->cur.geometry.h);
/*
   ENFN->context_color_set(output,
                           context,
                           230, 160, 30, 100);
   ENFN->rectangle_draw(output,
                        context,
                        surface,
                        obj->cur.geometry.x + x,
                        obj->cur.geometry.y + y,
                        obj->cur.geometry.w,
                        obj->cur.geometry.h);
 */
#define COLOR_ONLY_SET(object, sub, col) \
	ENFN->context_color_set(output, context, \
				object->sub.col.r, \
				object->sub.col.g, \
				object->sub.col.b, \
				object->sub.col.a);

#define COLOR_SET(object, sub, col) \
        if (obj->cur.clipper)\
	   ENFN->context_color_set(output, context, \
				((int)object->sub.col.r * ((int)obj->cur.clipper->cur.cache.clip.r + 1)) >> 8, \
				((int)object->sub.col.g * ((int)obj->cur.clipper->cur.cache.clip.g + 1)) >> 8, \
				((int)object->sub.col.b * ((int)obj->cur.clipper->cur.cache.clip.b + 1)) >> 8, \
				((int)object->sub.col.a * ((int)obj->cur.clipper->cur.cache.clip.a + 1)) >> 8); \
        else\
	   ENFN->context_color_set(output, context, \
				object->sub.col.r, \
				object->sub.col.g, \
				object->sub.col.b, \
				object->sub.col.a);

#define COLOR_SET_AMUL(object, sub, col, amul) \
        if (obj->cur.clipper) \
	    ENFN->context_color_set(output, context, \
				(((int)object->sub.col.r) * ((int)obj->cur.clipper->cur.cache.clip.r) * (amul)) / 65025, \
				(((int)object->sub.col.g) * ((int)obj->cur.clipper->cur.cache.clip.g) * (amul)) / 65025, \
				(((int)object->sub.col.b) * ((int)obj->cur.clipper->cur.cache.clip.b) * (amul)) / 65025, \
				(((int)object->sub.col.a) * ((int)obj->cur.clipper->cur.cache.clip.a) * (amul)) / 65025); \
        else \
	    ENFN->context_color_set(output, context, \
				(((int)object->sub.col.r) * (amul)) / 255, \
				(((int)object->sub.col.g) * (amul)) / 255, \
				(((int)object->sub.col.b) * (amul)) / 255, \
				(((int)object->sub.col.a) * (amul)) / 255);

#define DRAW_TEXT(ox, oy) \
   if ((o->font) && (it->text_props.len > 0)) \
     ENFN->font_draw(output, \
		     context, \
		     surface, \
		     o->font, \
		     obj->cur.geometry.x + x + sl + ox + it->x, \
		     obj->cur.geometry.y + y + st + oy + \
		     (int) \
		     (((o->max_ascent * obj->cur.geometry.h) / obj->cur.geometry.h)), \
		     obj->cur.geometry.w, \
		     obj->cur.geometry.h, \
		     obj->cur.geometry.w, \
		     obj->cur.geometry.h, \
		     &it->text_props);

   /* shadows */
   shad_dst = shad_sz = dx = dy = haveshad = 0;
   switch (o->cur.style & EVAS_TEXT_STYLE_MASK_BASIC)
     {
      case EVAS_TEXT_STYLE_SHADOW:
      case EVAS_TEXT_STYLE_OUTLINE_SOFT_SHADOW:
         shad_dst = 1;
         haveshad = 1;
         break;
      case EVAS_TEXT_STYLE_OUTLINE_SHADOW:
      case EVAS_TEXT_STYLE_FAR_SHADOW:
         shad_dst = 2;
         haveshad = 1;
         break;
      case EVAS_TEXT_STYLE_FAR_SOFT_SHADOW:
         shad_dst = 2;
         shad_sz = 2;
         haveshad = 1;
         break;
      case EVAS_TEXT_STYLE_SOFT_SHADOW:
         shad_dst = 1;
         shad_sz = 2;
         haveshad = 1;
         break;
      default:
         break;
     }
   if (haveshad)
     {
        if (shad_dst > 0)
          {
             switch (o->cur.style & EVAS_TEXT_STYLE_MASK_SHADOW_DIRECTION)
               {
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_BOTTOM_RIGHT:
                   dx = 1;
                   dy = 1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_BOTTOM:
                   dx = 0;
                   dy = 1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_BOTTOM_LEFT:
                   dx = -1;
                   dy = 1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_LEFT:
                   dx = -1;
                   dy = 0;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_TOP_LEFT:
                   dx = -1;
                   dy = -1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_TOP:
                   dx = 0;
                   dy = -1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_TOP_RIGHT:
                   dx = 1;
                   dy = -1;
                   break;
                case EVAS_TEXT_STYLE_SHADOW_DIRECTION_RIGHT:
                   dx = 1;
                   dy = 0;
                default:
                   break;
               }
             dx *= shad_dst;
             dy *= shad_dst;
          }
     }
   EINA_INLIST_FOREACH(EINA_INLIST_GET(o->items), it)
     {
        // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
        if (it->emoticon_item)
          continue;
        //

        // HAVE_EMOJI
        it->text_props.have_emoji = 0;
        //

        /* Shadows */
        if (haveshad)
          {
             switch (shad_sz)
               {
                case 0:
                  COLOR_SET(o, cur, shadow);
                  DRAW_TEXT(dx, dy);
                  break;
                case 2:
                  for (j = 0; j < 5; j++)
                    {
                       for (i = 0; i < 5; i++)
                         {
                            if (vals[i][j] != 0)
                              {
                                 COLOR_SET_AMUL(o, cur, shadow, vals[i][j] * 50);
                                 DRAW_TEXT(i - 2 + dx, j - 2 + dy);
                              }
                         }
                    }
                  break;
                default:
                  break;
               }
          }

        /* glows */
        if (o->cur.style == EVAS_TEXT_STYLE_GLOW)
          {
             for (j = 0; j < 5; j++)
               {
                  for (i = 0; i < 5; i++)
                    {
                       if (vals[i][j] != 0)
                         {
                            COLOR_SET_AMUL(o, cur, glow, vals[i][j] * 50);
                            DRAW_TEXT(i - 2, j - 2);
                         }
                    }
               }
             COLOR_SET(o, cur, glow2);
             DRAW_TEXT(-1, 0);
             DRAW_TEXT(1, 0);
             DRAW_TEXT(0, -1);
             DRAW_TEXT(0, 1);
          }

        /* outlines */
        if ((o->cur.style == EVAS_TEXT_STYLE_OUTLINE) ||
              (o->cur.style == EVAS_TEXT_STYLE_OUTLINE_SHADOW) ||
              (o->cur.style == EVAS_TEXT_STYLE_OUTLINE_SOFT_SHADOW))
          {
             COLOR_SET(o, cur, outline);
             DRAW_TEXT(-1, 0);
             DRAW_TEXT(1, 0);
             DRAW_TEXT(0, -1);
             DRAW_TEXT(0, 1);
          }
        else if (o->cur.style == EVAS_TEXT_STYLE_SOFT_OUTLINE)
          {
             for (j = 0; j < 5; j++)
               {
                  for (i = 0; i < 5; i++)
                    {
                       if (((i != 2) || (j != 2)) && (vals[i][j] != 0))
                         {
                            COLOR_SET_AMUL(o, cur, outline, vals[i][j] * 50);
                            DRAW_TEXT(i - 2, j - 2);
                         }
                    }
               }
          }

        // TIZEN ONLY (20131106) : Font effect for tizen.
        if ((o->cur.style == EVAS_TEXT_STYLE_TIZEN_GLOW_SHADOW) ||
            (o->cur.style == EVAS_TEXT_STYLE_TIZEN_SOFT_GLOW_SHADOW) ||
            (o->cur.style == EVAS_TEXT_STYLE_TIZEN_SHADOW))
          {
             if (o->cur.style == EVAS_TEXT_STYLE_TIZEN_GLOW_SHADOW)
               {
                  for (j = -2; j <= 2; j++)
                    {
                       for (i = -2; i <= 2; i++)
                         {
                            if ((i != 0) && (j != 0))
                              {
                                 if(tizen_glow_shadow_outerglow[i*i + j*j] == 0) continue;
                                 COLOR_SET_AMUL(o, cur, glow, tizen_glow_shadow_outerglow[i*i + j*j]);
                            DRAW_TEXT(j, i);
                         }
                    }
                    }
                  for (j = -3; j <= 3; j++)
                    {
                       for (i = -3; i <= 3; i++)
                         {
                            if(tizen_glow_shadow_shadow[i*i + j*j] == 0) continue;
                            COLOR_SET_AMUL(o, cur, shadow, tizen_glow_shadow_shadow[i*i + j*j]);
                            DRAW_TEXT(j + 1, i + 1);
                         }
                    }
               }
             else if (o->cur.style == EVAS_TEXT_STYLE_TIZEN_SOFT_GLOW_SHADOW)
               {
                  for (j = -2; j <= 2; j++)
                    {
                       for (i = -2; i <= 2; i++)
                         {
                            if ((i != 0) && (j != 0))
                              {
                                 if(tizen_soft_glow_shadow_outerglow[i*i + j*j] == 0) continue;
                                 COLOR_SET_AMUL(o, cur, glow, tizen_soft_glow_shadow_outerglow[i*i + j*j]);
                                 DRAW_TEXT(j, i);
                              }
                         }
                    }
                  for (j = -3; j <= 3; j++)
                    {
                       for (i = -3; i <= 3; i++)
                         {
                            if(tizen_soft_glow_shadow_shadow[i*i + j*j] == 0) continue;
                            COLOR_SET_AMUL(o, cur, shadow, tizen_soft_glow_shadow_shadow[i*i + j*j]);
                            DRAW_TEXT(j + 1, i + 1);
                         }
                    }
               }
             else if (o->cur.style == EVAS_TEXT_STYLE_TIZEN_SHADOW)
               {
                  for (j = -3; j <= 3; j++)
                    {
                       for (i = -3; i <= 3; i++)
                         {
                            if(tizen_shadow_shadow[i*i + j*j] == 0) continue;
                            COLOR_SET_AMUL(o, cur, shadow, tizen_shadow_shadow[i*i + j*j]);
                            DRAW_TEXT(j, i + 2);
                         }
                    }
               }
          }
        //////////////////////

        // HAVE_EMOJI
        it->text_props.have_emoji = 1;
        //

        /* normal text */
        COLOR_ONLY_SET(obj, cur.cache, clip);
        DRAW_TEXT(0, 0);
     }

   // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
   _text_emoticon_render(obj, output, context, surface, x, y);
   //
}

static void
evas_object_text_render_pre(Evas_Object *obj)
{
   Evas_Object_Text *o;
   int is_v = 0, was_v = 0;
   /* dont pre-render the obj twice! */
   if (obj->pre_render_done) return;
   obj->pre_render_done = 1;
   /* pre-render phase. this does anything an object needs to do just before
    rendering. This could mean loading the image data, retrieving it from
    elsewhere, decoding video etc.
    Then when this is done the object needs to figure if it changed and
    if so what and where and add the appropriate redraw rectangles */
   o = (Evas_Object_Text *)(obj->object_data);
   /* if someone is clipping this obj - go calculate the clipper */
   if (obj->cur.clipper)
     {
	if (obj->cur.cache.clip.dirty)
	  evas_object_clip_recalc(obj->cur.clipper);
	obj->cur.clipper->func->render_pre(obj->cur.clipper);
     }
   /* If object size changed and ellipsis is set */
   if (((o->cur.ellipsis >= 0.0 ||
        o->cur.ellipsis != o->prev.ellipsis) &&
       ((obj->cur.geometry.w != o->last_computed.w) ||
        (obj->cur.geometry.h != o->last_computed.h))) ||
       (obj->cur.scale != obj->prev.scale))
     {
        _evas_object_text_recalc(obj, o->cur.text);
        evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes,
                                            obj);
	goto done;
     }
   /* now figure what changed and add draw rects
    if it just became visible or invisible */
   is_v = evas_object_is_visible(obj);
   was_v = evas_object_was_visible(obj);
   if (is_v != was_v)
     {
	evas_object_render_pre_visible_change(&obj->layer->evas->clip_changes,
					      obj, is_v, was_v);
	goto done;
     }
   if (obj->changed_map || obj->changed_src_visible)
     {
        evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes,
                                            obj);
        goto done;
     }
   /* its not visible - we accounted for it appearing or not so just abort */
   if (!is_v) goto done;
   /* clipper changed this is in addition to anything else for obj */
   evas_object_render_pre_clipper_change(&obj->layer->evas->clip_changes, obj);
   /* if we restacked (layer or just within a layer) and dont clip anyone */
   if (obj->restack)
     {
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes,
					    obj);
	goto done;
     }
   /* if it changed color */
   if ((obj->cur.color.r != obj->prev.color.r) ||
       (obj->cur.color.g != obj->prev.color.g) ||
       (obj->cur.color.b != obj->prev.color.b) ||
       (obj->cur.color.a != obj->prev.color.a))
     {
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes,
					    obj);
	goto done;
     }
   /* if it changed geometry - and obviously not visibility or color
    calculate differences since we have a constant color fill
    we really only need to update the differences */
   if ((obj->cur.geometry.x != obj->prev.geometry.x) ||
       (obj->cur.geometry.y != obj->prev.geometry.y) ||
       (obj->cur.geometry.w != obj->prev.geometry.w) ||
       (obj->cur.geometry.h != obj->prev.geometry.h))
     {
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes,
					    obj);
	goto done;
     }
   if (obj->cur.render_op != obj->prev.render_op)
     {
	evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes,
					    obj);
	goto done;
     }
   if (o->changed)
     {
        evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes,
                                            obj);
        goto done;
     }
   done:
   // HAVE_UNICODE_EMOTICON(2013.10.07): Basic implementation for supporting unicode 6.0 emoticon.
   _evas_object_text_emoticon_update_geometry(obj);
   //
   evas_object_render_pre_effect_updates(&obj->layer->evas->clip_changes,
					 obj, is_v, was_v);
}

static void
evas_object_text_render_post(Evas_Object *obj)
{
   Evas_Object_Text *o;

   /* this moves the current data to the previous state parts of the object
    in whatever way is safest for the object. also if we don't need object
    data anymore we can free it if the object deems this is a good idea */
   o = (Evas_Object_Text *)(obj->object_data);
   /* remove those pesky changes */
   evas_object_clip_changes_clean(obj);
   /* move cur to prev safely for object data */
   evas_object_cur_prev(obj);
   /* o->prev = o->cur; */
   o->changed = 0;
}

static unsigned int
evas_object_text_id_get(Evas_Object *obj)
{
   Evas_Object_Text *o;

   o = (Evas_Object_Text *)(obj->object_data);
   if (!o) return 0;
   return MAGIC_OBJ_TEXT;
}

static unsigned int
evas_object_text_visual_id_get(Evas_Object *obj)
{
   Evas_Object_Text *o;

   o = (Evas_Object_Text *)(obj->object_data);
   if (!o) return 0;
   return MAGIC_OBJ_SHAPE;
}

static void *
evas_object_text_engine_data_get(Evas_Object *obj)
{
   Evas_Object_Text *o;

   o = (Evas_Object_Text *)(obj->object_data);
   if (!o) return NULL;
   return o->font;
}

static int
evas_object_text_is_opaque(Evas_Object *obj __UNUSED__)
{
   /* this returns 1 if the internal object data implies that the object is
    currently fully opaque over the entire gradient it occupies */
   return 0;
}

static int
evas_object_text_was_opaque(Evas_Object *obj __UNUSED__)
{
   /* this returns 1 if the internal object data implies that the object was
    currently fully opaque over the entire gradient it occupies */
   return 0;
}

static void
evas_object_text_scale_update(Evas_Object *obj)
{
   Evas_Object_Text *o;
   int size;
   const char *font;

   o = (Evas_Object_Text *)(obj->object_data);
   font = eina_stringshare_add(o->cur.font);
   size = o->cur.size;
   if (o->cur.font) eina_stringshare_del(o->cur.font);
   o->cur.font = NULL;
   o->prev.font = NULL;
   o->cur.size = 0;
   o->prev.size = 0;
   evas_object_text_font_set(obj, font, size);
}

void
_evas_object_text_rehint(Evas_Object *obj)
{
   Evas_Object_Text *o;
   int is, was;

   o = (Evas_Object_Text *)(obj->object_data);
   if (!o->font) return;
   evas_font_load_hinting_set(obj->layer->evas, o->font,
			      obj->layer->evas->hinting);
   was = evas_object_is_in_output_rect(obj,
				       obj->layer->evas->pointer.x,
				       obj->layer->evas->pointer.y, 1, 1);
   /* DO II */
   _evas_object_text_recalc(obj, o->cur.text);
   o->changed = 1;
   evas_object_change(obj);
   evas_object_clip_dirty(obj);
   evas_object_coords_recalc(obj);
   is = evas_object_is_in_output_rect(obj,
				      obj->layer->evas->pointer.x,
				      obj->layer->evas->pointer.y, 1, 1);
   if ((is || was) && obj->cur.visible)
     evas_event_feed_mouse_move(obj->layer->evas,
				obj->layer->evas->pointer.x,
				obj->layer->evas->pointer.y,
				obj->layer->evas->last_timestamp,
				NULL);
   evas_object_inform_call_resize(obj);
}

static void
_evas_object_text_recalc(Evas_Object *obj, Eina_Unicode *text)
{
   Evas_Object_Text *o;
   o = (Evas_Object_Text *)(obj->object_data);

   if (!text) text = eina_unicode_strdup(EINA_UNICODE_EMPTY_STRING);

   _evas_object_text_layout(obj, o, text);

   /* Calc ascent/descent. */
     {
        Evas_Object_Text_Item *item;

        for (item = o->items ; item ;
              item = EINA_INLIST_CONTAINER_GET(
                 EINA_INLIST_GET(item)->next, Evas_Object_Text_Item))
          {
             int asc = 0, desc = 0;
             int max_asc = 0, max_desc = 0;

             /* Skip items without meaning full information. */
             if (!item->text_props.font_instance)
                continue;

             asc = evas_common_font_instance_ascent_get(item->text_props.font_instance);
             desc = evas_common_font_instance_descent_get(item->text_props.font_instance);

             // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
             if (item->emoticon_item)
               asc = item->h - desc;
             //

             if (asc > o->ascent)
                o->ascent = asc;
             if (desc > o->descent)
                o->descent = desc;

             max_asc = evas_common_font_instance_max_ascent_get(item->text_props.font_instance);
             max_desc = evas_common_font_instance_max_descent_get(item->text_props.font_instance);

             // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
             if (item->emoticon_item)
               {
                  max_asc = asc;
                  max_desc = desc;
               }
             //

             if (max_asc > o->max_ascent)
                o->max_ascent = max_asc;
             if (max_desc > o->max_descent)
                o->max_descent = max_desc;

             // HAVE_UNICODE_EMOTICON(2013.12.06): refactored emoticon drawing.
             if (item->emoticon_item)
               _evas_object_text_emoticon_update_size(o, item);
             //
          }
     }

   if ((o->font) && (o->items))
     {
	int w, h;
	int l = 0, r = 0, t = 0, b = 0;

        w = _evas_object_text_horiz_advance_get(o);
        h = _evas_object_text_vert_advance_get(obj, o);
	evas_text_style_pad_get(o->cur.style, &l, &r, &t, &b);

        if (o->cur.ellipsis >= 0.0)
	  {
             obj->cur.geometry.w = w + l + r < obj->cur.geometry.w || obj->cur.geometry.w == 0 ?
               w + l + r : obj->cur.geometry.w;
          }
	else
	  {
             obj->cur.geometry.w = w + l + r;
	  }
        obj->cur.geometry.h = h + t + b;
////        obj->cur.cache.geometry.validity = 0;
     }
   else
     {
	int t = 0, b = 0;

	evas_text_style_pad_get(o->cur.style, NULL, NULL, &t, &b);
	obj->cur.geometry.w = 0;
        obj->cur.geometry.h = o->max_ascent + o->max_descent + t + b;
////        obj->cur.cache.geometry.validity = 0;
     }
   o->last_computed.w = obj->cur.geometry.w;
   o->last_computed.h = obj->cur.geometry.h;
   evas_object_clip_dirty(obj);
}
