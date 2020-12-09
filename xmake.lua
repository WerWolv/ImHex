
toolchain("mygcc")
    set_toolset("cc", "gcc-10")
    set_toolset("cxx", "g++-10")
    set_toolset("ld", "g++-10")
toolchain_end()

add_requires("brew::libmagic","nlohmann_json","brew::glfw", "brew::capstone","brew::openssl@1.1","brew::python@3.8/python-3.8")

target("ImHex")
    set_kind("binary")
    set_languages("c99", "c++20")
    set_toolchains("mygcc")

    add_packages("brew::libmagic","nlohmann_json","brew::glfw","brew::capstone","brew::openssl@1.1","brew::python@3.8/python-3.8")

    add_files("source/main.cpp")
    add_files("source/window.cpp")
    add_files("source/helpers/utils.cpp")
    add_files("source/helpers/crypto.cpp")
    add_files("source/helpers/patches.cpp")
    add_files("source/helpers/math_evaluator.cpp")
    add_files("source/helpers/project_file_handler.cpp")
    add_files("source/helpers/loader_script_handler.cpp")
    add_files("source/lang/preprocessor.cpp")
    add_files("source/lang/lexer.cpp")
    add_files("source/lang/parser.cpp")
    add_files("source/lang/validator.cpp")
    add_files("source/lang/evaluator.cpp")
    add_files("source/providers/file_provider.cpp")
    add_files("source/views/view_hexeditor.cpp")
    add_files("source/views/view_pattern.cpp")
    add_files("source/views/view_pattern_data.cpp")
    add_files("source/views/view_hashes.cpp")
    add_files("source/views/view_information.cpp")
    add_files("source/views/view_help.cpp")
    add_files("source/views/view_tools.cpp")
    add_files("source/views/view_strings.cpp")
    add_files("source/views/view_data_inspector.cpp")
    add_files("source/views/view_disassembler.cpp")
    add_files("source/views/view_bookmarks.cpp")
    add_files("source/views/view_patches.cpp")
    add_files("libs/glad/source/glad.c")
    add_files("libs/ImGui/source/imgui.cpp")
    add_files("libs/ImGui/source/imgui_draw.cpp")
    add_files("libs/ImGui/source/imgui_widgets.cpp")
    add_files("libs/ImGui/source/imgui_demo.cpp")
    add_files("libs/ImGui/source/imgui_impl_glfw.cpp")
    add_files("libs/ImGui/source/imgui_impl_opengl3.cpp")
    add_files("libs/ImGui/source/ImGuiFileBrowser.cpp")
    add_files("libs/ImGui/source/TextEditor.cpp")

    add_includedirs("include")
    add_includedirs("libs/ImGui/include")
    add_includedirs("libs/glad/include")
    add_includedirs("/usr/local/include")
    


    add_linkdirs(
    "/usr/local/lib",
    "/Library/Frameworks/Python.framework/Versions/3.8/lib/python3.8/config-3.8-darwin")
        
    add_cxflags(
    "-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.0.sdk")
    
    add_ldflags("-lpython3.8",{force = true})

    on_load(function (target)
        import("detect.sdks.find_xcode")
        local toolchain = find_xcode("/Applications/Xcode.app")
        target:add("cxflags",
            "-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX" .. toolchain.sdkver .. ".sdk")

        target:add("linkdirs","/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX" .. toolchain.sdkver .. ".sdk/usr/lib")
    end)
