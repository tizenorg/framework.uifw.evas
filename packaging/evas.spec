%define build_with_gles 0


Name:       evas
Summary:    Multi-platform Canvas Library
Version:    1.1.0+svn.67705slp2
Release:    2.4
Group:      TO_BE/FILLED_IN
License:    BSD
URL:        http://www.enlightenment.org/
Source0:    http://download.enlightenment.org/releases/evas-%{version}.tar.gz
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
%ifarch %{arm}
#BuildRequires:  pkgconfig(gles20)
#BuildRequires:   opengl-es-devel
%endif
BuildRequires:  pkgconfig(gl)
BuildRequires:  libjpeg-devel
BuildRequires:  giflib-devel


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
%configure  \
     	--enable-pthreads \
     	--enable-winkcodec=yes \
 	--disable-image-loader-svg \
        --enable-simple-x11 \
        --with-x \
        --enable-fb \
        --enable-xrender-x11 \
        --enable-line-dither-mask \
        --disable-image-loader-edb \
        --disable-rpath $(arch_flags) \
        --enable-harfbuzz \
	--enable-gl-xlib \
%ifarch %{arm}
        --enable-gl-flavor-gles \
        --enable-gles-variety-sgx \
	--enable-cpu-neon \
%endif
%if 0%{?build_with_gles} 
	--enable-gl-flavor-gles  \
	--enable-gles-variety-sgx
%else
        --enable-gl-x11  
%endif
  

make %{?jobs:-j%jobs}

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%{_libdir}/libevas.so.*
%{_libdir}/evas/modules/engines/*/*/module.so
%{_libdir}/evas/modules/loaders/*/*/module.so
%{_libdir}/evas/modules/savers/*/*/module.so
/usr/bin/evas_cserve
/usr/bin/evas_cserve_tool

%files devel
%{_includedir}/evas-1/*.h
%{_libdir}/libevas.so
%{_libdir}/pkgconfig/*.pc
/usr/share/evas/examples/*
