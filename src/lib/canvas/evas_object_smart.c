#include "evas_common.h"
#include "evas_private.h"

typedef struct _Evas_Object_Smart      Evas_Object_Smart;
typedef struct _Evas_Smart_Callback    Evas_Smart_Callback;

struct _Evas_Object_Smart
{
   DATA32            magic;
   void             *engine_data;
   void             *data;
   Eina_List        *callbacks;
   Eina_Inlist      *contained;
   Evas_Smart_Cb_Description_Array callbacks_descriptions;
   int               walking_list;
   Eina_Bool         deletions_waiting : 1;
   Eina_Bool         need_recalculate : 1;
};

struct _Evas_Smart_Callback
{
   const char *event;
   Evas_Smart_Cb func;
   void *func_data;
   Evas_Callback_Priority priority;
   char  delete_me : 1;
};

/* private methods for smart objects */
static void evas_object_smart_callbacks_clear(Evas_Object *obj);
static void evas_object_smart_init(Evas_Object *obj);
static void *evas_object_smart_new(void);
static void evas_object_smart_render(Evas_Object *obj, void *output, void *context, void *surface, int x, int y);
static void evas_object_smart_free(Evas_Object *obj);
static void evas_object_smart_render_pre(Evas_Object *obj);
static void evas_object_smart_render_post(Evas_Object *obj);

static unsigned int evas_object_smart_id_get(Evas_Object *obj);
static unsigned int evas_object_smart_visual_id_get(Evas_Object *obj);
static void *evas_object_smart_engine_data_get(Evas_Object *obj);

static const Evas_Object_Func object_func =
{
   /* methods (compulsory) */
   evas_object_smart_free,
   evas_object_smart_render,
   evas_object_smart_render_pre,
   evas_object_smart_render_post,
   evas_object_smart_id_get,
   evas_object_smart_visual_id_get,
   evas_object_smart_engine_data_get,
   /* these are optional. NULL = nothing */
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL
};

EVAS_MEMPOOL(_mp_obj);
EVAS_MEMPOOL(_mp_cb);

/* public funcs */
EAPI void
evas_object_smart_data_set(Evas_Object *obj, void *data)
{
   Evas_Object_Smart *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return;
   MAGIC_CHECK_END();
   o->data = data;
}

EAPI void *
evas_object_smart_data_get(const Evas_Object *obj)
{
   Evas_Object_Smart *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   if (!o) return NULL;
   return o->data;
}

EAPI Evas_Smart *
evas_object_smart_smart_get(const Evas_Object *obj)
{
   Evas_Object_Smart *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return NULL;
   MAGIC_CHECK_END();
   return obj->smart.smart;
}

