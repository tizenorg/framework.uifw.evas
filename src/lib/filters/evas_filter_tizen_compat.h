#ifndef EVAS_FILTERS_TIZEN_COMPAT_H
#define EVAS_FILTERS_TIZEN_COMPAT_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if (VMAJ == 1) && (VMIN == 7)

/* This file will contain a few Tizen compilation fixups */

#define EVAS_TIZEN_FIXUPS 1

#include "evas_common.h"

// EFL 1.8

#define CRI(...) CRIT(__VA_ARGS__)

// Eina 1.8

/**
 * @def EINA_INLIST_FREE
 * @param list The list to free.
 * @param it The pointer to the list item, i.e. a pointer to each item
 * that is part of the list.
 *
 * NOTE: it is the duty of the body loop to properly remove the item from the
 * inlist and free it. This function will turn into a infinite loop if you
 * don't remove all items from the list.
 * @since 1.8
 */
#define EINA_INLIST_FREE(list, it)                              \
  for (it = (__typeof__(it)) list; list; it = (__typeof__(it)) list)

static inline Eina_Inlist *eina_inlist_last(Eina_Inlist *i)
{
   return i->last;
}

#endif // Evas 1.7

#endif // EVAS_FILTERS_TIZEN_COMPAT_H
