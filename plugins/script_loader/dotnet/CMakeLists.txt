project(dotnet_assemblies)

function(add_dotnet_assembly name)
    set(OUTPUT_DLL ${CMAKE_CURRENT_BINARY_DIR}/../../${name}.dll)
    set(OUTPUT_RUNTIMECONFIG ${CMAKE_CURRENT_BINARY_DIR}/../../${name}.runtimeconfig.json)

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/../../${name}.dll
        COMMAND ${DOTNET_EXECUTABLE} build ${CMAKE_CURRENT_SOURCE_DIR}/${name}/${name}.csproj --nologo -c Release -o ${CMAKE_CURRENT_BINARY_DIR}/../.. && ${CMAKE_COMMAND} -DOUTPUT_RUNTIMECONFIG="${OUTPUT_RUNTIMECONFIG}" -P ${CMAKE_CURRENT_SOURCE_DIR}/post_process_runtimeconfig.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name}/${name}.csproj
        COMMENT "Building ${name}.dll"
    )


    add_custom_target(${name} ALL DEPENDS ${OUTPUT_DLL})

    install(FILES
                ${OUTPUT_DLL}
                ${OUTPUT_RUNTIMECONFIG}
            DESTINATION
                ${PLUGINS_INSTALL_LOCATION}
    )

    file(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/${name}/*.cs)
    target_sources(${name} PRIVATE ${sources})
endfunction()

add_dotnet_assembly(AssemblyLoader)