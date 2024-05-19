
option(IMHEX_IDE_HELPERS_OVERRIDE_XCODE_COMPILER "Enable choice of compiler for Xcode builds, despite CMake's best efforts" OFF)
option(IMHEX_IDE_HELPERS_INTRUSIVE_IDE_TWEAKS    "Enable intrusive CMake tweaks to better support IDEs with folder support" OFF)

# The CMake infrastructure silently ignores the CMAKE_<>_COMPILER settings when
#  using the `Xcode` generator. 
#
# A particularly nasty (and potentially only) way of getting around this is to 
#  temporarily lie about the generator being used, while CMake determines and
#  locks in the compiler to use.
#
# Needless to say, this is hacky and fragile. Use at your own risk!
if (IMHEX_IDE_HELPERS_OVERRIDE_XCODE_COMPILER AND CMAKE_GENERATOR STREQUAL "Xcode")
    set(CMAKE_GENERATOR "Unknown")
    enable_language(C CXX)
    
    set(CMAKE_GENERATOR "Xcode")

    set(CMAKE_XCODE_ATTRIBUTE_CC  "${CMAKE_C_COMPILER}")
    set(CMAKE_XCODE_ATTRIBUTE_CXX "${CMAKE_CXX_COMPILER}")

    if (CLANG)
        set(CMAKE_XCODE_ATTRIBUTE_LD         "${CMAKE_C_COMPILER}")
        set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS "${CMAKE_CXX_COMPILER}")
    endif()

    # By default Xcode passes a `-index-store-path=<...>` parameter to the compiler
    #  during builds to build code completion indexes. This is not supported by
    #  anything other than AppleClang
    set(CMAKE_XCODE_ATTRIBUTE_COMPILER_INDEX_STORE_ENABLE "NO")
endif()

# Generate a launch/build scheme for all targets
set(CMAKE_XCODE_GENERATE_SCHEME YES)

# Utility function that helps avoid messing with non-standard targets
macro(returnIfTargetIsNonTweakable target)
    get_target_property(targetIsAliased ${target} ALIASED_TARGET)
    get_target_property(targetIsImported ${target} IMPORTED)

    if (targetIsAliased OR targetIsImported)
        return()
    endif()

    get_target_property(targetType ${target} TYPE)
    if (targetType MATCHES "INTERFACE_LIBRARY|UNKNOWN_LIBRARY")
        return()
    endif()
endmacro()

# Targets usually don't specify their private headers, nor group their source files
#  which results in very spotty coverage by IDEs with folders support
#
# Unfortunately, CMake does not have a `target_source_group` like construct yet, therefore 
#  we have to play by the limitations of `source_group`. 
#
# A particularly problematic part is that the function must be called within the directoryies
#  scope for the grouping to take effect.
#
# See: https://discourse.cmake.org/t/topic/7388
function(tweakTargetForIDESupport target)
    returnIfTargetIsNonTweakable(${target})

    # Don't assume directory structure of third parties
    get_target_property(targetSourceDir ${target} SOURCE_DIR)
    if (targetSourceDir MATCHES "third_party")
        return()
    endif()

    # Add headers to target
    get_target_property(targetSourceDir ${target} SOURCE_DIR)
    if (targetSourceDir)
        file(GLOB_RECURSE targetPrivateHeaders CONFIGURE_DEPENDS "${targetSourceDir}/include/*.hpp")

        target_sources(${target} PRIVATE "${targetPrivateHeaders}")
    endif()

    # Organize target sources into directory tree
    get_target_property(sources ${target} SOURCES)
    foreach(file IN LISTS sources)
        get_filename_component(path "${file}" ABSOLUTE)

        if (NOT path MATCHES "^${targetSourceDir}")
            continue()
        endif()

        source_group(TREE "${targetSourceDir}" PREFIX "Source Tree" FILES "${file}")
    endforeach()
endfunction()

if (IMHEX_IDE_HELPERS_INTRUSIVE_IDE_TWEAKS)
    # See tweakTargetForIDESupport for rationale

    function(add_library target)
        _add_library(${target} ${ARGN})

        tweakTargetForIDESupport(${target})
    endfunction()

    function(add_executable target)
        _add_executable(${target} ${ARGN})

        tweakTargetForIDESupport(${target})
    endfunction()
endif()

# Adjust target's FOLDER property, which is an IDE only preference
function(_tweakTarget target path)
    get_target_property(targetType ${target} TYPE)

    if (TARGET generator-${target})
        set_target_properties(generator-${target} PROPERTIES FOLDER "romfs/${target}")
    endif()
    if (TARGET romfs_file_packer-${target})
        set_target_properties(romfs_file_packer-${target} PROPERTIES FOLDER "romfs/${target}")
    endif()
    if (TARGET libromfs-${target})
        set_target_properties(libromfs-${target} PROPERTIES FOLDER "romfs/${target}")
    endif()

    if (${targetType} MATCHES "EXECUTABLE|LIBRARY")
        set_target_properties(${target} PROPERTIES FOLDER "${path}")
    endif()
endfunction()

macro(_tweakTargetsRecursive dir)
    get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir IN LISTS subdirectories)
        _tweakTargetsRecursive("${subdir}")
    endforeach()

    if(${dir} STREQUAL ${CMAKE_SOURCE_DIR})
        return()
    endif()

    get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    file(RELATIVE_PATH rdir ${CMAKE_SOURCE_DIR} "${dir}/..")

    foreach(target ${targets})
        _tweakTarget(${target} "${rdir}")
    endforeach()
endmacro()

# Tweak all targets this CMake build is aware about
function(tweakTargetsForIDESupport)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    _tweakTargetsRecursive("${CMAKE_SOURCE_DIR}")
endfunction()
