#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_EVIL
# include <Evil.h>
#endif

#include <ctype.h>

#include "evas_macros.h"

#include "evas_cserve2.h"
#include "evas_cserve2_slave.h"

#define FILE_BUFFER_SIZE 1024 * 32
#define FILE_BUFFER_UNREAD_SIZE 16

/* The buffer to load pmaps images */
typedef struct Pmaps_Buffer Pmaps_Buffer;

struct Pmaps_Buffer
{
   Eina_File *file;
   void *map;
   size_t position;

   /* the buffer */
   DATA8 buffer[FILE_BUFFER_SIZE];
   DATA8 unread[FILE_BUFFER_UNREAD_SIZE];
   DATA8 *current;
   DATA8 *end;
   char type[3];
   unsigned char unread_len:7;
   unsigned char last_buffer:1;

   /* image properties */
   int w;
   int h;
   int max;

   /* interface */
   int (*int_get) (Pmaps_Buffer *b, int *val);
   int (*color_get) (Pmaps_Buffer *b, DATA32 *color);
};

/* internal used functions */
static Eina_Bool pmaps_buffer_open(Pmaps_Buffer *b, const char *filename, int *error);
static void pmaps_buffer_close(Pmaps_Buffer *b);
static Eina_Bool pmaps_buffer_header_parse(Pmaps_Buffer *b, int *error);
static int pmaps_buffer_plain_int_get(Pmaps_Buffer *b, int *val);
static int pmaps_buffer_1byte_int_get(Pmaps_Buffer *b, int *val);
static int pmaps_buffer_2byte_int_get(Pmaps_Buffer *b, int *val);
static int pmaps_buffer_gray_get(Pmaps_Buffer *b, DATA32 *color);
static int pmaps_buffer_rgb_get(Pmaps_Buffer *b, DATA32 *color);
static int pmaps_buffer_plain_bw_get(Pmaps_Buffer *b, DATA32 *color);

static size_t pmaps_buffer_plain_update(Pmaps_Buffer *b);
static size_t pmaps_buffer_raw_update(Pmaps_Buffer *b);
static int pmaps_buffer_comment_skip(Pmaps_Buffer *b);

static Eina_Bool
evas_image_load_file_head_pmaps(Evas_Img_Load_Params *ilp, const char *file, const char *key __UNUSED__, int *error)
{
   Pmaps_Buffer b;

   if (!pmaps_buffer_open(&b, file, error))
     {
	pmaps_buffer_close(&b);
	return EINA_FALSE;
     }

   if (!pmaps_buffer_header_parse(&b, error))
     {
	pmaps_buffer_close(&b);
	return EINA_FALSE;
     }

   ilp->w = b.w;
   ilp->h = b.h;

   pmaps_buffer_close(&b);
   *error = EVAS_LOAD_ERROR_NONE;
   return EINA_TRUE;
}

static Eina_Bool
evas_image_load_file_data_pmaps(Evas_Img_Load_Params *ilp, const char *file, const char *key __UNUSED__, int *error)
{
   Pmaps_Buffer b;
   int pixels;
   DATA32 *ptr;

   if (!pmaps_buffer_open(&b, file, error))
     {
	pmaps_buffer_close(&b);
	return EINA_FALSE;
     }

   if (!pmaps_buffer_header_parse(&b, error))
     {
	pmaps_buffer_close(&b);
	return EINA_FALSE;
     }

   pixels = b.w * b.h;

   ptr = ilp->buffer;
   if (!ptr)
     {
	pmaps_buffer_close(&b);
	*error = EVAS_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED;
	return EINA_FALSE;
     }

   if (b.type[1] != '4')
     {
	while (pixels > 0 && b.color_get(&b, ptr))
	  {
	     pixels--;
	     ptr++;
	  }
     }
   else
     {
	while (pixels > 0
	       && (b.current != b.end || pmaps_buffer_raw_update(&b)))
	  {
	     int i;

	     for (i = 7; i >= 0 && pixels > 0; i--)
	       {
		  if (*b.current & (1 << i))
		     *ptr = 0xff000000;
		  else
		     *ptr = 0xffffffff;
		  ptr++;
		  pixels--;
	       }
	     b.current++;
	  }
     }

   /* if there are some pix missing, give them a proper default */
   memset(ptr, 0xff, 4 * pixels);
   pmaps_buffer_close(&b);

   *error = EVAS_LOAD_ERROR_NONE;
   return EINA_TRUE;
}

