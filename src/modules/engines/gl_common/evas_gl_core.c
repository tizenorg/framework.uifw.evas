#include "evas_gl_core_private.h"
#include <dlfcn.h>

// EVGL GL Format Pair
typedef struct _GL_Format
{
   int    bit;
   GLenum fmt;
} GL_Format;

// Globals
static Evas_GL_API gl_funcs;
EVGL_Engine *evgl_engine = NULL;
int _evas_gl_log_dom = -1;

static void _surface_cap_print(EVGL_Engine *ee, int error);

//---------------------------------------------------------------//
// Internal Resources:
//  - Surface and Context used for internal buffer creation
//---------------------------------------------------------------//
static void *
_internal_resources_create(EVGL_Engine *ee)
{
   EVGL_Resource *rsc = NULL;

   // Check if engine is valid
   if (!ee)
     {
        ERR("EVGL Engine not initialized!");
        return NULL;
     }

   // Allocate resource
   rsc = calloc(1, sizeof(EVGL_Resource));
   if (!rsc)
     {
        ERR("Error allocating EVGL_Resource");
        return NULL;
     }

   // Add to the resource resource list for cleanup
   LKL(ee->resource_lock);
   rsc->id = ee->resource_count++;
   ee->resource_list = eina_list_prepend(ee->resource_list, rsc);
   LKU(ee->resource_lock);

   // Create resource surface
   // Use Evas' surface if it's in the same thread
   if (rsc->id == ee->main_tid)
      rsc->surface = ee->funcs->evas_surface_get(ee->engine_data);
   else
     {
        rsc->window = ee->funcs->native_window_create(ee->engine_data);
        if (!rsc->window)
          {
             ERR("Error creating native window");
             goto error;
          }

        rsc->surface = ee->funcs->surface_create(ee->engine_data, rsc->window);
        if (!rsc->surface)
          {
             ERR("Error creating native surface");
             goto error;
          }
     }

   if (!rsc->surface)
     {
        ERR("Internal resource surface failed.");
        goto error;
     }

   // Create a resource context
   rsc->context = ee->funcs->context_create(ee->engine_data, NULL);
   if (!rsc->context)
     {
        ERR("Internal resource context creations failed.");
        goto error;
     }

   // Set the resource in TLS
   if (eina_tls_set(ee->resource_key, (void*)rsc) == EINA_FALSE)
     {
        ERR("Failed setting TLS Resource");
        goto error;
     }

   return rsc;

error:
   if (rsc->context)
        ee->funcs->context_destroy(ee->engine_data, rsc->context);
   if (rsc->surface)
      if (rsc->id != ee->main_tid)    // 0 is the main thread
         ee->funcs->surface_destroy(ee->engine_data, rsc->surface);
   if (rsc->window)
      if (rsc->id != ee->main_tid)    // 0 is the main thread
         ee->funcs->native_window_destroy(ee->engine_data, rsc->window);

   if (rsc) free(rsc);

   return NULL;
}

static int
_internal_resources_destroy(EVGL_Engine *ee)
{
   EVGL_Resource *rsc = NULL;
   Eina_List     *l   = NULL;

   // Check if engine is valid
   if (!ee)
     {
        ERR("EVGL Engine not initialized!");
        return 0;
     }

   LKL(ee->resource_lock);
   EINA_LIST_FOREACH(ee->resource_list, l, rsc)
     {
        // Surfaces are taken care by destroying the surface pool
        // Only delete contexts here.
        if (rsc->context)
           ee->funcs->context_destroy(ee->engine_data, rsc->context);
        if (rsc->window)
           if (rsc->id != ee->main_tid)    // 0 is the main thread
              ee->funcs->native_window_destroy(ee->engine_data, rsc->window);
        if (rsc->surface)
           if (rsc->id != ee->main_tid)    // 0 is the main thread
              ee->funcs->surface_destroy(ee->engine_data, rsc->surface);
        free(rsc);
     }
   eina_list_free(ee->resource_list);
   LKU(ee->resource_lock);

   return 1;
}

static int
_internal_resource_make_current(EVGL_Engine *ee, EVGL_Context *ctx)
{
   EVGL_Resource *rsc = NULL;
   void *surface = NULL;
   void *context = NULL;
   int ret = 0;

   // Retrieve the resource object
   if (!(rsc = _evgl_tls_resource_get(ee)))
     {
        ERR("Error retrieving resource from TLS");
        return 0;
     }

   // Set context from input or from resource
   if (ctx)
      context = ctx->context;
   else
      context = (void*)rsc->context;

   // Update in case they've changed
   if (rsc->id == ee->main_tid)
      rsc->surface = ee->funcs->evas_surface_get(ee->engine_data);

   surface = (void*)rsc->surface;

   // Do the make current
   ret = ee->funcs->make_current(ee->engine_data, surface, context, 1);
   if (!ret)
     {
        ERR("Engine make_current with internal resources failed.");
        return 0;
     }

   return 1;
}

//---------------------------------------------------------------//
// Surface Related Functions
//  - Texture/ Renderbuffer Creation/ Attachment to FBO
//  - Surface capability check
//  - Internal config choose function
//---------------------------------------------------------------//
// Create and allocate 2D texture
void
_texture_2d_create(GLuint *tex, GLint ifmt, GLenum fmt, GLenum type, int w, int h)
{
   glGenTextures(1, tex);
   glBindTexture(GL_TEXTURE_2D, *tex );
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, NULL);
   glBindTexture(GL_TEXTURE_2D, 0);
}

// Destroy Texture
void
_texture_destroy(GLuint *tex)
{
   if (*tex)
     {
        glDeleteTextures(1, tex);
        *tex = 0;
     }
}

// Attach 2D texture with the given format to already bound FBO
// *NOTE: attach2 here is used for depth_stencil attachment in GLES env.
void
_texture_2d_attach(GLuint tex, GLenum attach, GLenum attach2, int samples)
{
   if (samples)
     {
#ifdef GL_GLES
        //<<< TODO : CHECK EXTENSION SUPPORT>>>
        EXT_FUNC(glFramebufferTexture2DMultisample)(GL_FRAMEBUFFER,
                                                    attach,
                                                    GL_TEXTURE_2D, tex,
                                                    0, samples);

        if (attach2)
           EXT_FUNC(glFramebufferTexture2DMultisample)(GL_FRAMEBUFFER,
                                                       attach2,
                                                       GL_TEXTURE_2D, tex,
                                                       0, samples);
#else
        ERR("MSAA not supported.  Should not have come in here...!");
#endif
     }
   else
     {
        glFramebufferTexture2D(GL_FRAMEBUFFER, attach, GL_TEXTURE_2D, tex, 0);

        if (attach2)
           glFramebufferTexture2D(GL_FRAMEBUFFER, attach2, GL_TEXTURE_2D, tex, 0);

     }
}

