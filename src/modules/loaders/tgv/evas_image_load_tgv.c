#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef _WIN32
# include <winsock2.h>
#endif /* ifdef _WIN32 */

#include "lz4.h"
#include "rg_etc1.h"
#include "evas_common.h"
#include "evas_private.h"

#ifdef BUILD_NEON
#include <arm_neon.h>
#endif

/**************************************************************
 * The TGV file format is oriented around compression mecanism
 * that hardware are good at decompressing. We do still provide
 * a fully software implementation in case your hardware doesn't
 * handle it. As OpenGL is pretty bad at handling border of
 * texture, we do duplicate the first pixels of every border.
 *
 * This file format is designed to compress/decompress things
 * in block area. Giving opportunity to store really huge file
 * and only decompress/compress them as we need. Note that region
 * only work with software decompression as we don't have a sane
 * way to duplicate border to avoid artifact when scaling texture.
 *
 * The file format is as follow :
 * - char     magic[4]: "TGV1"
 * - uint8_t  block_size (real block size = (4 << bits[0-3], 4 << bits[4-7])
 * - uint8_t  algorithm (0 -> ETC1, 1 -> ETC2 RGB, 2 -> ETC2 RGBA, 3 -> ETC1+Alpha)
 * - uint8_t  options[2] (bitmask: 1 -> lz4, 2 for block-less, 4 -> unpremultiplied)
 * - uint32_t width
 * - uint32_t height
 * - blocks[]
 *   - 0 length encoded compress size (if length == 64 * block_size => no compression)
 *   - lzma encoded etc1 block
 *
 * If the format is ETC1+Alpha (algo = 3), then a second image is encoded
 * in ETC1 right after the first one, and it contains grey-scale alpha
 * values.
 **************************************************************/

// FIXME: wondering if we should support mipmap
// TODO: support ETC1+ETC2 images (RGB only)

static const Evas_Colorspace cspaces_etc1[2] = {
  EVAS_COLORSPACE_ETC1,
  EVAS_COLORSPACE_ARGB8888
};

static const Evas_Colorspace cspaces_rgb8_etc2[2] = {
  EVAS_COLORSPACE_RGB8_ETC2,
  EVAS_COLORSPACE_ARGB8888
};

static const Evas_Colorspace cspaces_rgba8_etc2_eac[2] = {
  EVAS_COLORSPACE_RGBA8_ETC2_EAC,
  EVAS_COLORSPACE_ARGB8888
};

static const Evas_Colorspace cspaces_etc1_alpha[2] = {
  EVAS_COLORSPACE_ETC1_ALPHA,
  EVAS_COLORSPACE_ARGB8888
};

static int
roundup(int val, int rup)
{
   if (val >= 0 && rup > 0)
     return (val + rup - 1) - ((val + rup - 1) % rup);
   return 0;
}

#define OFFSET_BLOCK_SIZE 4
#define OFFSET_ALGORITHM 5
#define OFFSET_OPTIONS 6
#define OFFSET_WIDTH 8
#define OFFSET_HEIGHT 12
#define OFFSET_BLOCKS 16

static Eina_Bool
evas_image_load_file_head_tgv(Image_Entry *ie, const char *file,
                              const char *key __UNUSED__, int *error)
{
   Eina_File *f;
   char *m = NULL;
   unsigned int w, h;

   f = eina_file_open(file, EINA_FALSE);
   *error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
   if (!f) return EINA_FALSE;

   *error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
   if (eina_file_size_get(f) <= OFFSET_BLOCKS)
      goto close_file;

   m = eina_file_map_all(f, EINA_FILE_SEQUENTIAL);
   if (!m)
     {
        *error = EVAS_LOAD_ERROR_CORRUPT_FILE;
        goto close_file;
     }

   if (strncmp(m, "TGV1", 4) != 0)
     goto close_file;

   switch (m[OFFSET_ALGORITHM] & 0xFF)
     {
      case 0:
        ie->cspaces = cspaces_etc1;
        ie->flags.alpha = EINA_FALSE;
        break;
      case 1:
        ie->cspaces = cspaces_rgb8_etc2;
        ie->flags.alpha = EINA_FALSE;
        break;
      case 2:
        ie->cspaces = cspaces_rgba8_etc2_eac;
        ie->flags.alpha = EINA_TRUE;
        break;
      case 3:
        ie->cspaces = cspaces_etc1_alpha;
        ie->flags.alpha = EINA_TRUE;
        break;
      default:
        *error = EVAS_LOAD_ERROR_CORRUPT_FILE;
        goto close_file;
     }

   w = ntohl(*((unsigned int*) &(m[OFFSET_WIDTH])));
   h = ntohl(*((unsigned int*) &(m[OFFSET_HEIGHT])));

   if (ie->load_opts.region.w == 0 && ie->load_opts.region.h == 0)
     {
        ie->w = w;
        ie->h = h;
     }
   else
     {
        Eina_Rectangle r, lo_region;

        // ETC1 colorspace doesn't work with region
        ie->cspaces = NULL;

        EINA_RECTANGLE_SET(&r, 0, 0, w, h);
        EINA_RECTANGLE_SET(&lo_region,
                           ie->load_opts.region.x, ie->load_opts.region.y,
                           ie->load_opts.region.w, ie->load_opts.region.h);
        if (!eina_rectangle_intersection(&r, &lo_region))
          goto close_file;

        ie->w = r.w;
        ie->h = r.h;
     }

   eina_file_map_free(f, m);
   eina_file_close(f);
   *error = EVAS_LOAD_ERROR_NONE;
   return EINA_TRUE;

close_file:
   if (m) eina_file_map_free(f, m);
   eina_file_close(f);
   return EINA_FALSE;
}

