#include <hex/mcp/client.hpp>
#include <hex/mcp/server.hpp>

#include <hex.hpp>

#include <string>
#include <cstdlib>

#include <fmt/format.h>
#include <hex/api/imhex_api/system.hpp>
#include <hex/helpers/logger.hpp>
#include <nlohmann/json.hpp>
#include <wolv/literals.hpp>
#include <wolv/net/socket_client.hpp>

namespace hex::mcp {

    using namespace wolv::literals;

    int Client::run(std::istream &input, std::ostream &output) {
        wolv::net::SocketClient client(wolv::net::SocketClient::Type::TCP, true);
        client.connect("127.0.0.1", Server::McpInternalPort);

        while (true) {
            std::string request;
            std::getline(input, request);

            if (ImHexApi::System::isMainInstance()) {
                JsonRpc response(request);
                response.setError(JsonRpc::ErrorCode::InternalError, "No other instance of ImHex is running. Make sure that you have ImHex open already.");
                output << response.execute([](auto, auto){ return nlohmann::json::object(); }).value_or("") << '\n';
                continue;
            }

            client.writeString(request);
            auto response = client.readString(4_MiB);
            if (!response.empty() && response.front() != 0x00)
                output << response << '\n';

            if (!client.isConnected())
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return EXIT_SUCCESS;
    }
}
