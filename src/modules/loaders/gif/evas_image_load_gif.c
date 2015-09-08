#include "evas_common.h"
#include "evas_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <gif_lib.h>

#ifndef MIN
# define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#define byte2_to_int(a,b)         (((b)<<8)|(a))
#define FRAME_MAX 1024

#define LOADERR(x) \
do { \
   *error = (x); \
     printf("loaderr\n"); \
   goto on_error; \
} while (0)

#define PIX(_x, _y) rows[yin + _y][xin + _x]
#define CMAP(_v) cmap->Colors[_v]
#define PIXLK(_p) ARGB_JOIN(0xff, CMAP(_p).Red, CMAP(_p).Green, CMAP(_p).Blue)


typedef struct _Frame_Info Frame_Info;

struct _Frame_Info
{
   int x, y, w, h;
   unsigned short delay;
   short transparent : 10;
   short dispose : 6;
   short interlace : 1;
};


static Image_Entry_Frame *
_find_frame(Image_Entry *ie, int index)
{
   Eina_List *l;
   Image_Entry_Frame *frame;

   if (!ie) return EINA_FALSE;
   if (!ie->frames) return EINA_FALSE;

   EINA_LIST_FOREACH(ie->frames, l, frame)
      if (frame->index == index) return frame;
   return NULL;
}

// fill in am image with a specific rgba color value
static void
_fill_image(DATA32 *data, int rowpix, DATA32 val, int x, int y, int w, int h)
{
   int xx, yy;
   DATA32 *p;

   for (yy = 0; yy < h; yy++)
     {
        p = data + ((y + yy) * rowpix) + x;
        for (xx = 0; xx < w; xx++)
          {
             *p = val;
             p++;
          }
     }
}

static void
_clip_coords(int imw, int imh, int *xin, int *yin,
             int x0, int y0, int w0, int h0,
             int *x, int *y, int *w, int *h)
{
   if (x0 < 0)
     {
        w0 += x0;
        *xin = -x0;
        x0 = 0;
     }
   if ((x0 + w0) > imw) w0 = imw - x0;
   if (y0 < 0)
     {
        h0 += y0;
        *yin = -y0;
        y0 = 0;
     }
   if ((y0 + h0) > imh) h0 = imh - y0;
   *x = x0;
   *y = y0;
   *w = w0;
   *h = h0;
}

static void
_fill_frame(DATA32 *data, int rowpix, GifFileType *gif, Frame_Info *finfo,
            int x, int y, int w, int h)
{
   // solid color fill for pre frame region
   if (finfo->transparent < 0)
     {
        ColorMapObject *cmap;
        int bg;

        // work out color to use from cmap
        if (gif->Image.ColorMap) cmap = gif->Image.ColorMap;
        else cmap = gif->SColorMap;
        bg = gif->SBackGroundColor;
        // and do the fill
        _fill_image
          (data, rowpix,
           ARGB_JOIN(0xff, CMAP(bg).Red, CMAP(bg).Green, CMAP(bg).Blue),
           x, y, w, h);
     }
   // fill in region with 0 (transparent)
   else
     _fill_image(data, rowpix, 0, x, y, w, h);
}

// store common fields from gif file info into frame info
static void
_store_frame_info(GifFileType *gif, Frame_Info *finfo)
{
   finfo->x = gif->Image.Left;
   finfo->y = gif->Image.Top;
   finfo->w = gif->Image.Width;
   finfo->h = gif->Image.Height;
   finfo->interlace = gif->Image.Interlace;
}

// check if image fills "screen space" and if so, if it is transparent
// at all then the image could be transparent - OR if image doesnt fill,
// then it could be trasnparent (full coverage of screen). some gifs will
// be recognized as solid here for faster rendering, but not all.
static void
_check_transparency(Eina_Bool *full, Frame_Info *finfo, int w, int h)
{
   if ((finfo->x == 0) && (finfo->y == 0) &&
       (finfo->w == w) && (finfo->h == h))
     {
        if (finfo->transparent >= 0) *full = EINA_FALSE;
     }
   else *full = EINA_FALSE;
}

