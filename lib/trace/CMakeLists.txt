project(tracing)

option(IMHEX_TRACE_EXCEPTIONS "Hook thrown exceptions to display a stack trace when possible" ON)

if (IMHEX_EXTERNAL_PLUGIN_BUILD)
    add_library(tracing IMPORTED SHARED GLOBAL)

    set(LIBIMHEX_LIBRARY_TYPE_PUBLIC INTERFACE)
    set(LIBIMHEX_LIBRARY_TYPE_PRIVATE INTERFACE)
else()
    if (IMHEX_STATIC_LINK_PLUGINS)
        set(TRACING_LIBRARY_TYPE OBJECT)
    else()
        set(TRACING_LIBRARY_TYPE STATIC)
    endif()

    add_library(tracing ${TRACING_LIBRARY_TYPE}
            source/stacktrace.cpp
            source/exceptions.cpp
    )
    target_link_libraries(tracing PRIVATE LLVMDemangle ${FMT_LIBRARIES})

    set(LIBIMHEX_LIBRARY_TYPE_PUBLIC PUBLIC)
    set(LIBIMHEX_LIBRARY_TYPE_PRIVATE PRIVATE)
endif()

target_include_directories(tracing ${LIBIMHEX_LIBRARY_TYPE_PUBLIC} include)

if (NOT IMHEX_DISABLE_STACKTRACE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        include(CheckSourceRuns)
        set(CMAKE_REQUIRED_LINK_OPTIONS "-lstdc++exp")
        check_source_runs(CXX "
            #include <stacktrace>
            int main() {
                auto stacktrace = std::stacktrace::current();
            }
        " HAVE_STDCPPEXP)
        set(CMAKE_REQUIRED_LINK_OPTIONS "-lstdc++_libbacktrace")
        check_source_runs(CXX "
            #include <stacktrace>
            int main() {
                auto stacktrace = std::stacktrace::current();
            }
        " HAVE_STDCPP_LIBBACKTRACE)

        if (HAVE_STDCPPEXP)
            message(STATUS "<stacktrace> enabled!")
            target_link_libraries(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} stdc++exp dl)
            if (HAVE_STDCPP_LIBBACKTRACE)
                target_link_libraries(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} stdc++_libbacktrace)
            endif()
            target_compile_definitions(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} HEX_HAS_STD_STACKTRACE)
        endif()
    endif()

    if (WIN32 AND NOT HAVE_STDCPPEXP)
        message(STATUS "StackWalk enabled!")
        target_link_libraries(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} DbgHelp.lib)
    else ()
        find_package(Backtrace)
        if (${Backtrace_FOUND})
            message(STATUS "Backtrace enabled! Header: ${Backtrace_HEADER}")

            if (Backtrace_HEADER STREQUAL "backtrace.h")
                target_link_libraries(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} ${Backtrace_LIBRARY})
                target_include_directories(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} ${Backtrace_INCLUDE_DIR})
                target_compile_definitions(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} BACKTRACE_HEADER=<${Backtrace_HEADER}>)
                target_compile_definitions(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} HEX_HAS_BACKTRACE)
            elseif (Backtrace_HEADER STREQUAL "execinfo.h")
                target_link_libraries(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE}  ${Backtrace_LIBRARY})
                target_include_directories(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} ${Backtrace_INCLUDE_DIR})
                target_compile_definitions(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} BACKTRACE_HEADER=<${Backtrace_HEADER}>)
                target_compile_definitions(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} HEX_HAS_EXECINFO)
            endif()
        endif()
    endif()
endif()

if (IMHEX_TRACE_EXCEPTIONS)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_link_options(tracing ${LIBIMHEX_LIBRARY_TYPE_PUBLIC} "-Wl,--wrap=_ZSt21__glibcxx_assert_failPKciS0_S0_")
        target_compile_definitions(tracing ${LIBIMHEX_LIBRARY_TYPE_PRIVATE} HEX_WRAP_GLIBCXX_ASSERT_FAIL)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Not supported currently
    endif()
endif()
