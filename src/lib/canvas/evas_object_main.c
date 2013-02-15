#include "evas_common.h"
#include "evas_private.h"

EVAS_MEMPOOL(_mp_obj);
EVAS_MEMPOOL(_mp_sh);

static Eina_Inlist *
get_layer_objects(Evas_Layer *l)
{
   if ((!l) || (!l->objects)) return NULL;
   return (EINA_INLIST_GET(l->objects));
}

/* evas internal stuff */
Evas_Object *
evas_object_new(Evas *e __UNUSED__)
{
   Evas_Object *obj;

   EVAS_MEMPOOL_INIT(_mp_obj, "evas_object", Evas_Object, 32, NULL);
   obj = EVAS_MEMPOOL_ALLOC(_mp_obj, Evas_Object);
   if (!obj) return NULL;
   EVAS_MEMPOOL_PREP(_mp_obj, obj, Evas_Object);

   obj->magic = MAGIC_OBJ;
   obj->cur.scale = 1.0;
   obj->prev.scale = 1.0;
   obj->is_frame = EINA_FALSE;

   return obj;
}

void
evas_object_change_reset(Evas_Object *obj)
{
   obj->changed = EINA_FALSE;
   obj->changed_move = EINA_FALSE;
   obj->changed_color = EINA_FALSE;
   obj->changed_map = EINA_FALSE;
   obj->changed_pchange = EINA_FALSE;
}

void
evas_object_cur_prev(Evas_Object *obj)
{
   if (!obj->prev.valid_map)
     {
        if (obj->prev.map != obj->cur.map)
          evas_map_free(obj->prev.map);
        if (obj->cache_map == obj->prev.map)
          obj->cache_map = NULL;
        obj->prev.map = NULL;
     }

   if (obj->cur.map != obj->prev.map)
     {
        if (obj->cache_map) evas_map_free(obj->cache_map);
        obj->cache_map = obj->prev.map;
     }
   obj->prev = obj->cur;
}

void
evas_object_free(Evas_Object *obj, int clean_layer)
{
   int was_smart_child = 0;

#if 0 // filtering disabled
   evas_filter_free(obj);
#endif
   if (!strcmp(obj->type, "image")) evas_object_image_video_surface_set(obj, NULL);
   evas_object_map_set(obj, NULL);
   if (obj->prev.map) evas_map_free(obj->prev.map);
   if (obj->cache_map) evas_map_free(obj->cache_map);
   if (obj->map.surface)
     {
        obj->layer->evas->engine.func->image_map_surface_free
          (obj->layer->evas->engine.data.output,
              obj->map.surface);
        obj->map.surface = NULL;
     }
   evas_object_grabs_cleanup(obj);
   evas_object_intercept_cleanup(obj);
   if (obj->smart.parent) was_smart_child = 1;
   evas_object_smart_cleanup(obj);
   obj->func->free(obj);
   if (!was_smart_child) evas_object_release(obj, clean_layer);
   if (obj->clip.clipees)
     eina_list_free(obj->clip.clipees);
   evas_object_clip_changes_clean(obj);
   evas_object_event_callback_all_del(obj);
   evas_object_event_callback_cleanup(obj);
   if (obj->spans)
     {
        free(obj->spans);
        obj->spans = NULL;
     }
   while (obj->data.elements)
     {
        Evas_Data_Node *node;

        node = obj->data.elements->data;
        obj->data.elements = eina_list_remove(obj->data.elements, node);
        free(node);
     }
   obj->magic = 0;
   if (obj->size_hints)
     {
       EVAS_MEMPOOL_FREE(_mp_sh, obj->size_hints);
     }
   EVAS_MEMPOOL_FREE(_mp_obj, obj);
}

void
evas_object_change(Evas_Object *obj)
{
   Eina_List *l;
   Evas_Object *obj2;
   Eina_Bool movch = EINA_FALSE;

   if (obj->layer->evas->nochange) return;
   obj->layer->evas->changed = EINA_TRUE;

   if (obj->changed_move)
     {
        movch = EINA_TRUE;
        obj->changed_move = EINA_FALSE;
     }

   if (obj->changed) return;

   evas_render_object_recalc(obj);
   /* set changed flag on all objects this one clips too */
   if (!((movch) && (obj->is_static_clip)))
     {
        EINA_LIST_FOREACH(obj->clip.clipees, l, obj2)
          evas_object_change(obj2);
     }
   EINA_LIST_FOREACH(obj->proxy.proxies, l, obj2)
     {
        evas_object_change(obj2);
     }
   if (obj->smart.parent) evas_object_change(obj->smart.parent);
}

void
evas_object_render_pre_visible_change(Eina_Array *rects, Evas_Object *obj, int is_v, int was_v)
{
   if (obj->smart.smart) return;
   if (is_v == was_v) return;
   if (is_v)
     {
        evas_add_rect(rects,
                      obj->cur.cache.clip.x,
                      obj->cur.cache.clip.y,
                      obj->cur.cache.clip.w,
                      obj->cur.cache.clip.h);
     }
   else
     {
        evas_add_rect(rects,
                      obj->prev.cache.clip.x,
                      obj->prev.cache.clip.y,
                      obj->prev.cache.clip.w,
                      obj->prev.cache.clip.h);
     }
}

