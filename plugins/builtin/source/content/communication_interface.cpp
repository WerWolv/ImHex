#include <hex/api/content_registry.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    void registerNetworkEndpoints() {
        ContentRegistry::CommunicationInterface::registerNetworkEndpoint("pattern_editor/set_code", [](const nlohmann::json &data) -> nlohmann::json {
            auto code = data["code"].get<std::string>();

            EventManager::post<RequestSetPatternLanguageCode>(code);

            return { };
        });

        ContentRegistry::CommunicationInterface::registerNetworkEndpoint("imhex/capabilities", [](const nlohmann::json &) -> nlohmann::json {
            nlohmann::json result;

            result["build"] = {
                { "version",    IMHEX_VERSION           },
                { "commit",     GIT_COMMIT_HASH_LONG    },
                { "branch",     GIT_BRANCH              },
            };

            std::vector<std::string> commands;
            for (const auto&[command, callback] : ContentRegistry::CommunicationInterface::impl::getNetworkEndpoints())
                commands.emplace_back(command);

            result["commands"] = commands;

            return result;
        });
    }

}