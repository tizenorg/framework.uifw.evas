#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "evas_cs.h"

int _cserve_tool_log_dom = -1;

int
main(int argc, char **argv)
{
   int i;

   evas_init();
   if (!evas_cserve_init())
     {
        printf("ERROR: Cannot connect to cserve. abort\n");
        exit(-1);
     }

   for (i = 1; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-h")) ||
            (!strcmp(argv[i], "-help")) ||
            (!strcmp(argv[i], "--help")))
          {
             printf("Options:\n"
                    "\t-h                               This help\n"
                    "\tgetconfig                        Get configuration values\n"
                    "\tsetconfig CSIZE CTIME CTIMECHECK Set the config values\n"
                    "\tgetstats                         Get current cache statistics\n"
                    "\tgetinfo                          Get current cache content info\n"
                    );
             exit(0);
          }
        else if ((!strcmp(argv[i], "getconfig")))
          {
             Op_Getconfig_Reply config;
             
             if (!evas_cserve_raw_config_get(&config))
               {
                  printf("ERROR: cannot fetch config.\n");
                  exit(-1);
               }
             printf("-REPLY-\n");
             printf("csize: %i\n", config.cache_max_usage);
             printf("ctime: %i\n", config.cache_item_timeout);
             printf("ctimecheck: %i\n", config.cache_item_timeout_check);
             printf("-OK-\n");
          }
        else if ((!strcmp(argv[i], "setconfig")) && (i < (argc - 3)))
          {
             Op_Setconfig config;
             
             i++;
             config.cache_max_usage = atoi(argv[i]);
             i++;
             config.cache_item_timeout = atoi(argv[i]);
             i++;
             config.cache_item_timeout_check = atoi(argv[i]);
             if (!evas_cserve_raw_config_set(&config))
               {
                  printf("ERROR: cannot set config.\n");
                  exit(-1);
               }
          }
        else if ((!strcmp(argv[i], "getstats")))
          {
             Op_Getstats_Reply stats;
             
             if (!evas_cserve_raw_stats_get(&stats))
               {
                  printf("ERROR: cannot fetch stats.\n");
                  exit(-1);
               }
             printf("-REPLY-\n");
             printf("saved_memory: %i Kb\n", stats.saved_memory / 1024);
             printf("wasted_memory: %i Kb\n", stats.wasted_memory / 1024);
             printf("saved_memory_peak: %i Kb\n", stats.saved_memory_peak / 1024);
             printf("wasted_memory_peak: %i Kb\n", stats.wasted_memory_peak / 1024);
             printf("saved_time_image_header_load: %1.3f sec\n", stats.saved_time_image_header_load);
             printf("saved_time_image_data_load: %1.3f sec\n", stats.saved_time_image_data_load);
             printf("-OK-\n");
          }
        else if ((!strcmp(argv[i], "getinfo")))
          {
             Op_Getinfo_Reply *info;
             unsigned char *p;
             int h, j;

             info = evas_cserve_raw_info_get();
             if (!info)
               {
                  printf("ERROR: cannot fetch info.\n");
                  exit(-1);
               }
             j = info->active.count + info->cached.count;
             printf("-REPLY-\n");
             printf("active_count: %i\n", info->active.count);
             printf("active_memory: %i Kb\n", info->active.mem_total);
             printf("cache_count: %i\n", info->cached.count);
             printf("cache_memory: %i Kb\n", info->cached.mem_total);
             p = (unsigned char *)info;
             p += sizeof(Op_Getinfo_Reply);
             for (h = 0; h < j; h++)
               {
                  Op_Getinfo_Item it;
                  char *file, *key, buf[512];
                  struct tm *ltm;
                  
                  memcpy(&it, p, sizeof(Op_Getinfo_Item));
                  file = (char*) (p + sizeof(Op_Getinfo_Item));
                  key = file + strlen(file) + 1;
                  printf("-IMAGE- [#%i]\n", h);
                  printf("  file       : %s\n", file);
                  printf("  key        : %s\n", key);
                  printf("  size       : %i x %i\n", it.w, it.h);
                  printf("  active     : %i\n", (int)it.active);
                  printf("  memory used: %i bytes (%i Kb)\n", it.memory_footprint, (it.memory_footprint + 1023) / 1024);
                  printf("  has alpha  : %i\n", (int)it.alpha);
                  printf("  data loaded: %i\n", (int)it.data_loaded);
                  printf("  dead       : %i\n", (int)it.dead);
                  printf("  useless    : %i\n", (int)it.useless);
                  printf("  image refs : %i\n", it.refcount);
                  printf("  data  refs : %i\n", it.data_refcount);
                  printf("  header load: %1.5f sec\n", it.head_load_time);
                  printf("  data load  : %1.5f sec\n", it.data_load_time);
                  if (it.cached_time == 0)
                    printf("  cached at  : N/A\n");
                  else
                    {
                       ltm = localtime(&(it.cached_time));
                       if (ltm)
                         {
                            strftime(buf, sizeof(buf), "%Y.%m.%d %T", ltm);
                            printf("  cached at  : %s\n", buf);
                         }
                    }
                  ltm = localtime(&(it.file_mod_time));
                  if (ltm)
                    {
                       strftime(buf, sizeof(buf), "%Y.%m.%d %T", ltm);
                       printf("  file mod at: %s\n", buf);
                    }
                  ltm = localtime(&(it.file_checked_time));
                  if (ltm)
                    {
                       strftime(buf, sizeof(buf), "%Y.%m.%d %T", ltm);
                       printf("  file check : %s\n", buf);
                    }
                  p += sizeof(Op_Getinfo_Item) + it.file_key_size;
               }
             free(info);
             printf("-OK-\n");
          }
     }
   evas_cserve_shutdown();
   evas_shutdown();
   return 0;
}
