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
		dev-libs/mbedtls
		dev-cpp/nlohmann_json
		dbus
		xdg-desktop-portal
		"
BDEPEND="${DEPEND}"
