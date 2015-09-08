#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <fcntl.h>
#include <unistd.h>

#include "Evas.h"
#include "Evas_Engine_Buffer.h"
#include "evas_bench.h"

static const char *
_test_image_get(const char *name)
{
   static char filename[PATH_MAX];

   snprintf(filename, PATH_MAX, TESTS_SRC_DIR"/images/%s", name);

   return filename;
}

static Evas *
_setup_evas()
{
   Evas *evas;
   Evas_Engine_Info_Buffer *einfo;

   evas = evas_new();

   evas_output_method_set(evas, evas_render_method_lookup("buffer"));
   einfo = (Evas_Engine_Info_Buffer *)evas_engine_info_get(evas);

   einfo->info.depth_type = EVAS_ENGINE_BUFFER_DEPTH_RGB32;
   einfo->info.dest_buffer = malloc(sizeof (char) * 500 * 500 * 4);
   einfo->info.dest_buffer_row_bytes = 500 * sizeof (char) * 4;

   evas_engine_info_set(evas, (Evas_Engine_Info *)einfo);

   evas_output_size_set(evas, 500, 500);
   evas_output_viewport_set(evas, 0, 0, 500, 500);

   return evas;
}

// Compatibility import from EFL 1.10
static int eina_file_mkstemp(const char *templatename, Eina_Tmpstr **path)
{

   char buffer[PATH_MAX];
   const char *tmpdir = NULL;
   const char *XXXXXX = NULL;
   int fd, len;
   mode_t old_umask;

#ifndef HAVE_EVIL
#if defined(HAVE_GETUID) && defined(HAVE_GETEUID)
   if (getuid() == geteuid())
#endif
     tmpdir = getenv("TMPDIR");
   if (!tmpdir) tmpdir = "/tmp";
#else
   tmpdir = (char *)evil_tmpdir_get();
#endif /* ! HAVE_EVIL */

   len = snprintf(buffer, PATH_MAX, "%s/%s", tmpdir, templatename);

   /*
    * Make sure temp file is created with secure permissions,
    * http://man7.org/linux/man-pages/man3/mkstemp.3.html#NOTES
    */
   old_umask = umask(S_IRWXG|S_IRWXO);
   if ((XXXXXX = strstr(buffer, "XXXXXX.")) != NULL)
     {
        int suffixlen = buffer + len - XXXXXX - 6;
        fd = mkstemps(buffer, suffixlen);
     }
   else
     fd = mkstemp(buffer);
   umask(old_umask);

   if (path) *path = eina_tmpstr_add(buffer);
   if (fd < 0)
     return -1;

   return fd;
}

static void
evas_bench_saver_tgv(int request)
{
   Evas *e = _setup_evas();
   const char *source;
   Eina_Tmpstr *dest;
   Evas_Object *o;
   int i;

   source = _test_image_get("mars_rover_panorama_half-size.jpg");
   eina_file_mkstemp("evas_saver_benchXXXXXX.tgv", &dest);

   o = evas_object_image_add(e);
   evas_object_image_file_set(o, source, NULL);

   for (i = 0; i < request; i++)
     {
        evas_object_image_save(o, dest, NULL, "compress=1 quality=50");
     }

   unlink(dest);
   eina_tmpstr_del(dest);

   evas_free(e);
}

void evas_bench_saver(Eina_Benchmark *bench)
{
   eina_benchmark_register(bench, "tgv-saver", EINA_BENCHMARK(evas_bench_saver_tgv), 20, 2000, 100);
}