// Attach a renderbuffer with the given format to already bound FBO
void
_renderbuffer_create(GLuint *buf, GLenum fmt, int w, int h, int samples)
{
   glGenRenderbuffers(1, buf);
   glBindRenderbuffer(GL_RENDERBUFFER, *buf);
   if (samples)
#ifdef GL_GLES
      EXT_FUNC(glRenderbufferStorageMultisample)(GL_RENDERBUFFER, samples, fmt, w, h);
#else
      ERR("MSAA not supported.  Should not have come in here...!");
#endif
   else
      glRenderbufferStorage(GL_RENDERBUFFER, fmt, w, h);
   glBindRenderbuffer(GL_RENDERBUFFER, 0);

   return;
   samples = 0;
}

void
_renderbuffer_destroy(GLuint *buf)
{
   if (*buf)
     {
        glDeleteRenderbuffers(1, buf);
        *buf = 0;
     }
}

// Attach a renderbuffer with the given format to already bound FBO
void
_renderbuffer_attach(GLuint buf, GLenum attach)
{
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, attach, GL_RENDERBUFFER, buf);
}

// Check whether the given FBO surface config is supported by the driver
static int
_fbo_surface_cap_test(GLint color_ifmt, GLenum color_fmt,
                      GLenum depth_fmt, GLenum stencil_fmt, int mult_samples)
{
   GLuint fbo = 0;
   GLuint color_buf = 0;
   GLuint depth_buf = 0;
   GLuint stencil_buf = 0;
   GLuint depth_stencil_buf = 0;
   int depth_stencil = 0;
   int fb_status = 0;
   int w = 2, h = 2;   // Test it with a simple (2,2) surface.  Should I test it with NPOT?

   // Gen FBO
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);

   // Color Buffer Texture
   if ((color_ifmt) && (color_fmt))
     {
        _texture_2d_create(&color_buf, color_ifmt, color_fmt, GL_UNSIGNED_BYTE, w, h);
        _texture_2d_attach(color_buf, GL_COLOR_ATTACHMENT0, 0, mult_samples);
     }

   // Check Depth_Stencil Format First
#ifdef GL_GLES
   if (depth_fmt == GL_DEPTH_STENCIL_OES)
     {
        _texture_2d_create(&depth_stencil_buf, depth_fmt,
                           depth_fmt, GL_UNSIGNED_INT_24_8_OES, w, h);
        _texture_2d_attach(depth_stencil_buf, GL_DEPTH_ATTACHMENT,
                           GL_STENCIL_ATTACHMENT, mult_samples);
        depth_stencil = 1;
     }
#else
   if (depth_fmt == GL_DEPTH24_STENCIL8)
     {
        _renderbuffer_create(&depth_stencil_buf, depth_fmt, w, h, mult_samples);
        _renderbuffer_attach(depth_stencil_buf, GL_DEPTH_STENCIL_ATTACHMENT);
        depth_stencil = 1;
     }
#endif

   // Depth Attachment
   if ((!depth_stencil) && (depth_fmt))
     {
        _renderbuffer_create(&depth_buf, depth_fmt, w, h, mult_samples);
        _renderbuffer_attach(depth_buf, GL_DEPTH_ATTACHMENT);
     }

   // Stencil Attachment
   if ((!depth_stencil) && (stencil_fmt))
     {
        _renderbuffer_create(&stencil_buf, stencil_fmt, w, h, mult_samples);
        _renderbuffer_attach(stencil_buf, GL_STENCIL_ATTACHMENT);
     }

   // Check FBO for completeness
   fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

   // Delete Created Resources
   _texture_destroy(&color_buf);
   _renderbuffer_destroy(&depth_buf);
   _renderbuffer_destroy(&stencil_buf);
#ifdef GL_GLES
   _texture_destroy(&depth_stencil_buf);
#else
   _renderbuffer_destroy(&depth_stencil_buf);
#endif

   // Delete FBO
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   if (fbo) glDeleteFramebuffers(1, &fbo);

   // Return the result
   if (fb_status != GL_FRAMEBUFFER_COMPLETE)
   {
       // Put Error Log...
      return 0;
   }
   else
      return 1;
}

int
_surface_cap_test(EVGL_Surface_Format *fmt, GL_Format *color,
                  GL_Format *depth, GL_Format *stencil, int samples)
{
   int ret = 0;

   if ( (depth->bit == DEPTH_STENCIL) && (stencil->bit != STENCIL_BIT_8))
      return 0;

   ret = _fbo_surface_cap_test((GLint)color->fmt,
                               color->fmt,
                               depth->fmt,
                               stencil->fmt, samples);
   if (ret)
     {
        fmt->color_bit  = color->bit;
        fmt->color_ifmt = (GLint)color->fmt;
        fmt->color_fmt  = color->fmt;
        fmt->samples = samples;

        if (depth->bit == DEPTH_STENCIL)
          {
             // Depth Stencil Case
             fmt->depth_stencil_fmt = depth->fmt;
             fmt->depth_bit   = DEPTH_BIT_24;
             fmt->depth_fmt   = 0;
             fmt->stencil_bit = STENCIL_BIT_8;
             fmt->stencil_fmt = 0;
          }
        else
          {
             fmt->depth_stencil_fmt = 0;
             fmt->depth_bit = depth->bit;
             fmt->depth_fmt = depth->fmt;
             fmt->stencil_bit = stencil->bit;
             fmt->stencil_fmt = stencil->fmt;
          }
     }

   return ret;
}


