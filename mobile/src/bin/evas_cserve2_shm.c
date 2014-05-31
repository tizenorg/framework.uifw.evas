#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "evas_cserve2.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct _Shm_Mapping
{
   const char *name;
   size_t length;
   Eina_Inlist *segments;
};

typedef struct _Shm_Mapping Shm_Mapping;

struct _Shm_Handle
{
   EINA_INLIST;
   Shm_Mapping *mapping;
   off_t map_offset;
   off_t image_offset;
   size_t map_size;
   size_t image_size;
   int refcount;
   void *data;
};

static int id = 0;

size_t
cserve2_shm_size_normalize(size_t size)
{
   long pagesize;
   size_t normalized;

   pagesize = sysconf(_SC_PAGESIZE);
   if (pagesize < 1)
     {
        ERR("sysconf() reported weird value for PAGESIZE, assuming 4096.");
        pagesize = 4096;
     }

   normalized = ((size + pagesize - 1) / pagesize) * pagesize;

   return normalized;
}

Shm_Handle *
cserve2_shm_request(size_t size)
{
   Shm_Mapping *map;
   Shm_Handle *shm;
   char shmname[NAME_MAX];
   size_t map_size;
   int fd;

   map = calloc(1, sizeof(Shm_Mapping));
   if (!map)
     {
        ERR("Failed to allocate mapping handler.");
        return NULL;
     }

   do {
        snprintf(shmname, sizeof(shmname), "/evas-shm-img-%d", id++);
        fd = shm_open(shmname, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (fd == -1 && errno != EEXIST)
          {
             ERR("Failed to create shared memory object '%s': %m", shmname);
             free(map);
             return NULL;
          }
   } while (fd == -1);

   map_size = cserve2_shm_size_normalize(size);

   if (ftruncate(fd, map_size) == -1)
     {
        ERR("Failed to set size of shared file: %m");
        close(fd);
        free(map);
        return NULL;
     }
   close(fd);

   map->name = eina_stringshare_add(shmname);
   map->length = map_size;

   shm = calloc(1, sizeof(Shm_Handle));
   if (!shm)
     {
        ERR("Failed to allocate shared memory handler.");
        eina_stringshare_del(map->name);
        free(map);
        return NULL;
     }

   map->segments = eina_inlist_append(map->segments, EINA_INLIST_GET(shm));
   shm->mapping = map;
   shm->map_offset = 0;
   shm->image_offset = 0;

   shm->image_size = size;
   shm->map_size = map_size;

   return shm;
}

void
cserve2_shm_unref(Shm_Handle *shm)
{
   Shm_Mapping *map = shm->mapping;

   map->segments = eina_inlist_remove(map->segments, EINA_INLIST_GET(shm));

   if (shm->data)
     munmap(shm->data, shm->image_size);
   free(shm);

   if (map->segments)
     return;

   shm_unlink(map->name);
   eina_stringshare_del(map->name);
   free(map);
}

const char *
cserve2_shm_name_get(const Shm_Handle *shm)
{
   return shm->mapping->name;
}

off_t
cserve2_shm_map_offset_get(const Shm_Handle *shm)
{
   return shm->map_offset;
}

off_t
cserve2_shm_offset_get(const Shm_Handle *shm)
{
   return shm->image_offset;
}

size_t
cserve2_shm_map_size_get(const Shm_Handle *shm)
{
   return shm->map_size;
}

size_t
cserve2_shm_size_get(const Shm_Handle *shm)
{
   return shm->image_size;
}

void *
cserve2_shm_map(Shm_Handle *shm)
{
   int fd;
   const char *name;

   if (shm->refcount++)
     return shm->data;

   name = cserve2_shm_name_get(shm);

   fd = shm_open(name, O_RDWR, S_IWUSR);
   if (fd == -1)
     return MAP_FAILED;

   shm->data = mmap(NULL, shm->image_size, PROT_WRITE, MAP_SHARED,
                    fd, shm->image_offset);

   close(fd);

   return shm->data;
}

void
cserve2_shm_unmap(Shm_Handle *shm)
{
   if (--shm->refcount)
     return;

   munmap(shm->data, shm->image_size);
   shm->data = NULL;
}
