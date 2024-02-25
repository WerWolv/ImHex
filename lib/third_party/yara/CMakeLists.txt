cmake_minimum_required(VERSION 3.16)

set(LIBYARA_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/yara/libyara")

set(LIBYARA_INCLUDES
        ${LIBYARA_SOURCE_PATH}/include/yara/ahocorasick.h
        ${LIBYARA_SOURCE_PATH}/include/yara/arena.h
        ${LIBYARA_SOURCE_PATH}/include/yara/atoms.h
        ${LIBYARA_SOURCE_PATH}/include/yara/bitmask.h
        ${LIBYARA_SOURCE_PATH}/include/yara/compiler.h
        ${LIBYARA_SOURCE_PATH}/include/yara/error.h
        ${LIBYARA_SOURCE_PATH}/include/yara/exec.h
        ${LIBYARA_SOURCE_PATH}/include/yara/exefiles.h
        ${LIBYARA_SOURCE_PATH}/include/yara/filemap.h
        ${LIBYARA_SOURCE_PATH}/include/yara/hash.h
        ${LIBYARA_SOURCE_PATH}/include/yara/integers.h
        ${LIBYARA_SOURCE_PATH}/include/yara/libyara.h
        ${LIBYARA_SOURCE_PATH}/include/yara/limits.h
        ${LIBYARA_SOURCE_PATH}/include/yara/mem.h
        ${LIBYARA_SOURCE_PATH}/include/yara/modules.h
        ${LIBYARA_SOURCE_PATH}/include/yara/object.h
        ${LIBYARA_SOURCE_PATH}/include/yara/parser.h
        ${LIBYARA_SOURCE_PATH}/include/yara/proc.h
        ${LIBYARA_SOURCE_PATH}/include/yara/re.h
        ${LIBYARA_SOURCE_PATH}/include/yara/rules.h
        ${LIBYARA_SOURCE_PATH}/include/yara/scan.h
        ${LIBYARA_SOURCE_PATH}/include/yara/scanner.h
        ${LIBYARA_SOURCE_PATH}/include/yara/sizedstr.h
        ${LIBYARA_SOURCE_PATH}/include/yara/stack.h
        ${LIBYARA_SOURCE_PATH}/include/yara/stopwatch.h
        ${LIBYARA_SOURCE_PATH}/include/yara/stream.h
        ${LIBYARA_SOURCE_PATH}/include/yara/strutils.h
        ${LIBYARA_SOURCE_PATH}/include/yara/threading.h
        ${LIBYARA_SOURCE_PATH}/include/yara/types.h
        ${LIBYARA_SOURCE_PATH}/include/yara/utils.h
        ${LIBYARA_SOURCE_PATH}/crypto.h
        ${LIBYARA_SOURCE_PATH}/tlshc/tlsh_impl.h
        ${LIBYARA_SOURCE_PATH}/tlshc/tlsh_util.h
        )

set(LIBYARA_SOURCE
        ${LIBYARA_SOURCE_PATH}/grammar.y
        ${LIBYARA_SOURCE_PATH}/ahocorasick.c
        ${LIBYARA_SOURCE_PATH}/arena.c
        ${LIBYARA_SOURCE_PATH}/atoms.c
        ${LIBYARA_SOURCE_PATH}/base64.c
        ${LIBYARA_SOURCE_PATH}/bitmask.c
        ${LIBYARA_SOURCE_PATH}/compiler.c
        ${LIBYARA_SOURCE_PATH}/endian.c
        ${LIBYARA_SOURCE_PATH}/exec.c
        ${LIBYARA_SOURCE_PATH}/exefiles.c
        ${LIBYARA_SOURCE_PATH}/filemap.c
        ${LIBYARA_SOURCE_PATH}/hash.c
        ${LIBYARA_SOURCE_PATH}/hex_grammar.y
        ${LIBYARA_SOURCE_PATH}/hex_lexer.l
        ${LIBYARA_SOURCE_PATH}/lexer.l
        ${LIBYARA_SOURCE_PATH}/libyara.c
        ${LIBYARA_SOURCE_PATH}/mem.c
        ${LIBYARA_SOURCE_PATH}/modules.c
        ${LIBYARA_SOURCE_PATH}/notebook.c
        ${LIBYARA_SOURCE_PATH}/object.c
        ${LIBYARA_SOURCE_PATH}/parser.c
        ${LIBYARA_SOURCE_PATH}/proc.c
        ${LIBYARA_SOURCE_PATH}/re.c
        ${LIBYARA_SOURCE_PATH}/re_grammar.y
        ${LIBYARA_SOURCE_PATH}/re_lexer.l
        ${LIBYARA_SOURCE_PATH}/rules.c
        ${LIBYARA_SOURCE_PATH}/scan.c
        ${LIBYARA_SOURCE_PATH}/scanner.c
        ${LIBYARA_SOURCE_PATH}/sizedstr.c
        ${LIBYARA_SOURCE_PATH}/simple_str.c
        ${LIBYARA_SOURCE_PATH}/stack.c
        ${LIBYARA_SOURCE_PATH}/stopwatch.c
        ${LIBYARA_SOURCE_PATH}/strutils.c
        ${LIBYARA_SOURCE_PATH}/stream.c
        ${LIBYARA_SOURCE_PATH}/threading.c
        ${LIBYARA_SOURCE_PATH}/lexer.c
        ${LIBYARA_SOURCE_PATH}/hex_lexer.c
        ${LIBYARA_SOURCE_PATH}/grammar.c
        ${LIBYARA_SOURCE_PATH}/re_lexer.c
        ${LIBYARA_SOURCE_PATH}/hex_grammar.c
        ${LIBYARA_SOURCE_PATH}/re_grammar.c
        ${LIBYARA_SOURCE_PATH}/proc/none.c
        ${LIBYARA_SOURCE_PATH}/tlshc/tlsh.c
        ${LIBYARA_SOURCE_PATH}/tlshc/tlsh_impl.c
        ${LIBYARA_SOURCE_PATH}/tlshc/tlsh_util.c
        )

set(LIBYARA_MODULES
        ${LIBYARA_SOURCE_PATH}/modules/console/console.c
        ${LIBYARA_SOURCE_PATH}/modules/dex/dex.c
        ${LIBYARA_SOURCE_PATH}/modules/dotnet/dotnet.c
        ${LIBYARA_SOURCE_PATH}/modules/elf/elf.c
        ${LIBYARA_SOURCE_PATH}/modules/hash/hash.c
        ${LIBYARA_SOURCE_PATH}/modules/macho/macho.c
        ${LIBYARA_SOURCE_PATH}/modules/magic/magic.c
        ${LIBYARA_SOURCE_PATH}/modules/math/math.c
        ${LIBYARA_SOURCE_PATH}/modules/pe/pe.c
        ${LIBYARA_SOURCE_PATH}/modules/pe/pe_utils.c
        ${LIBYARA_SOURCE_PATH}/modules/string/string.c
        ${LIBYARA_SOURCE_PATH}/modules/tests/tests.c
        ${LIBYARA_SOURCE_PATH}/modules/time/time.c
)

find_package(mbedTLS 2.26.0 REQUIRED)

add_library(libyara STATIC ${LIBYARA_SOURCE} ${LIBYARA_INCLUDES} ${LIBYARA_MODULES})

# Add mbedtls crypto wrappers
target_compile_definitions(libyara PRIVATE
        HAVE_MBEDTLS
        USE_NO_PROC BUCKETS_256 CHECKSUM_3B
        HASH_MODULE DOTNET_MODULE MAGIC_MODULE MACHO_MODULE DEX_MODULE
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(libyara PRIVATE -Wno-shift-count-overflow -Wno-stringop-overflow)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(libyara PRIVATE -Wno-pointer-sign -Wno-tautological-constant-out-of-range-compare)
endif ()

target_include_directories(
        libyara
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/yara> $<BUILD_INTERFACE:${LIBYARA_SOURCE_PATH}/include>
        PRIVATE ${LIBYARA_SOURCE_PATH} ${MBEDTLS_INCLUDE_DIR}
)

target_link_directories(libyara PRIVATE ${MBEDTLS_LIBRARY_DIR})

if (UNIX)
    target_link_libraries(libyara PRIVATE magic ${MBEDTLS_LIBRARIES} pthread)
else ()
    target_link_libraries(libyara PRIVATE magic ${MBEDTLS_LIBRARIES})
endif ()

include(GNUInstallDirs)

set(CMAKE_STATIC_LIBRARY_PREFIX "")