int
_surface_cap_check(EVGL_Engine *ee)
{
   int num_fmts = 0;
   int i, j, k, m;

   GL_Format color[]   = {
                           { COLOR_RGB_888,   GL_RGB },
                           { COLOR_RGBA_8888, GL_RGBA },
                           { -1, -1 },
                         };

#ifdef GL_GLES
   GL_Format depth[]   = {
                           { DEPTH_NONE,   0 },
                           { DEPTH_STENCIL, GL_DEPTH_STENCIL_OES },
                           { DEPTH_BIT_8,   GL_DEPTH_COMPONENT },
                           { DEPTH_BIT_16,  GL_DEPTH_COMPONENT16 },
                           { DEPTH_BIT_24,  GL_DEPTH_COMPONENT24_OES },
                           { DEPTH_BIT_32,  GL_DEPTH_COMPONENT32_OES },
                           { -1, -1 },
                         };
   GL_Format stencil[] = {
                           { STENCIL_NONE, 0 },
                           { STENCIL_BIT_1, GL_STENCIL_INDEX1_OES },
                           { STENCIL_BIT_4, GL_STENCIL_INDEX4_OES },
                           { STENCIL_BIT_8, GL_STENCIL_INDEX8 },
                           { -1, -1 },
                         };
#else
   GL_Format depth[]   = {
                           { DEPTH_NONE,   0 },
                           { DEPTH_STENCIL, GL_DEPTH24_STENCIL8 },
                           { DEPTH_BIT_8,   GL_DEPTH_COMPONENT },
                           { DEPTH_BIT_16,  GL_DEPTH_COMPONENT16 },
                           { DEPTH_BIT_24,  GL_DEPTH_COMPONENT24 },
                           { DEPTH_BIT_32,  GL_DEPTH_COMPONENT32 },
                           { -1, -1 },
                         };
   GL_Format stencil[] = {
                           { STENCIL_NONE, 0 },
                           { STENCIL_BIT_1, GL_STENCIL_INDEX1 },
                           { STENCIL_BIT_4, GL_STENCIL_INDEX4 },
                           { STENCIL_BIT_8, GL_STENCIL_INDEX8 },
                           { -1, -1 },
                         };
#endif

   int msaa_samples[4] = {0, -1, -1, -1};  // { NO_MSAA, LOW, MED, HIGH }

   EVGL_Surface_Format *fmt = NULL;

   // Check Surface Cap for MSAA
   if (ee->caps.msaa_supported)
     {
        if ((ee->caps.msaa_samples[2] != ee->caps.msaa_samples[1]) &&
            (ee->caps.msaa_samples[2] != ee->caps.msaa_samples[0]))
             msaa_samples[3] = ee->caps.msaa_samples[2];    // HIGH
        if ((ee->caps.msaa_samples[1] != ee->caps.msaa_samples[0]))
             msaa_samples[2] = ee->caps.msaa_samples[1];    // MED
        if (ee->caps.msaa_samples[0])
             msaa_samples[1] = ee->caps.msaa_samples[0];    // LOW
     }


   // MSAA
   for ( m = 0; m < 4; m++)
     {
        if (msaa_samples[m] < 0) continue;

        // Color Formats
        i = 0;
        while ( color[i].bit >= 0 )
          {
             j = 0;
             // Depth Formats
             while ( depth[j].bit >= 0 )
               {
                  k = 0;
                  // Stencil Formats
                  while ( stencil[k].bit >= 0)
                    {
                       fmt = &ee->caps.fbo_fmts[num_fmts];
                       if (_surface_cap_test(fmt, &color[i], &depth[j], &stencil[k], msaa_samples[m]))
                          num_fmts++;
                       k++;
                    }
                  j++;
               }
             i++;
          }
     }

   return num_fmts;
}

static int
_surface_cap_load(EVGL_Engine *ee, Eet_File *ef)
{
   int res = 0, i = 0, length = 0;
   char tag[80];
   void *data = 0;

   data = eet_read(ef, "num_fbo_fmts", &length);
   if ((!data) || (length <= 0)) goto finish;
   ee->caps.num_fbo_fmts = atoi(data);
   free(data);
   data = 0;

   for (i = 0; i < ee->caps.num_fbo_fmts; ++i)
     {
        EVGL_Surface_Format *fmt = &ee->caps.fbo_fmts[i];

        snprintf(tag, sizeof(tag), "fbo_%d", i);
        data = eet_read(ef, tag, &length);
        if ((!data) || (length <= 0)) goto finish;
        sscanf(data, "%d%d%d%d%d%d%d%d%d%d",
               &(fmt->index),
               &(fmt->color_bit), &(fmt->color_ifmt), &(fmt->color_fmt),
               &(fmt->depth_bit), &(fmt->depth_fmt),
               &(fmt->stencil_bit), &(fmt->stencil_fmt),
               &(fmt->depth_stencil_fmt),
               &(fmt->samples));
        free(data);
        data = 0;
     }

   res = 1;
   goto finish;

finish:
   if (data) free(data);
   return res;
}

static int
_surface_cap_save(EVGL_Engine *ee, Eet_File *ef)
{
   int i = 0;
   char tag[80], data[80];;

   snprintf(data, sizeof(data), "%d", ee->caps.num_fbo_fmts);
   if (eet_write(ef, "num_fbo_fmts", data, sizeof(data), 1) < 0)
      return 0;

   for (i = 0; i < ee->caps.num_fbo_fmts; ++i)
     {
        EVGL_Surface_Format *fmt = &ee->caps.fbo_fmts[i];

        snprintf(tag, sizeof(tag), "fbo_%d", i);
        snprintf(data, sizeof(data), "%d %d %d %d %d %d %d %d %d %d",
                 fmt->index,
                 fmt->color_bit, fmt->color_ifmt, fmt->color_fmt,
                 fmt->depth_bit, fmt->depth_fmt,
                 fmt->stencil_bit, fmt->stencil_fmt,
                 fmt->depth_stencil_fmt,
                 fmt->samples);
        if (eet_write(ef, tag, data, sizeof(data), 1) < 0) return 0;
     }

   return 1;
}

static int
_surface_cap_cache_load(EVGL_Engine *ee)
{
   /* check eet */
   Eet_File *et = NULL;
   char cap_dir_path[PATH_MAX];
   char cap_file_path[PATH_MAX];
   unsigned int i;

   if (!evas_gl_common_file_cache_dir_check(cap_dir_path, sizeof(cap_dir_path)))
      return 0;

   if (!evas_gl_common_file_cache_file_check(cap_dir_path, "surface_cap",
                                             cap_file_path, sizeof(cap_dir_path)))
      return 0;

   /* use eet */
   if (!eet_init()) return 0;
   et = eet_open(cap_file_path, EET_FILE_MODE_READ);
   if (!et) goto error;

   if (!_surface_cap_load(ee, et))
      goto error;

   if (et) eet_close(et);
   eet_shutdown();
   return 1;

error:
   if (et) eet_close(et);
   eet_shutdown();
   return 0;
}

