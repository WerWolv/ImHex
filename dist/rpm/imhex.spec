%define source_date_epoch_from_changelog 0

Name:           imhex
Version:        VERSION
Release:        0%{?dist}
Summary:        A hex editor for reverse engineers and programmers

License:        GPL-2.0-only AND Zlib AND MIT AND Apache-2.0
# imhex is gplv2.  capstone is custom.
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
BuildRequires:  libarchive-devel
BuildRequires:  libzstd-devel
BuildRequires:  zlib-devel
BuildRequires:  bzip2-devel
BuildRequires:  xz-devel
BuildRequires:  llvm-devel
BuildRequires:  mbedtls-devel
BuildRequires:  yara-devel
BuildRequires:  nativefiledialog-extended-devel
BuildRequires:  lz4-devel
BuildRequires:  libssh2-devel
%if 0%{?rhel} == 9
BuildRequires:  gcc-toolset-14
%endif
%if 0%{?fedora} || 0%{?rhel} > 9
BuildRequires:  capstone-devel
%endif
BuildRequires:  lunasvg-devel


Provides:       bundled(gnulib)
%if 0%{?rhel} == 10
Provides:       bundled(capstone) = 5.0.1
%endif
Provides:       bundled(imgui) = 1.90.8
Provides:       bundled(libromfs)
Provides:       bundled(microtar)
Provides:       bundled(libpl) = %{version}
Provides:       bundled(xdgpp)
# working on packaging this, bundling for now as to now delay updates
Provides:       bundled(miniaudio) = 0.11.11

# [7:02 PM] WerWolv: We're not supporting 32 bit anyways soooo
# [11:38 AM] WerWolv: Officially supported are x86_64 and aarch64
ExclusiveArch:  x86_64 %{arm64}

%description
ImHex is a Hex Editor, a tool to display, decode and analyze binary data to
reverse engineer their format, extract informations or patch values in them.

What makes ImHex special is that it has many advanced features that can often
only be found in paid applications. Such features are a completely custom binary
template and pattern language to decode and highlight structures in the data, a
graphical node-based data processor to pre-process values before they're
displayed, a disassembler, diffing support, bookmarks and much much more. At the
same time ImHex is completely free and open source under the GPLv2 language.


%package devel
Summary:        Development files for %{name}
License:        GPL-2.0-only
%description devel
%{summary}


%prep
%autosetup -n ImHex -p1
# remove bundled libs we aren't using
rm -rf lib/third_party/{curl,fmt,llvm,nlohmann_json,yara}
%if 0%{?fedora} || 0%{?rhel} > 9
rm -rf lib/third_party/capstone
%endif

# rhel 9 doesn't support all of the new appstream metainfo tags
%if 0%{?rhel} && 0%{?rhel} < 10
sed -i -e '/url type="vcs-browser"/d' \
	-e '/url type="contribute"/d' \
	dist/net.werwolv.ImHex.metainfo.xml
%endif

%build
%if 0%{?rhel} == 9
. /opt/rh/gcc-toolset-14/enable
%set_build_flags
CXXFLAGS+=" -std=gnu++2b"
%endif
%cmake \
 -D CMAKE_BUILD_TYPE=Release             \
 -D IMHEX_STRIP_RELEASE=OFF              \
 -D IMHEX_OFFLINE_BUILD=ON               \
 -D USE_SYSTEM_NLOHMANN_JSON=ON          \
 -D USE_SYSTEM_FMT=ON                    \
 -D USE_SYSTEM_CURL=ON                   \
 -D USE_SYSTEM_LLVM=ON                   \
 -D USE_SYSTEM_MD4C=OFF                  \
%if 0%{?fedora} || 0%{?rhel} > 9
 -D USE_SYSTEM_CAPSTONE=ON               \
%endif
 -D USE_SYSTEM_LUNASVG=ON                \
 -D USE_SYSTEM_YARA=ON                   \
 -D USE_SYSTEM_NFD=ON                    \
 -D IMHEX_ENABLE_UNIT_TESTS=ON           \
%if 0%{?rhel}
 -D IMHEX_BUILD_HARDENING=OFF
%endif
# disable built-in build hardening because it is already
# done in rhel buildroots.  adding the flags again from
# upstream generates build errors

%cmake_build


%check
# build binaries required for tests
%cmake_build --target unit_tests
%ctest --exclude-regex '(Helpers/StoreAPI|Helpers/TipsAPI|Helpers/ContentAPI)'
# Helpers/*API exclude tests that require network access


%install
%cmake_install
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop

# this is a symlink for the old appdata name that we don't need
rm -f %{buildroot}%{_metainfodir}/net.werwolv.ImHex.appdata.xml

# AppData
appstream-util validate-relax --nonet %{buildroot}%{_metainfodir}/net.werwolv.ImHex.metainfo.xml

# install licenses
%if 0%{?rhel} == 9
cp -a lib/third_party/capstone/LICENSES/LICENSE.TXT                  %{buildroot}%{_datadir}/licenses/%{name}/capstone-LICENSE
%endif
cp -a lib/third_party/microtar/LICENSE                               %{buildroot}%{_datadir}/licenses/%{name}/microtar-LICENSE
cp -a lib/third_party/xdgpp/LICENSE                                  %{buildroot}%{_datadir}/licenses/%{name}/xdgpp-LICENSE


%files
%license %{_datadir}/licenses/%{name}/
%doc README.md
%{_bindir}/imhex
%{_datadir}/imhex/imhex
%{_datadir}/pixmaps/%{name}.*
%{_datadir}/applications/%{name}.desktop
%{_libdir}/libimhex.so.*
%{_libdir}/%{name}/
%{_metainfodir}/net.werwolv.ImHex.metainfo.xml
%exclude %{_bindir}/imhex-updater
%{_datadir}/mime/packages/%{name}.xml


%files devel
%{_libdir}/libimhex.so
%{_datadir}/%{name}/sdk/
