MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = \
-I$(top_srcdir)/src/lib \
-I$(top_srcdir)/src/modules/engines/buffer \
-DPACKAGE_BUILD_DIR=\"`pwd`/$(top_builddir)\" \
@EVAS_CFLAGS@ @EINA_CFLAGS@

if INSTALL_EXAMPLES
AM_CPPFLAGS += -DTESTS_SRC_DIR=\"$(prefix)/share/evas/benchmarks\"
else
AM_CPPFLAGS += -DTESTS_SRC_DIR=\"$(abs_top_srcdir)/src/benchmarks/evas\"
endif

EXTRA_PROGRAMS = evas_bench

benchmark: evas_bench

evas_bench_SOURCES = \
evas_bench.c \
evas_bench_loader.c \
evas_bench_saver.c \
evas_bench.h

nodist_EXTRA_evas_bench_SOURCES = dummy.cc
evas_bench_LDADD = \
$(top_builddir)/src/lib/libevas.la \
@EVAS_LIBS@

IMAGES = images/Light-50.tgv \
images/mars_rover_panorama_half-size.jpg \
images/Pic1-50.tgv \
images/Sunrise-100.tgv

EXTRA_DIST = $(IMAGES)

clean-local:
	rm -rf *.gcno ..\#..\#src\#*.gcov *.gcda

#if ALWAYS_BUILD_EXAMPLES
if BUILD_EXAMPLES
if INSTALL_EXAMPLES
benchmarkdir = $(datadir)/$(PACKAGE)/benchmarks
benchmark_imagesdir = $(datadir)/$(PACKAGE)/benchmarks/images
benchmark_PROGRAMS = $(EXTRA_PROGRAMS)
benchmark_images_DATA = $(IMAGES)
else
noinst_PROGRAMS = $(EXTRA_PROGRAMS)
endif
endif