static int
_surface_cap_cache_save(EVGL_Engine *ee)
{
   /* check eet */
   Eet_File *et = NULL; //check eet file
   int tmpfd;
   int res = 0;
   char cap_dir_path[PATH_MAX];
   char cap_file_path[PATH_MAX];
   char tmp_file[PATH_MAX];
   unsigned int i;

   if (!evas_gl_common_file_cache_dir_check(cap_dir_path, sizeof(cap_dir_path)))
     {
        res = evas_gl_common_file_cache_mkpath(cap_dir_path);
        if (!res) return 0; /* we can't make directory */
     }

   evas_gl_common_file_cache_file_check(cap_dir_path, "surface_cap", cap_file_path,
                                        sizeof(cap_dir_path));

   /* use mkstemp for writing */
   snprintf(tmp_file, sizeof(tmp_file), "%s.XXXXXX", cap_file_path);
   tmpfd = mkstemp(tmp_file);
   if (tmpfd < 0) goto error;
   close(tmpfd);

   /* use eet */
   if (!eet_init()) goto error;

   et = eet_open(tmp_file, EET_FILE_MODE_WRITE);
   if (!et) goto error;

   if (!_surface_cap_save(ee, et)) goto error;

   if (eet_close(et) != EET_ERROR_NONE) goto error;
   if (rename(tmp_file,cap_file_path) < 0) goto error;
   eet_shutdown();
   return 1;

error:
   if (et) eet_close(et);
   if (evas_gl_common_file_cache_file_exists(tmp_file)) unlink(tmp_file);
   eet_shutdown();
   return 0;
}

static int
_surface_cap_init(EVGL_Engine *ee)
{
   int max_size = 0;

   // Do internal make current
   if (!_internal_resource_make_current(ee, NULL))
     {
        ERR("Error doing an internal resource make current");
        return 0;
     }

   // Query the max width and height of the surface
   glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_size);

   ee->caps.max_w = max_size;
   ee->caps.max_h = max_size;
   DBG("Max Surface Width: %d   Height: %d", ee->caps.max_w, ee->caps.max_h);

   // Check for MSAA support
#ifdef GL_GLES
   int max_samples = 0;

   if (EXTENSION_SUPPORT(multisampled_render_to_texture))
     {
        glGetIntegerv(GL_MAX_SAMPLES_IMG, &max_samples);

        if (max_samples >= 2)
          {
             ee->caps.msaa_samples[0] = 2;
             ee->caps.msaa_samples[1] = (max_samples>>1) < 2 ? 2 : (max_samples>>1);
             ee->caps.msaa_samples[2] = max_samples;
             ee->caps.msaa_supported  = 1;
          }
     }
#endif

   // Load Surface Cap
   if (!_surface_cap_cache_load(ee))
   {
	   // Check Surface Cap
	   ee->caps.num_fbo_fmts = _surface_cap_check(ee);
		_surface_cap_cache_save(ee);
        printf("SAVEED EXISTED VALUES\n");
   }
	else
	{
        printf("LOADED EXISTED VALUES\n");
	}

   if (ee->caps.num_fbo_fmts)
     {
        _surface_cap_print(ee, 0);
        DBG("Number of supported surface formats: %d", ee->caps.num_fbo_fmts);
        return 1;
     }
   else
     {
        ERR("There are no available surface formats. Error!");
        return 0;
     }
}

static const char *
_glenum_string_get(GLenum e)
{
   switch (e)
     {
      case 0:
         return "0";
      case GL_RGB:
         return "GL_RGB";
      case GL_RGBA:
         return "GL_RGBA";

#ifdef GL_GLES
         // Depth
      case GL_DEPTH_COMPONENT:
         return "GL_DEPTH_COMPONENT";
      case GL_DEPTH_COMPONENT16:
         return "GL_DEPTH_COMPONENT16";
      case GL_DEPTH_COMPONENT24_OES:
         return "GL_DEPTH_COMPONENT24_OES";

         // Stencil
      case GL_STENCIL_INDEX:
         return "GL_STENCIL_INDEX";
      case GL_STENCIL_INDEX1_OES:
         return "GL_STENCIL_INDEX1_OES";
      case GL_STENCIL_INDEX4_OES:
         return "GL_STENCIL_INDEX4_OES";
      case GL_STENCIL_INDEX8:
         return "GL_STENCIL_INDEX8";

         // Depth_Stencil
      case GL_DEPTH_STENCIL_OES:
         return "GL_DEPTH_STENCIL_OES";
#else
         // Depth
      case GL_DEPTH_COMPONENT:
         return "GL_DEPTH_COMPONENT";
      case GL_DEPTH_COMPONENT16:
         return "GL_DEPTH_COMPONENT16";
      case GL_DEPTH_COMPONENT24:
         return "GL_DEPTH_COMPONENT24";
      case GL_DEPTH_COMPONENT32:
         return "GL_DEPTH_COMPONENT32";

         // Stencil
      case GL_STENCIL_INDEX:
         return "GL_STENCIL_INDEX";
      case GL_STENCIL_INDEX1:
         return "GL_STENCIL_INDEX1";
      case GL_STENCIL_INDEX4:
         return "GL_STENCIL_INDEX4";
      case GL_STENCIL_INDEX8:
         return "GL_STENCIL_INDEX8";

         // Depth_Stencil
      case GL_DEPTH24_STENCIL8:
         return "GL_DEPTH24_STENCIL8";
#endif
      default:
         return "ERR";
     }
}

