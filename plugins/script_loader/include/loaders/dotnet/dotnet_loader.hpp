#pragma once

#include <loaders/loader.hpp>

namespace hex::plugin::loader {

    class DotNetLoader : public PluginLoader {
    public:
        DotNetLoader();
        ~DotNetLoader() override = default;

        bool loadAll() override;
    };

}