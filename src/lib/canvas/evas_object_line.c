#include "evas_common.h"
#include "evas_private.h"

/* private magic number for line objects */
static const char o_type[] = "line";

/* private struct for line object internal data */
typedef struct _Evas_Object_Line      Evas_Object_Line;

struct _Evas_Object_Line
{
   DATA32            magic;
   struct {
      struct {
         Evas_Coord    x1, y1, x2, y2;
         struct {
            Evas_Coord w, h;
         } object;
      } cache;
      Evas_Coord     x1, y1, x2, y2;
   } cur, prev;

   void             *engine_data;
   Eina_Bool         changed : 1;
};

/* private methods for line objects */
static void evas_object_line_init(Evas_Object *obj);
static void *evas_object_line_new(void);
static void evas_object_line_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y);
static void evas_object_line_free(Evas_Object *obj);
static void evas_object_line_render_pre(Evas_Object *obj);
static void evas_object_line_render_post(Evas_Object *obj);

static unsigned int evas_object_line_id_get(Evas_Object *obj);
static unsigned int evas_object_line_visual_id_get(Evas_Object *obj);
static void *evas_object_line_engine_data_get(Evas_Object *obj);

static int evas_object_line_is_opaque(Evas_Object *obj);
static int evas_object_line_was_opaque(Evas_Object *obj);
static int evas_object_line_is_inside(Evas_Object *obj, Evas_Coord x, Evas_Coord y);
static int evas_object_line_was_inside(Evas_Object *obj, Evas_Coord x, Evas_Coord y);
static void evas_object_line_coords_recalc(Evas_Object *obj);

static const Evas_Object_Func object_func =
{
   /* methods (compulsory) */
   evas_object_line_free,
   evas_object_line_render,
   evas_object_line_render_pre,
   evas_object_line_render_post,
   evas_object_line_id_get,
   evas_object_line_visual_id_get,
   evas_object_line_engine_data_get,
   /* these are optional. NULL = nothing */
   NULL,
   NULL,
   NULL,
   NULL,
   evas_object_line_is_opaque,
   evas_object_line_was_opaque,
   evas_object_line_is_inside,
   evas_object_line_was_inside,
   evas_object_line_coords_recalc,
   NULL,
   NULL,
   NULL,
   NULL
};

/* the actual api call to add a rect */
/* it has no other api calls as all properties are standard */

EVAS_MEMPOOL(_mp_obj);

EAPI Evas_Object *
evas_object_line_add(Evas *e)
{
   Evas_Object *obj;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return NULL;
   MAGIC_CHECK_END();
   obj = evas_object_new(e);
   evas_object_line_init(obj);
   evas_object_inject(obj, e);
   return obj;
}

EAPI void
evas_object_line_xy_set(Evas_Object *obj, Evas_Coord x1, Evas_Coord y1, Evas_Coord x2, Evas_Coord y2)
{
   Evas_Object_Line *o;
   Evas_Coord min_x, max_x, min_y, max_y;
   int is, was = 0;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Line *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Line, MAGIC_OBJ_LINE);
   return;
   MAGIC_CHECK_END();
   if ((x1 == o->cur.x1) && (y1 == o->cur.y1) &&
       (x2 == o->cur.x2) && (y2 == o->cur.y2)) return;
   if (obj->layer->evas->events_frozen <= 0)
     {
        if (!evas_event_passes_through(obj) &&
            !evas_event_freezes_through(obj))
          was = evas_object_is_in_output_rect(obj,
                                              obj->layer->evas->pointer.x,
                                              obj->layer->evas->pointer.y,
                                              1, 1);
     }
   if (x1 < x2)
     {
        min_x = x1;
        max_x = x2;
     }
   else
     {
        min_x = x2;
        max_x = x1;
     }
   if (y1 < y2)
     {
        min_y = y1;
        max_y = y2;
     }
   else
     {
        min_y = y2;
        max_y = y1;
     }
   obj->cur.geometry.x = min_x;
   obj->cur.geometry.y = min_y;
   obj->cur.geometry.w = max_x - min_x + 2;
   obj->cur.geometry.h = max_y - min_y + 2;
////   obj->cur.cache.geometry.validity = 0;
   o->cur.x1 = x1 - min_x;
   o->cur.y1 = y1 - min_y;
   o->cur.x2 = x2 - min_x;
   o->cur.y2 = y2 - min_y;
   o->changed = EINA_TRUE;
   evas_object_change(obj);
   evas_object_coords_recalc(obj);
   evas_object_clip_dirty(obj);
   if (obj->layer->evas->events_frozen <= 0)
     {
        is = evas_object_is_in_output_rect(obj,
                                           obj->layer->evas->pointer.x,
                                           obj->layer->evas->pointer.y, 1, 1);
        if (!evas_event_passes_through(obj) &&
            !evas_event_freezes_through(obj))
          {
             if ((is ^ was) && obj->cur.visible)
               evas_event_feed_mouse_move(obj->layer->evas,
                                          obj->layer->evas->pointer.x,
                                          obj->layer->evas->pointer.y,
                                          obj->layer->evas->last_timestamp,
                                          NULL);
          }
     }
   evas_object_inform_call_move(obj);
   evas_object_inform_call_resize(obj);
}