void
evas_object_render_pre_clipper_change(Eina_Array *rects, Evas_Object *obj)
{
   if (obj->smart.smart) return;
   if (obj->cur.clipper == obj->prev.clipper) return;
   if ((obj->cur.clipper) && (obj->prev.clipper))
     {
        /* get difference rects between clippers */
        evas_rects_return_difference_rects(rects,
                                           obj->cur.clipper->cur.cache.clip.x,
                                           obj->cur.clipper->cur.cache.clip.y,
                                           obj->cur.clipper->cur.cache.clip.w,
                                           obj->cur.clipper->cur.cache.clip.h,
                                           obj->prev.clipper->prev.cache.clip.x,
                                           obj->prev.clipper->prev.cache.clip.y,
                                           obj->prev.clipper->prev.cache.clip.w,
                                           obj->prev.clipper->prev.cache.clip.h);
     }
   else if (obj->cur.clipper)
     {
        evas_rects_return_difference_rects(rects,
                                           obj->cur.geometry.x,
                                           obj->cur.geometry.y,
                                           obj->cur.geometry.w,
                                           obj->cur.geometry.h,
////	rl = evas_rects_return_difference_rects(obj->cur.cache.geometry.x,
////						obj->cur.cache.geometry.y,
////						obj->cur.cache.geometry.w,
////						obj->cur.cache.geometry.h,
                                           obj->cur.clipper->cur.cache.clip.x,
                                           obj->cur.clipper->cur.cache.clip.y,
                                           obj->cur.clipper->cur.cache.clip.w,
                                           obj->cur.clipper->cur.cache.clip.h);
     }
   else if (obj->prev.clipper)
     {
     evas_rects_return_difference_rects(rects,
                                        obj->prev.geometry.x,
                                        obj->prev.geometry.y,
                                        obj->prev.geometry.w,
                                        obj->prev.geometry.h,
////	rl = evas_rects_return_difference_rects(obj->prev.cache.geometry.x,
////						obj->prev.cache.geometry.y,
////						obj->prev.cache.geometry.w,
////						obj->prev.cache.geometry.h,
                                        obj->prev.clipper->prev.cache.clip.x,
                                        obj->prev.clipper->prev.cache.clip.y,
                                        obj->prev.clipper->prev.cache.clip.w,
                                        obj->prev.clipper->prev.cache.clip.h);
     }
}

void
evas_object_render_pre_prev_cur_add(Eina_Array *rects, Evas_Object *obj)
{
   evas_add_rect(rects,
                 obj->cur.cache.clip.x,
                 obj->cur.cache.clip.y,
                 obj->cur.cache.clip.w,
                 obj->cur.cache.clip.h);
   evas_add_rect(rects,
                 obj->prev.cache.clip.x,
                 obj->prev.cache.clip.y,
                 obj->prev.cache.clip.w,
                 obj->prev.cache.clip.h);
/*
        evas_add_rect(rects,
                      obj->cur.geometry.x,
                      obj->cur.geometry.y,
                      obj->cur.geometry.w,
                      obj->cur.geometry.h);
////	    obj->cur.cache.geometry.x,
////	    obj->cur.cache.geometry.y,
////	    obj->cur.cache.geometry.w,
////	    obj->cur.cache.geometry.h);
        evas_add_rect(rects,
                      obj->prev.geometry.x,
                      obj->prev.geometry.y,
                      obj->prev.geometry.w,
                      obj->prev.geometry.h);
////	    obj->prev.cache.geometry.x,
////	    obj->prev.cache.geometry.y,
////	    obj->prev.cache.geometry.w,
////	    obj->prev.cache.geometry.h);
*/
}

void
evas_object_clip_changes_clean(Evas_Object *obj)
{
   Eina_Rectangle *r;

   EINA_LIST_FREE(obj->clip.changes, r)
     eina_rectangle_free(r);
}

void
evas_object_render_pre_effect_updates(Eina_Array *rects, Evas_Object *obj, int is_v, int was_v __UNUSED__)
{
   Eina_Rectangle *r;
   Evas_Object *clipper;
   Eina_List *l;
   unsigned int i;
   Eina_Array_Iterator it;
   int x, y, w, h;

   if (obj->smart.smart) goto end;
   /* FIXME: was_v isn't used... why? */
   if (!obj->clip.clipees)
     {
        EINA_ARRAY_ITER_NEXT(rects, i, r, it)
          {
             /* get updates and clip to current clip */
             x = r->x;
             y = r->y;
             w = r->w;
             h = r->h;
             RECTS_CLIP_TO_RECT(x, y, w, h,
                                obj->cur.cache.clip.x,
                                obj->cur.cache.clip.y,
                                obj->cur.cache.clip.w,
                                obj->cur.cache.clip.h);
             if ((w > 0) && (h > 0))
               obj->layer->evas->engine.func->output_redraws_rect_add(obj->layer->evas->engine.data.output,
                                                                      x, y, w, h);
             /* get updates and clip to previous clip */
             x = r->x;
             y = r->y;
             w = r->w;
             h = r->h;
             RECTS_CLIP_TO_RECT(x, y, w, h,
                                obj->prev.cache.clip.x,
                                obj->prev.cache.clip.y,
                                obj->prev.cache.clip.w,
                                obj->prev.cache.clip.h);
             if ((w > 0) && (h > 0))
               obj->layer->evas->engine.func->output_redraws_rect_add(obj->layer->evas->engine.data.output,
                                                                      x, y, w, h);
          }
        /* if the object is actually visible, take any parent clip changes */
        if (is_v)
          {
             clipper = obj->cur.clipper;
             while (clipper)
               {
                  EINA_LIST_FOREACH(clipper->clip.changes, l, r)
                    {
                       /* get updates and clip to current clip */
                       x = r->x; y = r->y; w = r->w; h = r->h;
                       RECTS_CLIP_TO_RECT(x, y, w, h,
                                          obj->cur.cache.clip.x,
                                          obj->cur.cache.clip.y,
                                          obj->cur.cache.clip.w,
                                          obj->cur.cache.clip.h);
                       if ((w > 0) && (h > 0))
                         obj->layer->evas->engine.func->output_redraws_rect_add(obj->layer->evas->engine.data.output,
                                                                                x, y, w, h);
                       /* get updates and clip to previous clip */
                       x = r->x; y = r->y; w = r->w; h = r->h;
                       RECTS_CLIP_TO_RECT(x, y, w, h,
                                          obj->prev.cache.clip.x,
                                          obj->prev.cache.clip.y,
                                          obj->prev.cache.clip.w,
                                          obj->prev.cache.clip.h);
                       if ((w > 0) && (h > 0))
                         obj->layer->evas->engine.func->output_redraws_rect_add(obj->layer->evas->engine.data.output,
                                                                                x, y, w, h);
                    }
                  clipper = clipper->cur.clipper;
               }
          }
     }
   else
     {
        evas_object_clip_changes_clean(obj);
        EINA_ARRAY_ITER_NEXT(rects, i, r, it)
           obj->clip.changes = eina_list_append(obj->clip.changes, r);
        eina_array_clean(rects);
     }

 end:
   EINA_ARRAY_ITER_NEXT(rects, i, r, it)
     eina_rectangle_free(r);
   eina_array_clean(rects);
}

