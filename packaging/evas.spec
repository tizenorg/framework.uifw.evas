#sbs-git:slp/pkgs/e/evas evas 1.1.0+svn.69113slp2+build01 828d8bb285397266eb8985fd081fa2692fa3a7d6
Name:       evas
Summary:    Multi-platform Canvas Library
Version:    1.7.1+svn.77561+build05r02
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
BuildRequires:  pkgconfig(xpm)
BuildRequires:  pkgconfig(zlib)
BuildRequires:  pkgconfig(harfbuzz)
BuildRequires:  pkgconfig(sm)
BuildRequires:  libjpeg-turbo-devel
BuildRequires:  giflib-devel
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(gles11)
BuildRequires:  pkgconfig(gles20)
%ifarch %{arm}
BuildRequires:  pkgconfig(libdri2)
BuildRequires:  pkgconfig(xfixes)
BuildRequires:  pkgconfig(libtbm)
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
export CFLAGS+=" -fvisibility=hidden -ffast-math -fPIC"
export LDFLAGS+=" -fvisibility=hidden -Wl,--hash-style=both -Wl,--as-needed"

%autogen
%configure --disable-static \
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
	--enable-tile-rotate \
%ifarch %{arm}
     	--enable-pthreads \
	--enable-cpu-neon \
     	--enable-winkcodec=yes
%else
	--enable-pthreads \
    	--enable-winkcodec=no
%endif
  
make %{?jobs:-j%jobs}


%install
rm -rf %{buildroot}
%make_install
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
%{_libdir}/evas/cserve2/loaders/*/*/module.so
%{_bindir}/evas_cserve2_client
%{_bindir}/evas_cserve2_usage
%{_bindir}/evas_cserve2_debug
%{_libexecdir}/evas_cserve2
%{_libexecdir}/evas_cserve2_slave
%{_libexecdir}/dummy_slave
%manifest %{name}.manifest
/usr/share/license/%{name}
%manifest %{name}.manifest
# The temp file for eina_prefix by raster
%{_datadir}/evas/checkme


%files devel
%defattr(-,root,root,-)
%{_includedir}/evas-1/*.h
%{_libdir}/libevas.so
%{_libdir}/pkgconfig/*.pc
%{_datadir}/evas/examples/*
