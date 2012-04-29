#ifndef _EVAS_ENGINE_WAYLAND_SHM_H
# define _EVAS_ENGINE_WAYLAND_SHM_H

typedef struct _Evas_Engine_Info_Wayland_Shm Evas_Engine_Info_Wayland_Shm;
struct _Evas_Engine_Info_Wayland_Shm 
{
   Evas_Engine_Info magic;

   struct 
     {
        void *dest;
        int rotation;

        Eina_Bool destination_alpha : 1;
        Eina_Bool debug : 1;
     } info;

   Evas_Engine_Render_Mode render_mode;
};

#endif
