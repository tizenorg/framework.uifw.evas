
MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = \
-I. \
-I$(top_srcdir)/src/lib \
-I$(top_srcdir)/src/lib/include \
-I$(top_srcdir)/src/lib/engines/common \
-DPACKAGE_BIN_DIR=\"$(bindir)\" \
-DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(datadir)/$(PACKAGE)\" \
@FREETYPE_CFLAGS@ \
@EVAS_GENERAL_CFLAGS@ \
@EVIL_CFLAGS@ \
@FRIBIDI_CFLAGS@ \
@PIXMAN_CFLAGS@

noinst_LTLIBRARIES      = libevas_filters.la
libevas_filters_la_SOURCES  = \
evas_filter_blend.c \
evas_filter_blur.c \
evas_filter_bump.c \
evas_filter.c \
evas_filter_curve.c \
evas_filter_displace.c \
evas_filter_mask.c \
evas_filter_parser.c \
evas_filter_transform.c \
evas_filter_utils.c \
evas_filter_private.h

EXTRA_DIST = \
blur/blur_box_alpha_.c \
blur/blur_box_alpha_i386.c \
blur/blur_box_alpha_neon.c \
blur/blur_box_alpha_sse3.c \
blur/blur_box_rgba_.c \
blur/blur_box_rgba_i386.c \
blur/blur_box_rgba_neon.c \
blur/blur_box_rgba_sse3.c \
blur/blur_gaussian_alpha_.c \
blur/blur_gaussian_rgba_.c

libevas_filters_la_LIBADD = @EVAS_GENERAL_LIBS@ @EVIL_LIBS@

