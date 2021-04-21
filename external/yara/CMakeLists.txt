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
        )

set(LIBYARA_MODULES
        ${LIBYARA_SOURCE_PATH}/modules/tests/tests.c
        ${LIBYARA_SOURCE_PATH}/modules/pe/pe.c
        ${LIBYARA_SOURCE_PATH}/modules/pe/pe_utils.c
        ${LIBYARA_SOURCE_PATH}/modules/elf/elf.c
        ${LIBYARA_SOURCE_PATH}/modules/math/math.c
        ${LIBYARA_SOURCE_PATH}/modules/time/time.c
        ${LIBYARA_SOURCE_PATH}/modules/macho/macho.c
        ${LIBYARA_SOURCE_PATH}/modules/hash/hash.c
        ${LIBYARA_SOURCE_PATH}/modules/dex/dex.c
        ${LIBYARA_SOURCE_PATH}/modules/dotnet/dotnet.c
        ${LIBYARA_SOURCE_PATH}/modules/magic/magic.c)

# Add mbedtls crypto wrappers
file(READ crypto_mbedtls.h MBEDTLS_CRYPTO_H)
file(WRITE ${LIBYARA_SOURCE_PATH}/crypto.h "${MBEDTLS_CRYPTO_H}")
add_compile_definitions("HAVE_MBEDTLS")

add_compile_definitions("USE_NO_PROC")

add_compile_definitions("HASH_MODULE")
add_compile_definitions("DOTNET_MODULE")
add_compile_definitions("MAGIC_MODULE")
add_compile_definitions("MACHO_MODULE")
add_compile_definitions("DEX_MODULE")


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-shift-count-overflow")
add_library(libyara STATIC ${LIBYARA_SOURCE} ${LIBYARA_INCLUDES} ${LIBYARA_MODULES})

target_include_directories(
        libyara
        PUBLIC $<BUILD_INTERFACE:${LIBYARA_SOURCE_PATH}/include> $<INSTALL_INTERFACE:include>
        PRIVATE ${LIBYARA_SOURCE_PATH} ${MBEDTLS_INCLUDE_DIR}
)

if (UNIX)
    target_link_libraries(libyara magic pthread)
else ()
    target_link_libraries(libyara magic)
endif ()

include(GNUInstallDirs)
configure_file(${LIBYARA_SOURCE_PATH}/yara.pc.in
        ${LIBYARA_SOURCE_PATH}/yara.pc @ONLY)
set(CMAKE_STATIC_LIBRARY_PREFIX "")