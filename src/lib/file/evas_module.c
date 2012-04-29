#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>

#include <evas_common.h>
#include <evas_private.h>
#include <evas_module.h>


static Eina_Hash *evas_modules[4] = {
  NULL,
  NULL,
  NULL,
  NULL
};

static Eina_List *eina_evas_modules = NULL;
static Eina_List *evas_module_paths = NULL;
static Eina_Array *evas_engines = NULL;

static Eina_List *
_evas_module_append(Eina_List *list, char *path)
{
   if (path)
     {
	if (evas_file_path_exists(path))
	  list = eina_list_append(list, path);
	else
	  free(path);
     }
   return list;
}

/* this will alloc a list of paths to search for the modules */
/* by now these are:  */
/* 1. ~/.evas/modules/ */
/* 2. $(EVAS_MODULE_DIR)/evas/modules/ */
/* 3. dladdr/evas/modules/ */
/* 4. PREFIX/evas/modules/ */
void
evas_module_paths_init(void)
{
   char *path;

   /* 1. ~/.evas/modules/ */
   path = eina_module_environment_path_get("HOME", "/.evas/modules");
   evas_module_paths = _evas_module_append(evas_module_paths, path);

   /* 2. $(EVAS_MODULE_DIR)/evas/modules/ */
   path = eina_module_environment_path_get("EVAS_MODULES_DIR", "/evas/modules");
   if (eina_list_search_unsorted(evas_module_paths, (Eina_Compare_Cb) strcmp, path))
     free(path);
   else
     evas_module_paths = _evas_module_append(evas_module_paths, path);

   /* 3. libevas.so/../evas/modules/ */
   path = eina_module_symbol_path_get(evas_module_paths_init, "/evas/modules");
   if (eina_list_search_unsorted(evas_module_paths, (Eina_Compare_Cb) strcmp, path))
     free(path);
   else
     evas_module_paths = _evas_module_append(evas_module_paths, path);

   /* 4. PREFIX/evas/modules/ */
#ifndef _MSC_VER
   path = PACKAGE_LIB_DIR "/evas/modules";
   if (!eina_list_search_unsorted(evas_module_paths, (Eina_Compare_Cb) strcmp, path))
     {
	path = strdup(path);
	if (path)
	  evas_module_paths = _evas_module_append(evas_module_paths, path);
     }
#endif
}

#define EVAS_EINA_STATIC_MODULE_DEFINE(Tn, Name)	\
  Eina_Bool evas_##Tn##_##Name##_init(void);		\
  void evas_##Tn##_##Name##_shutdown(void);

#define EVAS_EINA_STATIC_MODULE_USE(Tn, Name)	\
  { evas_##Tn##_##Name##_init, evas_##Tn##_##Name##_shutdown }

EVAS_EINA_STATIC_MODULE_DEFINE(engine, buffer);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, direct3d);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, directfb);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, fb);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, gl_x11);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, gl_sdl);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, psl1ght);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_16);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_16_ddraw);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_16_sdl);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_16_wince);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_16_x11);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_8);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_8_x11);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_ddraw);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_gdi);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_generic);
EVAS_EINA_STATIC_MODULE_DEFINE(engine, software_x11);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, bmp);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, edb);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, eet);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, generic);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, gif);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, ico);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, jpeg);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, pmaps);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, png);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, psd);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, svg);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, tga);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, tiff);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, wbmp);
EVAS_EINA_STATIC_MODULE_DEFINE(image_loader, xpm);
EVAS_EINA_STATIC_MODULE_DEFINE(image_saver, edb);
EVAS_EINA_STATIC_MODULE_DEFINE(image_saver, eet);
EVAS_EINA_STATIC_MODULE_DEFINE(image_saver, jpeg);
EVAS_EINA_STATIC_MODULE_DEFINE(image_saver, png);
EVAS_EINA_STATIC_MODULE_DEFINE(image_saver, tiff);