int
evas_object_was_in_output_rect(Evas_Object *obj, int x, int y, int w, int h)
{
   if (obj->smart.smart && !obj->prev.map && !obj->prev.usemap) return 0;
   /* assumes coords have been recalced */
   if ((RECTS_INTERSECT(x, y, w, h,
                        obj->prev.cache.clip.x,
                        obj->prev.cache.clip.y,
                        obj->prev.cache.clip.w,
                        obj->prev.cache.clip.h)))
     return 1;
   return 0;
}

int
evas_object_was_opaque(Evas_Object *obj)
{
   if (obj->smart.smart) return 0;
   if (obj->prev.cache.clip.a == 255)
     {
        if (obj->func->was_opaque)
          return obj->func->was_opaque(obj);
        return 1;
     }
   return 0;
}

int
evas_object_is_inside(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   if (obj->smart.smart) return 0;
   if (obj->func->is_inside)
     return obj->func->is_inside(obj, x, y);
   return 0;
}

int
evas_object_was_inside(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   if (obj->smart.smart) return 0;
   if (obj->func->was_inside)
     return obj->func->was_inside(obj, x, y);
   return 0;
}
/* routines apps will call */

EAPI void
evas_object_ref(Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   obj->ref++;
}

EAPI void
evas_object_unref(Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->ref == 0) return;
   obj->ref--;
   if ((obj->del_ref) && (obj->ref == 0)) evas_object_del(obj);
}

EAPI int
evas_object_ref_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   return obj->ref;
}

EAPI void
evas_object_del(Evas_Object *obj)
{
   if (!obj) return;
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();

   if (obj->delete_me) return;

   if (obj->ref > 0)
     {
        obj->del_ref = EINA_TRUE;
        return;
     }

   evas_object_hide(obj);
   if (obj->focused)
     {
        obj->focused = EINA_FALSE;
        obj->layer->evas->focused = NULL;
        _evas_object_event_new();
        evas_object_event_callback_call(obj, EVAS_CALLBACK_FOCUS_OUT, NULL, _evas_event_counter);
        _evas_post_event_callback_call(obj->layer->evas);
     }
   _evas_object_event_new();
   evas_object_event_callback_call(obj, EVAS_CALLBACK_DEL, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
   if (obj->mouse_grabbed > 0)
      obj->layer->evas->pointer.mouse_grabbed -= obj->mouse_grabbed;
   if ((obj->mouse_in) || (obj->mouse_grabbed > 0))
      obj->layer->evas->pointer.object.in = eina_list_remove(obj->layer->evas->pointer.object.in, obj);
   obj->mouse_grabbed = 0;
   obj->mouse_in = 0;
   if (obj->name) evas_object_name_set(obj, NULL);
   if (!obj->layer)
     {
        evas_object_free(obj, 1);
        return;
     }
   evas_object_grabs_cleanup(obj);
   while (obj->clip.clipees)
     evas_object_clip_unset(obj->clip.clipees->data);
   while (obj->proxy.proxies)
     evas_object_image_source_unset(obj->proxy.proxies->data);
   if (obj->cur.clipper) evas_object_clip_unset(obj);
   evas_object_map_set(obj, NULL);
   if (obj->smart.smart) evas_object_smart_del(obj);
   _evas_object_event_new();
   evas_object_event_callback_call(obj, EVAS_CALLBACK_FREE, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
   evas_object_smart_cleanup(obj);
   obj->delete_me = 1;
   evas_object_change(obj);
}

void
evas_object_update_bounding_box(Evas_Object *obj)
{
   Eina_Bool propagate = EINA_FALSE;
   Eina_Bool computeminmax = EINA_FALSE;
   Evas_Coord x, y, w, h;
   Evas_Coord px, py, pw, ph;
   Eina_Bool noclip;

   if (!obj->smart.parent) return ;
   if (obj->child_has_map) return ; /* Disable bounding box computation for this object and its parent */
   /* We could also remove object that are not visible from the bounding box, use the clipping information
      to reduce the bounding of the object they are clipping, but for the moment this will do it's jobs */
   noclip = !(obj->clip.clipees || obj->is_static_clip);

   if (obj->smart.smart)
     {
        x = obj->cur.bounding_box.x;
        y = obj->cur.bounding_box.y;
        w = obj->cur.bounding_box.w;
        h = obj->cur.bounding_box.h;
        px = obj->prev.bounding_box.x;
        py = obj->prev.bounding_box.y;
        pw = obj->prev.bounding_box.w;
        ph = obj->prev.bounding_box.h;
     }
   else
     {
        x = obj->cur.geometry.x;
        y = obj->cur.geometry.y;
        w = obj->cur.geometry.w;
        h = obj->cur.geometry.h;
        px = obj->prev.geometry.x;
        py = obj->prev.geometry.y;
        pw = obj->prev.geometry.w;
        ph = obj->prev.geometry.h;
     }

   /* We are not yet trying to find the smallest bounding box, but we want to find a good approximation quickly.
    * That's why we initialiaze min and max search to geometry of the parent object.
    */

   if (obj->smart.parent->cur.valid_bounding_box)
     {
        /* Update left limit */
        if (noclip && x < obj->smart.parent->cur.bounding_box.x)
          {
             obj->smart.parent->cur.bounding_box.w += obj->smart.parent->cur.bounding_box.x - x;
             obj->smart.parent->cur.bounding_box.x = x;
             propagate = EINA_TRUE;
          }
        else if ((px == obj->smart.parent->prev.bounding_box.x && x > obj->smart.parent->cur.bounding_box.x)
                 || (!noclip && x == obj->smart.parent->cur.bounding_box.x))
          {
             computeminmax = EINA_TRUE;
          }

        /* Update top limit */
        if (noclip && y < obj->smart.parent->cur.bounding_box.y)
          {
             obj->smart.parent->cur.bounding_box.h += obj->smart.parent->cur.bounding_box.x - x;
             obj->smart.parent->cur.bounding_box.y = y;
             propagate = EINA_TRUE;
          }
        else if ((py == obj->smart.parent->prev.bounding_box.y && y  > obj->smart.parent->cur.bounding_box.y)
                 || (!noclip && y == obj->smart.parent->cur.bounding_box.y))
          {
             computeminmax = EINA_TRUE;
          }

        /* Update right limit */
        if (noclip && x + w > obj->smart.parent->cur.bounding_box.x + obj->smart.parent->cur.bounding_box.w)
          {
             obj->smart.parent->cur.bounding_box.w = x + w - obj->smart.parent->cur.bounding_box.x;
             propagate = EINA_TRUE;
          }
        else if ((px + pw == obj->smart.parent->prev.bounding_box.x + obj->smart.parent->prev.bounding_box.w &&
                  x + w < obj->smart.parent->cur.bounding_box.x + obj->smart.parent->cur.bounding_box.w)
                 || (!noclip && x + w == obj->smart.parent->cur.bounding_box.x + obj->smart.parent->cur.bounding_box.w))
          {
             computeminmax = EINA_TRUE;
          }

        /* Update bottom limit */
        if (noclip && y + h > obj->smart.parent->cur.bounding_box.y + obj->smart.parent->cur.bounding_box.h)
          {
             obj->smart.parent->cur.bounding_box.h = y + h - obj->smart.parent->cur.bounding_box.y;
             propagate = EINA_TRUE;
          }
        else if ((py + ph == obj->smart.parent->prev.bounding_box.y + obj->smart.parent->prev.bounding_box.h &&
                  y + h < obj->smart.parent->cur.bounding_box.y + obj->smart.parent->cur.bounding_box.h) ||
                 (!noclip && y + h == obj->smart.parent->cur.bounding_box.y + obj->smart.parent->cur.bounding_box.h))
          {
             computeminmax = EINA_TRUE;
          }

	if (computeminmax)
          {
             evas_object_smart_need_bounding_box_update(obj->smart.parent);
          }
     }
   else
     {
        if (noclip)
          {
             obj->smart.parent->cur.bounding_box.x = x;
             obj->smart.parent->cur.bounding_box.y = y;
             obj->smart.parent->cur.bounding_box.w = w;
             obj->smart.parent->cur.bounding_box.h = h;
             obj->smart.parent->cur.valid_bounding_box = EINA_TRUE;
             propagate = EINA_TRUE;
          }
     }

   if (propagate)
     evas_object_update_bounding_box(obj->smart.parent);
}

EAPI void
evas_object_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   Evas *evas;
   int is, was = 0, pass = 0, freeze = 0;
   int nx = 0, ny = 0;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me) return;

   nx = x;
   ny = y;

   evas = obj->layer->evas;

   if ((!obj->is_frame) && (obj != evas->framespace.clip))
     {
        if ((!obj->smart.parent) && (obj->smart.smart))
          {
             nx += evas->framespace.x;
             ny += evas->framespace.y;
          }
     }

   if (evas_object_intercept_call_move(obj, nx, ny)) return;

   if (obj->doing.in_move > 0)
     {
        WRN("evas_object_move() called on object %p when in the middle of moving the same object", obj);
        return;
     }

   if ((obj->cur.geometry.x == nx) && (obj->cur.geometry.y == ny)) return;

   if (obj->layer->evas->events_frozen <= 0)
     {
        pass = evas_event_passes_through(obj);
        freeze = evas_event_freezes_through(obj);
        if ((!pass) && (!freeze))
          was = evas_object_is_in_output_rect(obj,
                                              obj->layer->evas->pointer.x,
                                              obj->layer->evas->pointer.y, 1, 1);
     }
   obj->doing.in_move++;

   if (obj->smart.smart)
     {
        if (obj->smart.smart->smart_class->move)
          obj->smart.smart->smart_class->move(obj, nx, ny);
     }

   obj->cur.geometry.x = nx;
   obj->cur.geometry.y = ny;

   evas_object_update_bounding_box(obj);

////   obj->cur.cache.geometry.validity = 0;
   obj->changed_move = EINA_TRUE;
   evas_object_change(obj);
   evas_object_clip_dirty(obj);
   obj->doing.in_move--;
   if (obj->layer->evas->events_frozen <= 0)
     {
        evas_object_recalc_clippees(obj);
        if (!pass)
          {
             if (!obj->smart.smart)
               {
                  is = evas_object_is_in_output_rect(obj,
                                                     obj->layer->evas->pointer.x,
                                                     obj->layer->evas->pointer.y, 1, 1);
                  if ((is ^ was) && obj->cur.visible)
                    evas_event_feed_mouse_move(obj->layer->evas,
                                               obj->layer->evas->pointer.x,
                                               obj->layer->evas->pointer.y,
                                               obj->layer->evas->last_timestamp,
                                               NULL);
               }
          }
     }
   evas_object_inform_call_move(obj);
}

