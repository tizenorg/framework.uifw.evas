#sbs-git:slp/pkgs/e/evas evas 1.1.0+svn.69113slp2+build01 828d8bb285397266eb8985fd081fa2692fa3a7d6
Name:       evas
Summary:    Multi-platform Canvas Library
Version:    1.7.1+svn.77561+build108b10
Release:    1
Group:      System/Libraries
License:    BSD
URL:        http://www.enlightenment.org/
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(eina)
BuildRequires:  pkgconfig(eet)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xi)
BuildRequires:  pkgconfig(xrender)
BuildRequires:  pkgconfig(fontconfig)
BuildRequires:  pkgconfig(freetype2)
BuildRequires:  pkgconfig(fribidi)
BuildRequires:  pkgconfig(xext)
BuildRequires:  pkgconfig(libpng)
BuildRequires:  pkgconfig(zlib)
BuildRequires:  pkgconfig(harfbuzz)
BuildRequires:  giflib-devel
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(harfbuzz)
BuildRequires:  pkgconfig(gles20)
BuildRequires:  pkgconfig(zlib)
%if %{_repository} == "mobile"
BuildRequires:  pkgconfig(xpm)
BuildRequires:  pkgconfig(sm)
BuildRequires:  libjpeg-turbo-devel
BuildRequires:  pkgconfig(gles11)
BuildRequires:  pkgconfig(libdri2)
BuildRequires:  pkgconfig(xfixes)
BuildRequires:  pkgconfig(libtbm)
%endif
%if %{_repository} == "wearable"
BuildRequires:  libjpeg-devel
%ifarch %{arm}
BuildRequires:  pkgconfig(libdri2)
BuildRequires:  pkgconfig(xfixes)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:  native-buffer-devel
%endif
%endif
				
%description
Enlightenment DR17 advanced canvas library Evas is an advanced canvas library, providing six engines for rendering: X11,
 OpenGL (hardware accelerated), DirectFB, the framebuffer, Microsoft Windows
 and Qtopia.
 .
 Due to its simple API, evas can be developed with rapidly, and cleanly.
 .
 This package contains the core library and a set of image loaders and/or savers
 for various formats: eet, gif, jpeg, png, svg, tiff, bmp, wbmp and xpm


%package devel
Summary:    Multi-platform Canvas Library (devel)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}


%description devel
Enlightenment DR17 advanced canvas library (devel)


%prep
%setup -q

%build
%ifarch %{arm}
	export CFLAGS+=" -fvisibility=hidden -ffast-math -mfpu=neon -mfloat-abi=softfp -fPIC"
	export CXXFLAGS+=" -mfpu=neon -mfloat-abi=softfp"
%else
	export CFLAGS+=" -fvisibility=hidden -ffast-math -fPIC"
%endif
export LDFLAGS+=" -fvisibility=hidden -Wl,--hash-style=both -Wl,--as-needed "

cd %{_repository} && %autogen --disable-static \
	--disable-image-loader-svg \
	--enable-simple-x11 \
	--with-x \
	--enable-fb \
	--enable-xrender-x11 \
	--enable-line-dither-mask \
	--disable-image-loader-edb \
	--disable-rpath \
	--enable-gl-x11 \
	--enable-gl-flavor-gles \
	--enable-gles-variety-sgx \
	--enable-pixman \
	--enable-pixman-image \
	--enable-pixman-image-scale-sample \
%if %{_repository} == "wearable"
	--disable-wayland-shm \
	--disable-wayland-egl \
	--disable-evas-cserve \
	--disable-evas-cserve2 \
	--disable-image-loader-pmaps \
	--disable-image-loader-psd \
	--disable-image-loader-tga \
	--disable-image-loader-xpm \
%ifarch %{arm} 
    --enable-native-buffer \
%endif
%endif
%ifarch %{arm}
    --enable-pthreads \
    --enable-cpu-neon \
    --enable-winkcodec=yes \
%else
    --enable-pthreads \
    --enable-winkcodec=no \
%endif
    --enable-tile-rotate 

make %{?jobs:-j%jobs}


%install
#rm -rf %{buildroot}
cd %{_repository} && %make_install
mkdir -p %{buildroot}/usr/share/license
cp %{_builddir}/%{buildsubdir}/COPYING %{buildroot}/usr/share/license/%{name}


%post -p /sbin/ldconfig


%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%{_libdir}/libevas.so.*
%{_libdir}/evas/modules/engines/*/*/module.so
%{_libdir}/evas/modules/loaders/*/*/module.so
%{_libdir}/evas/modules/savers/*/*/module.so
%if %{_repository} == "mobile"
%{_libdir}/evas/cserve2/loaders/*/*/module.so
%{_bindir}/evas_cserve2_client
%{_bindir}/evas_cserve2_usage
%{_bindir}/evas_cserve2_debug
%{_libexecdir}/evas_cserve2
%{_libexecdir}/evas_cserve2_slave
%{_libexecdir}/dummy_slave
%endif
%manifest %{name}.manifest
/usr/share/license/%{name}
# The temp file for eina_prefix by raster
%{_datadir}/evas/checkme


%files devel
%defattr(-,root,root,-)
%{_includedir}/evas-1/*.h
%{_libdir}/libevas.so
%{_libdir}/pkgconfig/*.pc
%{_datadir}/evas/examples/*
