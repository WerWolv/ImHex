#pragma once

#include <loaders/loader.hpp>

#include <wolv/io/fs.hpp>

#include <functional>

namespace hex::script::loader {

    class DotNetLoader : public ScriptLoader {
    public:
        DotNetLoader() : ScriptLoader(".NET") {}
        ~DotNetLoader() override = default;

        bool initialize() override;
        bool loadAll() override;
        void clearScripts() override;

    private:
        std::function<int(const std::string &, bool, const std::fs::path&)>  m_runMethod;
        std::function<bool(const std::string &, const std::fs::path&)> m_methodExists;
        std::fs::path::string_type m_assemblyLoaderPathString;
    };

}