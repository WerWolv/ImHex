#pragma once

#include <list>
#include <string>
#include <string_view>

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/concepts.hpp>
#include <hex/helpers/tar.hpp>

namespace hex {

    class ProjectFile {
    public:
        struct Handler {
            using Function = std::function<bool(const std::fs::path &, Tar &tar)>;
            std::fs::path basePath;
            bool required;
            Function load, store;
        };

        struct ProviderHandler {
            using Function = std::function<bool(prv::Provider *provider, const std::fs::path &, Tar &tar)>;
            std::fs::path basePath;
            bool required;
            Function load, store;
        };

        static bool load(const std::fs::path &filePath);
        static bool store(std::optional<std::fs::path> filePath = std::nullopt);

        static bool hasPath();

        static void registerHandler(const Handler &handler) {
            getHandlers().push_back(handler);
        }

        static void registerPerProviderHandler(const ProviderHandler &handler) {
            getProviderHandlers().push_back(handler);
        }

        static std::vector<Handler>& getHandlers() {
            return s_handlers;
        }

        static std::vector<ProviderHandler>& getProviderHandlers() {
            return s_providerHandlers;
        }

    private:
        ProjectFile() = default;

        static std::fs::path s_currProjectPath;
        static std::vector<Handler> s_handlers;
        static std::vector<ProviderHandler> s_providerHandlers;
    };

}