// allocate frame and frame info and append to list and store fields
static Frame_Info *
_new_frame(Image_Entry *ie,
           int transparent, int dispose, int delay,
           int index)
{
   Image_Entry_Frame *frame;
   Frame_Info *finfo;

   // allocate frame and frame info data (MUSt be separate)
   frame = calloc(1, sizeof(Image_Entry_Frame));
   if (!frame) return NULL;
   finfo = calloc(1, sizeof(Frame_Info));
   if (!finfo)
     {
        free(frame);
        return NULL;
     }
   // record transparent index to be used or -1 if none
   // for this SPECIFIC frame
   finfo->transparent = transparent;
   // record dispose mode (3 bits)
   finfo->dispose = dispose;
   // record delay (2 bytes so max 65546 /100 sec)
   finfo->delay = delay;
   // record the index number we are at
   frame->index = index;
   // that frame is stored AT image/screen size
   frame->info = finfo;
   ie->frames = eina_list_append(ie->frames, frame);
   return finfo;
}

static Eina_Bool
_decode_image(GifFileType *gif, DATA32 *data, int rowpix, int xin, int yin,
              int transparent, int x, int y, int w, int h, Eina_Bool fill)
{
   int intoffset[] = { 0, 4, 2, 1 };
   int intjump[] = { 8, 8, 4, 2 };
   int i, xx, yy, pix;
   GifRowType *rows;
   Eina_Bool ret = EINA_FALSE;
   ColorMapObject *cmap;
   DATA32 *p;

   // build a blob of memory to have pointers to rows of pixels
   // AND store the decoded gif pixels (1 byte per pixel) as welll
   rows = malloc((h * sizeof(GifRowType *)) + (w * h * sizeof(GifPixelType)));
   if (!rows) goto on_error;

   // fill in the pointers at the start
   for (yy = 0; yy < h; yy++)
     {
        rows[yy] = ((unsigned char *)rows) + (h * sizeof(GifRowType *)) +
          (yy * w * sizeof(GifPixelType));
     }

   // if give is interlaced, walk interlace pattern and decode into rows
   if (gif->Image.Interlace)
     {
        for (i = 0; i < 4; i++)
          {
             for (yy = intoffset[i]; yy < h; yy += intjump[i])
               {
                  if (DGifGetLine(gif, rows[yy], w) != GIF_OK)
                    goto on_error;
               }
          }
     }
   // normal top to bottom - decode into rows
   else
     {
        for (yy = 0; yy < h; yy++)
          {
             if (DGifGetLine(gif, rows[yy], w) != GIF_OK)
               goto on_error;
          }
     }

   // work out what colormap to use
   if (gif->Image.ColorMap) cmap = gif->Image.ColorMap;
   else cmap = gif->SColorMap;

   // if we need to deal with transparent pixels at all...
   if (transparent >= 0)
     {
        // if we are told to FILL (overwrite with transparency kept)
        if (fill)
          {
             for (yy = 0; yy < h; yy++)
               {
                  p = data + ((y + yy) * rowpix) + x;
                  for (xx = 0; xx < w; xx++)
                    {
                       pix = PIX(xx, yy);
                       if (pix != transparent) *p = PIXLK(pix);
                       else *p = 0;
                       p++;
                    }
               }
          }
        // paste on top with transparent pixels untouched
        else
          {
             for (yy = 0; yy < h; yy++)
               {
                  p = data + ((y + yy) * rowpix) + x;
                  for (xx = 0; xx < w; xx++)
                    {
                       pix = PIX(xx, yy);
                       if (pix != transparent) *p = PIXLK(pix);
                       p++;
                    }
               }
          }
     }
   else
     {
        // walk pixels without worring about transparency at all
        for (yy = 0; yy < h; yy++)
          {
             p = data + ((y + yy) * rowpix) + x;
             for (xx = 0; xx < w; xx++)
               {
                  pix = PIX(xx, yy);
                  *p = PIXLK(pix);
                  p++;
               }
          }
     }
   ret = EINA_TRUE;

on_error:
   free(rows);
   return ret;
}

