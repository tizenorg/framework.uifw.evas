#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "evas_common.h"
#include "evas_xlib_image.h"
#include "evas_xlib_outbuf.h"

Eina_Bool
evas_xlib_image_shm_copy(RGBA_Image *im)
{
   Native *n = NULL;
   Display *d;
   Evas_X_Image *exim;
   Evas_Native_Surface *ns;

   if (im->native.data)
     n = im->native.data;
   exim = n->exim;
   d = n->d;
   ns = &(n->ns);
   if ((n) && (n->ns.type == EVAS_NATIVE_SURFACE_X11))
     {
        XGrabServer(d);
        if (!XShmGetImage(d, (Drawable)ns->data.x11.pixmap, exim->xim, 0, 0, 0xffffffff))
          {
             ERR("XShmGetImage failed.");
             XUngrabServer(d);
             return EINA_FALSE;
          }
        XUngrabServer(d);
        XSync(d, 0);
     }
   im->image.data = exim->data;
   return EINA_TRUE;
}

void
evas_xlib_image_free(Evas_X_Image *exim)
{
   XShmDetach(exim->dis, &exim->shminfo);
   XDestroyImage(exim->xim);
   exim->xim = NULL;
   shmdt(exim->shminfo.shmaddr);
   shmctl(exim->shminfo.shmid, IPC_RMID, 0);
}

Evas_X_Image *
evas_xlib_image_new(int w, int h, Visual *vis, int depth)
{
   Evas_X_Image *exim;

   exim = calloc(1, sizeof(Evas_X_Image));
   if (!exim)
     return NULL;

   exim->w = w;
   exim->h = h;
   exim->visual = vis;
   exim->depth = depth;
   return exim;
}

void
evas_xlib_image_shm_create(Evas_X_Image *exim, Display *display)
{
   exim->xim = XShmCreateImage(display, exim->visual, exim->depth,
                             ZPixmap, NULL, &(exim->shminfo),
                             exim->w, exim->h);
   if (!exim->xim)
   {
     ERR("XShmCreateImage failed.");
     return;
   }

   exim->shminfo.shmid = shmget(IPC_PRIVATE,
                              exim->xim->bytes_per_line * exim->xim->height,
                              IPC_CREAT | 0666);
   if (exim->shminfo.shmid == -1)
     {
        ERR("shmget failed.");
        XDestroyImage(exim->xim);
        exim->xim = NULL;
        return;
     }

   exim->shminfo.readOnly = False;
   exim->shminfo.shmaddr = shmat(exim->shminfo.shmid, 0, 0);
   exim->xim->data = exim->shminfo.shmaddr;
   if ((exim->xim->data == (char *)-1) ||
       (!exim->xim->data))
     {
        ERR("shmat failed.");
        shmdt(exim->shminfo.shmaddr);
        shmctl(exim->shminfo.shmid, IPC_RMID, 0);
        XDestroyImage(exim->xim);
        exim->xim = NULL;
        return;
     }

   XShmAttach(display, &exim->shminfo);
   exim->data = (unsigned char *)exim->xim->data;

   exim->bpl = exim->xim->bytes_per_line;
   exim->rows = exim->xim->height;
   if (exim->xim->bits_per_pixel <= 8)
     exim->bpp = 1;
   else if (exim->xim->bits_per_pixel <= 16)
     exim->bpp = 2;
   else
     exim->bpp = 4;
}

void *
evas_xlib_image_data_get(Evas_X_Image *exim,
                         int *bpl,
                         int *rows,
                         int *bpp,
                         Display *display)
{
   if (!exim->xim)
     {
        evas_xlib_image_shm_create(exim, display);
     }
   if (!exim->xim)
     {
        return NULL;
     }
   if (bpl) *bpl = exim->bpl;
   if (rows) *rows = exim->rows;
   if (bpp) *bpp = exim->bpp;
   exim->dis = display;

   return exim->data;
}

static void
_native_free_cb(void *data EINA_UNUSED, void *image)
{
   RGBA_Image *im = image;
   Native *n = im->native.data;

   //TODO: deal with pixmap hash
   if (n->exim)
     {
        evas_xlib_image_free(n->exim);
        n->exim = NULL;
     }
   n->visual = NULL;
   n->d = NULL;

   im->native.data        = NULL;
   im->native.func.data   = NULL;
   im->native.func.free   = NULL;
   im->image.data         = NULL;
   free(n);
}

void *
evas_xlib_image_native_set(void *data, void *image, void *native)
{
   Display *d = NULL;
   Visual  *vis = NULL;
   Pixmap   pm = 0;
   Native  *n = NULL;
   int      depth = 0;
   RGBA_Image *im = image, im2;
   int w, h;
   Evas_X_Image *exim;
   char* pix;
   Evas_Native_Surface *ns = native;
   Outbuf *ob = (Outbuf *)data;

   d = ob->priv.x11.xlib.disp;
   vis = ns->data.x11.visual;
   pm = ns->data.x11.pixmap;
   depth = ns->data.x11.depth;

   if (ns)
     {
        if (im->native.data)
          {
             //image have native surface already
             Evas_Native_Surface *ens = im->native.data;
             if ((ens->data.x11.visual == vis) &&
                 (ens->data.x11.pixmap == pm))
               return im;
          }
     }
   if ((!ns) && (!im->native.data)) return im;

   //Clean already existed native
   if (im->native.data)
     {
        if (im->native.func.free)
          im->native.func.free(im->native.func.data, im);
        //TODO:disable native image
     }
   if (!ns) return im;
   //TODO: deal with pixmap cache
   w = im->cache_entry.w;
   h = im->cache_entry.h;

   exim = evas_xlib_image_new(w, h, vis, depth);
   if (!exim)
     {
        ERR("evas_xlib_image_new failed.");
        return EINA_FALSE;
     }

   if (ns)
     {
        n = calloc(1, sizeof(Native));
        if (n)
          {
             memcpy(&(n->ns), ns, sizeof(Evas_Native_Surface));
             n->pixmap = pm;
             n->visual = vis;
             n->d = d;
             n->exim = exim;
             im->native.data = n;
             im->native.func.data = NULL;
             im->native.func.free = _native_free_cb;
          }
     }

   pix = evas_xlib_image_data_get(exim, NULL, NULL, NULL, d);
   if (pix == NULL)
   {
      ERR("evas_xlib_image_data_get failed.");
   }
   evas_xlib_image_shm_copy(im);
   return im;
}