static const struct {
   Eina_Bool (*init)(void);
   void (*shutdown)(void);
} evas_static_module[] = {
#ifdef EVAS_STATIC_BUILD_BUFFER
  EVAS_EINA_STATIC_MODULE_USE(engine, buffer),
#endif
#ifdef EVAS_STATIC_BUILD_DIRECT3D
  EVAS_EINA_STATIC_MODULE_USE(engine, direct3d),
#endif
#ifdef EVAS_STATIC_BUILD_DIRECTFB
  EVAS_EINA_STATIC_MODULE_USE(engine, directfb),
#endif
#ifdef EVAS_STATIC_BUILD_FB
  EVAS_EINA_STATIC_MODULE_USE(engine, fb),
#endif
#ifdef EVAS_STATIC_BUILD_GL_X11
  EVAS_EINA_STATIC_MODULE_USE(engine, gl_x11),
#endif
#ifdef EVAS_STATIC_BUILD_GL_SDL
  EVAS_EINA_STATIC_MODULE_USE(engine, gl_sdl),
#endif
#ifdef EVAS_STATIC_BUILD_PSL1GHT
  EVAS_EINA_STATIC_MODULE_USE(engine, psl1ght),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_16
  EVAS_EINA_STATIC_MODULE_USE(engine, software_16),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_16_DDRAW
  EVAS_EINA_STATIC_MODULE_USE(engine, software_16_ddraw),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_16_SDL
  EVAS_EINA_STATIC_MODULE_USE(engine, software_16_sdl),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_16_WINCE
  EVAS_EINA_STATIC_MODULE_USE(engine, software_16_wince),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_16_X11
  EVAS_EINA_STATIC_MODULE_USE(engine, software_16_x11),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_16_DDRAW
  EVAS_EINA_STATIC_MODULE_USE(engine, software_ddraw),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_16_GDI
  EVAS_EINA_STATIC_MODULE_USE(engine, software_gdi),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_8
  EVAS_EINA_STATIC_MODULE_USE(engine, software_8),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_8_X11
  EVAS_EINA_STATIC_MODULE_USE(engine, software_8_x11),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_GENERIC
  EVAS_EINA_STATIC_MODULE_USE(engine, software_generic),
#endif
#ifdef EVAS_STATIC_BUILD_SOFTWARE_X11
  EVAS_EINA_STATIC_MODULE_USE(engine, software_x11),
#endif
#ifdef EVAS_STATIC_BUILD_BMP
  EVAS_EINA_STATIC_MODULE_USE(image_loader, bmp),
#endif
#ifdef EVAS_STATIC_BUILD_EDB
  EVAS_EINA_STATIC_MODULE_USE(image_loader, edb),
#endif
#ifdef EVAS_STATIC_BUILD_EET
  EVAS_EINA_STATIC_MODULE_USE(image_loader, eet),
#endif
#ifdef EVAS_STATIC_BUILD_GENERIC
  EVAS_EINA_STATIC_MODULE_USE(image_loader, generic),
#endif
#ifdef EVAS_STATIC_BUILD_GIF
  EVAS_EINA_STATIC_MODULE_USE(image_loader, gif),
#endif
#ifdef EVAS_STATIC_BUILD_ICO
  EVAS_EINA_STATIC_MODULE_USE(image_loader, ico),
#endif
#ifdef EVAS_STATIC_BUILD_JPEG
  EVAS_EINA_STATIC_MODULE_USE(image_loader, jpeg),
#endif
#ifdef EVAS_STATIC_BUILD_PMAPS
  EVAS_EINA_STATIC_MODULE_USE(image_loader, pmaps),
#endif
#ifdef EVAS_STATIC_BUILD_PNG
  EVAS_EINA_STATIC_MODULE_USE(image_loader, png),
#endif
#ifdef EVAS_STATIC_BUILD_PSD
  EVAS_EINA_STATIC_MODULE_USE(image_loader, psd),
#endif
#ifdef EVAS_STATIC_BUILD_SVG
  EVAS_EINA_STATIC_MODULE_USE(image_loader, svg),
#endif
#ifdef EVAS_STATIC_BUILD_TGA
  EVAS_EINA_STATIC_MODULE_USE(image_loader, tga),
#endif
#ifdef EVAS_STATIC_BUILD_TIFF
  EVAS_EINA_STATIC_MODULE_USE(image_loader, tiff),
#endif
#ifdef EVAS_STATIC_BUILD_WBMP
  EVAS_EINA_STATIC_MODULE_USE(image_loader, wbmp),
#endif
#ifdef EVAS_STATIC_BUILD_XPM
  EVAS_EINA_STATIC_MODULE_USE(image_loader, xpm),
#endif
#ifdef EVAS_STATIC_BUILD_EDB
  EVAS_EINA_STATIC_MODULE_USE(image_saver, edb),
#endif
#ifdef EVAS_STATIC_BUILD_EET
  EVAS_EINA_STATIC_MODULE_USE(image_saver, eet),
#endif
#if defined (EVAS_BUILD_SAVER_JPEG) && defined (EVAS_STATIC_BUILD_JPEG)
  EVAS_EINA_STATIC_MODULE_USE(image_saver, jpeg),
#endif
#ifdef EVAS_STATIC_BUILD_PNG
  EVAS_EINA_STATIC_MODULE_USE(image_saver, png),
#endif
#ifdef EVAS_STATIC_BUILD_TIFF
  EVAS_EINA_STATIC_MODULE_USE(image_saver, tiff),
#endif
  { NULL, NULL }
};

