project(dotnet_assemblies)

function(add_dotnet_assembly name)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/../../${name}.dll
        COMMAND ${DOTNET_EXECUTABLE} build ${CMAKE_CURRENT_SOURCE_DIR}/${name}/${name}.csproj --nologo -c Release -o ${CMAKE_CURRENT_BINARY_DIR}/../..
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name}/${name}.csproj
        COMMENT "Building ${name}.dll"
    )
    add_custom_target(${name} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/../../${name}.dll)
    install(FILES
                ${CMAKE_CURRENT_BINARY_DIR}/../../${name}.dll
                ${CMAKE_CURRENT_BINARY_DIR}/../../${name}.runtimeconfig.json
            DESTINATION
                ${PLUGINS_INSTALL_LOCATION}
    )

    file(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/${name}/*.cs)
    target_sources(${name} PRIVATE ${sources})
endfunction()

add_dotnet_assembly(AssemblyLoader)