static inline unsigned int
_tgv_length_get(const char *m, unsigned int length, unsigned int *offset)
{
   unsigned int r = 0;
   unsigned int shift = 0;

   while (*offset < length && ((*m) & 0x80))
     {
        r = r | (((*m) & 0x7F) << shift);
        shift += 7;
        m++;
        (*offset)++;
     }
   if (*offset < length)
     {
        r = r | (((*m) & 0x7F) << shift);
        (*offset)++;
     }

   return r;
}

Eina_Bool
evas_image_load_file_data_tgv(Image_Entry *ie, const char *file,
                               const char *key __UNUSED__, int *error)
{
   Eina_File *f;
   char *m = NULL;
   unsigned int bwidth, bheight;
   unsigned int *p;
   unsigned char *p_etc;
   char *buffer = NULL;
   Eina_Rectangle master;
   unsigned int block_length;
   unsigned int length, offset;
   unsigned int x, y, w, h;
   unsigned int block_count;
   unsigned int etc_width = 0;
   unsigned int etc_block_size;
   unsigned int num_planes = 1, plane;
   unsigned int alpha_offset = 0;
   Evas_Colorspace cspace, file_cspace;
   Eina_Bool compress, blockless, unpremul;
   Eina_Bool r = EINA_FALSE;

   f = eina_file_open(file, EINA_FALSE);
   *error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
   if (!f) return EINA_FALSE;

   *error = EVAS_LOAD_ERROR_CORRUPT_FILE;
   if (eina_file_size_get(f) <= OFFSET_BLOCKS)
      goto close_file;

   m = eina_file_map_all(f, EINA_FILE_SEQUENTIAL);
   if (!m) goto close_file;

   if (strncmp(m, "TGV1", 4) != 0)
     goto close_file;

   compress = m[OFFSET_OPTIONS] & 0x1;
   blockless = (m[OFFSET_OPTIONS] & 0x2) != 0;
   unpremul = (m[OFFSET_OPTIONS] & 0x4) != 0;
   w = ntohl(*((unsigned int*) &(m[OFFSET_WIDTH])));
   h = ntohl(*((unsigned int*) &(m[OFFSET_HEIGHT])));

   switch (m[OFFSET_ALGORITHM] & 0xFF)
     {
      case 0:
        file_cspace = EVAS_COLORSPACE_ETC1;
        if (ie->flags.alpha != EINA_FALSE) goto close_file;
        etc_block_size = 8;
        etc_width = ((w + 2) / 4 + ((w + 2) % 4 ? 1 : 0)) * 8;
        break;
      case 1:
        file_cspace = EVAS_COLORSPACE_RGB8_ETC2;
        if (ie->flags.alpha != EINA_FALSE) goto close_file;
        etc_block_size = 8;
        etc_width = ((w + 2) / 4 + ((w + 2) % 4 ? 1 : 0)) * 8;
        break;
      case 2:
        file_cspace = EVAS_COLORSPACE_RGBA8_ETC2_EAC;
        if (ie->flags.alpha != EINA_TRUE) goto close_file;
        etc_block_size = 16;
        etc_width = ((w + 2) / 4 + ((w + 2) % 4 ? 1 : 0)) * 16;
        break;
      case 3:
        file_cspace = EVAS_COLORSPACE_ETC1_ALPHA;
        if (ie->flags.alpha != EINA_TRUE) goto close_file;
        etc_block_size = 8;
        etc_width = ((w + 2) / 4 + ((w + 2) % 4 ? 1 : 0)) * 8;
        num_planes = 2;
        alpha_offset = (((w + 2 + 3) / 4) * ((h + 2 + 3) / 4)) * 8 / sizeof(*p_etc);
        break;
      default:
        *error = EVAS_LOAD_ERROR_CORRUPT_FILE;
        goto close_file;
     }

   if (ie->space != EVAS_COLORSPACE_ARGB8888 && ie->space != file_cspace)
     {
        if (!((ie->space == EVAS_COLORSPACE_RGB8_ETC2) &&
              (file_cspace == EVAS_COLORSPACE_ETC1)))
          goto close_file;
        // else: ETC2 is compatible with ETC1 and is preferred
     }

   // FIXME: I very much doubt that region load will work with this logic.
   if (ie->w != w) goto close_file;
   if (ie->h != h) goto close_file;

   if (blockless)
     {
        bwidth = roundup(w + 2, 4);
        bheight = roundup(h + 2, 4);
     }
   else
     {
        bwidth = 4 << (m[OFFSET_BLOCK_SIZE] & 0x0f);
        bheight = 4 << ((m[OFFSET_BLOCK_SIZE] & 0xf0) >> 4);
     }

   EINA_RECTANGLE_SET(&master,
                      ie->load_opts.region.x, ie->load_opts.region.y,
                      ie->w, ie->h);

   cspace = ie->space;
   switch (cspace)
     {
      case EVAS_COLORSPACE_ETC1:
      case EVAS_COLORSPACE_RGB8_ETC2:
      case EVAS_COLORSPACE_RGBA8_ETC2_EAC:
      case EVAS_COLORSPACE_ETC1_ALPHA:
        if (master.x % 4 || master.y % 4)
          abort();
        break;
      case EVAS_COLORSPACE_ARGB8888:
        // Offset to take duplicated pixels into account
        master.x += 1;
        master.y += 1;
        break;
      default: abort();
     }

   evas_cache_image_surface_alloc(ie, w, h);
   p = evas_cache_image_pixels(ie);
   if (!p) goto close_file;
   p_etc = (unsigned char*) p;

   length = eina_file_size_get(f);
   offset = OFFSET_BLOCKS;

   // Allocate space for each ETC block (8 or 16 bytes per 4 * 4 pixels group)
   block_count = bwidth * bheight / (4 * 4);
   if (compress)
     buffer = alloca(etc_block_size * block_count);

   for (plane = 0; plane < num_planes; plane++)
     for (y = 0; y < h + 2; y += bheight)
       for (x = 0; x < w + 2; x += bwidth)
         {
            Eina_Rectangle current;
            const char *data_start;
            const char *it;
            unsigned int expand_length;
            unsigned int i, j;

            block_length = _tgv_length_get(m + offset, length, &offset);

            if (block_length == 0) goto close_file;

            data_start = m + offset;
            offset += block_length;

            EINA_RECTANGLE_SET(&current, x, y,
                               bwidth, bheight);

            if (!eina_rectangle_intersection(&current, &master))
              continue ;

            if (compress)
              {
                 expand_length = LZ4_uncompress(data_start, buffer,
                                                block_count * etc_block_size);
                 // That's an overhead for now, need to be fixed
                 if (expand_length != block_length)
                   goto close_file;
              }
            else
              {
                 buffer = (void*) data_start;
                 if (block_count * etc_block_size != block_length)
                   goto close_file;
              }
            it = buffer;

            for (i = 0; i < bheight; i += 4)
              for (j = 0; j < bwidth; j += 4, it += etc_block_size)
                {
                   Eina_Rectangle current_etc;
                   unsigned int temporary[4 * 4];
                   unsigned int offset_x, offset_y;
                   int k, l;

                   EINA_RECTANGLE_SET(&current_etc, x + j, y + i, 4, 4);

                   if (!eina_rectangle_intersection(&current_etc, &current))
                     continue;

                   switch (cspace)
                     {
                      case EVAS_COLORSPACE_ARGB8888:
                        switch (file_cspace)
                          {
                           case EVAS_COLORSPACE_ETC1:
                           case EVAS_COLORSPACE_ETC1_ALPHA:
                             if (!rg_etc1_unpack_block(it, temporary, 0))
                               {
                                  // TODO: Should we decode as RGB8_ETC2?
                                  fprintf(stderr, "ETC1: Block starting at {%i, %i} is corrupted!\n", x + j, y + i);
                                  continue;
                               }
                             break;
                           case EVAS_COLORSPACE_RGB8_ETC2:
                             rg_etc2_rgb8_decode_block((uint8_t *) it, temporary);
                             break;
                           case EVAS_COLORSPACE_RGBA8_ETC2_EAC:
                             rg_etc2_rgba8_decode_block((uint8_t *) it, temporary);
                             break;
                           default: abort();
                          }

                        offset_x = current_etc.x - x - j;
                        offset_y = current_etc.y - y - i;

                        if (!plane)
                          {
#ifdef BUILD_NEON
                             // FIXME: This should not be using an evas func.
                             if (evas_common_cpu_has_feature(CPU_FEATURE_NEON))
                               {
                                  uint32_t *dst = &p[current_etc.x - 1 + (current_etc.y - 1) * master.w];
                                  uint32_t *src = &temporary[offset_x + offset_y * 4];
                                  for (k = 0; k < current_etc.h; k++)
                                    {
                                       if (current_etc.w == 4)
                                         vst1q_u32(dst, vld1q_u32(src));
                                       else if (current_etc.w == 3)
                                         {
                                            vst1_u32(dst, vld1_u32(src));
                                            *(dst + 2) = *(src + 2);
                                         }
                                       else if (current_etc.w == 2)
                                         vst1_u32(dst, vld1_u32(src));
                                       else
                                          *dst = *src;
                                       dst += master.w;
                                       src += 4;
                                    }
                               }
                             else
#endif
                                for (k = 0; k < current_etc.h; k++)
                                  {
                                     memcpy(&p[current_etc.x - 1 + (current_etc.y - 1 + k) * master.w],
                                            &temporary[offset_x + (offset_y + k) * 4],
                                            current_etc.w * sizeof (unsigned int));
                                  }
                          }
                        else
                          {
                             for (k = 0; k < current_etc.h; k++)
                               for (l = 0; l < current_etc.w; l++)
                                 {
                                    unsigned int *rgbdata =
                                      &p[current_etc.x - 1 + (current_etc.y - 1 + k) * master.w + l];
                                    unsigned int *adata =
                                      &temporary[offset_x + (offset_y + k) * 4 + l];
                                    A_VAL(rgbdata) = G_VAL(adata);
                                 }
                          }
                        break;
                      case EVAS_COLORSPACE_ETC1:
                      case EVAS_COLORSPACE_RGB8_ETC2:
                      case EVAS_COLORSPACE_RGBA8_ETC2_EAC:
                        memcpy(&p_etc[(current_etc.x / 4) * etc_block_size +
                              (current_etc.y / 4) * etc_width],
                              it, etc_block_size);
                        break;
                      case EVAS_COLORSPACE_ETC1_ALPHA:
                        memcpy(&p_etc[(current_etc.x / 4) * etc_block_size +
                                      (current_etc.y / 4) * etc_width +
                                      plane * alpha_offset],
                               it, etc_block_size);
                        break;
                      default:
                        abort();
                     }
                } // bx,by inside blocks
         } // x,y macroblocks

   // TODO: Add support for more unpremultiplied modes (ETC2)
   if ((cspace == EVAS_COLORSPACE_ARGB8888) && unpremul)
     evas_common_image_premul(ie);

   *error = EVAS_LOAD_ERROR_NONE;
   r = EINA_TRUE;

close_file:
   if (m) eina_file_map_free(f, m);
   eina_file_close(f);
   return r;
}

Evas_Image_Load_Func evas_image_load_tgv_func =
{
  EINA_TRUE, // Threadable
  evas_image_load_file_head_tgv,
  evas_image_load_file_data_tgv,
  NULL, // Frame duration
  EINA_FALSE // Region
};

static int
module_open(Evas_Module *em)
{
   if (!em) return 0;
   em->functions = (void *)(&evas_image_load_tgv_func);
   return 1;
}

static void
module_close(Evas_Module *em EINA_UNUSED)
{
}

static Evas_Module_Api evas_modapi =
{
   EVAS_MODULE_API_VERSION,
   "tgv",
   "none",
   {
     module_open,
     module_close
   }
};

EVAS_MODULE_DEFINE(EVAS_MODULE_TYPE_IMAGE_LOADER, image_loader, tgv);

#ifndef EVAS_STATIC_BUILD_TGV
EVAS_EINA_MODULE_DEFINE(image_loader, tgv);
#endif