static void
_flush_older_frames(Image_Entry *ie,
                    int w, int h,
                    Image_Entry_Frame *thisframe,
                    Image_Entry_Frame *prevframe)
{
   Eina_List *l;
   Image_Entry_Frame *frame;
   // target is the amount of memory we want to be under for stored frames
   int total = 0, target = 512 * 1024;

   // total up the amount of memory used by stored frames for this image
   EINA_LIST_FOREACH(ie->frames, l, frame)
     {
        if (frame->data) total++;
     }
   total *= (w * h * sizeof(DATA32));
   // if we use less than target (512k) for frames - dont flush
   if (total < target) return;
   // clean oldest frames first and go until below target or until we loop
   // around back to this frame (curent)
   EINA_LIST_FOREACH(ie->frames, l, frame)
     {
        if (frame == thisframe) break;
     }
   if (!l) return;
   // start on next frame after thisframe
   l = l->next;
   // handle wrap to start
   if (!l) l = ie->frames;
   // now walk until we hit thisframe again... then stop walk.
   while (l)
     {
        frame = l->data;
        if (frame == thisframe) break;
        if (frame->data)
          {
             if ((frame != thisframe) && (frame != prevframe))
               {
                  free(frame->data);
                  frame->data = NULL;
                  // subtract memory used and if below target - stop flush
                  total -= (w * h * sizeof(DATA32));
                  if (total < target) break;
               }
          }
        // go to next - handle wrap to start
        l = l->next;
        if (!l) l = ie->frames;
     }
}

static int
_gif_file_open(const char *file, GifFileType **gif)
{
   int fd;
#ifndef __EMX__
   fd = open(file, O_RDONLY);
#else
   fd = open(file, O_RDONLY | O_BINARY);
#endif
   if (fd < 0) return EVAS_LOAD_ERROR_DOES_NOT_EXIST;

   *gif = DGifOpenFileHandle(fd);
   if (!(*gif))
     {
        return EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
     }

   return EVAS_LOAD_ERROR_NONE;
}

static Eina_Bool
evas_image_load_file_head_gif(Image_Entry *ie,
                              const char *file,
                              const char *key __UNUSED__,
                              int *error)
{
   GifFileType *gif = NULL;
   GifRecordType rec;
   int w, h;
   int loop_count = -1;
   //it is possible which gif file have error midle of frames,
   //in that case we should play gif file until meet error frame.
   int image_count = 0;
   Frame_Info *finfo = NULL;
   Eina_Bool full = EINA_TRUE;
   Eina_Bool ret = EINA_FALSE;

   // 1. ask libgif to open the file
   int err = _gif_file_open(file, &gif);
   if (err) LOADERR(err);

   // 2. get the gif screen size (the actual image size)
   w = gif->SWidth;
   h = gif->SHeight;

   // 3. if size is invalid - abort here
   if ((w < 1) || (h < 1) || (w > IMG_MAX_SIZE) || (h > IMG_MAX_SIZE) ||
       IMG_TOO_BIG(w, h))
     {
        if (IMG_TOO_BIG(w, h))
          LOADERR(EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED);
        LOADERR(EVAS_LOAD_ERROR_GENERIC);
     }

   ie->w = w;
   ie->h = h;

   // 4. get info
   do
     {
        if (DGifGetRecordType(gif, &rec) == GIF_ERROR)
          {
             if (image_count > 1) break;
             LOADERR(EVAS_LOAD_ERROR_UNKNOWN_FORMAT);
          }
        // get image description section
        if (rec == IMAGE_DESC_RECORD_TYPE)
          {
             int img_code;
             GifByteType *img;

             if (DGifGetImageDesc(gif) == GIF_ERROR)
               LOADERR(EVAS_LOAD_ERROR_UNKNOWN_FORMAT);
             if (DGifGetCode(gif, &img_code, &img) == GIF_ERROR)
               LOADERR(EVAS_LOAD_ERROR_UNKNOWN_FORMAT);
             while (img)
               {
                  img = NULL;
                  DGifGetCodeNext(gif, &img);
               }
             if (finfo)
               {
                  _store_frame_info(gif, finfo);
                  _check_transparency(&full, finfo, w, h);
               }
             else
               {
                  finfo = _new_frame(ie, -1, 0, 0, image_count + 1);
                  if (!finfo)
                    LOADERR(EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED);
                  _store_frame_info(gif, finfo);
                  _check_transparency(&full, finfo, w, h);
               }
             image_count++;
          }
        else if (rec == EXTENSION_RECORD_TYPE)
          {
             int ext_code;
             GifByteType *ext;

             ext = NULL;
             DGifGetExtension(gif, &ext_code, &ext);
             while (ext)
               {
                  if (ext_code == 0xf9) /* Graphic Control Extension */
                    {
                       finfo = _new_frame
                         (ie,
                          (ext[1] & 1) ? ext[4] : -1, // transparency index
                          (ext[1] >> 2) & 0x7, // dispose mode
                          ((int)ext[3] << 8) | (int)ext[2], // delay
                          image_count + 1);
                       if (!finfo)
                         LOADERR(EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED);
                    }
                  else if (ext_code == 0xff) /* application extension */
                    {
                       if (!strncmp ((char*)(&ext[1]), "NETSCAPE2.0", 11) ||
                           !strncmp ((char*)(&ext[1]), "ANIMEXTS1.0", 11))
                         {
                            ext=NULL;
                            DGifGetExtensionNext(gif, &ext);

                            if (ext[1] == 0x01)
                              {
                                 loop_count = ext[2] + (ext[3] << 8);
                                 if (loop_count > 0) loop_count++;
                              }
                         }
                     }

                  ext = NULL;
                  DGifGetExtensionNext(gif, &ext);
               }
          }
   } while (rec != TERMINATE_RECORD_TYPE);

   if ((gif->ImageCount > 1) || (image_count > 1))
     {
        ie->flags.animated = 1;
        ie->loop_count = loop_count;
        ie->loop_hint = EVAS_IMAGE_ANIMATED_HINT_LOOP;
        ie->frame_count = MIN(gif->ImageCount, image_count);
     }
   if (!full) ie->flags.alpha = 1;
   ie->cur_frame = 1;

   *error = EVAS_LOAD_ERROR_NONE;
   ret = EINA_TRUE;

on_error:
   if (gif) DGifCloseFile(gif);
   return ret;
}

