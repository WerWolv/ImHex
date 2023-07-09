#pragma once

#include <loaders/loader.hpp>

#include <wolv/io/fs.hpp>

#include <functional>

namespace hex::script::loader {

    class DotNetLoader : public ScriptLoader {
    public:
        DotNetLoader();
        ~DotNetLoader() override = default;

        bool loadAll() override;

    private:
        std::function<bool(const std::fs::path&)> m_loadAssembly;
    };

}