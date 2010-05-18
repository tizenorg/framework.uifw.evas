#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_EVIL
# include <Evil.h>
#endif

#include <stdio.h>
#include <sys/stat.h>

#include "evas_common.h"
#include "evas_private.h"

#define EVAS_WINK_MODULE_NAME bmp
#include "evas_wink.h"

static Eina_Bool evas_image_load_file_head_bmp(Image_Entry *ie, const char *file, const char *key, int *error) EINA_ARG_NONNULL(1, 2, 4);
static Eina_Bool evas_image_load_file_data_bmp(Image_Entry *ie, const char *file, const char *key, int *error) EINA_ARG_NONNULL(1, 2, 4);

static Evas_Image_Load_Func evas_image_load_bmp_func =
{
  EINA_TRUE,
  evas_image_load_file_head_bmp,
  evas_image_load_file_data_bmp
};


typedef struct tagRGBQUAD {
	unsigned char       rgbBlue;
	unsigned char       rgbGreen;
	unsigned char       rgbRed;
	unsigned char       rgbReserved;
} RGBQUAD;

#define BI_RGB       0
#define BI_RLE8      1
#define BI_RLE4      2
#define BI_BITFIELDS 3

/* 21.3.3006 - Use enumeration for RLE encoding. This makes it more readable */
enum {
	RLE_NEXT = 0, /* Next line */
	RLE_END = 1,  /* End of RLE encoding */
	RLE_MOVE = 2  /* Move by X and Y (Offset is stored in two next bytes) */
};

# define IMAGE_DIMENSIONS_OK(w, h) \
   ( ((w) > 0) && ((h) > 0) && \
     ((unsigned long long)(w) * (unsigned long long)(w) <= (1ULL << 29) - 1) )

static int
ReadleShort(FILE * file, unsigned short *ret)
{
	unsigned char       b[2];

	if (fread(b, sizeof(unsigned char), 2, file) != 2)
		return 0;

	*ret = (b[1] << 8) | b[0];   
	return 1;
}

static int
ReadleLong(FILE * file, unsigned long *ret)
{
	unsigned char       b[4];

	if (fread(b, sizeof(unsigned char), 4, file) != 4)
		return 0;

	*ret = (b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0];
	return 1;
}

static Eina_Bool
evas_image_load_file_head_bmp(Image_Entry *ie, const char *file, const char *key __UNUSED__, int *error)
{
#ifdef USE_WINK_CODEC
   return evas_image_load_file_head_bmp_wink(ie, file, key, error);
#endif

   FILE               *f;
   char                type[2];
   unsigned long       size, offset, headSize;
   unsigned short      tmpShort;
   unsigned long       w, h;
 
   f = fopen(file, "rb");
   if (!f)
     {
      *error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
      return EINA_FALSE;
     }

   /* header */
   {
      struct stat         statbuf;

      if (stat(file, &statbuf) == -1)
        {
           *error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
           fclose(f);
           return EINA_FALSE;
        }
      size = statbuf.st_size;

      if (fread(type, 1, 2, f) != 2)
        {
           *error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
           fclose(f);
           return EINA_FALSE;
        }
      if (strncmp(type, "BM", 2))
        {
           *error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
           fclose(f);
           return EINA_FALSE;
        }

      fseek(f, 8, SEEK_CUR);
      ReadleLong(f, &offset);
      ReadleLong(f, &headSize);
      if (offset >= size)
        {
           *error = EVAS_LOAD_ERROR_CORRUPT_FILE;
           fclose(f);
           return EINA_FALSE;
        }
      if (headSize == 12)
        {
           ReadleShort(f, &tmpShort);
           w = tmpShort;
           ReadleShort(f, &tmpShort);
           h = tmpShort;
        }
      else if (headSize == 40)
        {
           ReadleLong(f, &w);
           ReadleLong(f, &h);
	}
   }

   ie->w = w;
   ie->h = (h > 0 ? h : -h);

   *error = EVAS_LOAD_ERROR_NONE;

   return EINA_TRUE;
}

