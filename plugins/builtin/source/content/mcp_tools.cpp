#include <content/providers/file_provider.hpp>
#include <hex/api/content_registry/communication_interface.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/helpers/crypto.hpp>
#include <romfs/romfs.hpp>
#include <wolv/literals.hpp>

namespace hex::plugin::builtin {

    using namespace wolv::literals;

    void registerMCPTools() {
        ContentRegistry::MCP::registerTool(romfs::get("mcp/tools/open_file.json").string(), [](const nlohmann::json &data) -> nlohmann::json {
            auto filePath = data.at("file_path").get<std::string>();

            auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.file", true);
            if (auto *fileProvider = dynamic_cast<FileProvider*>(provider.get()); fileProvider != nullptr) {
                fileProvider->setPath(filePath);

                ImHexApi::Provider::openProvider(provider);
            }

            nlohmann::json result = {
                { "handle", provider->getID() },
                { "name", provider->getName() },
                { "type", provider->getTypeName().get() },
                { "size", provider->getSize() },
                { "is_writable", provider->isWritable() }
            };

            return mcp::StructuredContent {
                .text = result.dump(),
                .data = result
            };
        });

        ContentRegistry::MCP::registerTool(romfs::get("mcp/tools/list_open_data_sources.json").string(), [](const nlohmann::json &) -> nlohmann::json {
            const auto &providers = ImHexApi::Provider::getProviders();
            nlohmann::json array = nlohmann::json::array();
            for (const auto &provider : providers) {
                nlohmann::json providerInfo;
                providerInfo["name"] = provider->getName();
                providerInfo["type"] = provider->getTypeName().get();
                providerInfo["size"] = provider->getSize();
                providerInfo["is_writable"] = provider->isWritable();
                providerInfo["handle"] = provider->getID();

                array.push_back(providerInfo);
            }

            nlohmann::json result;
            result["data_sources"] = array;

            return mcp::StructuredContent {
                .text = result.dump(),
                .data = result
            };
        });

        ContentRegistry::MCP::registerTool(romfs::get("mcp/tools/select_data_source.json").string(), [](const nlohmann::json &data) -> nlohmann::json {
            const auto &providers = ImHexApi::Provider::getProviders();
            auto handle = data.at("handle").get<u64>();

            for (size_t i = 0; i < providers.size(); i++) {
                if (providers[i]->getID() == handle) {
                    ImHexApi::Provider::setCurrentProvider(static_cast<i64>(i));
                    break;
                }
            }

            nlohmann::json result = {
                { "selected_handle", ImHexApi::Provider::get()->getID() }
            };
            return mcp::StructuredContent {
                .text = result.dump(),
                .data = result
            };
        });

        ContentRegistry::MCP::registerTool(romfs::get("mcp/tools/read_data.json").string(), [](const nlohmann::json &data) -> nlohmann::json {
            auto address = data.at("address").get<u64>();
            auto size = data.at("size").get<u64>();

            size = std::min<size_t>(size, 16_MiB);

            auto provider = ImHexApi::Provider::get();
            std::vector<u8> buffer(std::min(size, provider->getActualSize() - address));
            provider->read(address, buffer.data(), buffer.size());

            auto base64 = crypt::encode64(buffer);

            nlohmann::json result = {
                { "handle", provider->getID() },
                { "data", std::string(base64.begin(), base64.end()) },
                { "data_size", buffer.size() }
            };
            return mcp::StructuredContent {
                .text = result.dump(),
                .data = result
            };
        });
    }

}
