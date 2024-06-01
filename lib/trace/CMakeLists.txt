project(tracing)

option(IMHEX_TRACE_EXCEPTIONS "Hook thrown exceptions to display a stack trace when possible" ON)
option(IMHEX_INSTRUMENT_FUNCTIONS "Hook all function entries and exits to profile things in Tracy" OFF)

add_library(tracing OBJECT
    source/stacktrace.cpp
    source/exceptions.cpp
)
target_include_directories(tracing PUBLIC include)
target_link_libraries(tracing PRIVATE stdc++exp)

if (NOT IMHEX_DISABLE_STACKTRACE)
    if (WIN32)
        message(STATUS "StackWalk enabled!")
        target_link_libraries(tracing PRIVATE DbgHelp.lib)
    else ()
        find_package(Backtrace)
        if (${Backtrace_FOUND})
            message(STATUS "Backtrace enabled! Header: ${Backtrace_HEADER}")

            if (Backtrace_HEADER STREQUAL "backtrace.h")
                target_link_libraries(tracing PRIVATE ${Backtrace_LIBRARY})
                target_include_directories(tracing PRIVATE ${Backtrace_INCLUDE_DIR})
                target_compile_definitions(tracing PRIVATE BACKTRACE_HEADER=<${Backtrace_HEADER}>)
                target_compile_definitions(tracing PRIVATE HEX_HAS_BACKTRACE)
            elseif (Backtrace_HEADER STREQUAL "execinfo.h")
                target_link_libraries(tracing PRIVATE  ${Backtrace_LIBRARY})
                target_include_directories(tracing PRIVATE ${Backtrace_INCLUDE_DIR})
                target_compile_definitions(tracing PRIVATE BACKTRACE_HEADER=<${Backtrace_HEADER}>)
                target_compile_definitions(tracing PRIVATE HEX_HAS_EXECINFO)
            endif()
        endif()
    endif()

    target_link_libraries(tracing PRIVATE LLVMDemangle)
endif()

if (IMHEX_TRACE_EXCEPTIONS)
    target_link_options(tracing PUBLIC "-Wl,--wrap=__cxa_throw")
endif()

if (IMHEX_INSTRUMENT_FUNCTIONS)
    target_sources(tracing PUBLIC
        source/instr_entry.cpp
        source/instrumentation.cpp
    )

    FetchContent_Declare(
            tracy
            GIT_REPOSITORY https://github.com/wolfpld/tracy.git
            GIT_TAG v0.10
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(tracy)
    target_compile_options(TracyClient PRIVATE "-Wno-error")

    target_compile_options(tracing PUBLIC "-finstrument-functions")
    target_link_libraries(tracing PRIVATE TracyClient)
endif()