/* this will alloc an Evas_Module struct for each module
 * it finds on the paths */
void
evas_module_init(void)
{
   int i;

   evas_module_paths_init();

   evas_modules[EVAS_MODULE_TYPE_ENGINE] = eina_hash_string_small_new(/* FIXME: Add a function to cleanup stuff. */ NULL);
   evas_modules[EVAS_MODULE_TYPE_IMAGE_LOADER] = eina_hash_string_small_new(/* FIXME: Add a function to cleanup stuff. */ NULL);
   evas_modules[EVAS_MODULE_TYPE_IMAGE_SAVER] = eina_hash_string_small_new(/* FIXME: Add a function to cleanup stuff. */ NULL);
   evas_modules[EVAS_MODULE_TYPE_OBJECT] = eina_hash_string_small_new(/* FIXME: Add a function to cleanup stuff. */ NULL);

   evas_engines = eina_array_new(4);

   for (i = 0; evas_static_module[i].init; ++i)
     evas_static_module[i].init();
}

Eina_Bool
evas_module_register(const Evas_Module_Api *module, Evas_Module_Type type)
{
   Evas_Module *em;

   if ((unsigned int)type > 3) return EINA_FALSE;
   if (!module) return EINA_FALSE;
   if (module->version != EVAS_MODULE_API_VERSION) return EINA_FALSE;

   em = eina_hash_find(evas_modules[type], module->name);
   if (em) return EINA_FALSE;

   em = calloc(1, sizeof (Evas_Module));
   if (!em) return EINA_FALSE;

   em->definition = module;

   if (type == EVAS_MODULE_TYPE_ENGINE)
     {
	eina_array_push(evas_engines, em);
	em->id_engine = eina_array_count(evas_engines);
     }

   eina_hash_direct_add(evas_modules[type], module->name, em);

   return EINA_TRUE;
}

Eina_List *
evas_module_engine_list(void)
{
   Evas_Module *em;
   Eina_List *r = NULL, *l, *ll;
   Eina_Array_Iterator iterator;
   Eina_Iterator *it, *it2;
   unsigned int i;
   const char *s, *s2;
   char buf[4096];

   EINA_LIST_FOREACH(evas_module_paths, l, s)
     {
        snprintf(buf, sizeof(buf), "%s/engines", s);
        it = eina_file_direct_ls(buf);
        if (it)
          {
             Eina_File_Direct_Info *fi;

             EINA_ITERATOR_FOREACH(it, fi)
               {
                  const char *fname = fi->path + fi->name_start;
                  snprintf(buf, sizeof(buf), "%s/engines/%s/%s", 
                           s, fname, MODULE_ARCH);
                  it2 = eina_file_ls(buf);
                  if (it2)
                    {
                       EINA_LIST_FOREACH(r, ll, s2)
                         {
                            if (!strcmp(fname, s2)) break;
                         }
                       if (!ll)
                         r = eina_list_append(r, eina_stringshare_add(fname));
                       eina_iterator_free(it2);
                    }
               }
             eina_iterator_free(it);
          }
     }
   
   EINA_ARRAY_ITER_NEXT(evas_engines, i, em, iterator)
     {
        EINA_LIST_FOREACH(r, ll, s2)
          {
             if (!strcmp(em->definition->name, s2)) break;
          }
        if (!ll)
          r = eina_list_append(r, eina_stringshare_add(em->definition->name));
     }

   return r;
}

Eina_Bool
evas_module_unregister(const Evas_Module_Api *module, Evas_Module_Type type)
{
   Evas_Module *em;

   if ((unsigned int)type > 3) return EINA_FALSE;
   if (!module) return EINA_FALSE;

   em = eina_hash_find(evas_modules[type], module->name);
   if (!em || em->definition != module) return EINA_FALSE;

   if (type == EVAS_MODULE_TYPE_ENGINE)
     eina_array_data_set(evas_engines, em->id_engine, NULL);

   eina_hash_del(evas_modules[type], module->name, em);
   free(em);

   return EINA_TRUE;
}

