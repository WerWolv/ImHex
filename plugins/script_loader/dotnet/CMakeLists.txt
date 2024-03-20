project(dotnet_assemblies)

function(add_dotnet_assembly name)
    set(OUTPUT_DLL ${CMAKE_CURRENT_BINARY_DIR}/../../${name}.dll)
    set(OUTPUT_RUNTIMECONFIG ${CMAKE_CURRENT_BINARY_DIR}/../../${name}.runtimeconfig.json)

    file(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/${name}/*.cs)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/../../${name}.dll
        COMMAND ${DOTNET_EXECUTABLE} build ${CMAKE_CURRENT_SOURCE_DIR}/${name}/${name}.csproj -nologo -consoleLoggerParameters:NoSummary -verbosity:quiet -c Release -o ${CMAKE_CURRENT_BINARY_DIR}/../..
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name}/${name}.csproj ${sources}
        COMMENT "Building ${name}.dll"
    )

    add_custom_target(${name} ALL DEPENDS ${OUTPUT_DLL})

    install(FILES
                ${OUTPUT_DLL}
                ${OUTPUT_RUNTIMECONFIG}
            DESTINATION
                ${PLUGINS_INSTALL_LOCATION}
    )
endfunction()

add_dotnet_assembly(AssemblyLoader)