dnl use: EVAS_CHECK_LOADER_DEP_EDB(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_EDB],
[

requirement=""
have_dep="no"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

PKG_CHECK_MODULES([EDB], [edb], [have_dep="yes" requirement="edb"], [have_dep="no"])
evas_image_loader_[]$1[]_cflags="${EDB_CFLAGS}"
evas_image_loader_[]$1[]_libs="${EDB_LIBS}"

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x$2" = "xstatic" ; then
   requirement_evas="${requirement} ${requirement_evas}"
fi

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_EET(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_EET],
[

requirement=""
have_dep="no"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

PKG_CHECK_MODULES([EET], [eet >= 1.6.0], [have_dep="yes" requirement="eet >= 1.4.0"], [have_dep="no"])
evas_image_loader_[]$1[]_cflags="${EET_CFLAGS}"
evas_image_loader_[]$1[]_libs="${EET_LIBS}"

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x$2" = "xstatic" ; then
   requirement_evas="${requirement} ${requirement_evas}"
fi

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_GIF(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_GIF],
[

have_dep="no"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_CHECK_HEADER([gif_lib.h], [have_dep="yes"])

if test "x${have_dep}"  = "xyes" ; then
   AC_CHECK_LIB([gif],
      [DGifOpenFileName],
      [
       evas_image_loader_[]$1[]_libs="-lgif"
      ],
      [have_dep="no"]
   )

   if test "x${have_dep}"  = "xno" ; then
      AC_CHECK_LIB([ungif],
         [DGifOpenFileName],
         [
          have_dep="yes"
          evas_image_loader_[]$1[]_libs="-lungif"
         ]
      )
   fi
fi

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_JPEG(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_JPEG],
[

have_dep="no"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_CHECK_HEADER([jpeglib.h], [have_dep="yes"])

if test "x${have_dep}"  = "xyes" ; then
   AC_CHECK_LIB([jpeg],
      [jpeg_CreateDecompress],
      [
       evas_image_loader_[]$1[]_libs="-ljpeg"
       AC_COMPILE_IFELSE(
          [AC_LANG_PROGRAM(
              [[
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>
              ]],
              [[
struct jpeg_decompress_struct decomp;
decomp.region_x = 0;
              ]])],
          [have_jpeg_region="yes"],
          [have_jpeg_region="no"])
      ],
      [have_dep="no"]
   )
   if test "x${have_jpeg_region}" = "xyes" ; then
     AC_DEFINE(BUILD_LOADER_JPEG_REGION, [1], [JPEG Region Decode Support])
   fi
fi

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_PMAPS(loader, want_static[[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_PMAPS],
[

have_dep="yes"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_PNG(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_PNG],
[

requirement=""
have_dep="no"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

dnl libpng.pc is the latest version of libpng that ahs been installed.
dnl We check it first.
PKG_CHECK_MODULES([PNG],
   [libpng >= 1.2.10],
   [have_dep="yes" requirement="libpng"],
   [have_dep="no"])

if test "x${have_dep}" = "xno" ; then
   PKG_CHECK_MODULES([PNG],
      [libpng15],
      [have_dep="yes" requirement="libpng15"],
      [have_dep="no"])
fi

if test "x${have_dep}" = "xno" ; then
   PKG_CHECK_MODULES([PNG],
      [libpng14],
      [have_dep="yes" requirement="libpng14"],
      [have_dep="no"])
fi

if test "x${have_dep}" = "xno" ; then
   PKG_CHECK_MODULES([PNG],
      [libpng12 >= 1.2.10],
      [have_dep="yes" requirement="libpng12"],
      [have_dep="no"])
fi

evas_image_loader_[]$1[]_cflags="${PNG_CFLAGS}"
evas_image_loader_[]$1[]_libs="${PNG_LIBS}"

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x$2" = "xstatic" ; then
   requirement_evas="${requirement} ${requirement_evas}"
fi

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_SVG(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_SVG],
[

requirement=""
have_esvg="no"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""
version_esvg="0.0.18"
version_ender="0.0.6"

PKG_CHECK_MODULES([SVG],
   [esvg >= ${version_esvg} ender >= ${version_ender}],
   [have_dep="yes" have_esvg="yes" requirement="esvg >= ${version_esvg} ender >= ${version_ender}"],
   [have_dep="no"])

if test "x${have_dep}" = "xyes" ; then
   evas_image_loader_[]$1[]_cflags="${SVG_CFLAGS}"
   evas_image_loader_[]$1[]_libs="${SVG_LIBS}"
fi

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x$2" = "xstatic" ; then
   requirement_evas="${requirement} ${requirement_evas}"
fi

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_TIFF(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_TIFF],
[

have_dep="no"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_CHECK_HEADER([tiffio.h], [have_dep="yes"])

if test "x${have_dep}"  = "xyes" ; then
   AC_CHECK_LIB([tiff],
      [TIFFReadScanline],
      [
       evas_image_loader_[]$1[]_libs="-ltiff"
      ],
      [have_dep="no"]
   )

   if test "x${have_dep}"  = "xno" ; then
      AC_CHECK_LIB([tiff],
         [TIFFReadScanline],
         [
          have_dep="yes"
          evas_image_loader_[]$1[]_libs="-ltiff -ljpeg -lz -lm"
         ]
      )
   fi

   if test "x${have_dep}"  = "xno" ; then
      AC_CHECK_LIB([tiff34],
         [TIFFReadScanline],
         [
          have_dep="yes"
          evas_image_loader_[]$1[]_libs="-ltiff34 -ljpeg -lz -lm"
         ]
      )
   fi
fi

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_XPM(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_XPM],
[

have_dep="yes"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_BMP(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_BMP],
[

have_dep="yes"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_ICO(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_ICO],
[

have_dep="yes"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_TGA(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_TGA],
[

have_dep="yes"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_WBMP(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_WBMP],
[

have_dep="yes"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_PSD(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_PSD],
[

have_dep="yes"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_WEBP(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_WEBP],
[

have_dep="no"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_CHECK_HEADER([webp/decode.h], [have_dep="yes"])

if test "x${have_dep}"  = "xyes" ; then
   AC_CHECK_LIB([webp],
      [WebPDecodeRGBA],
      [
       evas_image_loader_[]$1[]_libs="-lwebp"
      ],
      [have_dep="no"]
   )
fi

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_LOADER_DEP_GENERIC(loader, want_static[, ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])

AC_DEFUN([EVAS_CHECK_LOADER_DEP_GENERIC],
[

have_dep="yes"
evas_image_loader_[]$1[]_cflags=""
evas_image_loader_[]$1[]_libs=""

AC_SUBST([evas_image_loader_$1_cflags])
AC_SUBST([evas_image_loader_$1_libs])

if test "x${have_dep}" = "xyes" ; then
  m4_default([$3], [:])
else
  m4_default([$4], [:])
fi

])

dnl use: EVAS_CHECK_IMAGE_LOADER(loader, want_loader, macro)


AC_DEFUN([EVAS_CHECK_IMAGE_LOADER],
[

m4_pushdef([UP], m4_toupper([$1]))
m4_pushdef([DOWN], m4_tolower([$1]))

want_loader="$2"
want_static_loader="no"
have_loader="no"
have_evas_image_loader_[]DOWN="no"

AC_ARG_ENABLE([image-loader-[]DOWN],
   [AC_HELP_STRING([--enable-image-loader-[]DOWN], [enable $1 image loader])],
   [
    if test "x${enableval}" = "xyes" ; then
       want_loader="yes"
    else
       if test "x${enableval}" = "xstatic" ; then
          want_loader="static"
       else
          want_loader="no"
       fi
    fi
   ]
)

AC_MSG_CHECKING([whether to enable $1 image loader])
AC_MSG_RESULT([${want_loader}])

if test "x${want_loader}" = "xyes" -o "x${want_loader}" = "xstatic" -o "x${want_loader}" = "xauto"; then
   m4_default([EVAS_CHECK_LOADER_DEP_]m4_defn([UP]))(DOWN, ${want_loader}, [have_loader="yes"], [have_loader="no"])
fi

if test "x${have_loader}" = "xno" -a "x${want_loader}" = "xyes" -a "x${use_strict}" = "xyes" ; then
   AC_MSG_ERROR([$1 dependencies not found (strict dependencies checking)])
fi

AC_MSG_CHECKING([whether $1 image loader will be built])
AC_MSG_RESULT([${have_loader}])

if test "x${have_loader}" = "xyes" ; then
   if test "x${want_loader}" = "xstatic" ; then
      have_evas_image_loader_[]DOWN="static"
      want_static_loader="yes"
   else
      have_evas_image_loader_[]DOWN="yes"
   fi
fi

if test "x${have_loader}" = "xyes" ; then
   AC_DEFINE(BUILD_LOADER_[]UP, [1], [UP Image Loader Support])
fi

AM_CONDITIONAL(BUILD_LOADER_[]UP, [test "x${have_loader}" = "xyes"])

if test "x${want_static_loader}" = "xyes" ; then
   AC_DEFINE(EVAS_STATIC_BUILD_[]UP, [1], [Build $1 image loader inside libevas])
   have_static_module="yes"
fi

AM_CONDITIONAL(EVAS_STATIC_BUILD_[]UP, [test "x${want_static_loader}" = "xyes"])

m4_popdef([UP])
m4_popdef([DOWN])

])

dnl use: EVAS_CHECK_FONT_LOADER(want)


AC_DEFUN([EVAS_CHECK_FONT_LOADER],
[

pushdef([UP], translit([$1], [a-z], [A-Z]))dnl
pushdef([DOWN], translit([$1], [A-Z], [a-z]))dnl

want_loader="$1"
have_evas_font_loader_eet="no"

AC_ARG_ENABLE([font-loader-eet],
   [AC_HELP_STRING([--disable-font-loader-eet],
       [disable EET font loader. [[default=enabled]]])],
   [want_loader=${enableval}]
)

AC_MSG_CHECKING([whether to enable Eet font loader])
AC_MSG_RESULT([${want_loader}])

if test "x$want_loader" = "xyes" -o "x$want_loader" = "xauto"; then
    PKG_CHECK_MODULES([EET], [eet >= 1.6.0], [have_evas_font_loader_eet="yes"], [have_evas_font_loader_eet="no"])
fi

if test "x${have_evas_font_loader_eet}" = "xno" -a "x$want_loader" = "xyes" -a "x$use_strict" = "xyes" ; then
   AC_MSG_ERROR([Eet dependencies not found (strict dependencies checking)])
fi

if test "x$have_evas_font_loader_eet" = "xyes" ; then
   AC_DEFINE([BUILD_FONT_LOADER_EET], [1], [EET Font Loader Support])
   requirement_evas="eet >= 1.6.0 ${requirement_evas}"
fi

popdef([UP])
popdef([DOWN])

])
