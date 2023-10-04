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

        using get_hostfxr_path_fn = int(*)(char_t * buffer, size_t * buffer_size, const struct get_hostfxr_parameters *parameters);

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
                void *h = dlopen(path, RTLD_LAZY);
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
            #if defined(OS_WINDOWS)
                auto netHostLibrary = loadLibrary(L"nethost.dll");
            #elif  defined(OS_LINUX)
                auto netHostLibrary = loadLibrary("libnethost.so");
            #elif defined(OS_MACOS)
                void *netHostLibrary = nullptr;
                for (const auto &pluginPath : fs::getDefaultPaths(fs::ImHexPath::Plugins)) {
                    auto frameworksPath = pluginPath.parent_path().parent_path() / "Frameworks";

                    netHostLibrary = loadLibrary((frameworksPath / "libnethost.dylib").c_str());
                    if (netHostLibrary != nullptr)
                        break;
                }
            #endif

            if (netHostLibrary == nullptr) {
                log::error("Could not load libnethost!");
                return false;
            }

            auto get_hostfxr_path_ptr = getExport<get_hostfxr_path_fn>(netHostLibrary, "get_hostfxr_path");

            std::array<char_t, 300> buffer = { 0 };
            size_t bufferSize = buffer.size();
            if (get_hostfxr_path_ptr(buffer.data(), &bufferSize, nullptr) != 0) {
                log::error("Could not get hostfxr path!");
                return false;
            }

            void *hostfxrLibrary = loadLibrary(buffer.data());
            if (hostfxrLibrary == nullptr) {
                log::error("Could not load hostfxr library!");
                return false;
            }

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
                hostfxr_delegate_type::hdt_load_assembly_and_get_function_pointer,
                (void**)&loadAssemblyFunction
            );

            if (result != 0 || loadAssemblyFunction == nullptr) {
                throw std::runtime_error("Failed to get load_assembly_and_get_function_pointer delegate");
            }

            return loadAssemblyFunction;
        }
    }


    bool DotNetLoader::initialize() {
        if (!loadHostfxr()) {
            log::error("Failed to initialize dotnet loader, could not load hostfxr");
            return false;
        }

        for (const auto& path : hex::fs::getDefaultPaths(hex::fs::ImHexPath::Plugins)) {
            auto assemblyLoader = path / "AssemblyLoader.dll";
            if (!wolv::io::fs::exists(assemblyLoader))
                continue;

            auto loadAssembly = getLoadAssemblyFunction(std::fs::absolute(path) / "AssemblyLoader.runtimeconfig.json");

            auto dotnetType = STRING("ImHex.EntryPoint, AssemblyLoader");

            const char_t *dotnetTypeMethod = STRING("ExecuteScript");
            this-> m_assemblyLoaderPathString = assemblyLoader.native();

            component_entry_point_fn entryPoint = nullptr;
            u32 result = loadAssembly(
                    this->m_assemblyLoaderPathString.c_str(),
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

            return true;
        }

        return false;
    }

    bool DotNetLoader::loadAll() {
        this->clearScripts();

        for (const auto &imhexPath : hex::fs::getDefaultPaths(hex::fs::ImHexPath::Scripts)) {
            auto directoryPath = imhexPath / "custom" / "dotnet";
            if (!wolv::io::fs::exists(directoryPath))
                wolv::io::fs::createDirectories(directoryPath);

            if (!wolv::io::fs::exists(directoryPath))
                continue;

            for (const auto &entry : std::fs::directory_iterator(directoryPath)) {
                if (!entry.is_directory())
                    continue;

                const auto &scriptPath = entry.path() / "Main.dll";
                if (!std::fs::exists(scriptPath))
                    continue;

                this->addScript(entry.path().stem().string(), [this, scriptPath] {
                    this->m_loadAssembly(scriptPath);
                });
            }
        }

        return true;
    }

}