static void
_surface_cap_print(EVGL_Engine *ee, int error)
{
   int i = 0;
#define PRINT_LOG(...) \
   if (error) \
      ERR(__VA_ARGS__); \
   else \
      DBG(__VA_ARGS__);

   PRINT_LOG("----------------------------------------------------------------------------------------------------------------");
   PRINT_LOG("                 Evas GL Supported Surface Format                                                               ");
   PRINT_LOG("----------------------------------------------------------------------------------------------------------------\n");
   PRINT_LOG(" Max Surface Width: %d Height: %d", ee->caps.max_w, ee->caps.max_h);
   PRINT_LOG(" Multisample Support: %d", ee->caps.msaa_supported);
   //if (ee->caps.msaa_supported)
     {
        PRINT_LOG("             Low  Samples: %d", ee->caps.msaa_samples[0]);
        PRINT_LOG("             Med  Samples: %d", ee->caps.msaa_samples[1]);
        PRINT_LOG("             High Samples: %d", ee->caps.msaa_samples[2]);
     }
   PRINT_LOG("[Index] [Color Format]  [------Depth Bits------]      [----Stencil Bits---]     [---Depth_Stencil---]  [Samples]");

#define PRINT_SURFACE_CAP(IDX, COLOR, DEPTH, STENCIL, DS, SAMPLE) \
     { \
        PRINT_LOG("  %3d  %10s    %25s  %25s  %25s  %5d", IDX, _glenum_string_get(COLOR), _glenum_string_get(DEPTH), _glenum_string_get(STENCIL), _glenum_string_get(DS), SAMPLE ); \
     }

   for (i = 0; i < ee->caps.num_fbo_fmts; ++i)
     {
        EVGL_Surface_Format *fmt = &ee->caps.fbo_fmts[i];
        PRINT_SURFACE_CAP(i, fmt->color_fmt, fmt->depth_fmt, fmt->stencil_fmt, fmt->depth_stencil_fmt, fmt->samples);
     }

#undef PRINT_SURFACE_CAP
#undef PRINT_LOG
}


//--------------------------------------------------------//
// Start from here.....
//--------------------------------------------------------//
static int
_surface_buffers_fbo_set(EVGL_Surface *sfc, GLuint fbo)
{
   int status;

   glBindFramebuffer(GL_FRAMEBUFFER, fbo);

   // Detach any previously attached buffers
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D, 0, 0);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
#ifdef GL_GLES
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
#else
   glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
#endif


   // Render Target Texture
   if (sfc->color_buf)
      _texture_2d_attach(sfc->color_buf, GL_COLOR_ATTACHMENT0, 0, sfc->msaa_samples);


   // Depth Stencil RenderBuffer - Attach it to FBO
   if (sfc->depth_stencil_buf)
     {
#ifdef GL_GLES
        _texture_2d_attach(sfc->depth_stencil_buf, GL_DEPTH_ATTACHMENT,
                           GL_STENCIL_ATTACHMENT, sfc->msaa_samples);
#else
        _renderbuffer_attach(sfc->depth_stencil_buf, GL_DEPTH_STENCIL_ATTACHMENT);
#endif
     }

   // Depth RenderBuffer - Attach it to FBO
   if (sfc->depth_buf)
      _renderbuffer_attach(sfc->depth_buf, GL_DEPTH_ATTACHMENT);

   // Stencil RenderBuffer - Attach it to FBO
   if (sfc->stencil_buf)
      _renderbuffer_attach(sfc->stencil_buf, GL_STENCIL_ATTACHMENT);

   // Check FBO for completeness
   status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   if (status != GL_FRAMEBUFFER_COMPLETE)
     {
        ERR("FBO not complete. Error Code: %x!", status);
        return 0;
     }

   return 1;
}

static int
_surface_buffers_create(EVGL_Engine *ee, EVGL_Surface *sfc)
{
   GLuint fbo = 0;
   int ret = 0;

   // Set the context current with resource context/surface
   if (!_internal_resource_make_current(ee, NULL))
     {
        ERR("Error doing an internal resource make current");
        return 0;
     }

   // Create buffers
   if (sfc->color_fmt)
     {
        _texture_2d_create(&sfc->color_buf, sfc->color_ifmt, sfc->color_fmt,
                             GL_UNSIGNED_BYTE, sfc->w, sfc->h);
     }


   // Depth_stencil buffers or separate buffers
   if (sfc->depth_stencil_fmt)
     {
#ifdef GL_GLES
        _texture_2d_create(&sfc->depth_stencil_buf, sfc->depth_stencil_fmt,
                             sfc->depth_stencil_fmt, GL_UNSIGNED_INT_24_8_OES,
                             sfc->w, sfc->h);
#else
        _renderbuffer_create(&sfc->depth_stencil_buf, sfc->depth_stencil_fmt,
                               sfc->w, sfc->h, sfc->msaa_samples);
#endif
     }
   else
     {
        if (sfc->depth_fmt)
          {
             _renderbuffer_create(&sfc->depth_buf, sfc->depth_fmt, sfc->w, sfc->h,
                                    sfc->msaa_samples);
          }
        if (sfc->stencil_fmt)
          {
             _renderbuffer_create(&sfc->stencil_buf, sfc->stencil_fmt, sfc->w,
                                    sfc->h, sfc->msaa_samples);
          }
     }

   // Set surface buffers with an fbo
   if (!(ret = _surface_buffers_fbo_set(sfc, fbo)))
     {
        ERR("Attaching the surface buffers to an FBO failed.");
        if (sfc->color_buf)
           glDeleteTextures(1, &sfc->color_buf);
        if (sfc->depth_buf)
           glDeleteRenderbuffers(1, &sfc->depth_buf);
        if (sfc->stencil_buf)
           glDeleteRenderbuffers(1, &sfc->stencil_buf);
#ifdef GL_GLES
        if (sfc->depth_stencil_buf)
           glDeleteTextures(1, &sfc->depth_stencil_buf);
#else
        if (sfc->depth_stencil_buf)
           glDeleteRenderbuffers(1, &sfc->depth_stencil_buf);
#endif
     }

   if (!ee->funcs->make_current(ee->engine_data, NULL, NULL, 0))
     {
        ERR("Error doing make_current(NULL, NULL).");
        return 0;
     }
   return ret;
}


