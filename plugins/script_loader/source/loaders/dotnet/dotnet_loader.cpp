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
#include <wolv/utils/string.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

namespace hex::script::loader {

    namespace {

        #if defined(OS_WINDOWS)
            void *loadLibrary(const char_t *path) {
                try {
                    HMODULE h = ::LoadLibraryW(path);
                    return (void*)h;
                } catch (...) {
                    return nullptr;
                }
            }

            template<typename T>
            T getExport(void *h, const char *name) {
                try {
                    FARPROC f = ::GetProcAddress((HMODULE)h, name);
                    return reinterpret_cast<T>((void*)f);
                } catch (...) {
                    return nullptr;
                }
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

        hostfxr_initialize_for_runtime_config_fn hostfxr_initialize_for_runtime_config  = nullptr;
        hostfxr_get_runtime_delegate_fn hostfxr_get_runtime_delegate = nullptr;
        hostfxr_close_fn hostfxr_close = nullptr;

        bool loadHostfxr() {
            std::array<char_t, 300> buffer = { 0 };
            size_t bufferSize = buffer.size();
            if (get_hostfxr_path(buffer.data(), &bufferSize, nullptr) != 0) {
                return false;
            }

            void *hostfxrLibrary = loadLibrary(buffer.data());
            if (hostfxrLibrary == nullptr)
                return false;

            {
                hostfxr_initialize_for_runtime_config
                        = getExport<hostfxr_initialize_for_runtime_config_fn>(hostfxrLibrary, "hostfxr_initialize_for_runtime_config");
                hostfxr_get_runtime_delegate
                        = getExport<hostfxr_get_runtime_delegate_fn>(hostfxrLibrary, "hostfxr_get_runtime_delegate");
                hostfxr_close
                        = getExport<hostfxr_close_fn>(hostfxrLibrary, "hostfxr_close");

            }

            return
                hostfxr_initialize_for_runtime_config != nullptr &&
                hostfxr_get_runtime_delegate != nullptr &&
                hostfxr_close != nullptr;
        }

        load_assembly_and_get_function_pointer_fn getLoadAssemblyFunction(const std::fs::path &path) {
            load_assembly_and_get_function_pointer_fn loadAssemblyFunction = nullptr;

            hostfxr_handle ctx = nullptr;

            u32 result = hostfxr_initialize_for_runtime_config(path.c_str(), nullptr, &ctx);
            ON_SCOPE_EXIT {
                hostfxr_close(ctx);
            };

            if (result > 2 || ctx == nullptr) {
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

        for (const auto& path : hex::fs::getDefaultPaths(hex::fs::ImHexPath::Plugins)) {
            auto assemblyLoader = path / "AssemblyLoader.dll";
            if (!wolv::io::fs::exists(assemblyLoader))
                continue;

            auto loadAssembly = getLoadAssemblyFunction(std::fs::absolute(path) / "AssemblyLoader.runtimeconfig.json");

            auto dotnetType = STRING("ImHex.EntryPoint, AssemblyLoader");

            const char_t *dotnetTypeMethod = STRING("ExecuteScript");
            const auto &assemblyPathStr = assemblyLoader.native();

            component_entry_point_fn entryPoint = nullptr;
            u32 result = loadAssembly(
                    assemblyPathStr.c_str(),
                    dotnetType,
                    dotnetTypeMethod,
                    nullptr,
                    nullptr,
                    (void**)&entryPoint
            );

            if (result != 0 || entryPoint == nullptr) {
                log::error("Failed to load assembly loader '{}'", assemblyLoader.string());
                continue;
            }

            this->m_loadAssembly = [entryPoint](const std::fs::path &path) -> bool {
                auto string = wolv::util::toUTF8String(path);
                auto result = entryPoint(string.data(), string.size());

                return result == 0;
            };
        };
    }

    bool DotNetLoader::loadAll() {
        this->clearScripts();

        for (const auto &imhexPath : hex::fs::getDefaultPaths(hex::fs::ImHexPath::Scripts)) {
            auto directoryPath = imhexPath / "custom" / "dotnet";
            if (!std::fs::exists(directoryPath))
                continue;

            for (const auto &entry : std::fs::directory_iterator(directoryPath)) {
                if (!entry.is_regular_file())
                    continue;

                const auto &scriptPath = entry.path();
                if (!std::fs::exists(scriptPath))
                    continue;

                this->addScript(scriptPath.stem().string(), [this, scriptPath] {
                    this->m_loadAssembly(scriptPath);
                });
            }
        }

        return true;
    }

}