EAPI void
evas_object_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   int is, was = 0, pass = 0, freeze =0;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me) return;
   if (w < 0) w = 0; if (h < 0) h = 0;

   if (evas_object_intercept_call_resize(obj, w, h)) return;

   if (obj->doing.in_resize > 0)
     {
        WRN("evas_object_resize() called on object %p when in the middle of resizing the same object", obj);
        return;
     }

   if ((obj->cur.geometry.w == w) && (obj->cur.geometry.h == h)) return;

   if (obj->layer->evas->events_frozen <= 0)
     {
        pass = evas_event_passes_through(obj);
        freeze = evas_event_freezes_through(obj);
        if ((!pass) && (!freeze))
          was = evas_object_is_in_output_rect(obj,
                                              obj->layer->evas->pointer.x,
                                              obj->layer->evas->pointer.y, 1, 1);
     }
   obj->doing.in_resize++;

   if (obj->smart.smart)
     {
       if (obj->smart.smart->smart_class->resize)
          obj->smart.smart->smart_class->resize(obj, w, h);
     }

   obj->cur.geometry.w = w;
   obj->cur.geometry.h = h;

   evas_object_update_bounding_box(obj);

////   obj->cur.cache.geometry.validity = 0;
   evas_object_change(obj);
   evas_object_clip_dirty(obj);
   obj->doing.in_resize--;
   /* NB: evas_object_recalc_clippees was here previously ( < 08/07/2009) */
   if (obj->layer->evas->events_frozen <= 0)
     {
        /* NB: If this creates glitches on screen then move to above position */
        evas_object_recalc_clippees(obj);

        //   if (obj->func->coords_recalc) obj->func->coords_recalc(obj);
        if (!pass)
          {
             if (!obj->smart.smart)
               {
                  is = evas_object_is_in_output_rect(obj,
                                                     obj->layer->evas->pointer.x,
                                                     obj->layer->evas->pointer.y, 1, 1);
                  if ((is ^ was) && (obj->cur.visible))
                    evas_event_feed_mouse_move(obj->layer->evas,
                                               obj->layer->evas->pointer.x,
                                               obj->layer->evas->pointer.y,
                                               obj->layer->evas->last_timestamp,
                                               NULL);
               }
          }
     }
   evas_object_inform_call_resize(obj);
}