static int
_internal_config_set(EVGL_Engine *ee, EVGL_Surface *sfc, Evas_GL_Config *cfg)
{
   int i = 0, cfg_index = -1;
   int color_bit = 0, depth_bit = 0, stencil_bit = 0, msaa_samples = 0;

   // Convert Config Format to bitmask friendly format
   color_bit = (1 << cfg->color_format);
   if (cfg->depth_bits) depth_bit = (1 << (cfg->depth_bits-1));
   if (cfg->stencil_bits) stencil_bit = (1 << (cfg->stencil_bits-1));
   if (cfg->multisample_bits)
      msaa_samples = ee->caps.msaa_samples[cfg->multisample_bits-1];

   // Run through all the available formats and choose the first match
   for (i = 0; i < ee->caps.num_fbo_fmts; ++i)
     {
        // Check if the MSAA is supported.  Fallback if not.
        if ((msaa_samples) && (ee->caps.msaa_supported))
          {
             if (msaa_samples > ee->caps.fbo_fmts[i].samples)
                  continue;
          }

        if (color_bit & ee->caps.fbo_fmts[i].color_bit)
          {
             if (depth_bit)
               {
                  if (!(depth_bit & ee->caps.fbo_fmts[i].depth_bit))
                     continue;
               }

             if (stencil_bit)
               {
                  if (!(stencil_bit & ee->caps.fbo_fmts[i].stencil_bit))
                     continue;
               }

             // Set the surface format
             sfc->color_ifmt        = ee->caps.fbo_fmts[i].color_ifmt;
             sfc->color_fmt         = ee->caps.fbo_fmts[i].color_fmt;
             sfc->depth_fmt         = ee->caps.fbo_fmts[i].depth_fmt;
             sfc->stencil_fmt       = ee->caps.fbo_fmts[i].stencil_fmt;
             sfc->depth_stencil_fmt = ee->caps.fbo_fmts[i].depth_stencil_fmt;
             sfc->msaa_samples      = ee->caps.fbo_fmts[i].samples;

             // Direct Rendering Option
             if ( (!stencil_bit) || (ee->direct_override) )
                sfc->direct_fb_opt = cfg->options_bits & EVAS_GL_OPTIONS_DIRECT;

             cfg_index = i;
             break;
          }
     }

   if (cfg_index < 0)
     {
        ERR("Unable to find the matching config format.");
        return 0;
     }
   else
     {
        DBG("-------------Surface Config---------------");
        DBG("Selected Config Index: %d", cfg_index);
        DBG("  Color Format     : %s", _glenum_string_get(sfc->color_fmt));
        DBG("  Depth Format     : %s", _glenum_string_get(sfc->depth_fmt));
        DBG("  Stencil Format   : %s", _glenum_string_get(sfc->stencil_fmt));
        DBG("  D-Stencil Format : %s", _glenum_string_get(sfc->depth_stencil_fmt));
        DBG("  MSAA Samples     : %d", sfc->msaa_samples);
        DBG("  Direct Option    : %d", sfc->direct_fb_opt);
        sfc->cfg_index = cfg_index;
        return 1;
     }
}

static int
_evgl_direct_renderable(EVGL_Engine *ee, EVGL_Resource *rsc, EVGL_Surface *sfc)
{
   if (ee->force_direct_off) return 0;
   if (rsc->id != ee->main_tid) return 0;
   if (!sfc->direct_fb_opt) return 0;
   if (!rsc->direct_img_obj) return 0;

   return 1;
}

//---------------------------------------------------------------//
// Functions used by Evas GL module
//---------------------------------------------------------------//
EVGL_Resource *
_evgl_tls_resource_get(EVGL_Engine *ee)
{
   EVGL_Resource *rsc = NULL;

   // Check if engine is valid
   if (!ee)
     {
        ERR("Invalid EVGL Engine!");
        return NULL;
     }

   // Create internal resources if it hasn't been created already
   if (!(rsc = eina_tls_get(ee->resource_key)))
     {
        if (!(rsc = _internal_resources_create(ee)))
          {
             ERR("Error creating internal resources.");
             return NULL;
          }
     }
   return rsc;
}


EVGL_Context *
_evgl_current_context_get()
{
   EVGL_Resource *rsc;

   rsc = _evgl_tls_resource_get(evgl_engine);

   if (!rsc)
     {
        ERR("No current context set.");
        return NULL;
     }
   else
      return rsc->current_ctx;
}

int
_evgl_not_in_pixel_get(EVGL_Engine *ee)
{
   EVGL_Resource *rsc;

   if (!(rsc=_evgl_tls_resource_get(ee))) return 1;

   EVGL_Context *ctx = rsc->current_ctx;

   if ((!ee->force_direct_off) && (rsc->id == ee->main_tid) &&
       (ctx) && (ctx->current_sfc) && (ctx->current_sfc->direct_fb_opt) &&
       (!rsc->direct_img_obj))
      return 1;
   else
      return 0;
}

int
_evgl_direct_enabled(EVGL_Engine *ee)
{
   EVGL_Resource *rsc;
   EVGL_Surface  *sfc;

   if (!(rsc=_evgl_tls_resource_get(ee))) return 0;
   if (!(rsc->current_ctx)) return 0;
   if (!(sfc=rsc->current_ctx->current_sfc)) return 0;

   return _evgl_direct_renderable(ee, rsc, sfc);
}


//---------------------------------------------------------------//
// Exported functions for evas_engine to use

