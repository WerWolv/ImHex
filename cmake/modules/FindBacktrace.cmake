# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindBacktrace
-------------

Find provider for `backtrace(3) <https://man7.org/linux/man-pages/man3/backtrace.3.html>`__.

Checks if OS supports ``backtrace(3)`` via either ``libc`` or custom library.
This module defines the following variables:

``Backtrace_HEADER``
  The header file needed for ``backtrace(3)``. Cached.
  Could be forcibly set by user.
``Backtrace_INCLUDE_DIRS``
  The include directories needed to use ``backtrace(3)`` header.
``Backtrace_LIBRARIES``
  The libraries (linker flags) needed to use ``backtrace(3)``, if any.
``Backtrace_FOUND``
  Is set if and only if ``backtrace(3)`` support detected.

The following cache variables are also available to set or use:

``Backtrace_LIBRARY``
  The external library providing backtrace, if any.
``Backtrace_INCLUDE_DIR``
  The directory holding the ``backtrace(3)`` header.

Typical usage is to generate of header file using :command:`configure_file`
with the contents like the following::

 #cmakedefine01 Backtrace_FOUND
 #if Backtrace_FOUND
 # include <${Backtrace_HEADER}>
 #endif

And then reference that generated header file in actual source.
#]=======================================================================]

include(CMakePushCheckState)
include(CheckSymbolExists)
include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)

# List of variables to be provided to find_package_handle_standard_args()
set(_Backtrace_STD_ARGS Backtrace_INCLUDE_DIR)

if(Backtrace_HEADER)
  set(_Backtrace_HEADER_TRY "${Backtrace_HEADER}")
else(Backtrace_HEADER)
  set(_Backtrace_HEADER_TRY "execinfo.h")
endif(Backtrace_HEADER)

find_path(Backtrace_INCLUDE_DIR "${_Backtrace_HEADER_TRY}")
set(Backtrace_INCLUDE_DIRS ${Backtrace_INCLUDE_DIR})

if (NOT DEFINED Backtrace_LIBRARY)
  # First, check if we already have backtrace(), e.g., in libc
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES ${Backtrace_INCLUDE_DIRS})
  set(CMAKE_REQUIRED_QUIET ${Backtrace_FIND_QUIETLY})
  check_symbol_exists("backtrace" "${_Backtrace_HEADER_TRY}" _Backtrace_SYM_FOUND)
  cmake_pop_check_state()
endif()

if(_Backtrace_SYM_FOUND)
  # Avoid repeating the message() call below each time CMake is run.
  if(NOT Backtrace_FIND_QUIETLY AND NOT DEFINED Backtrace_LIBRARY)
    message(STATUS "backtrace facility detected in default set of libraries")
  endif()
  set(Backtrace_LIBRARY "" CACHE FILEPATH "Library providing backtrace(3), empty for default set of libraries")
else()
  # Check for external library, for non-glibc systems
  if(Backtrace_INCLUDE_DIR)
    # OpenBSD has libbacktrace renamed to libexecinfo
    find_library(Backtrace_LIBRARY "execinfo")
  else()     # respect user wishes
    set(_Backtrace_HEADER_TRY "backtrace.h")
    find_path(Backtrace_INCLUDE_DIR ${_Backtrace_HEADER_TRY})
    find_library(Backtrace_LIBRARY "backtrace")
  endif()

  # Prepend list with library path as it's more common practice
  set(_Backtrace_STD_ARGS Backtrace_LIBRARY ${_Backtrace_STD_ARGS})
endif()

set(Backtrace_LIBRARIES ${Backtrace_LIBRARY})
set(Backtrace_HEADER "${_Backtrace_HEADER_TRY}" CACHE STRING "Header providing backtrace(3) facility")

find_package_handle_standard_args(Backtrace FOUND_VAR Backtrace_FOUND REQUIRED_VARS ${_Backtrace_STD_ARGS})
mark_as_advanced(Backtrace_HEADER Backtrace_INCLUDE_DIR Backtrace_LIBRARY)