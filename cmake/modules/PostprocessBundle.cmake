# Adapted from the Dolphin project: https://dolphin-emu.org/
# This module can be used in two different ways.
#
# When invoked as `cmake -P PostprocessBundle.cmake`, it fixes up an
# application folder to be standalone. It bundles all required libraries from
# the system and fixes up library IDs. Any additional shared libraries, like
# plugins, that are found under Contents/MacOS/ will be made standalone as well.
#
# When called with `include(PostprocessBundle)`, it defines a helper
# function `postprocess_bundle` that sets up the command form of the
# module as a post-build step.

if(CMAKE_GENERATOR)
	# Being called as include(PostprocessBundle), so define a helper function.
	set(_POSTPROCESS_BUNDLE_MODULE_LOCATION "${CMAKE_CURRENT_LIST_FILE}")
	function(postprocess_bundle target)
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -DBUNDLE_PATH="$<TARGET_FILE_DIR:${target}>/../.."
				-P "${_POSTPROCESS_BUNDLE_MODULE_LOCATION}"
		)
	endfunction()
	return()
endif()

get_filename_component(BUNDLE_PATH "${BUNDLE_PATH}" ABSOLUTE)
message(STATUS "Fixing up application bundle: ${BUNDLE_PATH}")


# Make sure to fix up any additional shared libraries (like plugins) that are
# needed.
file(GLOB_RECURSE extra_libs "${BUNDLE_PATH}/Contents/MacOS/*.dylib")

message(STATUS "Fixing up application bundle: ${extra_dirs}")

# BundleUtilities doesn't support DYLD_FALLBACK_LIBRARY_PATH behavior, which
# makes it sometimes break on libraries that do weird things with @rpath. Specify
# equivalent search directories until https://gitlab.kitware.com/cmake/cmake/issues/16625
# is fixed and in our minimum CMake version.
set(extra_dirs "/usr/local/lib" "/lib" "/usr/lib")

# BundleUtilities is overly verbose, so disable most of its messages
function(message)
	if(NOT ARGV MATCHES "^STATUS;")
		_message(${ARGV})
	endif()
endfunction()

include(BundleUtilities)
set(BU_CHMOD_BUNDLE_ITEMS ON)
fixup_bundle("${BUNDLE_PATH}" "${extra_libs}" "${extra_dirs}" IGNORE_ITEM "Python")