// Initialize engine
//    - Allocate the engine struct
//    - Assign engine funcs form evas_engine
//    - Create internal resources: internal context, surface for resource creation
//    - Initialize extensions
//    - Check surface capability
//
// This code should be called during eng_setup() in evas_engine
EVGL_Engine *
evgl_engine_create(EVGL_Interface *efunc, void *engine_data)
{
   int direct_off = 0, debug_mode = 0;
   char *s = NULL;

   if (evgl_engine)
     {
        evgl_engine->engine_data = engine_data;
        return evgl_engine;
     }

   // Initialize Log Domain
   if (_evas_gl_log_dom < 0)
      _evas_gl_log_dom = eina_log_domain_register("EvasGL", EVAS_DEFAULT_LOG_COLOR);
   if (_evas_gl_log_dom < 0)
     {
        EINA_LOG_ERR("Can not create a module log domain.");
        return NULL;
     }

   // Check the validity of the efunc
   if ((!efunc) ||
       (!efunc->surface_create) ||
       (!efunc->context_create) ||
       (!efunc->make_current))
     {
        ERR("Invalid Engine Functions for Evas GL Engine.");
        return NULL;
     }

   // Allocate engine
   evgl_engine = calloc(1, sizeof(EVGL_Engine));
   if (!evgl_engine)
     {
        ERR("Error allocating EVGL Engine. GL initialization failed.");
        goto error;
     }

   // Assign functions
   evgl_engine->funcs       = efunc;
   evgl_engine->engine_data = engine_data;

   // Initialize Resource TLS
   if (eina_tls_new(&evgl_engine->resource_key) == EINA_FALSE)
     {
        ERR("Error creating tls key");
        goto error;
     }
   DBG("TLS KEY created: %d", evgl_engine->resource_key);

   // Initialize Extensions
   if (efunc->proc_address_get && efunc->ext_string_get)
      evgl_api_ext_init(efunc->proc_address_get, efunc->ext_string_get(engine_data));
   else
      ERR("Proc address get function not available.  Extension not initialized.");

   DBG("GLUE Extension String: %s", efunc->ext_string_get(engine_data));
   DBG("GL Extension String: %s", glGetString(GL_EXTENSIONS));

   // Surface Caps
   if (!_surface_cap_init(evgl_engine))
     {
        ERR("Error initializing surface cap");
        goto error;
     }

   // Check if Direct Rendering Override Force Off flag is on
   s = getenv("EVAS_GL_DIRECT_OVERRIDE_FORCE_OFF");
   if (s) direct_off = atoi(s);
   if (direct_off == 1)
      evgl_engine->force_direct_off = 1;

   // Check if API Debug mode is on
   s = getenv("EVAS_GL_API_DEBUG");
   if (s) debug_mode = atoi(s);
   if (debug_mode == 1)
      evgl_engine->api_debug_mode = 1;


   // Maint Thread ID (get tid not available in eina thread yet)
   evgl_engine->main_tid = 0;

   // Clear Function Pointers
   memset(&gl_funcs, 0, sizeof(Evas_GL_API));

   return evgl_engine;

error:
   if (evgl_engine->resource_key)
      eina_tls_free(evgl_engine->resource_key);
   if (evgl_engine)
      free(evgl_engine);
   return NULL;
}

// Terminate engine and all the resources
//    - destroy all internal resources
//    - free allocated engine struct
int evgl_engine_destroy(EVGL_Engine *ee)
{
   // Check if engine is valid
   if (!ee)
     {
        ERR("EVGL Engine not valid!");
        return 0;
     }

   // Log
   if (_evas_gl_log_dom >= 0) return 0;
   eina_log_domain_unregister(_evas_gl_log_dom);
   _evas_gl_log_dom = -1;

   // Destroy internal resources
   _internal_resources_destroy(ee);

   // Destroy TLS
   if (ee->resource_key)
      eina_tls_free(ee->resource_key);

   // Free engine
   free(ee);
   evgl_engine = NULL;

   return 1;
}

//-----------------------------------------------------//
// Exported functions for evas_engine to use
//    - We just need to implement these functions and have evas_engine load them :)
void *
evgl_surface_create(EVGL_Engine *ee, Evas_GL_Config *cfg, int w, int h)
{
   EVGL_Surface   *sfc = NULL;
   char *s = NULL;
   int direct_override = 0;

   // Check if engine is valid
   if (!ee)
     {
        ERR("Invalid EVGL Engine!");
        return NULL;
     }

   if (!cfg)
     {
        ERR("Invalid Config!");
        return NULL;
     }

   // Check the size of the surface
   if ((w > ee->caps.max_w) || (h > ee->caps.max_h))
     {
        ERR("Requested surface size [%d, %d] is greater than max supported size [%d, %d]",
             w, h, ee->caps.max_w, ee->caps.max_h);
        return NULL;
     }

   // Check for Direct rendering override env var.
   if (!ee->direct_override)
      if ((s = getenv("EVAS_GL_DIRECT_OVERRIDE")))
        {
           direct_override = atoi(s);
           if (direct_override == 1)
              ee->direct_override = 1;
        }

   // Allocate surface structure
   sfc = calloc(1, sizeof(EVGL_Surface));
   if (!sfc)
     {
        ERR("Surface allocation failed.");
        goto error;
     }

   // Set surface info
   sfc->w = w;
   sfc->h = h;

   // Set the internal config value
   if (!_internal_config_set(ee, sfc, cfg))
     {
        ERR("Unsupported Format!");
        goto error;
     }

   // Create internal buffers
   if (!_surface_buffers_create(ee, sfc))
     {
        ERR("Unable Create Specificed Surfaces.  Unsupported format!");
        goto error;
     };

   return sfc;

error:
   if (sfc) free(sfc);
   return NULL;
}


int
evgl_surface_destroy(EVGL_Engine *ee, EVGL_Surface *sfc)
{
   EVGL_Resource *rsc;

   // Check input parameter
   if ((!ee) || (!sfc))
     {
        ERR("Invalid input data.  Engine: %p  Surface:%p", ee, sfc);
        return 0;
     }

   // Retrieve the resource object
   if (!(rsc = _evgl_tls_resource_get(ee)))
     {
        ERR("Error retrieving resource from TLS");
        return 0;
     }

   if ((rsc->current_ctx) && (rsc->current_ctx->current_sfc == sfc) )
     {
        if (ee->api_debug_mode)
          {
             ERR("The surface is still current before it's being destroyed.");
             ERR("Doing make_current(NULL, NULL)");
          }
        else
          {
             WRN("The surface is still current before it's being destroyed.");
             WRN("Doing make_current(NULL, NULL)");
          }
        evgl_make_current(ee, NULL, NULL);
     }

   // Set the context current with resource context/surface
   if (!_internal_resource_make_current(ee, NULL))
     {
        ERR("Error doing an internal resource make current");
        return 0;
     }

   // Delete buffers
   if (sfc->color_buf)
      glDeleteTextures(1, &sfc->color_buf);

   if (sfc->depth_buf)
      glDeleteRenderbuffers(1, &sfc->depth_buf);

   if (sfc->stencil_buf)
      glDeleteRenderbuffers(1, &sfc->stencil_buf);

   if (sfc->depth_stencil_buf)
     {
#ifdef GL_GLES
        glDeleteTextures(1, &sfc->depth_stencil_buf);
#else
        glDeleteRenderbuffers(1, &sfc->depth_stencil_buf);
#endif
     }

   if (!ee->funcs->make_current(ee->engine_data, NULL, NULL, 0))
      ERR("Error doing make_current(NULL, NULL). Passing over the error...");

   free(sfc);
   sfc = NULL;

   return 1;

}