EAPI void
evas_object_line_xy_get(const Evas_Object *obj, Evas_Coord *x1, Evas_Coord *y1, Evas_Coord *x2, Evas_Coord *y2)
{
   Evas_Object_Line *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (x1) *x1 = 0;
   if (y1) *y1 = 0;
   if (x2) *x2 = 0;
   if (y2) *y2 = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Line *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Line, MAGIC_OBJ_LINE);
   if (x1) *x1 = 0;
   if (y1) *y1 = 0;
   if (x2) *x2 = 0;
   if (y2) *y2 = 0;
   return;
   MAGIC_CHECK_END();
   if (x1) *x1 = obj->cur.geometry.x + o->cur.x1;
   if (y1) *y1 = obj->cur.geometry.y + o->cur.y1;
   if (x2) *x2 = obj->cur.geometry.x + o->cur.x2;
   if (y2) *y2 = obj->cur.geometry.y + o->cur.y2;
}

/* all nice and private */
static void
evas_object_line_init(Evas_Object *obj)
{
   /* alloc image ob, setup methods and default values */
   obj->object_data = evas_object_line_new();
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
   obj->cur.anti_alias = EINA_TRUE;
   obj->cur.render_op = EVAS_RENDER_BLEND;
   /* set up object-specific settings */
   obj->prev = obj->cur;
   /* set up methods (compulsory) */
   obj->func = &object_func;
   obj->type = o_type;
}

static void *
evas_object_line_new(void)
{
   Evas_Object_Line *o;

   /* alloc obj private data */
   EVAS_MEMPOOL_INIT(_mp_obj, "evas_object_line", Evas_Object_Line, 4, NULL);
   o = EVAS_MEMPOOL_ALLOC(_mp_obj, Evas_Object_Line);
   if (!o) return NULL;
   EVAS_MEMPOOL_PREP(_mp_obj, o, Evas_Object_Line);
   o->magic = MAGIC_OBJ_LINE;
   o->cur.x1 = 0;
   o->cur.y1 = 0;
   o->cur.x2 = 31;
   o->cur.y2 = 31;
   o->prev = o->cur;
   return o;
}

static void
evas_object_line_free(Evas_Object *obj)
{
   Evas_Object_Line *o;

   /* frees private object data. very simple here */
   o = (Evas_Object_Line *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Line, MAGIC_OBJ_LINE);
   return;
   MAGIC_CHECK_END();
   /* free obj */
   o->magic = 0;
   EVAS_MEMPOOL_FREE(_mp_obj, o);
}

static void
evas_object_line_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y)
{
   Evas_Object_Line *o;

   /* render object to surface with context, and offxet by x,y */
   o = (Evas_Object_Line *)(obj->object_data);
   obj->layer->evas->engine.func->context_color_set(output,
                                                    context,
                                                    obj->cur.cache.clip.r,
                                                    obj->cur.cache.clip.g,
                                                    obj->cur.cache.clip.b,
                                                    obj->cur.cache.clip.a);
   obj->layer->evas->engine.func->context_multiplier_unset(output,
                                                           context);
   obj->layer->evas->engine.func->context_anti_alias_set(output, context,
                                                         obj->cur.anti_alias);
   obj->layer->evas->engine.func->context_render_op_set(output, context,
                                                        obj->cur.render_op);
   obj->layer->evas->engine.func->line_draw(output,
                                            context,
                                            surface,
                                            o->cur.cache.x1 + x,
                                            o->cur.cache.y1 + y,
                                            o->cur.cache.x2 + x,
                                            o->cur.cache.y2 + y);
}