static Eina_Bool
evas_image_load_file_data_gif(Image_Entry *ie,
                              const char *file,
                              const char *key __UNUSED__,
                              int *error)
{
   Eina_Bool ret = EINA_FALSE;
   GifRecordType rec;
   GifFileType *gif = NULL;
   Image_Entry_Frame *frame = NULL;
   int index = 0, imgnum = 0;
   Frame_Info *finfo;

   // 1. index set
   index = ie->cur_frame;

   if ((ie->flags.animated) &&
       ((index <= 0) || (index > FRAME_MAX) || (index > ie->frame_count)))
     LOADERR(EVAS_LOAD_ERROR_GENERIC);

   frame = _find_frame(ie, index);
   if (frame)
     {
        if ((frame->loaded) && (frame->data))
          // frame is already there and decoded - jump to end
          goto on_ok;
     }
   else
     LOADERR(EVAS_LOAD_ERROR_CORRUPT_FILE);

   // 2. ask libgif to open the file
   int err = _gif_file_open(file, &gif);
   if (err) LOADERR(err);

   // always start from the first frame - don't know the previous frame
   imgnum = 1;
   do
     {
        if (DGifGetRecordType(gif, &rec) == GIF_ERROR)
          LOADERR(EVAS_LOAD_ERROR_UNKNOWN_FORMAT);
        if (rec == EXTENSION_RECORD_TYPE)
          {
             int ext_code;
             GifByteType *ext;

             ext = NULL;
             DGifGetExtension(gif, &ext_code, &ext);
             while (ext)
               {
                  ext = NULL;
                  DGifGetExtensionNext(gif, &ext);
               }
          }
        else if (rec == IMAGE_DESC_RECORD_TYPE)
          {
             int xin = 0, yin = 0, x = 0, y = 0, w = 0, h = 0;
             int img_code;
             GifByteType *img;
             Image_Entry_Frame *prevframe = NULL;
             Image_Entry_Frame *thisframe = NULL;

             // get image desc
             if (DGifGetImageDesc(gif) == GIF_ERROR)
               LOADERR(EVAS_LOAD_ERROR_UNKNOWN_FORMAT);
             // get the previous frame entry AND the current one to fill in
             prevframe = _find_frame(ie, imgnum - 1);
             thisframe = _find_frame(ie, imgnum);

             // CASE 1: no data & animated
             if ((thisframe) && (!thisframe->data) && (ie->flags.animated))
               {
                  Eina_Bool first = EINA_FALSE;

                  // allocate it
                  thisframe->data =
                    malloc(ie->w * ie->h * sizeof(DATA32));
                  if (!thisframe->data)
                    LOADERR(EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED);
                  // if we have no prior frame OR prior frame data... empty
                  if ((!prevframe) || (!prevframe->data))
                    {
                       first = EINA_TRUE;
                       finfo = thisframe->info;
                       memset(thisframe->data, 0,
                              ie->w * ie->h * sizeof(DATA32));
                    }
                  // we have a prior frame to copy data from...
                  else
                    {
                       finfo = prevframe->info;

                       // fix coords of sub image in case it goes out...
                       _clip_coords(ie->w, ie->h, &xin, &yin,
                                    finfo->x, finfo->y, finfo->w, finfo->h,
                                    &x, &y, &w, &h);
                       // if dispose mode is not restore - then copy pre frame
                       if (finfo->dispose != 3) // GIF_DISPOSE_RESTORE
                         memcpy(thisframe->data, prevframe->data,
                              ie->w * ie->h * sizeof(DATA32));
                       // if dispose mode is "background" then fill with bg
                       if (finfo->dispose == 2) // GIF_DISPOSE_BACKGND
                         _fill_frame(thisframe->data, ie->w, gif,
                                     finfo, x, y, w, h);
                       else if (finfo->dispose == 3) // GIF_DISPOSE_RESTORE
                         {
                            Image_Entry_Frame *prevframe2;

                            // we need to copy data from one frame back
                            // from the prev frame into the current frame
                            // (copy the whole image - at least the sample
                            // GifWin.cpp from libgif indicates this is what
                            // needs doing
                            prevframe2 = _find_frame(ie, imgnum - 2);
                            if (prevframe2)
                              memcpy(thisframe->data, prevframe2->data,
                                     ie->w * ie->h * sizeof(DATA32));
                         }
                    }
                  // now draw this frame on top
                  finfo = thisframe->info;
                  _clip_coords(ie->w, ie->h, &xin, &yin,
                               finfo->x, finfo->y, finfo->w, finfo->h,
                               &x, &y, &w, &h);
                  if (!_decode_image(gif, thisframe->data, ie->w,
                                     xin, yin, finfo->transparent,
                                     x, y, w, h, first))
                    LOADERR(EVAS_LOAD_ERROR_CORRUPT_FILE);
                  // mark as loaded and done
                  thisframe->loaded = EINA_TRUE;
                  // and flush old memory if needed (too much)
                  _flush_older_frames(ie, ie->w, ie->h,
                                      thisframe, prevframe);
               }
             // CASE 2
             else if ((thisframe) && (!thisframe->data) &&
                      (!ie->flags.animated))
               {
                  // if we don't have the data decoded yet - decode it
                  if ((!thisframe->loaded) || (!thisframe->data))
                    {
                       DATA32 *pixels;
                       if (!evas_cache_image_pixels(ie))
                         evas_cache_image_surface_alloc(ie, ie->w, ie->h);
                       pixels = evas_cache_image_pixels(ie);

                       if (!pixels)
                         LOADERR(EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED);

                       // use frame info but we WONT allocate frame pixels
                       finfo = thisframe->info;
                       _clip_coords(ie->w, ie->h, &xin, &yin,
                                    finfo->x, finfo->y, finfo->w, finfo->h,
                                    &x, &y, &w, &h);
                       // clear out all pixels
                       _fill_frame(pixels, ie->w, gif,
                                   finfo, 0, 0, ie->w, ie->h);
                       // and decode the gif with overwriting
                       if (!_decode_image(gif, pixels, ie->w,
                                          xin, yin, finfo->transparent,
                                          x, y, w, h, EINA_TRUE))
                         LOADERR(EVAS_LOAD_ERROR_CORRUPT_FILE);
                       // mark as loaded and done
                       thisframe->loaded = EINA_TRUE;
                    }
               }
             // CASE 3
             else
               {
                  // skip decoding and just walk image to next
                  if (DGifGetCode(gif, &img_code, &img) == GIF_ERROR)
                    LOADERR(EVAS_LOAD_ERROR_UNKNOWN_FORMAT);
                  while (img)
                    {
                       img = NULL;
                       DGifGetCodeNext(gif, &img);
                    }
               }
             if (imgnum >= index) break;
             imgnum++;
          }
     }
   while (rec != TERMINATE_RECORD_TYPE);

on_ok:
   if ((ie->flags.animated) && (frame->data))
     {
        DATA32 *pixels;
        if (!evas_cache_image_pixels(ie))
          evas_cache_image_surface_alloc(ie, ie->w, ie->h);
        pixels = evas_cache_image_pixels(ie);

        if (!pixels)
          LOADERR(EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED);
        memcpy(pixels, frame->data, ie->w * ie->h * sizeof(DATA32));
     }

   *error = EVAS_LOAD_ERROR_NONE;
   ret = EINA_TRUE;

on_error:
   if (gif) DGifCloseFile(gif);
   return ret;
}