#if defined(__CEGCC__) || defined(__MINGW32CE__)
# define EVAS_MODULE_NAME_IMAGE_SAVER "saver_%s.dll"
# define EVAS_MODULE_NAME_IMAGE_LOADER "loader_%s.dll"
# define EVAS_MODULE_NAME_ENGINE "engine_%s.dll"
# define EVAS_MODULE_NAME_OBJECT "object_%s.dll"
#elif _WIN32
# define EVAS_MODULE_NAME_IMAGE_SAVER "module.dll"
# define EVAS_MODULE_NAME_IMAGE_LOADER "module.dll"
# define EVAS_MODULE_NAME_ENGINE "module.dll"
# define EVAS_MODULE_NAME_OBJECT "module.dll"
#else
# define EVAS_MODULE_NAME_IMAGE_SAVER "module.so"
# define EVAS_MODULE_NAME_IMAGE_LOADER "module.so"
# define EVAS_MODULE_NAME_ENGINE "module.so"
# define EVAS_MODULE_NAME_OBJECT "module.so"
#endif

Evas_Module *
evas_module_find_type(Evas_Module_Type type, const char *name)
{
   const char *path;
   const char *format = NULL;
   char buffer[4096];
   Evas_Module *em;
   Eina_Module *en;
   Eina_List *l;

   if ((unsigned int)type > 3) return NULL;

   em = eina_hash_find(evas_modules[type], name);
   if (em) return em;

   EINA_LIST_FOREACH(evas_module_paths, l, path)
     {
	switch (type)
	  {
	   case EVAS_MODULE_TYPE_ENGINE: format = "%s/engines/%s/%s/" EVAS_MODULE_NAME_ENGINE; break;
	   case EVAS_MODULE_TYPE_IMAGE_LOADER: format = "%s/loaders/%s/%s/" EVAS_MODULE_NAME_IMAGE_LOADER; break;
	   case EVAS_MODULE_TYPE_IMAGE_SAVER: format = "%s/savers/%s/%s/" EVAS_MODULE_NAME_IMAGE_SAVER; break;
	   case EVAS_MODULE_TYPE_OBJECT: format = "%s/object/%s/%s/" EVAS_MODULE_NAME_OBJECT; break;
	  }

	snprintf(buffer, sizeof (buffer), format, path, name, MODULE_ARCH, name);
	if (!evas_file_path_is_file(buffer)) continue;

	en = eina_module_new(buffer);
	if (!en) continue;

	if (!eina_module_load(en))
	  {
	     eina_module_free(en);
	     continue;
	  }

	em = eina_hash_find(evas_modules[type], name);
	if (em)
	  {
	     eina_evas_modules = eina_list_append(eina_evas_modules, en);
	     return em;
	  }

	eina_module_free(en);
     }

   return NULL;
}

Evas_Module *
evas_module_engine_get(int render_method)
{
   if ((render_method <= 0) ||
       ((unsigned int)render_method > eina_array_count(evas_engines)))
     return NULL;
   return eina_array_data_get(evas_engines, render_method - 1);
}

void
evas_module_foreach_image_loader(Eina_Hash_Foreach cb, const void *fdata)
{
   eina_hash_foreach(evas_modules[EVAS_MODULE_TYPE_IMAGE_LOADER], cb, fdata);
}

int
evas_module_load(Evas_Module *em)
{
   if (em->loaded) return 1;
   if (!em->definition) return 0;

   if (!em->definition->func.open(em)) return 0;
   em->loaded = 1;

#ifdef BUILD_ASYNC_PRELOAD
   LKI(em->lock);
#endif
   return 1;
}

void
evas_module_unload(Evas_Module *em)
{
   if (!em->loaded)
     return;
   if (!em->definition)
     return ;

// for now lets not unload modules - they may still be in use.   
//   em->definition->func.close(em);
//   em->loaded = 0;

#ifdef BUILD_ASYNC_PRELOAD
   LKD(em->lock);
#endif
}

void
evas_module_ref(Evas_Module *em)
{
#ifdef BUILD_ASYNC_PRELOAD
   LKL(em->lock);
#endif
   em->ref++;
#ifdef BUILD_ASYNC_PRELOAD
   LKU(em->lock);
#endif
}

