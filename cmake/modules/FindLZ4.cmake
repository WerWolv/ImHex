find_path(LZ4_INCLUDE_DIR
        NAMES lz4.h
        HINTS "${LZ4_INCLUDEDIR}" "${LZ4_HINTS}/include"
        PATHS
        /usr/local/include
        /usr/include
)

find_library(LZ4_LIBRARY
        NAMES lz4 liblz4
        HINTS "${LZ4_LIBDIR}" "${LZ4_HINTS}/lib"
        PATHS
        /usr/local/lib
        /usr/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args( LZ4 DEFAULT_MSG LZ4_LIBRARY LZ4_INCLUDE_DIR )

if( LZ4_FOUND )
    include( CheckIncludeFile )
    include( CMakePushCheckState )

    set( LZ4_INCLUDE_DIRS ${LZ4_INCLUDE_DIR} )
    set( LZ4_LIBRARIES ${LZ4_LIBRARY} )

    cmake_push_check_state()
    set( CMAKE_REQUIRED_INCLUDES ${LZ4_INCLUDE_DIRS} )
    check_include_file( lz4frame.h HAVE_LZ4FRAME_H )
    cmake_pop_check_state()

    if (WIN32)
        set ( LZ4_DLL_DIR "${LZ4_HINTS}/bin"
                CACHE PATH "Path to LZ4 DLL"
        )
        file( GLOB _lz4_dll RELATIVE "${LZ4_DLL_DIR}"
                "${LZ4_DLL_DIR}/lz4*.dll"
        )
        set ( LZ4_DLL ${_lz4_dll}
                # We're storing filenames only. Should we use STRING instead?
                CACHE FILEPATH "LZ4 DLL file name"
        )
        file( GLOB _lz4_pdb RELATIVE "${LZ4_DLL_DIR}"
                "${LZ4_DLL_DIR}/lz4*.pdb"
        )
        set ( LZ4_PDB ${_lz4_pdb}
                CACHE FILEPATH "LZ4 PDB file name"
        )
        mark_as_advanced( LZ4_DLL_DIR LZ4_DLL LZ4_PDB )
    endif()
else()
    set( LZ4_INCLUDE_DIRS )
    set( LZ4_LIBRARIES )
endif()

mark_as_advanced( LZ4_LIBRARIES LZ4_INCLUDE_DIRS )

add_library( LZ4::lz4 INTERFACE IMPORTED )
set_property( TARGET LZ4::lz4 PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${LZ4_INCLUDE_DIRS} )
set_property( TARGET LZ4::lz4 PROPERTY INTERFACE_LINK_LIBRARIES ${LZ4_LIBRARIES} )