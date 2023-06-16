#include <loaders/dotnet/dotnet_loader.hpp>

#include <stdexcept>

#if defined(OS_WINDOWS)
    #include <Windows.h>
    #define STRING(str) L##str
#else
    #include <dlfcn.h>
    #define STRING(str) str
#endif

#include <array>

#include <nethost.h>
#include <coreclr_delegates.h>
#include <hostfxr.h>

#include <hex/helpers/fs.hpp>
#include <wolv/io/fs.hpp>
#include <wolv/utils/guards.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

namespace hex::plugin::loader {

    namespace {

        #if defined(OS_WINDOWS)
            void *loadLibrary(const char_t *path) {
                HMODULE h = ::LoadLibraryW(path);
                return (void*)h;
            }

            template<typename T>
            T getExport(void *h, const char *name) {
                FARPROC f = ::GetProcAddress((HMODULE)h, name);
                return reinterpret_cast<T>((void*)f);
            }
        #else
            void *loadLibrary(const char_t *path) {
                void *h = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
                return h;
            }

            template<typename T>
            T getExport(void *h, const char *name) {
                void *f = dlsym(h, name);
                return reinterpret_cast<T>(f);
            }
        #endif

        hostfxr_initialize_for_dotnet_command_line_fn hostfxr_initialize_for_dotnet_command_line = nullptr;
        hostfxr_get_runtime_delegate_fn hostfxr_get_runtime_delegate = nullptr;
        hostfxr_close_fn hostfxr_close = nullptr;

        bool loadHostfxr() {
            std::array<char_t, 300> buffer = { 0 };
            size_t bufferSize = buffer.size();
            if (get_hostfxr_path(buffer.data(), &bufferSize, nullptr) != 0) {
                return false;
            }

            void *hostfxrLibrary = loadLibrary(buffer.data());
            {
                hostfxr_initialize_for_dotnet_command_line
                        = getExport<hostfxr_initialize_for_dotnet_command_line_fn>(hostfxrLibrary, "hostfxr_initialize_for_dotnet_command_line");
                hostfxr_get_runtime_delegate
                        = getExport<hostfxr_get_runtime_delegate_fn>(hostfxrLibrary, "hostfxr_get_runtime_delegate");
                hostfxr_close
                        = getExport<hostfxr_close_fn>(hostfxrLibrary, "hostfxr_close");

            }

            return
                hostfxr_initialize_for_dotnet_command_line != nullptr &&
                hostfxr_get_runtime_delegate != nullptr &&
                hostfxr_close != nullptr;
        }

        load_assembly_and_get_function_pointer_fn getLoadAssemblyFunction(const std::fs::path &path) {
            load_assembly_and_get_function_pointer_fn loadAssemblyFunction = nullptr;

            hostfxr_handle ctx = nullptr;

            auto pathString = path.native();
            std::array<const char_t *, 1> args = { pathString.data() };

            u32 result = hostfxr_initialize_for_dotnet_command_line(1, args.data(), nullptr, &ctx);
            ON_SCOPE_EXIT {
                hostfxr_close(ctx);
            };

            if (result != 0 || ctx == nullptr) {
                throw std::runtime_error(hex::format("Failed to initialize command line {:X}", result));
            }

            result = hostfxr_get_runtime_delegate(
                ctx,
                hdt_load_assembly_and_get_function_pointer,
                (void**)&loadAssemblyFunction
            );

            if (result != 0 || loadAssemblyFunction == nullptr) {
                throw std::runtime_error("Failed to get load_assembly_and_get_function_pointer delegate");
            }

            return loadAssemblyFunction;
        }
    }

    DotNetLoader::DotNetLoader() {
        AT_FIRST_TIME {
            if (!loadHostfxr()) {
                throw std::runtime_error("Failed to load hostfxr");
            }
        };
    }

    bool DotNetLoader::loadAll() {
        for (const auto& path : hex::fs::getDefaultPaths(hex::fs::ImHexPath::Plugins)) {
            auto dotnetPath = path / "managed" / "dotnet";
            if (!wolv::io::fs::exists(dotnetPath))
                continue;

            for (const auto& folder : std::fs::directory_iterator(dotnetPath)) {
                if (!std::fs::is_directory(folder))
                    continue;

                for (const auto& file : std::fs::directory_iterator(folder)) {
                    if (file.path().filename() != "Main.dll")
                        continue;

                    auto loadAssembly = getLoadAssemblyFunction(std::fs::absolute(file.path()));

                    const auto &assemblyPath = file.path();
                    auto dotnetType = STRING("ImHex.EntryPoint, Main");

                    const char_t *dotnetTypeMethod = STRING("ScriptMain");
                    const auto &assemblyPathStr = assemblyPath.native();

                    component_entry_point_fn entryPoint = nullptr;
                    u32 result = loadAssembly(
                        assemblyPathStr.c_str(),
                        dotnetType,
                        dotnetTypeMethod,
                        UNMANAGEDCALLERSONLY_METHOD,
                        nullptr,
                        (void**)&entryPoint
                    );

                    if (result != 0 || entryPoint == nullptr) {
                        log::error("Failed to load assembly '{}'", file.path().filename().string());
                        continue;
                    }

                    this->addPlugin(
                        folder.path().stem().string(),
                        [entryPoint]{
                            u8 param = 0;
                            entryPoint(&param, 1);
                        }
                    );
                }
            }
        }

        return true;
    }

}