void
evas_module_unref(Evas_Module *em)
{
#ifdef BUILD_ASYNC_PRELOAD
   LKL(em->lock);
#endif
   em->ref--;
#ifdef BUILD_ASYNC_PRELOAD
   LKU(em->lock);
#endif
}

static int use_count = 0;

void
evas_module_use(Evas_Module *em)
{
   em->last_used = use_count;
}

void
evas_module_clean(void)
{
   static int call_count = 0;
/*    int ago; */
   int noclean = -1;
/*    Eina_List *l; */
/*    Evas_Module *em; */

   /* only clean modules every 256 calls */
   call_count++;
   if (call_count <= 256) return;
   call_count = 0;

   if (noclean == -1)
     {
	if (getenv("EVAS_NOCLEAN"))
          noclean = 1;
	else 
          noclean = 0;
     }
   if (noclean == 1) return;

   /* disable module cleaning for now - may cause instability with some modules */
   return;

   /* FIXME: Don't know what it is supposed to do. */
/*    /\* incriment use counter = 28bits *\/ */
/*    use_count++; */
/*    if (use_count > 0x0fffffff) use_count = 0; */

/*    /\* printf("CLEAN!\n"); *\/ */
/*    /\* go through all modules *\/ */
/*    EINA_LIST_FOREACH(evas_modules, l, em) */
/*      { */
/*         /\* printf("M %s %i %i\n", em->name, em->ref, em->loaded); *\/ */
/* 	/\* if the module is refernced - skip *\/ */
/* 	if ((em->ref > 0) || (!em->loaded)) continue; */
/* 	/\* how many clean cycles ago was this module last used *\/ */
/* 	ago = use_count - em->last_used; */
/* 	if (em->last_used > use_count) ago += 0x10000000; */
/* 	/\* if it was used last more than N clean cycles ago - unload *\/ */
/* 	if (ago > 5) */
/* 	  { */
/*             /\* printf("  UNLOAD %s\n", em->name); *\/ */
/* 	     evas_module_unload(em); */
/* 	  } */
/*      } */
}

/* will dlclose all the modules loaded and free all the structs */
void
evas_module_shutdown(void)
{
   Eina_Module *en;
   char *path;
   int i;

   for (i = 0; evas_static_module[i].shutdown; ++i)
     evas_static_module[i].shutdown();

   EINA_LIST_FREE(eina_evas_modules, en)
     eina_module_free(en);

   eina_hash_free(evas_modules[EVAS_MODULE_TYPE_ENGINE]);
   evas_modules[EVAS_MODULE_TYPE_ENGINE] = NULL;
   eina_hash_free(evas_modules[EVAS_MODULE_TYPE_IMAGE_LOADER]);
   evas_modules[EVAS_MODULE_TYPE_IMAGE_LOADER] = NULL;
   eina_hash_free(evas_modules[EVAS_MODULE_TYPE_IMAGE_SAVER]);
   evas_modules[EVAS_MODULE_TYPE_IMAGE_SAVER] = NULL;
   eina_hash_free(evas_modules[EVAS_MODULE_TYPE_OBJECT]);
   evas_modules[EVAS_MODULE_TYPE_OBJECT] = NULL;

   EINA_LIST_FREE(evas_module_paths, path)
     free(path);

   eina_array_free(evas_engines);
   evas_engines = NULL;
}

EAPI int
_evas_module_engine_inherit(Evas_Func *funcs, char *name)
{
   Evas_Module *em;

   em = evas_module_find_type(EVAS_MODULE_TYPE_ENGINE, name);
   if (em)
     {
	if (evas_module_load(em))
	  {
	     /* FIXME: no way to unref */
	     evas_module_ref(em);
	     evas_module_use(em);
	     *funcs = *((Evas_Func *)(em->functions));
	     return 1;
	  }
     }
   return 0;
}

static Eina_Prefix *pfx = NULL;

EAPI const char *
_evas_module_libdir_get(void)
{
   if (!pfx) pfx = eina_prefix_new
      (NULL, _evas_module_libdir_get, "EVAS", "evas", NULL,
       PACKAGE_BIN_DIR, PACKAGE_LIB_DIR, PACKAGE_DATA_DIR, PACKAGE_DATA_DIR);
   if (!pfx) return NULL;
   return eina_prefix_lib_get(pfx);
}
