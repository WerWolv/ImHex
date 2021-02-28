# app-editors/ImHex
# Copyright 2020 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=7

DESCRIPTION="A hex editor for reverse engineers, programmers, and eyesight"
HOMEPAGE="https://github.com/WerWolv/ImHex"
SRC_URI=""
EGIT_REPO_URI="https://github.com/WerWolv/ImHex.git"

PYTHON_COMPAT=( python3_{6,7,8,9} )

inherit git-r3 python-single-r1 cmake

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64"
IUSE=""

REQUIRED_USE="${PYTHON_REQUIRED_USE}"

DEPEND=""
RDEPEND="${DEPEND}
		${PYTHON_DEPS}
		media-libs/glfw
		sys-apps/file
		dev-libs/mbedtls
		dev-libs/capstone
		dev-cpp/nlohmann_json
		media-libs/glm
		x11-libs/gtk+
		"
BDEPEND="${DEPEND}"
