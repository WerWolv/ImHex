#define CATCH_CONFIG_MAIN // Provides main() for Catch2 in this test file
#include <catch2/catch_all.hpp>
#include <hex/mcp_integration/command_processor.hpp> // For direct use if needed
#include <hex/plugins/mcp_integration/mcp_server.hpp>
#include <asio.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

// Helper to create a command JSON string
std::string createJsonCommand(const std::string& method, const nlohmann::json& params, int id = 1) {
    nlohmann::json cmd = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"id", id}
    };
    if (!params.is_null()) {
        cmd["params"] = params;
    }
    return cmd.dump();
}

class MCPTestClient {
public:
    MCPTestClient(asio::io_context& ioc) : m_ioc(ioc), m_socket(ioc) {}

    bool connect(const std::string& host, unsigned short port) {
        asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(host), port);
        asio::error_code ec;
        m_socket.connect(endpoint, ec);
        if (ec) {
            Catch:: μορφή("MCPTestClient: Connection failed: {}", ec.message());
        }
        return !ec;
    }

    void send(const std::string& message) {
        asio::error_code ec;
        asio::write(m_socket, asio::buffer(message + "\n"), ec); // Append newline
        if (ec) throw std::runtime_error("Send failed: " + ec.message());
    }

    std::string receive() {
        asio::streambuf response_buf;
        asio::error_code ec;
        asio::read_until(m_socket, response_buf, "\n", ec);
        if (ec && ec != asio::error::eof) throw std::runtime_error("Receive failed: " + ec.message());

        std::istream is(&response_buf);
        std::string response_line;
        std::getline(is, response_line); // Read up to newline
        return response_line;
    }

    void close() {
        asio::error_code ec;
        if (m_socket.is_open()) {
            m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            // Ignore not_connected error if already closed by server
            if (ec && ec != asio::error::not_connected) {
                 Catch:: μορφή("MCPTestClient: Shutdown failed: {}", ec.message());
            }
            m_socket.close(ec);
            if (ec) {
                 Catch:: μορφή("MCPTestClient: Close failed: {}", ec.message());
            }
        }
    }

private:
    asio::io_context& m_ioc;
    asio::ip::tcp::socket m_socket;
};


// Global command processor for the server instance used in tests
static hex::mcp::CommandProcessor g_test_command_processor;


TEST_CASE("MCP Server Integration Tests", "[mcp_server]") {
    const unsigned short TEST_PORT = 61101; // Use a specific port for testing
    hex::plugins::mcp::MCPServer server(g_test_command_processor);

    // Ensure server is stopped initially for clean test runs
    if (server.is_running()) {
        server.stop();
    }
    // Wait a bit for the port to be fully released if it was just stopped
    // This is OS-dependent and might not be strictly necessary with SO_REUSEADDR
    std::this_thread::sleep_for(std::chrono::milliseconds(250));


    SECTION("Server Start and Stop") {
        REQUIRE_FALSE(server.is_running());
        REQUIRE(server.start(TEST_PORT));
        // Give the server thread a moment to actually start listening
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        REQUIRE(server.is_running());
        REQUIRE(server.get_port() == TEST_PORT);
        server.stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Give time for stop
        REQUIRE_FALSE(server.is_running());
    }

    SECTION("Client Connect, Send Command, Receive Response") {
        REQUIRE(server.start(TEST_PORT));
        REQUIRE(server.is_running());
         std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Ensure server is up

        asio::io_context client_ioc;
        MCPTestClient client(client_ioc);

        REQUIRE(client.connect("127.0.0.1", TEST_PORT));

        std::string cmd_str = createJsonCommand("get_selection", {}, 101);
        client.send(cmd_str);
        std::string response_str = client.receive();

        nlohmann::json response_json = nlohmann::json::parse(response_str);
        REQUIRE(response_json["jsonrpc"] == "2.0");
        REQUIRE(response_json["id"] == 101);
        // Based on current implementation, get_selection returns nulls for no selection.
        REQUIRE(response_json.contains("result"));
        REQUIRE(response_json["result"]["start_offset"].is_null());
        REQUIRE(response_json["result"]["size"] == 0);


        client.close();
        server.stop();
    }

    SECTION("Client Sends Malformed JSON") {
        REQUIRE(server.start(TEST_PORT));
        REQUIRE(server.is_running());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        asio::io_context client_ioc;
        MCPTestClient client(client_ioc);
        REQUIRE(client.connect("127.0.0.1", TEST_PORT));

        client.send("this is not json");
        std::string response_str = client.receive();

        nlohmann::json response_json = nlohmann::json::parse(response_str);
        REQUIRE(response_json["jsonrpc"] == "2.0");
        REQUIRE(response_json.contains("error"));
        REQUIRE(response_json["error"]["code"] == -32700); // Parse error
        REQUIRE(response_json["id"] == -1); // Default ID for parse error in CommandProcessor

        client.close();
        server.stop();
    }

    SECTION("Client Sends Unknown Method") {
        REQUIRE(server.start(TEST_PORT));
        REQUIRE(server.is_running());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        asio::io_context client_ioc;
        MCPTestClient client(client_ioc);
        REQUIRE(client.connect("127.0.0.1", TEST_PORT));

        std::string cmd_str = createJsonCommand("unknown_method_test", {}, 102);
        client.send(cmd_str);
        std::string response_str = client.receive();

        nlohmann::json response_json = nlohmann::json::parse(response_str);
        REQUIRE(response_json["jsonrpc"] == "2.0");
        REQUIRE(response_json["id"] == 102);
        REQUIRE(response_json.contains("error"));
        REQUIRE(response_json["error"]["code"] == -32601); // Method not found

        client.close();
        server.stop();
    }

    SECTION("Multiple Commands on Same Connection") {
        REQUIRE(server.start(TEST_PORT));
        REQUIRE(server.is_running());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        asio::io_context client_ioc;
        MCPTestClient client(client_ioc);
        REQUIRE(client.connect("127.0.0.1", TEST_PORT));

        nlohmann::json sel_params = {{"start_offset", 10}, {"size", 5}};
        client.send(createJsonCommand("set_selection", sel_params, 1));
        nlohmann::json r1 = nlohmann::json::parse(client.receive());
        REQUIRE(r1["id"] == 1);
        REQUIRE(r1.contains("result"));
        REQUIRE(r1["result"]["status"] == "success");

        client.send(createJsonCommand("get_selection", {}, 2));
        nlohmann::json r2 = nlohmann::json::parse(client.receive());
        REQUIRE(r2["id"] == 2);
        REQUIRE(r2.contains("result"));
        REQUIRE(r2["result"]["start_offset"] == 10);
        REQUIRE(r2["result"]["size"] == 5);

        client.close();
        server.stop();
    }

    // Teardown: Ensure server is stopped after tests in this case.
    // This is automatically handled by RAII if server object goes out of scope
    // but explicit stop is good for clarity and immediate cleanup.
    if (server.is_running()) {
        server.stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Allow time for resources to free
    }
}
