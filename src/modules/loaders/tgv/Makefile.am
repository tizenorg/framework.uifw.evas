
MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = \
-I. \
-I$(top_srcdir)/src/lib \
-I$(top_srcdir)/src/lib/include \
-I$(top_srcdir)/src/static_deps/lz4 \
-I$(top_srcdir)/src/static_deps/rg_etc \
@FREETYPE_CFLAGS@ \
@FRIBIDI_CFLAGS@ \
@PIXMAN_CFLAGS@ \
@EVAS_GENERAL_CFLAGS@ \
@EVIL_CFLAGS@

if BUILD_LOADER_TGV
if !EVAS_STATIC_BUILD_TGV

pkgdir = $(libdir)/evas/modules/loaders/tgv/$(MODULE_ARCH)
pkg_LTLIBRARIES = module.la

module_la_SOURCES = evas_image_load_tgv.c \
$(top_srcdir)/src/static_deps/lz4/lz4.c \
$(top_srcdir)/src/static_deps/rg_etc/rg_etc1.c \
$(top_srcdir)/src/static_deps/rg_etc/rg_etc2.c
module_la_LIBADD = @EVAS_GENERAL_LIBS@ @EVIL_LIBS@ $(top_builddir)/src/lib/libevas.la
module_la_LDFLAGS = -no-undefined @lt_enable_auto_import@ -module -avoid-version
module_la_LIBTOOLFLAGS = --tag=disable-static

else

noinst_LTLIBRARIES = libevas_loader_tgv.la
libevas_loader_tgv_la_SOURCES = evas_image_load_tgv.c \
$(top_srcdir)/src/static_deps/lz4/lz4.c \
$(top_srcdir)/src/static_deps/rg_etc/rg_etc1.c \
$(top_srcdir)/src/static_deps/rg_etc/rg_etc2.c
libevas_loader_tgv_la_LIBADD =

endif
endif
