#include "evas_common.h"
#include "evas_convert_rgb_24.h"

#ifdef BUILD_CONVERT_24_RGB_888
void
evas_common_convert_rgba_to_24bpp_rgb_888(DATA32 *src, DATA8 *dst, int src_jump, int dst_jump, int w, int h, int dith_x __UNUSED__, int dith_y __UNUSED__, DATA8 *pal __UNUSED__)
{
   DATA32 *src_ptr;
   DATA8 *dst_ptr;
   int x, y;

   dst_ptr = (DATA8 *)dst;
   src_ptr = (DATA32 *)src;

   for (y = 0; y < h; y++)
     {
	for (x = 0; x < w; x++)
	  {
	     dst_ptr[0] = R_VAL(src_ptr);
	     dst_ptr[1] = G_VAL(src_ptr);
	     dst_ptr[2] = B_VAL(src_ptr);
	     src_ptr++;
	     dst_ptr+=3;
	  }
	src_ptr += src_jump;
	dst_ptr += (dst_jump * 3);
     }
   return;
}
#endif
#ifdef BUILD_CONVERT_24_RGB_666
void
evas_common_convert_rgba_to_24bpp_rgb_666(DATA32 *src, DATA8 *dst, int src_jump, int dst_jump, int w, int h, int dith_x __UNUSED__, int dith_y __UNUSED__, DATA8 *pal __UNUSED__)
{
   DATA32 *src_ptr;
   DATA8 *dst_ptr, *scratch_ptr;
   DATA32 scratch;
   int x, y;

   dst_ptr = (DATA8 *)dst;
   src_ptr = (DATA32 *)src;

   scratch_ptr = (DATA8 *)(&scratch);
   for (y = 0; y < h; y++)
     {
	for (x = 0; x < w; x++)
	  {
             scratch = 
               (((R_VAL(src_ptr) << 12) | (B_VAL(src_ptr) >> 2)) & 0x03f03f) |
               ((G_VAL(src_ptr) << 4) & 0x000fc0);
	     dst_ptr[0] = scratch_ptr[1];
	     dst_ptr[1] = scratch_ptr[2];
	     dst_ptr[2] = scratch_ptr[3];
	     src_ptr++;
	     dst_ptr+=3;
	  }
	src_ptr += src_jump;
	dst_ptr += (dst_jump * 3);
     }
   return;
}
#endif
#ifdef BUILD_CONVERT_24_BGR_888
void
evas_common_convert_rgba_to_24bpp_bgr_888(DATA32 *src, DATA8 *dst, int src_jump, int dst_jump, int w, int h, int dith_x __UNUSED__, int dith_y __UNUSED__, DATA8 *pal __UNUSED__)
{
   DATA32 *src_ptr;
   DATA8 *dst_ptr;
   int x, y;

   dst_ptr = (DATA8 *)dst;
   src_ptr = (DATA32 *)src;

   for (y = 0; y < h; y++)
     {
	for (x = 0; x < w; x++)
	  {
	     dst_ptr[2] = R_VAL(src_ptr);
	     dst_ptr[1] = G_VAL(src_ptr);
	     dst_ptr[0] = B_VAL(src_ptr);
	     src_ptr++;
	     dst_ptr+=3;
	  }
	src_ptr += src_jump;
	dst_ptr += (dst_jump * 3);
     }
   return;
}
#endif