/* internal used functions */
static Eina_Bool
pmaps_buffer_open(Pmaps_Buffer *b, const char *filename, int *error)
{
   size_t len;

   b->file = eina_file_open(filename, EINA_FALSE);
   if (!b->file)
     {
	*error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
	return EINA_FALSE;
     }

   b->map = eina_file_map_all(b->file, EINA_FILE_SEQUENTIAL);
   if (!b->map)
     {
        *error = EVAS_LOAD_ERROR_DOES_NOT_EXIST;
        eina_file_close(b->file);
        b->file = NULL;
        return EINA_FALSE;
     }

   b->position = 0;
   *b->buffer = 0;
   *b->unread = 0;
   b->last_buffer = 0;
   b->unread_len = 0;

   len = pmaps_buffer_plain_update(b);

   if (len < 3)
     {
	*error = EVAS_LOAD_ERROR_CORRUPT_FILE;
        eina_file_map_free(b->file, b->map);
        eina_file_close(b->file);
        b->map = NULL;
        b->file = NULL;
	return EINA_FALSE;
     }

   /* copy the type */
   b->type[0] = b->buffer[0];
   b->type[1] = b->buffer[1];
   b->type[2] = 0;
   /* skip the PX */
   b->current = b->buffer + 2;

   *error = EVAS_LOAD_ERROR_NONE;
   return EINA_TRUE;
}

static void
pmaps_buffer_close(Pmaps_Buffer *b)
{
   if (b->file)
     {
        if (b->map) eina_file_map_free(b->file, b->map);
        b->map = NULL;
        eina_file_close(b->file);
        b->file = NULL;
     }
}

static Eina_Bool
pmaps_buffer_header_parse(Pmaps_Buffer *b, int *error)
{
   /* if there is no P at the beginning it is not a file we can parse */
   if (b->type[0] != 'P')
     {
	*error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
	return EINA_FALSE;
     }

   /* get the width */
   if (!pmaps_buffer_plain_int_get(b, &(b->w)) || b->w < 1)
     {
	*error = EVAS_LOAD_ERROR_CORRUPT_FILE;
	return EINA_FALSE;
     }

   /* get the height */
   if (!pmaps_buffer_plain_int_get(b, &(b->h)) || b->h < 1)
     {
	*error = EVAS_LOAD_ERROR_CORRUPT_FILE;
	return EINA_FALSE;
     }

   /* get the maximum value. P1 and P4 don't have a maximum value. */
   if (!(b->type[1] == '1' || b->type[1] == '4')
       && (!pmaps_buffer_plain_int_get(b, &(b->max)) || b->max < 1))
     {
	*error = EVAS_LOAD_ERROR_UNKNOWN_FORMAT;
	return EINA_FALSE;
     }

   /* set up the color get callback */
   switch (b->type[1])
     {
	/* Black and White */
     case '1':
	b->color_get = pmaps_buffer_plain_bw_get;
	break;
     case '4':
	/* Binary black and white use another format */
	b->color_get = NULL;
	break;
     case '2':
     case '5':
	b->color_get = pmaps_buffer_gray_get;
	break;
     case '3':
     case '6':
	b->color_get = pmaps_buffer_rgb_get;
	break;
     case '7':
	/* XXX write me */
	return 0;
	break;
     default:
	return 0;
     }
   /* set up the int get callback */
   switch (b->type[1])
     {
	/* RAW */
     case '5':
     case '6':
	if (b->max < 256)
	   b->int_get = pmaps_buffer_1byte_int_get;
	else
	   b->int_get = pmaps_buffer_2byte_int_get;

	if (b->current == b->end && !pmaps_buffer_raw_update(b))
	   return 0;

	b->current++;
	break;
	/* Plain */
     case '2':
     case '3':
	b->int_get = pmaps_buffer_plain_int_get;
	break;
	/* Black and White Bitmaps don't use that callback */
     case '1':
     case '4':
	b->int_get = NULL;
	/* we need to skip the next character fpr P4 it
	 * doesn't hurt if we do it for the P1 as well */
	b->current++;
	break;
     }
   return 1;
}

static size_t
pmaps_buffer_plain_update(Pmaps_Buffer *b)
{
   size_t r;
   size_t max;

   /* if we already are in the last buffer we can not update it */
   if (b->last_buffer)
      return 0;

   /* if we have unread bytes we need to put them before the new read
    * stuff */
   if (b->unread_len)
      memcpy(b->buffer, b->unread, b->unread_len);

   max = FILE_BUFFER_SIZE - b->unread_len - 1;
   if (b->position + max > eina_file_size_get(b->file))
     max = eina_file_size_get(b->file) - b->position;

   memcpy(&b->buffer[b->unread_len], b->map + b->position, max);
   b->position += max;
   r = max + b->unread_len;

   /* we haven't read anything nor have we bytes in the unread buffer */
   if (r == 0)
     {
	b->buffer[0] = '\0';
	b->end = b->buffer;
	b->last_buffer = 1;
	return 0;
     }

   if (r < FILE_BUFFER_SIZE - 1)
     {
	/*we reached eof */ ;
	b->last_buffer = 1;
     }

   b->buffer[r] = 0;

   b->unread[0] = '\0';
   b->unread_len = 0;

   b->buffer[r] = '\0';
   b->current = b->buffer;
   b->end = b->buffer + r;

   return r;
}

