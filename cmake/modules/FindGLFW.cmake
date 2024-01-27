#.rst:
# Find GLFW
# ---------
#
# Finds the GLFW library using its cmake config if that exists, otherwise
# falls back to finding it manually. This module defines:
#
#  GLFW_FOUND               - True if GLFW library is found
#  GLFW::GLFW               - GLFW imported target
#
# Additionally, in case the config was not found, these variables are defined
# for internal usage:
#
#  GLFW_LIBRARY             - GLFW library
#  GLFW_DLL_DEBUG           - GLFW debug DLL on Windows, if found
#  GLFW_DLL_RELEASE         - GLFW release DLL on Windows, if found
#  GLFW_INCLUDE_DIR         - Root include dir
#

#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019,
#               2020, 2021, 2022 Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2016 Jonathan Hale <squareys@googlemail.com>
#
#   Permission is hereby granted, free of charge, to any person obtaining a
#   copy of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom the
#   Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.
#

# GLFW installs cmake package config files which handles dependencies in case
# GLFW is built statically. Try to find first, quietly, so it doesn't print
# loud messages when it's not found, since that's okay. If the glfw target
# already exists, it means we're using it through a CMake subproject -- don't
# attempt to find the package in that case.
if(NOT TARGET glfw)
    find_package(glfw3 CONFIG QUIET)
endif()

# If either a glfw config file was found or we have a subproject, point
# GLFW::GLFW to that and exit -- nothing else to do here.
if(TARGET glfw)
    if(NOT TARGET GLFW::GLFW)
        # Aliases of (global) targets are only supported in CMake 3.11, so we
        # work around it by this. This is easier than fetching all possible
        # properties (which are impossible to track of) and then attempting to
        # rebuild them into a new target.
        add_library(GLFW::GLFW INTERFACE IMPORTED)
        set_target_properties(GLFW::GLFW PROPERTIES INTERFACE_LINK_LIBRARIES glfw)
    endif()

    # Just to make FPHSA print some meaningful location, nothing else
    get_target_property(_GLFW_INTERFACE_INCLUDE_DIRECTORIES glfw INTERFACE_INCLUDE_DIRECTORIES)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args("GLFW" DEFAULT_MSG
            _GLFW_INTERFACE_INCLUDE_DIRECTORIES)

    if(CORRADE_TARGET_WINDOWS)
        # .dll is in LOCATION, .lib is in IMPLIB. Yay, useful!
        get_target_property(GLFW_DLL_DEBUG glfw IMPORTED_LOCATION_DEBUG)
        get_target_property(GLFW_DLL_RELEASE glfw IMPORTED_LOCATION_RELEASE)
    endif()

    return()
endif()

if(CORRADE_TARGET_WINDOWS)
    if(MSVC)
        if(MSVC_VERSION VERSION_LESS 1910)
            set(_GLFW_LIBRARY_PATH_SUFFIX lib-vc2015)
        elseif(MSVC_VERSION VERSION_LESS 1920)
            set(_GLFW_LIBRARY_PATH_SUFFIX lib-vc2017)
        elseif(MSVC_VERSION VERSION_LESS 1930)
            set(_GLFW_LIBRARY_PATH_SUFFIX lib-vc2019)
        elseif(MSVC_VERSION VERSION_LESS 1940)
            set(_GLFW_LIBRARY_PATH_SUFFIX lib-vc2022)
        else()
            message(FATAL_ERROR "Unsupported MSVC version")
        endif()
    elseif(MINGW)
        set(_GLFW_LIBRARY_PATH_SUFFIX lib-mingw-w64)
    else()
        message(FATAL_ERROR "Unsupported compiler")
    endif()
endif()

# In case no config file was found, try manually finding the library. Prefer
# the glfw3dll as it's a dynamic library.
find_library(GLFW_LIBRARY
        NAMES glfw glfw3dll glfw3
        PATH_SUFFIXES ${_GLFW_LIBRARY_PATH_SUFFIX})

if(CORRADE_TARGET_WINDOWS AND GLFW_LIBRARY MATCHES "glfw3dll.(lib|a)$")
    # TODO: debug?
    find_file(GLFW_DLL_RELEASE
            NAMES glfw3.dll
            PATH_SUFFIXES ${_GLFW_LIBRARY_PATH_SUFFIX})
endif()

# Include dir
find_path(GLFW_INCLUDE_DIR
        NAMES GLFW/glfw3.h)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args("GLFW" DEFAULT_MSG
        GLFW_LIBRARY
        GLFW_INCLUDE_DIR)

if(NOT TARGET GLFW::GLFW)
    add_library(GLFW::GLFW UNKNOWN IMPORTED)

    # Work around BUGGY framework support on macOS
    # https://cmake.org/Bug/view.php?id=14105
    if(CORRADE_TARGET_APPLE AND GLFW_LIBRARY MATCHES "\\.framework$")
        set_property(TARGET GLFW::GLFW PROPERTY IMPORTED_LOCATION ${GLFW_LIBRARY}/GLFW)
    else()
        set_property(TARGET GLFW::GLFW PROPERTY IMPORTED_LOCATION ${GLFW_LIBRARY})
    endif()

    set_property(TARGET GLFW::GLFW PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES ${GLFW_INCLUDE_DIR})
endif()

mark_as_advanced(GLFW_LIBRARY GLFW_INCLUDE_DIR)