EAPI void
evas_object_geometry_get(const Evas_Object *obj, Evas_Coord *x, Evas_Coord *y, Evas_Coord *w, Evas_Coord *h)
{
   Evas *evas;
   int nx = 0, ny = 0;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (x) *x = 0; if (y) *y = 0; if (w) *w = 0; if (h) *h = 0;
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me)
     {
        if (x) *x = 0; if (y) *y = 0; if (w) *w = 0; if (h) *h = 0;
        return;
     }

   nx = obj->cur.geometry.x;
   ny = obj->cur.geometry.y;

   evas = obj->layer->evas;

   if ((!obj->is_frame) && (obj != evas->framespace.clip))
     {
        if ((!obj->smart.parent) && (obj->smart.smart))
          {
             if (nx > 0) nx -= evas->framespace.x;
             if (ny > 0) ny -= evas->framespace.y;
          }
     }

   if (x) *x = nx;
   if (y) *y = ny;
   if (w) *w = obj->cur.geometry.w;
   if (h) *h = obj->cur.geometry.h;
}

static void
_evas_object_size_hint_alloc(Evas_Object *obj)
{
   if (obj->size_hints) return;

   EVAS_MEMPOOL_INIT(_mp_sh, "evas_size_hints", Evas_Size_Hints, 32, );
   obj->size_hints = EVAS_MEMPOOL_ALLOC(_mp_sh, Evas_Size_Hints);
   if (!obj->size_hints) return;
   EVAS_MEMPOOL_PREP(_mp_sh, obj->size_hints, Evas_Size_Hints);
   obj->size_hints->max.w = -1;
   obj->size_hints->max.h = -1;
   obj->size_hints->align.x = 0.5;
   obj->size_hints->align.y = 0.5;
   obj->size_hints->dispmode = EVAS_DISPLAY_MODE_NONE;
}

EAPI Evas_Display_Mode
evas_object_size_hint_display_mode_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EVAS_DISPLAY_MODE_NONE;
   MAGIC_CHECK_END();
   if ((!obj->size_hints) || obj->delete_me)
     return EVAS_DISPLAY_MODE_NONE;
   return obj->size_hints->dispmode;
}

EAPI void
evas_object_size_hint_display_mode_set(Evas_Object *obj, Evas_Display_Mode dispmode)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me)
     return;
   _evas_object_size_hint_alloc(obj);
   if (obj->size_hints->dispmode == dispmode) return;
   obj->size_hints->dispmode = dispmode;

   evas_object_inform_call_changed_size_hints(obj);
}

EAPI void
evas_object_size_hint_min_get(const Evas_Object *obj, Evas_Coord *w, Evas_Coord *h)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (w) *w = 0; if (h) *h = 0;
   return;
   MAGIC_CHECK_END();
   if ((!obj->size_hints) || obj->delete_me)
     {
        if (w) *w = 0; if (h) *h = 0;
        return;
     }
   if (w) *w = obj->size_hints->min.w;
   if (h) *h = obj->size_hints->min.h;
}

EAPI void
evas_object_size_hint_min_set(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me)
     return;
   _evas_object_size_hint_alloc(obj);
   if ((obj->size_hints->min.w == w) && (obj->size_hints->min.h == h)) return;
   obj->size_hints->min.w = w;
   obj->size_hints->min.h = h;

   evas_object_inform_call_changed_size_hints(obj);
}

EAPI void
evas_object_size_hint_max_get(const Evas_Object *obj, Evas_Coord *w, Evas_Coord *h)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (w) *w = -1; if (h) *h = -1;
   return;
   MAGIC_CHECK_END();
   if ((!obj->size_hints) || obj->delete_me)
     {
        if (w) *w = -1; if (h) *h = -1;
        return;
     }
   if (w) *w = obj->size_hints->max.w;
   if (h) *h = obj->size_hints->max.h;
}

EAPI void
evas_object_size_hint_max_set(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me)
     return;
   _evas_object_size_hint_alloc(obj);
   if ((obj->size_hints->max.w == w) && (obj->size_hints->max.h == h)) return;
   obj->size_hints->max.w = w;
   obj->size_hints->max.h = h;

   evas_object_inform_call_changed_size_hints(obj);
}

EAPI void
evas_object_size_hint_request_get(const Evas_Object *obj, Evas_Coord *w, Evas_Coord *h)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (w) *w = 0; if (h) *h = 0;
   return;
   MAGIC_CHECK_END();
   if ((!obj->size_hints) || obj->delete_me)
     {
        if (w) *w = 0; if (h) *h = 0;
        return;
     }
   if (w) *w = obj->size_hints->request.w;
   if (h) *h = obj->size_hints->request.h;
}

EAPI void
evas_object_size_hint_request_set(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me)
     return;
   _evas_object_size_hint_alloc(obj);
   if ((obj->size_hints->request.w == w) && (obj->size_hints->request.h == h)) return;
   obj->size_hints->request.w = w;
   obj->size_hints->request.h = h;

   evas_object_inform_call_changed_size_hints(obj);
}

EAPI void
evas_object_size_hint_aspect_get(const Evas_Object *obj, Evas_Aspect_Control *aspect, Evas_Coord *w, Evas_Coord *h)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (aspect) *aspect = EVAS_ASPECT_CONTROL_NONE;
   if (w) *w = 0; if (h) *h = 0;
   return;
   MAGIC_CHECK_END();
   if ((!obj->size_hints) || obj->delete_me)
     {
        if (aspect) *aspect = EVAS_ASPECT_CONTROL_NONE;
        if (w) *w = 0; if (h) *h = 0;
        return;
     }
   if (aspect) *aspect = obj->size_hints->aspect.mode;
   if (w) *w = obj->size_hints->aspect.size.w;
   if (h) *h = obj->size_hints->aspect.size.h;
}

