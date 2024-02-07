Name:           imhex
Version:        1.26.2
Release:        0%{?dist}
Summary:        A hex editor for reverse engineers and programmers

License:        GPL-2.0-only AND Zlib AND MIT AND Apache-2.0
# imhex is gplv2.  capstone is custom.  nativefiledialog is Zlib.
# see license dir for full breakdown
URL:            https://imhex.werwolv.net/
# We need the archive with deps bundled
Source0:        https://github.com/WerWolv/%{name}/releases/download/v%{version}/Full.Sources.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  desktop-file-utils
BuildRequires:  dbus-devel
BuildRequires:  file-devel
BuildRequires:  freetype-devel
BuildRequires:  fmt-devel
BuildRequires:  gcc-c++
BuildRequires:  libappstream-glib
BuildRequires:  libglvnd-devel
BuildRequires:  glfw-devel
BuildRequires:  json-devel
BuildRequires:  libcurl-devel
BuildRequires:  llvm-devel
BuildRequires:  mbedtls-devel
BuildRequires:  yara-devel
BuildRequires:  nativefiledialog-extended-devel
BuildRequires:  dotnet-sdk-8.0
BuildRequires:  libzstd-devel
BuildRequires:  zlib-devel
BuildRequires:  bzip2-devel
BuildRequires:  xz-devel
%if 0%{?rhel}
BuildRequires:  gcc-toolset-12
%endif

Provides:       bundled(gnulib)
Provides:       bundled(capstone) = 5.0-rc2
Provides:       bundled(imgui)
Provides:       bundled(libromfs)
Provides:       bundled(microtar)
Provides:       bundled(libpl)
Provides:       bundled(xdgpp)

# ftbfs on these arches.  armv7hl might compile when capstone 5.x
# is released upstream and we can build against it
# [7:02 PM] WerWolv: We're not supporting 32 bit anyways soooo
# [11:38 AM] WerWolv: Officially supported are x86_64 and aarch64
ExclusiveArch:  x86_64 %{arm64} ppc64le


%description
ImHex is a Hex Editor, a tool to display, decode and analyze binary data to
reverse engineer their format, extract informations or patch values in them.

What makes ImHex special is that it has many advanced features that can often
only be found in paid applications. Such features are a completely custom binary
template and pattern language to decode and highlight structures in the data, a
graphical node-based data processor to pre-process values before they're
displayed, a disassembler, diffing support, bookmarks and much much more. At the
same time ImHex is completely free and open source under the GPLv2 language.


%prep
%autosetup -n ImHex
# remove bundled libs we aren't using
rm -rf lib/third_party/{fmt,nlohmann_json,yara}

%build
%if 0%{?rhel}
. /opt/rh/gcc-toolset-12/enable
%set_build_flags
CXXFLAGS+=" -std=gnu++2b"
%endif
%cmake \
 -D CMAKE_BUILD_TYPE=Release             \
 -D IMHEX_STRIP_RELEASE=OFF              \
 -D IMHEX_OFFLINE_BUILD=ON               \
 -D USE_SYSTEM_NLOHMANN_JSON=ON          \
 -D USE_SYSTEM_FMT=ON                    \
 -D USE_SYSTEM_YARA=ON                   \
 -D USE_SYSTEM_NFD=ON                    \
 -D IMHEX_USE_GTK_FILE_PICKER=ON         \
 -D IMHEX_BUNDLE_DOTNET=OFF              \
# when capstone >= 5.x is released we should be able to build against \
# system libs of it \
# -D USE_SYSTEM_CAPSTONE=ON

%cmake_build


%check
%if 0%{?rhel}
. /opt/rh/gcc-toolset-12/enable
%set_build_flags
CXXFLAGS+=" -std=gnu++2b"
%endif


%install
%cmake_install
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop

# this is a symlink for the old appdata name that we don't need
rm -f %{buildroot}%{_metainfodir}/net.werwolv.%{name}.appdata.xml

# AppData
appstream-util validate-relax --nonet %{buildroot}%{_metainfodir}/net.werwolv.%{name}.metainfo.xml

# install licenses
cp -a lib/third_party/nativefiledialog/LICENSE                       %{buildroot}%{_datadir}/licenses/%{name}/nativefiledialog-LICENSE
cp -a lib/third_party/capstone/LICENSE.TXT                           %{buildroot}%{_datadir}/licenses/%{name}/capstone-LICENSE
cp -a lib/third_party/capstone/suite/regress/LICENSE                 %{buildroot}%{_datadir}/licenses/%{name}/capstone-regress-LICENSE
cp -a lib/third_party/microtar/LICENSE                               %{buildroot}%{_datadir}/licenses/%{name}/microtar-LICENSE
cp -a lib/third_party/xdgpp/LICENSE                                  %{buildroot}%{_datadir}/licenses/%{name}/xdgpp-LICENSE


%files
%license %{_datadir}/licenses/%{name}/
%doc README.md
%{_bindir}/imhex
%{_bindir}/imhex-updater
%{_datadir}/pixmaps/%{name}.png
%{_datadir}/applications/%{name}.desktop
%{_libdir}/libimhex.so*
%{_libdir}/libpl.so*
%{_libdir}/%{name}/
%{_metainfodir}/net.werwolv.%{name}.metainfo.xml


%changelog
