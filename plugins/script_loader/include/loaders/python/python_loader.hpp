#pragma once

#include <loaders/loader.hpp>

#include <wolv/io/fs.hpp>

#include <functional>

namespace hex::script::loader {

    class PythonLoader : public ScriptLoader {
    public:
        PythonLoader() : ScriptLoader("Python") {}
        ~PythonLoader() override = default;

        bool initialize() override;
        bool loadAll() override;

    private:
        std::vector<void*> m_loadedModules;
    };

}