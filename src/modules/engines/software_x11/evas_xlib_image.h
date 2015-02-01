#include "evas_engine.h"

typedef struct _Evas_X_Image Evas_X_Image;
typedef struct _Native Native;

struct _Evas_X_Image
{
   Display         *dis;
   Visual          *visual;
   XImage          *xim;
   XShmSegmentInfo  shminfo;
   int              depth;
   int              w, h;
   int              bpl, bpp, rows;
   unsigned char   *data;
};

struct _Native
{
   Evas_Native_Surface ns;
   Pixmap              pixmap;
   Visual             *visual;
   Display            *d;

   Evas_X_Image       *exim;
};

Evas_X_Image *evas_xlib_image_new(int w, int h, Visual *vis, int depth);

void evas_xlib_image_free(Evas_X_Image *exim);
void evas_xlib_image_shm_create(Evas_X_Image *exim, Display *display);
Eina_Bool evas_xlib_image_shm_copy(RGBA_Image *im);
void *evas_xlib_image_data_get(Evas_X_Image *exim, int *bpl, int *rows, int *bpp, Display *display);
void *evas_xlib_image_native_set(void *data, void *image, void *native);
