#include <hex/api/content_registry.hpp>
#include <hex/api/event_manager.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    void registerNetworkEndpoints() {
        ContentRegistry::CommunicationInterface::registerNetworkEndpoint("pattern_editor/set_code", [](const nlohmann::json &data) -> nlohmann::json {
            auto code = data.at("code").get<std::string>();

            RequestSetPatternLanguageCode::post(code);

            return { };
        });

        ContentRegistry::CommunicationInterface::registerNetworkEndpoint("imhex/capabilities", [](const nlohmann::json &) -> nlohmann::json {
            nlohmann::json result;

            result["build"] = {
                { "version", ImHexApi::System::getImHexVersion().get()   },
                { "commit",  ImHexApi::System::getCommitHash(true) },
                { "branch",  ImHexApi::System::getCommitBranch()   }
            };

            std::vector<std::string> commands;
            for (const auto&[command, callback] : ContentRegistry::CommunicationInterface::impl::getNetworkEndpoints())
                commands.emplace_back(command);

            result["commands"] = commands;

            return result;
        });
    }

}