EAPI void
evas_object_size_hint_aspect_set(Evas_Object *obj, Evas_Aspect_Control aspect, Evas_Coord w, Evas_Coord h)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me)
     return;
   _evas_object_size_hint_alloc(obj);
   if ((obj->size_hints->aspect.mode == aspect) && (obj->size_hints->aspect.size.w == w) && (obj->size_hints->aspect.size.h == h)) return;
   obj->size_hints->aspect.mode = aspect;
   obj->size_hints->aspect.size.w = w;
   obj->size_hints->aspect.size.h = h;

   evas_object_inform_call_changed_size_hints(obj);
}

EAPI void
evas_object_size_hint_align_get(const Evas_Object *obj, double *x, double *y)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (x) *x = 0.5; if (y) *y = 0.5;
   return;
   MAGIC_CHECK_END();
   if ((!obj->size_hints) || obj->delete_me)
     {
        if (x) *x = 0.5; if (y) *y = 0.5;
        return;
     }
   if (x) *x = obj->size_hints->align.x;
   if (y) *y = obj->size_hints->align.y;
}

EAPI void
evas_object_size_hint_align_set(Evas_Object *obj, double x, double y)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me)
     return;
   _evas_object_size_hint_alloc(obj);
   if ((obj->size_hints->align.x == x) && (obj->size_hints->align.y == y)) return;
   obj->size_hints->align.x = x;
   obj->size_hints->align.y = y;

   evas_object_inform_call_changed_size_hints(obj);
}

EAPI void
evas_object_size_hint_weight_get(const Evas_Object *obj, double *x, double *y)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (x) *x = 0.0; if (y) *y = 0.0;
   return;
   MAGIC_CHECK_END();
   if ((!obj->size_hints) || obj->delete_me)
     {
        if (x) *x = 0.0; if (y) *y = 0.0;
        return;
     }
   if (x) *x = obj->size_hints->weight.x;
   if (y) *y = obj->size_hints->weight.y;
}

EAPI void
evas_object_size_hint_weight_set(Evas_Object *obj, double x, double y)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me)
     return;
   _evas_object_size_hint_alloc(obj);
   if ((obj->size_hints->weight.x == x) && (obj->size_hints->weight.y == y)) return;
   obj->size_hints->weight.x = x;
   obj->size_hints->weight.y = y;

   evas_object_inform_call_changed_size_hints(obj);
}

EAPI void
evas_object_size_hint_padding_get(const Evas_Object *obj, Evas_Coord *l, Evas_Coord *r, Evas_Coord *t, Evas_Coord *b)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (l) *l = 0; if (r) *r = 0;
   if (t) *t = 0; if (b) *b = 0;
   return;
   MAGIC_CHECK_END();
   if ((!obj->size_hints) || obj->delete_me)
     {
        if (l) *l = 0; if (r) *r = 0;
        if (t) *t = 0; if (b) *b = 0;
        return;
     }
   if (l) *l = obj->size_hints->padding.l;
   if (r) *r = obj->size_hints->padding.r;
   if (t) *t = obj->size_hints->padding.t;
   if (b) *b = obj->size_hints->padding.b;
}

EAPI void
evas_object_size_hint_padding_set(Evas_Object *obj, Evas_Coord l, Evas_Coord r, Evas_Coord t, Evas_Coord b)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me)
     return;
   _evas_object_size_hint_alloc(obj);
   if ((obj->size_hints->padding.l == l) && (obj->size_hints->padding.r == r) && (obj->size_hints->padding.t == t) && (obj->size_hints->padding.b == b)) return;
   obj->size_hints->padding.l = l;
   obj->size_hints->padding.r = r;
   obj->size_hints->padding.t = t;
   obj->size_hints->padding.b = b;

   evas_object_inform_call_changed_size_hints(obj);
}

EAPI void
evas_object_show(Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me) return;
   if (evas_object_intercept_call_show(obj)) return;
   if (obj->smart.smart)
     {
       if (obj->smart.smart->smart_class->show)
         obj->smart.smart->smart_class->show(obj);
     }
   if (obj->cur.visible)
     {
        return;
     }
   obj->cur.visible = 1;
   evas_object_change(obj);
   evas_object_clip_dirty(obj);
   if (obj->layer->evas->events_frozen <= 0)
     {
        evas_object_clip_across_clippees_check(obj);
        evas_object_recalc_clippees(obj);
        if ((!evas_event_passes_through(obj)) &&
            (!evas_event_freezes_through(obj)))
          {
             if (!obj->smart.smart)
               {
                  if (evas_object_is_in_output_rect(obj,
                                                    obj->layer->evas->pointer.x,
                                                    obj->layer->evas->pointer.y, 1, 1))
                    evas_event_feed_mouse_move(obj->layer->evas,
                                               obj->layer->evas->pointer.x,
                                               obj->layer->evas->pointer.y,
                                               obj->layer->evas->last_timestamp,
                                               NULL);
               }
          }
     }
   evas_object_inform_call_show(obj);
}

