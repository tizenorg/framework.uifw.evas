
#include <cstdio>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <ddraw.h>

#include "evas_common.h"
#include "evas_engine.h"


typedef LONG (*fct_DirectDrawCreate)(LPGUID, LPUNKNOWN *, LPUNKNOWN *);

fct_DirectDrawCreate lib_DirectDrawCreate;

typedef struct Evas_Engine_WinCE_DDraw_Priv Evas_Engine_WinCE_DDraw_Priv;

struct Evas_Engine_WinCE_DDraw_Priv
{
   HMODULE             module;
   LPDIRECTDRAW        object;
   LPDIRECTDRAWSURFACE surface;
   int                 width;
   int                 height;
   int                 stride;
};

void *
evas_software_wince_ddraw_init(HWND window,
                               int  width,
                               int  height)
{
   DDSURFACEDESC                 surface_desc;
   Evas_Engine_WinCE_DDraw_Priv *priv;
   HRESULT                       res;

   priv = (Evas_Engine_WinCE_DDraw_Priv *)malloc(sizeof(Evas_Engine_WinCE_DDraw_Priv));
   if (!priv)
     return NULL;
   
   priv->module = LoadLibrary(L"ddraw.dll");
   if (!priv->module)
     {
        fprintf(stderr, "[Engine] [WinCE DDraw] Can not load ddraw.dll\n");
        goto free_priv;
     }

   lib_DirectDrawCreate = (fct_DirectDrawCreate)GetProcAddress(priv->module, L"DirectDrawCreate");
   if (!lib_DirectDrawCreate)
     {
        fprintf(stderr, "[Engine] [WinCE DDraw] Can not initialize DirectDraw\n");
        goto free_lib;
     }

   res = lib_DirectDrawCreate(NULL, (IUnknown**)&priv->object, NULL);
   if (FAILED(res))
     {
        fprintf(stderr, "[Engine] [WinCE DDraw] Can not create DirectDraw object\n");
        goto free_lib;
     }

   res = priv->object->SetCooperativeLevel(window, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
   if (FAILED(res))
     {
        fprintf(stderr, "[Engine] [WinCE DDraw] Can not set window to fullscreen\n");
        goto release_object;
     }

   memset(&surface_desc, 0, sizeof(surface_desc));
   surface_desc.dwSize = sizeof(surface_desc);
   surface_desc.dwFlags = DDSD_CAPS;
   surface_desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

   res = priv->object->CreateSurface(&surface_desc, &priv->surface, NULL);
   if (FAILED(res))
     {
        fprintf(stderr, "[Engine] [WinCE DDraw] Can not create surface\n");
        goto release_object;
     }

   memset(&surface_desc, 0, sizeof(surface_desc));
   surface_desc.dwSize = sizeof(surface_desc);
   res = priv->surface->Lock(NULL, &surface_desc, DDLOCK_READONLY, NULL);
   if (FAILED(res))
     {
        fprintf(stderr, "[Evas] [Engine] [WinCE DDraw] Can not lock surface\n");
        goto release_surface;
     }

   priv->width = surface_desc.dwWidth;
   priv->height = surface_desc.dwHeight;
   priv->stride = surface_desc.lPitch / 2;

   if ((priv->width != width) ||
       (priv->height != height))
     {
        fprintf(stderr, "[Engine] [WinCE DDraw] Size mismatch\n");
        fprintf(stderr, "[Engine] [WinCE DDraw] asked : %dx%d\n", width, height);
        fprintf(stderr, "[Engine] [WinCE DDraw] got   : %dx%d\n", priv->width, priv->height);
        goto release_surface;
     }

   res = priv->surface->Unlock(NULL);
   if (FAILED(res))
     {
        fprintf(stderr, "[Engine] [WinCE DDraw] Can not unlock surface\n");
        goto release_surface;
     }

   return priv;

 release_surface:
   priv->surface->Release();
 release_object:
   priv->object->Release();
 free_lib:
   FreeLibrary(priv->module);
 free_priv:
   free(priv);

  return 0;
}

void
evas_software_wince_ddraw_shutdown(void *priv)
{
   ((Evas_Engine_WinCE_DDraw_Priv *)priv)->surface->Release();
   ((Evas_Engine_WinCE_DDraw_Priv *)priv)->object->Release();
   FreeLibrary(((Evas_Engine_WinCE_DDraw_Priv *)priv)->module);
   free(priv);
}


FB_Output_Buffer *
evas_software_wince_ddraw_output_buffer_new(void *priv,
                                            int   width,
                                            int   height)
{
   FB_Output_Buffer *fbob;
   void             *buffer;

   fbob = (FB_Output_Buffer *)calloc(1, sizeof(FB_Output_Buffer));
   if (!fbob) return NULL;

   buffer = malloc (width * height * 2); /* we are sure to have 16bpp */
   if (!buffer)
     {
        free(fbob);
        return NULL;
     }

   fbob->priv = priv;

   fbob->im = (Soft16_Image *) evas_cache_image_data(evas_common_soft16_image_cache_get(), width, height, (DATA32 *)buffer, 0, EVAS_COLORSPACE_RGB565_A5P);
   if (fbob->im)
     fbob->im->stride = ((Evas_Engine_WinCE_DDraw_Priv *)priv)->stride;

   return fbob;
}

void
evas_software_wince_ddraw_output_buffer_free(FB_Output_Buffer *fbob)
{
   free(fbob->im->pixels);
   free(fbob);
}

void
evas_software_wince_ddraw_output_buffer_paste(FB_Output_Buffer *fbob)
{
   DDSURFACEDESC                 surface_desc;
   Evas_Engine_WinCE_DDraw_Priv *priv;
   HRESULT                       res;

   priv = (Evas_Engine_WinCE_DDraw_Priv *)fbob->priv;

   memset(&surface_desc, 0, sizeof(surface_desc));
   surface_desc.dwSize = sizeof(surface_desc);
   res = priv->surface->Lock(NULL, &surface_desc, DDLOCK_WRITEONLY, NULL);
   if (FAILED(res))
     return;

   if ((fbob->im->cache_entry.w == surface_desc.dwWidth) &&
       (fbob->im->cache_entry.h == surface_desc.dwHeight))
     memcpy(surface_desc.lpSurface, fbob->im->pixels,
            surface_desc.dwWidth * surface_desc.dwHeight * 2);

   priv->surface->Unlock(NULL);
}

void
evas_software_wince_ddraw_surface_resize(FB_Output_Buffer *fbob)
{
}