EAPI void
evas_object_smart_member_add(Evas_Object *obj, Evas_Object *smart_obj)
{
   Evas_Object_Smart *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   MAGIC_CHECK(smart_obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(smart_obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return;
   MAGIC_CHECK_END();

   if (obj->delete_me)
     {
        CRIT("Adding deleted object %p to smart obj %p", obj, smart_obj);
        abort();
        return;
     }
   if (smart_obj->delete_me)
     {
        CRIT("Adding object %p to deleted smart obj %p", obj, smart_obj);
        abort();
        return;
     }
   if (!smart_obj->layer)
     {
        CRIT("No evas surface associated with smart object (%p)", smart_obj);
        abort();
        return;
     }
   if ((obj->layer && smart_obj->layer) &&
       (obj->layer->evas != smart_obj->layer->evas))
     {
        CRIT("Adding object %p from Evas (%p) from another Evas (%p)", obj, obj->layer->evas, smart_obj->layer->evas);
        abort();
        return;
     }

   if (obj->smart.parent == smart_obj) return;

   if (obj->smart.parent) evas_object_smart_member_del(obj);

   evas_object_release(obj, 1);
   obj->layer = smart_obj->layer;
   obj->cur.layer = obj->layer->layer;
   obj->layer->usage++;
   obj->smart.parent = smart_obj;
   o->contained = eina_inlist_append(o->contained, EINA_INLIST_GET(obj));
   evas_object_smart_member_cache_invalidate(obj, EINA_TRUE, EINA_TRUE);
   obj->restack = 1;
   evas_object_change(obj);
   evas_object_mapped_clip_across_mark(obj);
   if (smart_obj->smart.smart->smart_class->member_add)
     smart_obj->smart.smart->smart_class->member_add(smart_obj, obj);
}

EAPI void
evas_object_smart_member_del(Evas_Object *obj)
{
   Evas_Object_Smart *o;
   Evas_Object *smart_obj;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();

   if (!obj->smart.parent) return;

   smart_obj = obj->smart.parent;
   if (smart_obj->smart.smart->smart_class->member_del)
     smart_obj->smart.smart->smart_class->member_del(smart_obj, obj);

   o = (Evas_Object_Smart *)(obj->smart.parent->object_data);
   o->contained = eina_inlist_remove(o->contained, EINA_INLIST_GET(obj));
   obj->smart.parent = NULL;
   evas_object_smart_member_cache_invalidate(obj, EINA_TRUE, EINA_TRUE);
   obj->layer->usage--;
   obj->cur.layer = obj->layer->layer;
   evas_object_inject(obj, obj->layer->evas);
   obj->restack = 1;
   evas_object_change(obj);
   evas_object_mapped_clip_across_mark(obj);
}

EAPI Evas_Object *
evas_object_smart_parent_get(const Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();

   return obj->smart.parent;
}

EAPI Eina_Bool
evas_object_smart_type_check(const Evas_Object *obj, const char *type)
{
   const Evas_Smart_Class *sc;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   if (!obj->smart.smart)
     return EINA_FALSE;
   sc = obj->smart.smart->smart_class;
   while (sc)
     {
        if (!strcmp(sc->name, type))
          return EINA_TRUE;
        sc = sc->parent;
     }

   return EINA_FALSE;
}

EAPI Eina_Bool
evas_object_smart_type_check_ptr(const Evas_Object *obj, const char *type)
{
   const Evas_Smart_Class *sc;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   if (!obj->smart.smart)
     return EINA_FALSE;
   sc = obj->smart.smart->smart_class;
   while (sc)
     {
        if (sc->name == type)
          return EINA_TRUE;
        sc = sc->parent;
     }

   return EINA_FALSE;
}

EAPI Eina_List *
evas_object_smart_members_get(const Evas_Object *obj)
{
   Evas_Object_Smart *o;
   Eina_List *members;
   Eina_Inlist *member;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return NULL;
   MAGIC_CHECK_END();

   members = NULL;
   for (member = o->contained; member; member = member->next)
     members = eina_list_append(members, member);

   return members;
}

const Eina_Inlist *
evas_object_smart_members_get_direct(const Evas_Object *obj)
{
   Evas_Object_Smart *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return NULL;
   MAGIC_CHECK_END();
   return o->contained;
}

void
_evas_object_smart_members_all_del(Evas_Object *obj)
{
   Evas_Object_Smart *o = (Evas_Object_Smart *)(obj->object_data);
   while (o->contained)
     evas_object_del((Evas_Object *)(o->contained));
}

EAPI Evas_Object *
evas_object_smart_add(Evas *e, Evas_Smart *s)
{
   Evas_Object *obj;

   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return NULL;
   MAGIC_CHECK_END();
   MAGIC_CHECK(s, Evas_Smart, MAGIC_SMART);
   return NULL;
   MAGIC_CHECK_END();

   obj = evas_object_new(e);
   if (!obj) return NULL;
   obj->smart.smart = s;
   obj->type = s->smart_class->name;
   evas_object_smart_init(obj);
   evas_object_inject(obj, e);

   evas_object_smart_use(s);

   if (s->smart_class->add) s->smart_class->add(obj);

   return obj;
}

static int
_callback_priority_cmp(const void *_a, const void *_b)
{
   const Evas_Smart_Callback *a, *b;
   a = (const Evas_Smart_Callback *) _a;
   b = (const Evas_Smart_Callback *) _b;
   if (a->priority < b->priority)
      return -1;
   else
      return 1;
}

EAPI void
evas_object_smart_callback_add(Evas_Object *obj, const char *event, Evas_Smart_Cb func, const void *data)
{
   evas_object_smart_callback_priority_add(obj, event,
         EVAS_CALLBACK_PRIORITY_DEFAULT, func, data);
}

EAPI void
evas_object_smart_callback_priority_add(Evas_Object *obj, const char *event, Evas_Callback_Priority priority, Evas_Smart_Cb func, const void *data)
{
   Evas_Object_Smart *o;
   Evas_Smart_Callback *cb;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return;
   MAGIC_CHECK_END();
   if (!event) return;
   if (!func) return;
   EVAS_MEMPOOL_INIT(_mp_cb, "evas_smart_callback", Evas_Smart_Callback, 512, );
   cb = EVAS_MEMPOOL_ALLOC(_mp_cb, Evas_Smart_Callback);
   if (!cb) return;
   EVAS_MEMPOOL_PREP(_mp_cb, cb, Evas_Smart_Callback);
   cb->event = eina_stringshare_add(event);
   cb->func = func;
   cb->func_data = (void *)data;
   cb->priority = priority;
   o->callbacks = eina_list_sorted_insert(o->callbacks, _callback_priority_cmp,
         cb);
}

EAPI void *
evas_object_smart_callback_del(Evas_Object *obj, const char *event, Evas_Smart_Cb func)
{
   Evas_Object_Smart *o;
   Eina_List *l;
   Evas_Smart_Callback *cb;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return NULL;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return NULL;
   MAGIC_CHECK_END();
   if (!event) return NULL;
   EINA_LIST_FOREACH(o->callbacks, l, cb)
     {
        if ((!strcmp(cb->event, event)) && (cb->func == func))
          {
             void *data;

             data = cb->func_data;
             cb->delete_me = 1;
             o->deletions_waiting = 1;
             evas_object_smart_callbacks_clear(obj);
             return data;
          }
     }
   return NULL;
}

EAPI void
evas_object_smart_callback_call(Evas_Object *obj, const char *event, void *event_info)
{
   Evas_Object_Smart *o;
   Eina_List *l;
   Evas_Smart_Callback *cb;
   const char *strshare;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return;
   MAGIC_CHECK_END();
   if (!event) return;
   if (obj->delete_me) return;
   o->walking_list++;
   strshare = eina_stringshare_add(event);
   EINA_LIST_FOREACH(o->callbacks, l, cb)
     {
        if (!cb->delete_me)
          {
             if (cb->event == strshare)
               cb->func(cb->func_data, obj, event_info);
          }
        if (obj->delete_me)
          break;
     }
   eina_stringshare_del(strshare);
   o->walking_list--;
   evas_object_smart_callbacks_clear(obj);
}

EAPI Eina_Bool
evas_object_smart_callbacks_descriptions_set(Evas_Object *obj, const Evas_Smart_Cb_Description *descriptions)
{
   const Evas_Smart_Cb_Description *d;
   Evas_Object_Smart *o;
   unsigned int i, count = 0;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   if ((!descriptions) || (!descriptions->name))
     {
        evas_smart_cb_descriptions_resize(&o->callbacks_descriptions, 0);
        return EINA_TRUE;
     }

   for (count = 0, d = descriptions; d->name; d++)
     count++;

   evas_smart_cb_descriptions_resize(&o->callbacks_descriptions, count);
   if (count == 0) return EINA_TRUE;

   for (i = 0, d = descriptions; i < count; d++, i++)
     o->callbacks_descriptions.array[i] = d;

   evas_smart_cb_descriptions_fix(&o->callbacks_descriptions);

   return EINA_TRUE;
}

EAPI void
evas_object_smart_callbacks_descriptions_get(const Evas_Object *obj, const Evas_Smart_Cb_Description ***class_descriptions, unsigned int *class_count, const Evas_Smart_Cb_Description ***instance_descriptions, unsigned int *instance_count)
{
   Evas_Object_Smart *o;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (class_descriptions) *class_descriptions = NULL;
   if (class_count) *class_count = 0;
   if (instance_descriptions) *instance_descriptions = NULL;
   if (instance_count) *instance_count = 0;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   if (class_descriptions) *class_descriptions = NULL;
   if (class_count) *class_count = 0;
   if (instance_descriptions) *instance_descriptions = NULL;
   if (instance_count) *instance_count = 0;
   return;
   MAGIC_CHECK_END();

   if (class_descriptions)
     *class_descriptions = obj->smart.smart->callbacks.array;
   if (class_count)
     *class_count = obj->smart.smart->callbacks.size;

   if (instance_descriptions)
     *instance_descriptions = o->callbacks_descriptions.array;
   if (instance_count)
     *instance_count = o->callbacks_descriptions.size;
}

EAPI void
evas_object_smart_callback_description_find(const Evas_Object *obj, const char *name, const Evas_Smart_Cb_Description **class_description, const Evas_Smart_Cb_Description **instance_description)
{
   Evas_Object_Smart *o;

   if (!name)
     {
        if (class_description) *class_description = NULL;
        if (instance_description) *instance_description = NULL;
        return;
     }

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   if (class_description) *class_description = NULL;
   if (instance_description) *instance_description = NULL;
   return;
   MAGIC_CHECK_END();
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   if (class_description) *class_description = NULL;
   if (instance_description) *instance_description = NULL;
   return;
   MAGIC_CHECK_END();

   if (class_description)
     *class_description = evas_smart_cb_description_find
        (&obj->smart.smart->callbacks, name);

   if (instance_description)
     *instance_description = evas_smart_cb_description_find
        (&o->callbacks_descriptions, name);
}

EAPI void
evas_object_smart_need_recalculate_set(Evas_Object *obj, Eina_Bool value)
{
   Evas_Object_Smart *o;
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = obj->object_data;
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return;
   MAGIC_CHECK_END();

   // XXX: do i need this?
   if (obj->delete_me) return;

   /* remove this entry from calc_list or processed list */
   if (eina_clist_element_is_linked(&obj->calc_entry))
     eina_clist_remove(&obj->calc_entry);

   value = !!value;
   if (value)
     eina_clist_add_tail(&obj->layer->evas->calc_list, &obj->calc_entry);
   else
     eina_clist_add_tail(&obj->layer->evas->calc_done, &obj->calc_entry);

   if (o->need_recalculate == value) return;

   if (obj->recalculate_cycle > 254)
     {
        ERR("Object %p is not stable during recalc loop", obj);
        return;
     }
   if (obj->layer->evas->in_smart_calc) obj->recalculate_cycle++;
   o->need_recalculate = value;
}

EAPI Eina_Bool
evas_object_smart_need_recalculate_get(const Evas_Object *obj)
{
   Evas_Object_Smart *o;
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return EINA_FALSE;
   MAGIC_CHECK_END();
   o = obj->object_data;
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return EINA_FALSE;
   MAGIC_CHECK_END();

   return o->need_recalculate;
}

EAPI void
evas_object_smart_calculate(Evas_Object *obj)
{
   Evas_Object_Smart *o;
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   o = obj->object_data;
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return;
   MAGIC_CHECK_END();

   if (!obj->smart.smart->smart_class->calculate)
     return;

   o->need_recalculate = 0;
   obj->smart.smart->smart_class->calculate(obj);
}

EAPI void
evas_smart_objects_calculate(Evas *e)
{
   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return;
   MAGIC_CHECK_END();
   evas_call_smarts_calculate(e);
}

EAPI int
evas_smart_objects_calculate_count_get(const Evas *e)
{
   MAGIC_CHECK(e, Evas, MAGIC_EVAS);
   return 0;
   MAGIC_CHECK_END();
   return e->smart_calc_count;
}

/**
 * Call calculate() on all smart objects that need_recalculate.
 *
 * @internal
 */
void
evas_call_smarts_calculate(Evas *e)
{
   Eina_Clist *elem;
   Evas_Object *obj;

//   printf("+CAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALC-----------v\n");
   evas_event_freeze(e);
   e->in_smart_calc++;

   while (NULL != (elem = eina_clist_head(&e->calc_list)))
     {
        Evas_Object_Smart *o;

        /* move the item to the processed list */
        obj = EINA_CLIST_ENTRY(elem, Evas_Object, calc_entry);
        eina_clist_remove(&obj->calc_entry);
        if (obj->delete_me) continue;
        eina_clist_add_tail(&e->calc_done, &obj->calc_entry);

        o = obj->object_data;

        if (o->need_recalculate)
          {
             o->need_recalculate = 0;
             obj->smart.smart->smart_class->calculate(obj);
          }
     }

   while (NULL != (elem = eina_clist_head(&e->calc_done)))
     {
        obj = EINA_CLIST_ENTRY(elem, Evas_Object, calc_entry);
        obj->recalculate_cycle = 0;
        eina_clist_remove(&obj->calc_entry);
     }

   e->in_smart_calc--;
   if (e->in_smart_calc == 0) e->smart_calc_count++;
   evas_event_thaw(e);
   evas_event_thaw_eval(e);
}

EAPI void
evas_object_smart_changed(Evas_Object *obj)
{
   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();
   evas_object_change(obj);
   evas_object_smart_need_recalculate_set(obj, 1);
}

/* internal calls */
static void
evas_object_smart_callbacks_clear(Evas_Object *obj)
{
   Evas_Object_Smart *o;
   Eina_List *l;
   Evas_Smart_Callback *cb;

   o = (Evas_Object_Smart *)(obj->object_data);

   if (o->walking_list) return;
   if (!o->deletions_waiting) return;
   for (l = o->callbacks; l;)
     {
        cb = eina_list_data_get(l);
        l = eina_list_next(l);
        if (cb->delete_me)
          {
             o->callbacks = eina_list_remove(o->callbacks, cb);
             if (cb->event) eina_stringshare_del(cb->event);
             EVAS_MEMPOOL_FREE(_mp_cb, cb);
          }
     }
}

void
evas_object_smart_del(Evas_Object *obj)
{
   Evas_Smart *s;

   if (obj->delete_me) return;
   s = obj->smart.smart;
   if ((s) && (s->smart_class->del)) s->smart_class->del(obj);
   if (obj->smart.parent) evas_object_smart_member_del(obj);
   if (s) evas_object_smart_unuse(s);
}

void
evas_object_smart_cleanup(Evas_Object *obj)
{
   Evas_Object_Smart *o;

   if (obj->smart.parent)
     evas_object_smart_member_del(obj);

   o = (Evas_Object_Smart *)(obj->object_data);
   if (o->magic == MAGIC_OBJ_SMART)
     {
        if (obj->calc_entry.next)
          eina_clist_remove(&obj->calc_entry);

        while (o->contained)
           evas_object_smart_member_del((Evas_Object *)o->contained);

        while (o->callbacks)
          {
             Evas_Smart_Callback *cb = o->callbacks->data;
             o->callbacks = eina_list_remove(o->callbacks, cb);
             if (cb->event) eina_stringshare_del(cb->event);
             EVAS_MEMPOOL_FREE(_mp_cb, cb);
          }

        evas_smart_cb_descriptions_resize(&o->callbacks_descriptions, 0);
        o->data = NULL;
     }

   obj->smart.parent = NULL;
   obj->smart.smart = NULL;
}

void
evas_object_smart_member_cache_invalidate(Evas_Object *obj,
                                          Eina_Bool pass_events,
                                          Eina_Bool freeze_events)
{
   Evas_Object_Smart *o;
   Evas_Object *member;

   MAGIC_CHECK(obj, Evas_Object, MAGIC_OBJ);
   return;
   MAGIC_CHECK_END();

   if (pass_events)
     obj->parent_cache.pass_events_valid = EINA_FALSE;
   if (freeze_events)
     obj->parent_cache.freeze_events_valid = EINA_FALSE;

   o = obj->object_data;
   if (o->magic != MAGIC_OBJ_SMART) return;

   EINA_INLIST_FOREACH(o->contained, member)
     evas_object_smart_member_cache_invalidate(member,
                                               pass_events,
                                               freeze_events);
}

void
evas_object_smart_member_raise(Evas_Object *member)
{
   Evas_Object_Smart *o;

   o = (Evas_Object_Smart *)(member->smart.parent->object_data);
   o->contained = eina_inlist_demote(o->contained, EINA_INLIST_GET(member));
}

void
evas_object_smart_member_lower(Evas_Object *member)
{
   Evas_Object_Smart *o;

   o = (Evas_Object_Smart *)(member->smart.parent->object_data);
   o->contained = eina_inlist_promote(o->contained, EINA_INLIST_GET(member));
}

void
evas_object_smart_member_stack_above(Evas_Object *member, Evas_Object *other)
{
   Evas_Object_Smart *o;

   o = (Evas_Object_Smart *)(member->smart.parent->object_data);
   o->contained = eina_inlist_remove(o->contained, EINA_INLIST_GET(member));
   o->contained = eina_inlist_append_relative(o->contained, EINA_INLIST_GET(member), EINA_INLIST_GET(other));
}

void
evas_object_smart_member_stack_below(Evas_Object *member, Evas_Object *other)
{
   Evas_Object_Smart *o;

   o = (Evas_Object_Smart *)(member->smart.parent->object_data);
   o->contained = eina_inlist_remove(o->contained, EINA_INLIST_GET(member));
   o->contained = eina_inlist_prepend_relative(o->contained, EINA_INLIST_GET(member), EINA_INLIST_GET(other));
}

/* all nice and private */
static void
evas_object_smart_init(Evas_Object *obj)
{
   /* alloc smart obj, setup methods and default values */
   obj->object_data = evas_object_smart_new();
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
}

static void *
evas_object_smart_new(void)
{
   Evas_Object_Smart *o;

   /* alloc obj private data */
   EVAS_MEMPOOL_INIT(_mp_obj, "evas_object_smart", Evas_Object_Smart, 256, NULL);
   o = EVAS_MEMPOOL_ALLOC(_mp_obj, Evas_Object_Smart);
   if (!o) return NULL;
   EVAS_MEMPOOL_PREP(_mp_obj, o, Evas_Object_Smart);
   o->magic = MAGIC_OBJ_SMART;
   return o;
}

static void
evas_object_smart_free(Evas_Object *obj)
{
   Evas_Object_Smart *o;

   /* frees private object data. very simple here */
   o = (Evas_Object_Smart *)(obj->object_data);
   MAGIC_CHECK(o, Evas_Object_Smart, MAGIC_OBJ_SMART);
   return;
   MAGIC_CHECK_END();
   /* free obj */
   o->magic = 0;
   EVAS_MEMPOOL_FREE(_mp_obj, o);
}

static void
evas_object_smart_render(Evas_Object *obj __UNUSED__, void *output __UNUSED__, void *context __UNUSED__, void *surface __UNUSED__, int x __UNUSED__, int y __UNUSED__)
{
   return;
}

static void
evas_object_smart_render_pre(Evas_Object *obj)
{
   if (obj->pre_render_done) return;
   if ((obj->cur.map != obj->prev.map) ||
       (obj->cur.usemap != obj->prev.usemap))
     {
        evas_object_render_pre_prev_cur_add(&obj->layer->evas->clip_changes, obj);
        goto done;
     }
done:
   obj->pre_render_done = 1;
}

static void
evas_object_smart_render_post(Evas_Object *obj)
{
   obj->prev = obj->cur;
}

static unsigned int evas_object_smart_id_get(Evas_Object *obj)
{
   Evas_Object_Smart *o;

   o = (Evas_Object_Smart *)(obj->object_data);
   if (!o) return 0;
   return MAGIC_OBJ_SMART;
}

static unsigned int evas_object_smart_visual_id_get(Evas_Object *obj)
{
   Evas_Object_Smart *o;

   o = (Evas_Object_Smart *)(obj->object_data);
   if (!o) return 0;
   return MAGIC_OBJ_CONTAINER;
}

static void *evas_object_smart_engine_data_get(Evas_Object *obj)
{
   Evas_Object_Smart *o;

   o = (Evas_Object_Smart *)(obj->object_data);
   if (!o) return NULL;
   return o->engine_data;
}
