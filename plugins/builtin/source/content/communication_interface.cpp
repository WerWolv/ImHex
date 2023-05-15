#include <hex/api/content_registry.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    void registerNetworkEndpoints() {
        ContentRegistry::CommunicationInterface::registerNetworkEndpoint("pattern_editor/set_code", [](const nlohmann::json &data) -> nlohmann::json {
            auto code = data["code"].get<std::string>();

            EventManager::post<RequestSetPatternLanguageCode>(code);

            return { };
        });
    }

}