
option(IMHEX_IDE_HELPERS_OVERRIDE_XCODE_COMPILER "Enable choice of compiler for Xcode builds, despite CMake's best efforts" OFF)
option(IMHEX_IDE_HELPERS_INTRUSIVE_IDE_TWEAKS "Enable intrusive CMake tweaks to better support IDEs with folder support" OFF)

if (IMHEX_IDE_HELPERS_OVERRIDE_XCODE_COMPILER AND CMAKE_GENERATOR MATCHES "Xcode")
    set(old_generator ${CMAKE_GENERATOR})
    set(CMAKE_GENERATOR "Unknown")
    enable_language(C CXX)
    set(CMAKE_GENERATOR "${old_generator}")

    set(CMAKE_XCODE_ATTRIBUTE_CC  "${CMAKE_C_COMPILER}")
    set(CMAKE_XCODE_ATTRIBUTE_CXX "${CMAKE_CXX_COMPILER}")

    if (CLANG)
        set(CMAKE_XCODE_ATTRIBUTE_LD         "${CMAKE_C_COMPILER}")
        set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS "${CMAKE_CXX_COMPILER}")
    endif()

    set(CMAKE_XCODE_ATTRIBUTE_COMPILER_INDEX_STORE_ENABLE "NO")
endif()

if (NOT DEFINED CMAKE_XCODE_GENERATE_SCHEME)
    set(CMAKE_XCODE_GENERATE_SCHEME YES)
endif()

macro(returnIfTargetIsNonTweakable target)
    get_target_property(targetIsAliased ${target} ALIASED_TARGET)
    if (NOT targetIsAliased)
        set(targetIsAliased FALSE)
    endif()

    get_target_property(targetIsImported ${target} IMPORTED)
    if (NOT targetIsImported)
        set(targetIsImported FALSE)
    endif()

    if (targetIsAliased OR targetIsImported)
        return()
    endif()

    get_target_property(targetType ${target} TYPE)
    if (NOT targetType OR targetType MATCHES "INTERFACE_LIBRARY|UNKNOWN_LIBRARY")
        return()
    endif()
endmacro()

function(addPrivateHeaders target)
    get_target_property(targetSourceDir ${target} SOURCE_DIR)
    if (targetSourceDir)
        file(GLOB_RECURSE targetPrivateHeaders "${targetSourceDir}/include/*.hpp")
        foreach(header IN LISTS targetPrivateHeaders)
            if(EXISTS "${header}")
                target_sources(${target} PRIVATE "${header}")
            endif()
        endforeach()
    endif()
endfunction()

macro(_tweakTargetsRecursive dir)
    get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir IN LISTS subdirectories)
        _tweakTargetsRecursive("${subdir}")
    endforeach()

    if (NOT dir OR dir STREQUAL CMAKE_SOURCE_DIR)
        return()
    endif()

    get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    file(RELATIVE_PATH rdir ${CMAKE_SOURCE_DIR} "${dir}/..")

    foreach(target ${targets})
        _tweakTarget(${target} "${rdir}")
    endforeach()
endmacro()

function(tweakTargetsForIDESupport)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    _tweakTargetsRecursive("${CMAKE_SOURCE_DIR}")
endfunction()