static Eina_Bool
evas_image_load_file_data_bmp(Image_Entry *ie, const char *file, const char *key __UNUSED__, int *error)
{
#ifdef USE_WINK_CODEC
   return evas_image_load_file_data_bmp_wink(ie, file, key, error);
#endif

   FILE               *f;
   char                type[2];
   unsigned long       size, offset, headSize, comp, imgsize, j, k, l;
   unsigned short      tmpShort, planes, bitcount, ncols, skip;
   unsigned char       byte = 0, g, b, r;
   long       i, w, h;
   unsigned short      x, y;
   DATA32             *dst_data;
   DATA32             *ptr, *data_end;
   unsigned char      *buffer_ptr, *buffer, *buffer_end;
   RGBQUAD             rgbQuads[256];
   unsigned long       rmask = 0xff, gmask = 0xff, bmask = 0xff;
   unsigned long       rshift = 0, gshift = 0, bshift = 0;
   unsigned long       rleftshift = 0, gleftshift = 0, bleftshift = 0;

   /*
    * 21.3.2006:
    * Added these two variables for RLE.
    */
   unsigned char       byte1, byte2;

   f = fopen(file, "rb");
   if (!f)
     {
      *error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
      return EINA_FALSE;
     }

   /* header */
   {
      struct stat         statbuf;

      if (stat(file, &statbuf) == -1)
        {
           *error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
           fclose(f);
           return EINA_FALSE;
        }
      size = statbuf.st_size;

      if (fread(type, 1, 2, f) != 2)
        {
           *error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
           fclose(f);
           return EINA_FALSE;
        }
      if (strncmp(type, "BM", 2))
        {
           *error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
           fclose(f);
           return EINA_FALSE;
        }

      fseek(f, 8, SEEK_CUR);
      ReadleLong(f, &offset);
      ReadleLong(f, &headSize);
      if (offset >= size)
        {
           *error = EVAS_LOAD_ERROR_CORRUPT_FILE;
           fclose(f);
           return EINA_FALSE;
        }
      if (headSize == 12)
        {
           ReadleShort(f, &tmpShort);
           w = tmpShort;
           ReadleShort(f, &tmpShort);
           h = tmpShort;
           ReadleShort(f, &planes);
           ReadleShort(f, &bitcount);
           imgsize = size - offset;
           comp = BI_RGB;
        }
      else if (headSize == 40)
        {
           ReadleLong(f, &w);
           ReadleLong(f, &h);
           ReadleShort(f, &planes);
           ReadleShort(f, &bitcount);
           ReadleLong(f, &comp);
           ReadleLong(f, &imgsize);
           imgsize = size - offset;
           fseek(f, 16, SEEK_CUR);
        }
      else
        {
           *error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
           fclose(f);
           return EINA_FALSE;
        }

      h = h > 0 ? h : -h;

      if (!IMAGE_DIMENSIONS_OK(w, h))
        {
           *error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
           fclose(f);
           return EINA_FALSE;
        }

      if (bitcount < 16)
        {
           ncols = (offset - headSize - 14);
           if (headSize == 12)
             {
                ncols /= 3;
                if (ncols > 256) ncols = 256;
                for (i = 0; i < ncols; i++)
                   fread(&rgbQuads[i], 3, 1, f);
             }
           else
             {
                ncols /= 4;
                if (ncols > 256) ncols = 256;
                fread(rgbQuads, 4, ncols, f);
             }
        }
      else if (bitcount == 16 || bitcount == 32)
        {
           if (comp == BI_BITFIELDS)
             {
                int                 bit;

                ReadleLong(f, &rmask);
                ReadleLong(f, &gmask);
                ReadleLong(f, &bmask);
                for (bit = bitcount - 1; bit >= 0; bit--)
                  {
                     if (bmask & (1 << bit)) bshift = bit;
                     if (gmask & (1 << bit)) gshift = bit;
                     if (rmask & (1 << bit)) rshift = bit;
                  }
                while(((((0xffffL & bmask) >> bshift) << bleftshift) & 0x80) == 0)
                  {
                     bleftshift++;
                  }
                while(((((0xffffL & gmask) >> gshift) << gleftshift) & 0x80) == 0)
                  {
                     gleftshift++;
                  }
                while(((((0xffffL & rmask) >> rshift) << rleftshift) & 0x80) == 0)
                  {
                     rleftshift++;
                  }
              }
           else if (bitcount == 16)
             {
                rmask = 0x7C00;
                gmask = 0x03E0;
                bmask = 0x001F;
                rshift = 10;
                gshift = 5;
                bshift = 0;
                rleftshift = gleftshift = bleftshift = 3;
             }
           else if (bitcount == 32)
             {
                rmask = 0x00FF0000;
                gmask = 0x0000FF00;
                bmask = 0x000000FF;
                rshift = 16;
                gshift = 8;
                bshift = 0;
             }
        }

      ie->w = w;
      ie->h = h;
   }

   if (1)
     {
        fseek(f, offset, SEEK_SET);
        buffer = malloc(imgsize);
        if (!buffer)
          {
             *error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
             fclose(f);
             return EINA_FALSE;
          }

        evas_cache_image_surface_alloc(ie, ie->w, ie->h);
        dst_data = evas_cache_image_pixels(ie);

        fread(buffer, imgsize, 1, f);
        fclose(f);
        buffer_ptr = buffer;
        buffer_end = buffer + imgsize;

        data_end = dst_data + w * h;
        ptr = dst_data + ((h - 1) * w);

        if (bitcount == 1)
          {
             if (comp == BI_RGB)
               {
                  skip = ((((w + 31) / 32) * 32) - w) / 8;
                  for (y = 0; y < h; y++)
                    {
                       for (x = 0; x < w && buffer_ptr < buffer_end; x++)
                         {
                            if ((x & 7) == 0)
                               byte = *(buffer_ptr++);
                            k = (byte >> 7) & 1;
                            *ptr++ = 0xff000000 |
                                (rgbQuads[k].rgbRed << 16) |
                                (rgbQuads[k].rgbGreen << 8) |
                                rgbQuads[k].rgbBlue;
                            byte <<= 1;
                         }
                       buffer_ptr += skip;
                       ptr -= w * 2;
                   }
               }
          }

        /*
         * 21.3.2006
         * Bug fixes and optimization:
         * 
         * RLE encoding is dangerous and can be used by attackers by creating special files.
         * We has 'buffer_ptr' and 'buffer_end' variables and buffer_end points to first 
         * unaccessible byte in buffer.
         * - If we use 'byte = *(buffer_ptr++) in main loop we must check if 
         *   'buffer_ptr != buffer_end', because special or incomplete bmp file can generate
         *   segfault (I was writing it, because in RLE we need to read depending count of
		 *   bytes that depends on requester operation).
         *   SOLUTION: Don't read one byte, read two bytes and check.
         * - If RLE teels us than single color length will be larger than allowed, we can
         *   stop, because bitmap is corrupted or crawled.
         *   SOLUTION: Check for length ('l' variable in RLE) and break loop if it's invalid
         *   IMPROVEMENTS: We can stop checking if 'x' is out of rangle, because it never be.
         * - In RLE4 use one bigger loop that fills two pixels. This is faster and cleaner.
         *   If one pixel remains (the tail), do it on end of the loop.
         * - If we will check x and y (new line and skipping), we can't go outsize imlib
         *   image buffer.
         */

        if (bitcount == 4)
          {
             if (comp == BI_RLE4)
               {
                  /*
                   * 21.3.2006: This is better than using 'if buffer_ptr + 1 < buffer_end'
                   */
                  unsigned char *buffer_end_minus_1 = buffer_end - 1;
                  x = 0;
                  y = 0;

                  for (i = 0; i < imgsize && buffer_ptr < buffer_end_minus_1; i++)
                    {
                       byte1 = buffer_ptr[0];
                       byte2 = buffer_ptr[1];
                       buffer_ptr += 2;
                       if (byte1)
                         {
                            DATA32 t1, t2;

                            l = byte1;
                            /* Check for invalid length */
                            if (l + x > w) goto _bail;

                            t1 = 0xff000000 | (rgbQuads[byte2 >>  4].rgbRed   << 16) |
                                              (rgbQuads[byte2 >>  4].rgbGreen <<  8) |
                                              (rgbQuads[byte2 >>  4].rgbBlue       ) ;
                            t2 = 0xff000000 | (rgbQuads[byte2 & 0xF].rgbRed   << 16) |
                                              (rgbQuads[byte2 & 0xF].rgbGreen <<  8) |
                                              (rgbQuads[byte2 & 0xF].rgbBlue       ) ;
                            for (j = l/2; j; j--) {
                               ptr[0] = t1;
                               ptr[1] = t2;
                               ptr += 2;
                            }
                            /* tail */
                            if (l & 1) *ptr++ = t1;
                            x += l;
                         }
                       else
                         {
                            switch (byte2)
                              {
                                case RLE_NEXT:
                                   x = 0;
                                   if (++y >= h) goto _bail;
                                   ptr = dst_data + (h - y - 1) * w;
                                   break;
                                case RLE_END:
                                   goto _bail;
                                case RLE_MOVE:
                                   /* Need to read two bytes */
                                   if (buffer_ptr >= buffer_end_minus_1) goto _bail; 
                                   x += buffer_ptr[0];
                                   y += buffer_ptr[1];
                                   buffer_ptr += 2;
                                   /* Check for correct coordinates */
                                   if (x >= w) goto _bail;
                                   if (y >= h) goto _bail;
                                   ptr = dst_data + (h - y - 1) * w + x;
                                   break;
                                default:
                                   l = byte2;
                                   /* Check for invalid length and valid buffer size */
                                   if (l + x > w) goto _bail;
                                   if (buffer_ptr + (l >> 1) + (l & 1) > buffer_end) goto _bail;

                                   for (j = l/2; j; j--) {
                                     byte = *buffer_ptr++;
                                     ptr[0] = 0xff000000 | (rgbQuads[byte >>  4].rgbRed   << 16) |
                                                           (rgbQuads[byte >>  4].rgbGreen <<  8) |
                                                           (rgbQuads[byte >>  4].rgbBlue       ) ;
                                     ptr[1] = 0xff000000 | (rgbQuads[byte & 0xF].rgbRed   << 16) |
                                                           (rgbQuads[byte & 0xF].rgbGreen <<  8) |
                                                           (rgbQuads[byte & 0xF].rgbBlue       ) ;
                                     ptr += 2;
                                   }
                                   if (l & 1) {
                                     byte = *buffer_ptr++;
                                     *ptr++ = 0xff000000 | (rgbQuads[byte >>  4].rgbRed   << 16) |
                                                           (rgbQuads[byte >>  4].rgbGreen <<  8) |
                                                           (rgbQuads[byte >>  4].rgbBlue       ) ;
                                   }
                                   x += l;

                                   if ((l & 3) == 1)
                                      buffer_ptr += 2;
                                   else if ((l & 3) == 2)
                                      buffer_ptr++;
                                   break;
                              }
                         }
                    }
               }
             else if (comp == BI_RGB)
               {
                  skip = ((((w + 7) / 8) * 8) - w) / 2;
                  for (y = 0; y < h; y++)
                    {
                       for (x = 0; x < w && buffer_ptr < buffer_end; x++)
                         {
                            if ((x & 1) == 0)
                               byte = *(buffer_ptr++);
                            k = (byte & 0xF0) >> 4;
                            *ptr++ = 0xff000000 |
                                (rgbQuads[k].rgbRed << 16) |
                                (rgbQuads[k].rgbGreen << 8) |
                                rgbQuads[k].rgbBlue;
                            byte <<= 4;
                         }
                       buffer_ptr += skip;
                       ptr -= w * 2;
                   }
               }
          }
        if (bitcount == 8)
          {
             if (comp == BI_RLE8)
               {
                  /*
                   * 21.3.2006: This is better than using 'if buffer_ptr + 1 < buffer_end'
                   */
                  unsigned char *buffer_end_minus_1 = buffer_end - 1;
                  x = 0;
                  y = 0;
                  for (i = 0; i < imgsize && buffer_ptr < buffer_end_minus_1; i++)
                    {
                       byte1 = buffer_ptr[0];
                       byte2 = buffer_ptr[1];
                       buffer_ptr += 2;
                       if (byte1)
                         {
                            DATA32 pix = 0xff000000 | (rgbQuads[byte2].rgbRed   << 16) |
                                                      (rgbQuads[byte2].rgbGreen <<  8) |
                                                      (rgbQuads[byte2].rgbBlue       ) ;
                            l = byte1;
                            if (x + l > w) goto _bail;
                            for (j = l; j; j--) *ptr++ = pix;
                            x += l;
                         }
                       else
                         {
                            switch (byte2)
                              {
                                case RLE_NEXT:
                                   x = 0;
                                   if (++y >= h) goto _bail;
                                   ptr = dst_data + ((h - y - 1) * w) + x;
                                   break;
                                case RLE_END:
                                   goto _bail;
                                case RLE_MOVE:
                                   /* Need to read two bytes */
                                   if (buffer_ptr >= buffer_end_minus_1) goto _bail; 
                                   x += buffer_ptr[0];
                                   y += buffer_ptr[1];
                                   buffer_ptr += 2;
                                   /* Check for correct coordinates */
                                   if (x >= w) goto _bail;
                                   if (y >= h) goto _bail;
                                   ptr = dst_data + ((h - y - 1) * w) + x;
                                   break;
                                default:
                                   l = byte2;
                                   if (x + l > w) goto _bail;
                                   if (buffer_ptr + l > buffer_end) goto _bail;
                                   for (j = 0; j < l; j++)
                                     {
                                        byte = *(buffer_ptr++);

                                        *ptr++ = 0xff000000 |
                                            (rgbQuads[byte].rgbRed << 16) |
                                            (rgbQuads[byte].rgbGreen << 8) |
                                            rgbQuads[byte].rgbBlue;
                                     }
                                   x += l;
                                   if (l & 1)
                                      buffer_ptr++;
                                   break;
                              }
                         }
                    }
              }
             else if (comp == BI_RGB)
               {
                  skip = (((w + 3) / 4) * 4) - w;
                  for (y = 0; y < h; y++)
                    {
                       for (x = 0; x < w && buffer_ptr < buffer_end; x++)
                         {
                            byte = *(buffer_ptr++);
                            *ptr++ = 0xff000000 |
                                (rgbQuads[byte].rgbRed << 16) |
                                (rgbQuads[byte].rgbGreen << 8) |
                                rgbQuads[byte].rgbBlue;
                         }
                       ptr -= w * 2;
                       buffer_ptr += skip;
                   }
               }

          }
        else if (bitcount == 16)
          {
             /* 21.3.2006 - Need to check for buffer_ptr + 1 < buffer_end */
             unsigned char *buffer_end_minus_1 = buffer_end - 1;
             skip = (((w * 16 + 31) / 32) * 4) - (w * 2);
             for (y = 0; y < h; y++)
               {
                  for (x = 0; x < w && buffer_ptr < buffer_end_minus_1; x++)
                    {
                       /*
                        * THIS WAS OLD CODE 
                        *
                        * r = ((unsigned short)(*buffer_ptr) & rmask) >> rshift;
                        * g = ((unsigned short)(*buffer_ptr) & gmask) >> gshift;
                        * b = ((unsigned short)(*(buffer_ptr++)) & bmask) >>
                        *   bshift;
                        * *ptr++ = 0xff000000 | (r << 16) | (g << 8) | b;
                        */
                       unsigned short pix = *(unsigned short *)buffer_ptr;
                       *ptr++ = 0xff000000 | ((((pix & rmask) >> rshift) << rleftshift) << 16) |
                                             ((((pix & gmask) >> gshift) << gleftshift) <<  8) |
                                             ((((pix & bmask) >> bshift) << bleftshift)      ) ;
                       buffer_ptr += 2;
                    }
                  ptr -= w * 2;
                  buffer_ptr += skip;
              }
          }
        else if (bitcount == 24)
          {
             /* 21.3.2006 - Fix: need to check for buffer_ptr + 2 < buffer_end */
             unsigned char *buffer_end_minus_2 = buffer_end - 2;
             skip = (4 - ((w * 3) % 4)) & 3;
             for (y = 0; y < h; y++)
               {
                  for (x = 0; x < w && buffer_ptr < buffer_end_minus_2; x++)
                    {
                       b = *(buffer_ptr++);
                       g = *(buffer_ptr++);
                       r = *(buffer_ptr++);
                       *ptr++ = 0xff000000 | (r << 16) | (g << 8) | b;
                    }
                  ptr -= w * 2;
                  buffer_ptr += skip;
              }
          }
        else if (bitcount == 32)
          {
             /* 21.3.2006 - Need to check buffer_ptr + 3 < buffer_end */
             unsigned char *buffer_end_minus_3 = buffer_end_minus_3;
             skip = (((w * 32 + 31) / 32) * 4) - (w * 4);
             for (y = 0; y < h; y++)
               {
                  for (x = 0; x < w && buffer_ptr < buffer_end_minus_3; x++)
                    {
                       /*
                        * THIS WAS OLD CODE: I don't understand it and it's invalid.
                        *
                        * r = ((unsigned long)(*buffer_ptr) & rmask) >> rshift;
                        * g = ((unsigned long)(*buffer_ptr) & gmask) >> gshift;
                        * b = ((unsigned long)(*buffer_ptr) & bmask) >> bshift;
                        * *ptr++ = 0xff000000 | (r << 16) | (g << 8) | b;
                        * r = *(buffer_ptr++);
                        * r = *(buffer_ptr++);
                        */

                       /* TODO: What about alpha channel...Is used? */
                       DATA32 pix = *(unsigned int *)buffer_ptr;
                       *ptr++ = 0xff000000 | (((pix & rmask) >> rshift) << 16) |
                                             (((pix & gmask) >> gshift) <<  8) |
                                             (((pix & bmask) >> bshift)      ) ; 
                       buffer_ptr += 4;
                    }
                  ptr -= w * 2;
                  buffer_ptr += skip;
              }
          }
_bail:
        free(buffer);
     }

   return EINA_TRUE;
}

static int
module_open(Evas_Module *em)
{
   if (!em) return 0;
   em->functions = (void *)(&evas_image_load_bmp_func);
   return 1;
}

static void
module_close(Evas_Module *em)
{
}

static Evas_Module_Api evas_modapi =
{
  EVAS_MODULE_API_VERSION,
  "bmp",
  "none",
  {
    module_open,
    module_close
  }
};

EVAS_MODULE_DEFINE(EVAS_MODULE_TYPE_IMAGE_LOADER, image_loader, bmp);

#ifndef EVAS_STATIC_BUILD_BMP
EVAS_EINA_MODULE_DEFINE(image_loader, bmp);
#endif
