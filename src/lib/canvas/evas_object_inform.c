#include "evas_common.h"
#include "evas_private.h"

/* local calls */

void
evas_object_inform_call_show(Evas_Object *obj)
{
   _evas_object_event_new();

   evas_object_event_callback_call(obj, EVAS_CALLBACK_SHOW, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

void
evas_object_inform_call_hide(Evas_Object *obj)
{
   _evas_object_event_new();

   evas_object_event_callback_call(obj, EVAS_CALLBACK_HIDE, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

void
evas_object_inform_call_move(Evas_Object *obj)
{
   _evas_object_event_new();

   evas_object_event_callback_call(obj, EVAS_CALLBACK_MOVE, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

void
evas_object_inform_call_resize(Evas_Object *obj)
{
   _evas_object_event_new();

   evas_object_event_callback_call(obj, EVAS_CALLBACK_RESIZE, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

void
evas_object_inform_call_restack(Evas_Object *obj)
{
   _evas_object_event_new();

   evas_object_event_callback_call(obj, EVAS_CALLBACK_RESTACK, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

void
evas_object_inform_call_changed_size_hints(Evas_Object *obj)
{
   _evas_object_event_new();

   evas_object_event_callback_call(obj, EVAS_CALLBACK_CHANGED_SIZE_HINTS, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

void
evas_object_inform_call_image_preloaded(Evas_Object *obj)
{
   if (!_evas_object_image_preloading_get(obj)) return;
   _evas_object_image_preloading_check(obj);
   _evas_object_image_preloading_set(obj, 0);
   _evas_object_event_new();

   evas_object_event_callback_call(obj, EVAS_CALLBACK_IMAGE_PRELOADED, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

void
evas_object_inform_call_image_unloaded(Evas_Object *obj)
{
   _evas_object_event_new();

   evas_object_event_callback_call(obj, EVAS_CALLBACK_IMAGE_UNLOADED, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

void
evas_object_inform_call_image_resize(Evas_Object *obj)
{
   _evas_object_event_new();
   evas_object_event_callback_call(obj, EVAS_CALLBACK_IMAGE_RESIZE, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

///Jiyoun: This code will be modified after opensource fix lockup issue
#define EVAS_CALLBACK_CANVAS_OBJECT_RENDER_PRE 100
#define EVAS_CALLBACK_CANVAS_OBJECT_RENDER_POST 101

void
evas_object_inform_call_render_pre(Evas_Object *obj)
{
   _evas_object_event_new();
   evas_object_event_callback_call(obj, EVAS_CALLBACK_CANVAS_OBJECT_RENDER_PRE, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

void
evas_object_inform_call_render_post(Evas_Object *obj)
{
   _evas_object_event_new();
   evas_object_event_callback_call(obj, EVAS_CALLBACK_CANVAS_OBJECT_RENDER_POST, NULL, _evas_event_counter);
   _evas_post_event_callback_call(obj->layer->evas);
}