static size_t
pmaps_buffer_raw_update(Pmaps_Buffer *b)
{
   size_t r;
   size_t max;

   if (b->last_buffer)
      return 0;

   if (b->unread_len)
      memcpy(b->buffer, b->unread, b->unread_len);

   max = FILE_BUFFER_SIZE - b->unread_len;
   if (b->position + max > eina_file_size_get(b->file))
     max = eina_file_size_get(b->file) - b->position;

   memcpy(&b->buffer[b->unread_len], b->map + b->position, max);
   b->position += max;
   r = max + b->unread_len;

   if (r < FILE_BUFFER_SIZE)
     {
	/*we reached eof */
	b->last_buffer = 1;
     }

   b->end = b->buffer + r;
   b->current = b->buffer;

   if (b->unread_len)
     {
	/* the buffer is now read */
	*b->unread = 0;
	b->unread_len = 0;
     }

   return r;
}

static int
pmaps_buffer_plain_int_get(Pmaps_Buffer *b, int *val)
{
   char *start;
   DATA8 lastc;

   /* first skip all white space
    * Note: we are skipping here actually every character than is not
    * a digit */
   while (!isdigit(*b->current))
     {
	if (*b->current == '\0')
	  {
	     if (!pmaps_buffer_plain_update(b))
		return 0;

	     continue;
	  }
	if (*b->current == '#' && !pmaps_buffer_comment_skip(b))
	   return 0;
	b->current++;
     }

   start = (char *)b->current;
   /* now find the end of the number */
   while (isdigit(*b->current))
      b->current++;

   lastc = *b->current;
   *b->current = '\0';
   *val = atoi(start);
   *b->current = lastc;

   return 1;
}

static int
pmaps_buffer_1byte_int_get(Pmaps_Buffer *b, int *val)
{
   /* are we at the end of the buffer? */
   if (b->current == b->end && !pmaps_buffer_raw_update(b))
      return 0;

   *val = *b->current;
   b->current++;

   return 1;
}
static int
pmaps_buffer_2byte_int_get(Pmaps_Buffer *b, int *val)
{
   /* are we at the end of the buffer? */
   if (b->current == b->end && !pmaps_buffer_raw_update(b))
      return 0;

   *val = (int)(*b->current << 8);
   b->current++;

   /* are we at the end of the buffer? */
   if (b->current == b->end && !pmaps_buffer_raw_update(b))
      return 0;

   *val |= *b->current;
   b->current++;

   return 1;
}

static int
pmaps_buffer_comment_skip(Pmaps_Buffer *b)
{
   while (*b->current != '\n')
     {
	if (*b->current == '\0')
	  {
	     if (!pmaps_buffer_plain_update(b))
		return 0;

	     continue;
	  }
	b->current++;
     }
   return 1;
}

static int
pmaps_buffer_rgb_get(Pmaps_Buffer *b, DATA32 *color)
{
   int vr, vg, vb;

   if (!b->int_get(b, &vr) || !b->int_get(b, &vg) || !b->int_get(b, &vb))
      return 0;

   if (b->max != 255)
     {
	vr = (vr * 255) / b->max;
	vg = (vg * 255) / b->max;
	vb = (vb * 255) / b->max;
     }
   if (vr > 255)
      vr = 255;
   if (vg > 255)
      vg = 255;
   if (vb > 255)
      vb = 255;

   *color = ARGB_JOIN(0xff, vr, vg, vb);

   return 1;
}

static int
pmaps_buffer_gray_get(Pmaps_Buffer *b, DATA32 *color)
{
   int val;

   if (!b->int_get(b, &val))
      return 0;

   if (b->max != 255)
      val = (val * 255) / b->max;
   if (val > 255)
      val = 255;
   *color = ARGB_JOIN(0xff, val, val, val);

   return 1;
}

static int
pmaps_buffer_plain_bw_get(Pmaps_Buffer *b, DATA32 *val)
{
   /* first skip all white space
    * Note: we are skipping here actually every character than is not
    * a digit */
   while (!isdigit(*b->current))
     {
	if (*b->current == '\0')
	  {
	     if (!pmaps_buffer_raw_update(b))
		return 0;

	     continue;
	  }
	if (*b->current == '#' && !pmaps_buffer_comment_skip(b))
	   return 0;
	b->current++;
     }

   if (*b->current == '0')
      *val = 0xffffffff;
   else
      *val = 0xff000000;

   b->current++;

   return 1;
}

static Evas_Loader_Module_Api modapi =
{
   EVAS_CSERVE2_MODULE_API_VERSION,
   "pmaps",
   evas_image_load_file_head_pmaps,
   evas_image_load_file_data_pmaps
};

static Eina_Bool
module_init(void)
{
   return evas_cserve2_loader_register(&modapi);
}

static void
module_shutdown(void)
{
}

EINA_MODULE_INIT(module_init);
EINA_MODULE_SHUTDOWN(module_shutdown);