void *
evgl_context_create(EVGL_Engine *ee, EVGL_Context *share_ctx)
{
   EVGL_Context *ctx   = NULL;

   // Check the input
   if (!ee)
     {
        ERR("Invalid EVGL Engine!");
        return NULL;
     }

   // Allocate context object
   ctx = calloc(1, sizeof(EVGL_Context));
   if (!ctx)
     {
        ERR("Error allocating context object.");
        return NULL;
     }

   if (share_ctx)
      ctx->context = ee->funcs->context_create(ee->engine_data, share_ctx->context);
   else
      ctx->context = ee->funcs->context_create(ee->engine_data, NULL);

   // Call engine create context
   if (!ctx)
     {
        ERR("Error creating context from the Engine.");
        free(ctx);
        return NULL;
     }

   return ctx;
}


int
evgl_context_destroy(EVGL_Engine *ee, EVGL_Context *ctx)
{
   // Check the input
   if ((!ee) || (!ctx))
     {
        ERR("Invalid input data.  Engine: %p  Context:%p", ee, ctx);
        return 0;
     }

   // Set the context current with resource context/surface
   if (!_internal_resource_make_current(ee, NULL))
     {
        ERR("Error doing an internal resource make current");
        return 0;
     }

   // Delete the FBO
   if (ctx->surface_fbo)
      glDeleteFramebuffers(1, &ctx->surface_fbo);

   // Unset the currrent context
   if (!ee->funcs->make_current(ee->engine_data, NULL, NULL, 0))
     {
        ERR("Error doing make_current(NULL, NULL).");
        return 0;
     }

   // Destroy engine context
   if (!ee->funcs->context_destroy(ee->engine_data, ctx->context))
     {
        ERR("Error destroying the engine context.");
        return 0;
     }

   // Free context
   free(ctx);
   ctx = NULL;

   return 1;
}


int
evgl_make_current(EVGL_Engine *ee, EVGL_Surface *sfc, EVGL_Context *ctx)
{
   EVGL_Resource *rsc;
   int curr_fbo = 0;

   // Check the input validity. If either sfc or ctx is NULL, it's also error.
   if ( (!ee) ||
        ((!sfc) && ctx) ||
        (sfc && (!ctx)) )
     {
        ERR("Invalid Inputs. Engine: %p  Surface: %p   Context: %p!", ee, sfc, ctx);
        return 0;
     }

   // Get TLS Resources
   if (!(rsc = _evgl_tls_resource_get(ee))) return 0;

   // Unset
   if ((!sfc) && (!ctx))
     {
        if (!ee->funcs->make_current(ee->engine_data, NULL, NULL, 0))
          {
             ERR("Error doing make_current(NULL, NULL).");
             return 0;
          }
        //FIXME!!!
        if (rsc->current_ctx)
          {
             rsc->current_ctx->current_sfc = NULL;
             rsc->current_ctx = NULL;
          }
        return 1;
     }


   // Do a make current
   if (!_internal_resource_make_current(ee, ctx))
     {
        ERR("Error doing a make current with internal surface. Context: %p", ctx);
        return 0;
     }

   // Normal FBO Rendering
   // Create FBO if it hasn't been created
   if (!ctx->surface_fbo)
      glGenFramebuffers(1, &ctx->surface_fbo);

   // Direct Rendering
   if (_evgl_direct_renderable(ee, rsc, sfc))
     {
        // This is to transition from FBO rendering to direct rendering
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &curr_fbo);
        if (ctx->surface_fbo == (GLuint)curr_fbo)
          {
             glBindFramebuffer(GL_FRAMEBUFFER, 0);
             ctx->current_fbo = 0;
          }
        rsc->direct_rendered = 1;
     }
   else
     {
        // Attach fbo and the buffers
        if (ctx->current_sfc != sfc)
          {
             if (!_surface_buffers_fbo_set(sfc, ctx->surface_fbo))
               {
                  ERR("Attaching buffers to context fbo failed. Engine: %p  Surface: %p Context FBO: %u", ee, sfc, ctx->surface_fbo);
                  return 0;
               }

             // Bind to the previously bound buffer
             if (ctx->current_fbo)
                glBindFramebuffer(GL_FRAMEBUFFER, ctx->current_fbo);
          }
        rsc->direct_rendered = 0;
     }

   ctx->current_sfc = sfc;
   rsc->current_ctx = ctx;

   return 1;
}

const char *
evgl_string_query(EVGL_Engine *ee __UNUSED__, int name)
{
   switch(name)
     {
      case EVAS_GL_EXTENSIONS:
         return (void*)evgl_api_ext_string_get();
      default:
         return "";
     };
}

void *
evgl_proc_address_get(const char *name __UNUSED__)
{
   // Will eventually deprecate this function
   return NULL;
}

int
evgl_native_surface_get(EVGL_Engine *ee, EVGL_Surface *sfc, Evas_Native_Surface *ns)
{
   // Check the input
   if ((!ee) || (!ns))
     {
        ERR("Invalid input data.  Engine: %p  NS:%p", ee, ns);
        return 0;
     }

   ns->type = EVAS_NATIVE_SURFACE_OPENGL;
   ns->version = EVAS_NATIVE_SURFACE_VERSION;
   ns->data.opengl.texture_id = sfc->color_buf;
   ns->data.opengl.framebuffer_id = sfc->color_buf;
   ns->data.opengl.x = 0;
   ns->data.opengl.y = 0;
   ns->data.opengl.w = sfc->w;
   ns->data.opengl.h = sfc->h;

   if (sfc->direct_fb_opt)
      ns->data.opengl.framebuffer_id = 0;

   return 1;
}

int
evgl_direct_rendered(EVGL_Engine *ee)
{
   EVGL_Resource *rsc;

   if (!(rsc=_evgl_tls_resource_get(ee))) return 0;

   return rsc->direct_rendered;
}

void
evgl_direct_img_obj_set(EVGL_Engine *ee, Evas_Object *img)
{
   EVGL_Resource *rsc;

   if (!(rsc=_evgl_tls_resource_get(ee))) return;

   rsc->direct_img_obj = img;
}

Evas_Object *
evgl_direct_img_obj_get(EVGL_Engine *ee)
{
   EVGL_Resource *rsc;

   if (!(rsc=_evgl_tls_resource_get(ee))) return NULL;

   return rsc->direct_img_obj;
}

Evas_GL_API *
evgl_api_get(EVGL_Engine *ee)
{
   _evgl_api_get(&gl_funcs, ee->api_debug_mode);

   return &gl_funcs;
}

//-----------------------------------------------------//


