#include <hex/mcp/client.hpp>
#include <hex/mcp/server.hpp>

#include <hex.hpp>

#include <string>
#include <cstdlib>

#include <fmt/format.h>
#include <hex/helpers/logger.hpp>
#include <nlohmann/json.hpp>
#include <wolv/net/socket_client.hpp>

namespace hex::mcp {

    int Client::run(std::istream &input, std::ostream &output) {
        wolv::net::SocketClient client(wolv::net::SocketClient::Type::TCP, true);
        client.connect("127.0.0.1", Server::McpInternalPort);

        if (!client.isConnected()) {
            log::resumeLogging();
            log::error("Cannot connect to ImHex. Do you have an instance running and is the MCP server enabled?");
            return EXIT_FAILURE;
        }

        while (true) {
            std::string request;
            std::getline(input, request);

            client.writeString(request);
            auto response = client.readString();
            if (!response.empty() && response.front() != 0x00)
                output << response << '\n';
        }

        return EXIT_SUCCESS;
    }
}