EAPI void
evas_object_hide(Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me) return;
   if (evas_object_intercept_call_hide(obj)) return;
   if (obj->smart.smart)
     {
       if (obj->smart.smart->smart_class->hide)
         obj->smart.smart->smart_class->hide(obj);
     }
   if (!obj->cur.visible)
     {
        return;
     }
   obj->cur.visible = 0;
   evas_object_change(obj);
   evas_object_clip_dirty(obj);
   if (obj->layer->evas->events_frozen <= 0)
     {
        evas_object_clip_across_clippees_check(obj);
        evas_object_recalc_clippees(obj);
        if ((!evas_event_passes_through(obj)) &&
            (!evas_event_freezes_through(obj)))
          {
             if ((!obj->smart.smart) ||
                 ((obj->cur.map) && (obj->cur.map->count == 4) && (obj->cur.usemap)))
               {
                  if (!obj->mouse_grabbed)
                    {
                       if (evas_object_is_in_output_rect(obj,
                                                         obj->layer->evas->pointer.x,
                                                         obj->layer->evas->pointer.y, 1, 1))
                          evas_event_feed_mouse_move(obj->layer->evas,
                                                     obj->layer->evas->pointer.x,
                                                     obj->layer->evas->pointer.y,
                                                     obj->layer->evas->last_timestamp,
                                                     NULL);
                    }
/* this is at odds to handling events when an obj is moved out of the mouse
 * ore resized out or clipped out. if mouse is grabbed - regardless of
 * visibility, mouse move events should keep happening and mouse up.
 * for better or worse it's at least consistent.
                  if (obj->delete_me) return;
                  if (obj->mouse_grabbed > 0)
                    obj->layer->evas->pointer.mouse_grabbed -= obj->mouse_grabbed;
                  if ((obj->mouse_in) || (obj->mouse_grabbed > 0))
                    obj->layer->evas->pointer.object.in = eina_list_remove(obj->layer->evas->pointer.object.in, obj);
                  obj->mouse_grabbed = 0;
                  if (obj->layer->evas->events_frozen > 0)
                    {
                       obj->mouse_in = 0;
                       return;
                    }
                  if (obj->mouse_in)
                    {
                       Evas_Event_Mouse_Out ev;

                       _evas_object_event_new();

                       obj->mouse_in = 0;
                       ev.buttons = obj->layer->evas->pointer.button;
                       ev.output.x = obj->layer->evas->pointer.x;
                       ev.output.y = obj->layer->evas->pointer.y;
                       ev.canvas.x = obj->layer->evas->pointer.x;
                       ev.canvas.y = obj->layer->evas->pointer.y;
                       ev.data = NULL;
                       ev.modifiers = &(obj->layer->evas->modifiers);
                       ev.locks = &(obj->layer->evas->locks);
                       ev.timestamp = obj->layer->evas->last_timestamp;
                       ev.event_flags = EVAS_EVENT_FLAG_NONE;
                       evas_object_event_callback_call(obj, EVAS_CALLBACK_MOUSE_OUT, &ev);
                       _evas_post_event_callback_call(obj->layer->evas);
                    }
 */
               }
          }
     }
   else
     {
/*
        if (obj->mouse_grabbed > 0)
          obj->layer->evas->pointer.mouse_grabbed -= obj->mouse_grabbed;
        if ((obj->mouse_in) || (obj->mouse_grabbed > 0))
          obj->layer->evas->pointer.object.in = eina_list_remove(obj->layer->evas->pointer.object.in, obj);
        obj->mouse_grabbed = 0;
        obj->mouse_in = 0;
 */
     }
   evas_object_inform_call_hide(obj);
}

EAPI Eina_Bool
evas_object_visible_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   if (obj->delete_me) return 0;
   return obj->cur.visible;
}

EAPI void
evas_object_color_set(Evas_Object *obj, int r, int g, int b, int a)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me) return;
   if (r > 255) r = 255; if (r < 0) r = 0;
   if (g > 255) g = 255; if (g < 0) g = 0;
   if (b > 255) b = 255; if (b < 0) b = 0;
   if (a > 255) a = 255; if (a < 0) a = 0;
   if (r > a)
     {
        r = a;
        ERR("Evas only handles pre multiplied colors!");
     }
   if (g > a)
     {
        g = a;
        ERR("Evas only handles pre multiplied colors!");
     }
   if (b > a)
     {
        b = a;
        ERR("Evas only handles pre multiplied colors!");
     }

   if (evas_object_intercept_call_color_set(obj, r, g, b, a)) return;
   if (obj->smart.smart)
     {
       if (obj->smart.smart->smart_class->color_set)
         obj->smart.smart->smart_class->color_set(obj, r, g, b, a);
     }
   if ((obj->cur.color.r == r) &&
       (obj->cur.color.g == g) &&
       (obj->cur.color.b == b) &&
       (obj->cur.color.a == a)) return;
   obj->cur.color.r = r;
   obj->cur.color.g = g;
   obj->cur.color.b = b;
   evas_object_clip_dirty(obj);

   if ((obj->cur.color.a == 0) && (a == 0) && (obj->cur.render_op == EVAS_RENDER_BLEND)) return;
   obj->cur.color.a = a;
   obj->changed_color = EINA_TRUE;
   evas_object_change(obj);
}

EAPI void
evas_object_color_get(const Evas_Object *obj, int *r, int *g, int *b, int *a)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (r) *r = 0; if (g) *g = 0; if (b) *b = 0; if (a) *a = 0;
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me)
     {
        if (r) *r = 0; if (g) *g = 0; if (b) *b = 0; if (a) *a = 0;
        return;
     }
   if (r) *r = obj->cur.color.r;
   if (g) *g = obj->cur.color.g;
   if (b) *b = obj->cur.color.b;
   if (a) *a = obj->cur.color.a;
}

EAPI void
evas_object_anti_alias_set(Evas_Object *obj, Eina_Bool anti_alias)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me) return;
   anti_alias = !!anti_alias;
   if (obj->cur.anti_alias == anti_alias)return;
   obj->cur.anti_alias = anti_alias;
   evas_object_change(obj);
}

EAPI Eina_Bool
evas_object_anti_alias_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   if (obj->delete_me) return 0;
   return obj->cur.anti_alias;
}

EAPI void
evas_object_scale_set(Evas_Object *obj, double scale)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me) return;
   if (obj->cur.scale == scale) return;
   obj->cur.scale = scale;
   evas_object_change(obj);
   if (obj->func->scale_update) obj->func->scale_update(obj);
}

EAPI double
evas_object_scale_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   if (obj->delete_me) return 1.0;
   return obj->cur.scale;
}

EAPI void
evas_object_render_op_set(Evas_Object *obj, Evas_Render_Op render_op)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   if (obj->delete_me) return;
   if (obj->cur.render_op == render_op) return;
   obj->cur.render_op = render_op;
   evas_object_change(obj);
}

EAPI Evas_Render_Op
evas_object_render_op_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   if (obj->delete_me) return EVAS_RENDER_BLEND;
   return obj->cur.render_op;
}

EAPI Evas *
evas_object_evas_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   if (obj->delete_me) return NULL;
   return obj->layer->evas;
}