static double
evas_image_load_frame_duration_gif(Image_Entry *ie,
                                   const char *file,
                                   const int start_frame,
                                   const int frame_num)
{
   GifFileType        *gif;
   GifRecordType       rec;
   int                 current_frame = 1;
   int                 remain_frames = frame_num;
   double              duration = 0;

   if (!ie->flags.animated) return -1;
   if ((start_frame + frame_num) > ie->frame_count) return -1;
   if (frame_num < 0) return -1;

   if (_gif_file_open(file, &gif)) return -1;

   do
     {
        if (DGifGetRecordType(gif, &rec) == GIF_ERROR)
          {
             rec = TERMINATE_RECORD_TYPE;
          }
        if (rec == IMAGE_DESC_RECORD_TYPE)
          {
             int                 img_code;
             GifByteType        *img;

             if (DGifGetImageDesc(gif) == GIF_ERROR)
               {
                  /* PrintGifError(); */
                  rec = TERMINATE_RECORD_TYPE;
               }
             current_frame++;
             /* we have to count frame, so use DGifGetCode and skip decoding */
             if (DGifGetCode(gif, &img_code, &img) == GIF_ERROR)
               {
                  rec = TERMINATE_RECORD_TYPE;
               }
             while (img)
               {
                  img = NULL;
                  DGifGetExtensionNext(gif, &img);
               }
          }
        else if (rec == EXTENSION_RECORD_TYPE)
          {
             int                 ext_code;
             GifByteType        *ext;

             ext = NULL;
             DGifGetExtension(gif, &ext_code, &ext);
             while (ext)
               {
                  if (ext_code == 0xf9) /* Graphic Control Extension */
                    {
                       if ((current_frame  >= start_frame) && (current_frame <= ie->frame_count))
                         {
                            int frame_duration = 0;
                            if (remain_frames < 0) break;
                            frame_duration = byte2_to_int (ext[2], ext[3]);
                            if (frame_duration == 0)
                              duration += 0.1;
                            else
                              duration += (double)frame_duration/100;
                            remain_frames --;
                         }
                    }
                  ext = NULL;
                  DGifGetExtensionNext(gif, &ext);
               }
         }
     } while (rec != TERMINATE_RECORD_TYPE);

   DGifCloseFile(gif);
   return duration;
}

static Evas_Image_Load_Func evas_image_load_gif_func =
{
  EINA_TRUE,
  evas_image_load_file_head_gif,
  evas_image_load_file_data_gif,
  evas_image_load_frame_duration_gif,
  EINA_FALSE
};

static int
module_open(Evas_Module *em)
{
   if (!em) return 0;
   em->functions = (void *)(&evas_image_load_gif_func);
   return 1;
}

static void
module_close(Evas_Module *em __UNUSED__)
{
}

static Evas_Module_Api evas_modapi =
{
  EVAS_MODULE_API_VERSION,
  "gif",
  "none",
  {
    module_open,
    module_close
  }
};

EVAS_MODULE_DEFINE(EVAS_MODULE_TYPE_IMAGE_LOADER, image_loader, gif);

#ifndef EVAS_STATIC_BUILD_GIF
EVAS_EINA_MODULE_DEFINE(image_loader, gif);
#endif
