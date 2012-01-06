#ifndef _EVAS_SCALE_MAIN_H
#define _EVAS_SCALE_MAIN_H


EAPI void evas_common_scale_init                            (void);

EAPI void evas_common_scale_rgba_in_to_out_clip_smooth      (RGBA_Image *src, RGBA_Image *dst, RGBA_Draw_Context *dc, int src_region_x, int src_region_y, int src_region_w, int src_region_h, int dst_region_x, int dst_region_y, int dst_region_w, int dst_region_h);

EAPI void evas_common_scale_rgba_in_to_out_clip_sample      (RGBA_Image *src, RGBA_Image *dst, RGBA_Draw_Context *dc, int src_region_x, int src_region_y, int src_region_w, int src_region_h, int dst_region_x, int dst_region_y, int dst_region_w, int dst_region_h);

EAPI void evas_common_rgba_image_scalecache_dump(void);

#endif /* _EVAS_SCALE_MAIN_H */
