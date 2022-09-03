# ftbfs without this
%global _lto_cflags %{nil}

Name:           imhex
Version:        %{_version}
Release:        0%{?dist}
Summary:        A hex editor for reverse engineers and programmers

License:        GPL-2.0-only
URL:            https://imhex.werwolv.net/

BuildRequires:  cmake
BuildRequires:  desktop-file-utils
BuildRequires:  dbus-devel
BuildRequires:  file-devel
BuildRequires:  freetype-devel
BuildRequires:  fmt-devel
BuildRequires:  gcc-c++
BuildRequires:  mesa-libGL-devel
BuildRequires:  glfw-devel
BuildRequires:  json-devel
BuildRequires:  libcurl-devel
BuildRequires:  llvm-devel
BuildRequires:  mbedtls-devel
BuildRequires:  python3-devel
%if 0%{?fedora} >= 37
BuildRequires:  yara-devel
%endif


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
# don't use the setup macro since we're pulling from git
cp -r %{_src_path}/* %{_builddir}/


%build
%cmake \
 -DCMAKE_BUILD_TYPE=%{_build_type} \
 -D USE_SYSTEM_NLOHMANN_JSON=ON \
 -D USE_SYSTEM_FMT=ON \
 -D USE_SYSTEM_CURL=ON \
 -D USE_SYSTEM_LLVM=ON \
 -DCMAKE_C_COMPILER_LAUNCHER=ccache        \
 -DCMAKE_CXX_COMPILER_LAUNCHER=ccache      \
 -D IMHEX_PATTERNS_PULL_MASTER=ON          \
%if 0%{?fedora} >= 37
 -D USE_SYSTEM_YARA=ON \
# if fedora <= 36 get updated to yara 4.2.x then they should \
# be able to build against system libs \
# https://bugzilla.redhat.com/show_bug.cgi?id=2112508 \
%endif
# when capstone >= 5.x is released we should be able to build against \
# system libs of it \
# -D USE_SYSTEM_CAPSTONE=ON

%cmake_build


%install
%cmake_install
desktop-file-validate %{buildroot}%{_datadir}/applications/imhex.desktop


%files
%dir %{_datadir}/licenses/imhex
%license %{_datadir}/licenses/imhex/LICENSE
%doc README.md
%{_bindir}/imhex
%dir %{_datadir}/imhex
%{_datadir}/imhex/*
%{_datadir}/pixmaps/imhex.png
%{_datadir}/applications/imhex.desktop
%{_prefix}/lib64/libimhex.so.%{_version}
%{_prefix}/lib64/imhex/plugins/*
%{_metainfodir}/%{name}.metainfo.xml

%changelog