static void
evas_object_line_render_pre(Evas_Object *obj)
{
   Evas_Object_Line *o;
   int is_v, was_v;
   Eina_Bool changed_color = EINA_FALSE;

   /* dont pre-render the obj twice! */
   if (obj->pre_render_done) return;
   obj->pre_render_done = EINA_TRUE;
   /* pre-render phase. this does anything an object needs to do just before */
   /* rendering. this could mean loading the image data, retrieving it from */
   /* elsewhere, decoding video etc. */
   /* then when this is done the object needs to figure if it changed and */
   /* if so what and where and add the appropriate redraw lines */
   o = (Evas_Object_Line *)(obj->object_data);
   /* if someone is clipping this obj - go calculate the clipper */
   if (obj->cur.clipper)
     {
        if (obj->cur.cache.clip.dirty)
          evas_object_clip_recalc(obj->cur.clipper);
        obj->cur.clipper->func->render_pre(obj->cur.clipper);
     }
   /* now figure what changed and add draw rects */
   /* if it just became visible or invisible */
   is_v = evas_object_is_visible(obj);
   was_v = evas_object_was_visible(obj);
   if (is_v != was_v)
     {
        evas_object_render_pre_visible_change(&obj->layer->evas->clip_changes,
                                              obj, is_v, was_v);
        goto done;
     }
   if (obj->changed_map)
     {
        evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes,
                                            obj);
        goto done;
     }
   /* it's not visible - we accounted for it appearing or not so just abort */
   if (!is_v) goto done;
   /* clipper changed this is in addition to anything else for obj */
   evas_object_render_pre_clipper_change(&obj->layer->evas->clip_changes, obj);

   if ((obj->cur.color.r != obj->prev.color.r) ||
       (obj->cur.color.g != obj->prev.color.g) ||
       (obj->cur.color.b != obj->prev.color.b) ||
       (obj->cur.color.a != obj->prev.color.a))
     changed_color = EINA_TRUE;

   /* if we restacked (layer or just within a layer) */
   /* or if it changed anti_alias */
   /* or if ii changed render op */
   /* or if it changed color */
   if ((obj->restack) ||
       (obj->cur.anti_alias != obj->prev.anti_alias) ||
       (obj->cur.render_op != obj->prev.render_op) ||
       (changed_color)
      )
     {
        evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes,
                                            obj);
        goto done;
     }

   /* if it changed geometry - and obviously not visibility or color */
   /* calculate differences since we have a constant color fill */
   /* we really only need to update the differences */
   if ((obj->cur.geometry.x != obj->prev.geometry.x) ||
       (obj->cur.geometry.y != obj->prev.geometry.y) ||
       (obj->cur.geometry.w != obj->prev.geometry.w) ||
       (obj->cur.geometry.h != obj->prev.geometry.h) ||
       ((o->changed) &&
        ((o->cur.x1 != o->prev.x1) ||
         (o->cur.y1 != o->prev.y1) ||
         (o->cur.x2 != o->prev.x2) ||
         (o->cur.y2 != o->prev.y2)))
      )
     {
        evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes,
                                            obj);
        goto done;
     }

done:
   evas_object_render_pre_effect_updates(&obj->layer->evas->clip_changes, obj,
                                         is_v, was_v);
}

static void
evas_object_line_render_post(Evas_Object *obj)
{
   Evas_Object_Line *o;

   /* this moves the current data to the previous state parts of the object */
   /* in whatever way is safest for the object. also if we don't need object */
   /* data anymore we can free it if the object deems this is a good idea */
   o = (Evas_Object_Line *)(obj->object_data);
   /* remove those pesky changes */
   evas_object_clip_changes_clean(obj);
   /* move cur to prev safely for object data */
   evas_object_cur_prev(obj);
   o->prev = o->cur;
   o->changed = EINA_FALSE;
}

static unsigned int evas_object_line_id_get(Evas_Object *obj)
{
   Evas_Object_Line *o;

   o = (Evas_Object_Line *)(obj->object_data);
   if (!o) return 0;
   return MAGIC_OBJ_LINE;
}

static unsigned int evas_object_line_visual_id_get(Evas_Object *obj)
{
   Evas_Object_Line *o;

   o = (Evas_Object_Line *)(obj->object_data);
   if (!o) return 0;
   return MAGIC_OBJ_SHAPE;
}

static void *evas_object_line_engine_data_get(Evas_Object *obj)
{
   Evas_Object_Line *o;

   o = (Evas_Object_Line *)(obj->object_data);
   if (!o) return NULL;
   return o->engine_data;
}

static int
evas_object_line_is_opaque(Evas_Object *obj __UNUSED__)
{
   /* this returns 1 if the internal object data implies that the object is */
   /* currently fully opaque over the entire line it occupies */
   return 0;
}

static int
evas_object_line_was_opaque(Evas_Object *obj __UNUSED__)
{
   /* this returns 1 if the internal object data implies that the object was */
   /* previously fully opaque over the entire line it occupies */
   return 0;
}

static int
evas_object_line_is_inside(Evas_Object *obj __UNUSED__, Evas_Coord x __UNUSED__, Evas_Coord y __UNUSED__)
{
   /* this returns 1 if the canvas co-ordinates are inside the object based */
   /* on object private data. not much use for rects, but for polys, images */
   /* and other complex objects it might be */
   return 1;
}

static int
evas_object_line_was_inside(Evas_Object *obj __UNUSED__, Evas_Coord x __UNUSED__, Evas_Coord y __UNUSED__)
{
   /* this returns 1 if the canvas co-ordinates were inside the object based */
   /* on object private data. not much use for rects, but for polys, images */
   /* and other complex objects it might be */
   return 1;
}

static void
evas_object_line_coords_recalc(Evas_Object *obj)
{
   Evas_Object_Line *o;

   o = (Evas_Object_Line *)(obj->object_data);
   o->cur.cache.x1 = obj->cur.geometry.x + o->cur.x1;
   o->cur.cache.y1 = obj->cur.geometry.y + o->cur.y1;
   o->cur.cache.x2 = obj->cur.geometry.x + o->cur.x2;
   o->cur.cache.y2 = obj->cur.geometry.y + o->cur.y2;
   o->cur.cache.object.w = obj->cur.geometry.w;
   o->cur.cache.object.h = obj->cur.geometry.h;
}
