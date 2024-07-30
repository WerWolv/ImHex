# app-editors/ImHex
# Copyright 2020 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=7

DESCRIPTION="A hex editor for reverse engineers, programmers, and eyesight"
HOMEPAGE="https://github.com/WerWolv/ImHex"
SRC_URI=""
EGIT_REPO_URI="https://github.com/WerWolv/ImHex.git"

inherit git-r3 cmake

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64"
IUSE=""

DEPEND=""
RDEPEND="${DEPEND}
		media-libs/glfw
		sys-apps/file
		net-libs/mbedtls
		dev-cpp/nlohmann_json
		sys-apps/dbus
		sys-apps/xdg-desktop-portal
		sys-libs/zlib
		app-arch/bzip2
		app-arch/lzma
		app-arch/zstd
		app-arch/lz4
		"
BDEPEND="${DEPEND}"
