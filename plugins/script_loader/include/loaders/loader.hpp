#pragma once

#include <functional>
#include <string>
#include <vector>
#include <hex/helpers/utils.hpp>

#if defined(OS_WINDOWS)
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

namespace hex::script::loader {

    class ScriptLoader;

    struct Script {
        std::string name;
        bool background;
        std::function<void()> entryPoint;
        const ScriptLoader *loader;
    };

    class ScriptLoader {
    public:
        ScriptLoader(std::string typeName) : m_typeName(std::move(typeName)) {}
        virtual ~ScriptLoader() = default;

        virtual bool initialize() = 0;
        virtual bool loadAll() = 0;

        void addScript(std::string name, bool background, std::function<void()> entryPoint) {
            m_scripts.emplace_back(std::move(name), background, std::move(entryPoint), this);
        }

        const auto& getScripts() const {
            return m_scripts;
        }

        const std::string& getTypeName() const {
            return m_typeName;
        }

    protected:
        void clearScripts() {
            m_scripts.clear();
        }

    private:
        std::vector<Script> m_scripts;
        std::string m_typeName;
    };

#if defined(OS_WINDOWS)
    inline void *loadLibrary(const wchar_t *path) {
        try {
            HMODULE h = ::LoadLibraryW(path);
            return h;
        } catch (...) {
            return nullptr;
        }
    }

    inline void *loadLibrary(const char *path) {
        try {
            auto utf16Path = hex::utf8ToUtf16(path);
            HMODULE h = ::LoadLibraryW(utf16Path.c_str());
            return h;
        } catch (...) {
            return nullptr;
        }
    }

    template<typename T>
    T getExport(void *h, const char *name) {
        try {
            FARPROC f = ::GetProcAddress(static_cast<HMODULE>(h), name);
            return reinterpret_cast<T>(reinterpret_cast<uintptr_t>(f));
        } catch (...) {
            return nullptr;
        }
    }
#else
    inline void *loadLibrary(const char *path) {
        void *h = dlopen(path, RTLD_LAZY);
        return h;
    }

    template<typename T>
    T getExport(void *h, const char *name) {
        void *f = dlsym(h, name);
        return reinterpret_cast<T>(f);
    }
#endif

}