EAPI Evas_Object *
evas_object_top_at_xy_get(const Evas *e, Evas_Coord x, Evas_Coord y, Eina_Bool include_pass_events_objects, Eina_Bool include_hidden_objects)
{
   Evas_Layer *lay;
   int xx, yy;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return NULL;
   MAGIC_CHECK_END();
   xx = x;
   yy = y;
////   xx = evas_coord_world_x_to_screen(e, x);
////   yy = evas_coord_world_y_to_screen(e, y);
   EINA_INLIST_REVERSE_FOREACH((EINA_INLIST_GET(e->layers)), lay)
     {
        Evas_Object *obj;

        EINA_INLIST_REVERSE_FOREACH(get_layer_objects(lay), obj)
          {
             if (obj->delete_me) continue;
             if ((!include_pass_events_objects) &&
                 (evas_event_passes_through(obj))) continue;
             if ((!include_hidden_objects) && (!obj->cur.visible)) continue;
             evas_object_clip_recalc(obj);
             if ((evas_object_is_in_output_rect(obj, xx, yy, 1, 1)) &&
                 (!obj->clip.clipees))
               return obj;
          }
     }
   return NULL;
}

EAPI Evas_Object *
evas_object_top_at_pointer_get(const Evas *e)
{
   return evas_object_top_at_xy_get(e, e->pointer.x, e->pointer.y, EINA_TRUE,
                                    EINA_TRUE);
}

EAPI Evas_Object *
evas_object_top_in_rectangle_get(const Evas *e, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, Eina_Bool include_pass_events_objects, Eina_Bool include_hidden_objects)
{
   Evas_Layer *lay;
   int xx, yy, ww, hh;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return NULL;
   MAGIC_CHECK_END();
   xx = x;
   yy = y;
   ww = w;
   hh = h;
////   xx = evas_coord_world_x_to_screen(e, x);
////   yy = evas_coord_world_y_to_screen(e, y);
////   ww = evas_coord_world_x_to_screen(e, w);
////   hh = evas_coord_world_y_to_screen(e, h);
   if (ww < 1) ww = 1;
   if (hh < 1) hh = 1;
   EINA_INLIST_REVERSE_FOREACH((EINA_INLIST_GET(e->layers)), lay)
     {
        Evas_Object *obj;

        EINA_INLIST_REVERSE_FOREACH(get_layer_objects(lay), obj)
          {
             if (obj->delete_me) continue;
             if ((!include_pass_events_objects) &&
                 (evas_event_passes_through(obj))) continue;
             if ((!include_hidden_objects) && (!obj->cur.visible)) continue;
             evas_object_clip_recalc(obj);
             if ((evas_object_is_in_output_rect(obj, xx, yy, ww, hh)) &&
                 (!obj->clip.clipees))
               return obj;
          }
     }
   return NULL;
}

EAPI Eina_List *
evas_objects_at_xy_get(const Evas *e, Evas_Coord x, Evas_Coord y, Eina_Bool include_pass_events_objects, Eina_Bool include_hidden_objects)
{
   Eina_List *in = NULL;
   Evas_Layer *lay;
   int xx, yy;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return NULL;
   MAGIC_CHECK_END();
   xx = x;
   yy = y;
////   xx = evas_coord_world_x_to_screen(e, x);
////   yy = evas_coord_world_y_to_screen(e, y);
   EINA_INLIST_REVERSE_FOREACH((EINA_INLIST_GET(e->layers)), lay)
     {
        Evas_Object *obj;

        EINA_INLIST_REVERSE_FOREACH(get_layer_objects(lay), obj)
          {
             if (obj->delete_me) continue;
             if ((!include_pass_events_objects) &&
                 (evas_event_passes_through(obj))) continue;
             if ((!include_hidden_objects) && (!obj->cur.visible)) continue;
             evas_object_clip_recalc(obj);
             if ((evas_object_is_in_output_rect(obj, xx, yy, 1, 1)) &&
                 (!obj->clip.clipees))
               in = eina_list_prepend(in, obj);
          }
     }
   return in;
}

/**
 * Retrieves the objects in the given rectangle region
 * @param   e The given evas object.
 * @param   x The horizontal coordinate.
 * @param   y The vertical coordinate.
 * @param   w The width size.
 * @param   h The height size.
 * @param   include_pass_events_objects Boolean Flag to include or not pass events objects
 * @param   include_hidden_objects Boolean Flag to include or not hidden objects
 * @return  The list of evas object in the rectangle region.
 *
 */
EAPI Eina_List *
evas_objects_in_rectangle_get(const Evas *e, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h, Eina_Bool include_pass_events_objects, Eina_Bool include_hidden_objects)
{
   Eina_List *in = NULL;
   Evas_Layer *lay;
   int xx, yy, ww, hh;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return NULL;
   MAGIC_CHECK_END();
   xx = x;
   yy = y;
   ww = w;
   hh = h;
////   xx = evas_coord_world_x_to_screen(e, x);
////   yy = evas_coord_world_y_to_screen(e, y);
////   ww = evas_coord_world_x_to_screen(e, w);
////   hh = evas_coord_world_y_to_screen(e, h);
   if (ww < 1) ww = 1;
   if (hh < 1) hh = 1;
   EINA_INLIST_REVERSE_FOREACH((EINA_INLIST_GET(e->layers)), lay)
     {
        Evas_Object *obj;

        EINA_INLIST_REVERSE_FOREACH(get_layer_objects(lay), obj)
          {
             if (obj->delete_me) continue;
             if ((!include_pass_events_objects) &&
                 (evas_event_passes_through(obj))) continue;
             if ((!include_hidden_objects) && (!obj->cur.visible)) continue;
             evas_object_clip_recalc(obj);
             if ((evas_object_is_in_output_rect(obj, xx, yy, ww, hh)) &&
                 (!obj->clip.clipees))
               in = eina_list_prepend(in, obj);
          }
     }
   return in;
}

EAPI const char *
evas_object_type_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   if (obj->delete_me) return "";
   return obj->type;
}

EAPI void
evas_object_precise_is_inside_set(Evas_Object *obj, Eina_Bool precise)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   obj->precise_is_inside = precise;
}

EAPI Eina_Bool
evas_object_precise_is_inside_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   return obj->precise_is_inside;
}

EAPI void
evas_object_static_clip_set(Evas_Object *obj, Eina_Bool is_static_clip)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   obj->is_static_clip = is_static_clip;
}

EAPI Eina_Bool
evas_object_static_clip_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return 0;
   MAGIC_CHECK_END();
   return obj->is_static_clip;
}

EAPI void
evas_object_is_frame_object_set(Evas_Object *obj, Eina_Bool is_frame)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   obj->is_frame = is_frame;
}

EAPI Eina_Bool
evas_object_is_frame_object_get(Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   return obj->is_frame;
}
