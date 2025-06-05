#include <hex/mcp_integration/command_processor.hpp>
#include <hex/helpers/logger.hpp> // Assuming a logger exists
#include <hex/api/imhex_api.hpp> // Required for ImHex API interactions
#include <hex/providers/provider.hpp> // Required for provider interactions
#include <hex/helpers/utils.hpp> // For formatAddress
#include <hex/api/event.hpp> // For EventManager
#include <vector>
#include <sstream> // For hex string to byte conversion
#include <iomanip> // For byte to hex string conversion


namespace hex {
    namespace mcp {

// Helper function to convert hex string to bytes (can be in an anonymous namespace or static)
namespace {
    std::vector<u8> hexStringToBytes(const std::string& hex) {
        std::vector<u8> bytes;
        if (hex.length() % 2 != 0 && hex.length() > 0) { // Handle odd length hex strings by warning, but still processing
             Logger::warning("MCP: hexStringToBytes received odd-length hex string: '%s'", hex.c_str());
        }
        for (unsigned int i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            if (byteString.length() == 1) { // Should only happen if original string was odd and this is the last byte
                byteString = "0" + byteString; // Pad
            }
            // Check for non-hex characters
            if (byteString.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                Logger::error("MCP: hexStringToBytes found non-hex characters in substring: '%s'", byteString.c_str());
                return {}; // Return empty vector on error
            }
            u8 byte = static_cast<u8>(strtol(byteString.c_str(), nullptr, 16));
            bytes.push_back(byte);
        }
        return bytes;
    }

    std::string bytesToHexString(const u8* data, size_t len) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i) {
            ss << std::setw(2) << static_cast<unsigned>(data[i]);
        }
        return ss.str();
    }
}

        CommandProcessor::CommandProcessor() {
            registerCommandHandler("search", [this](const json& params) { return this->handleSearch(params); });
            registerCommandHandler("get_offset_info", [this](const json& params) { return this->handleGetOffsetInfo(params); });
            registerCommandHandler("read_bytes", [this](const json& params) { return this->handleReadBytes(params); });
            registerCommandHandler("write_bytes", [this](const json& params) { return this->handleWriteBytes(params); });
            registerCommandHandler("get_selection", [this](const json& params) { return this->handleGetSelection(params); });
            registerCommandHandler("set_selection", [this](const json& params) { return this->handleSetSelection(params); });
        }
        CommandProcessor::~CommandProcessor() {}

        void CommandProcessor::registerCommandHandler(const std::string& methodName, CommandHandler handler) {
            if (m_commandHandlers.find(methodName) != m_commandHandlers.end()) {
                // Log warning: handler already registered
                Logger::warning("MCP: Command handler for '%s' already registered. Overwriting.", methodName.c_str());
            }
            m_commandHandlers[methodName] = handler;
            Logger::info("MCP: Registered command handler for '%s'", methodName.c_str());
        }

        std::string CommandProcessor::processCommand(const std::string& commandJson) {
            json response;
            int messageId = -1;

            try {
                json request = json::parse(commandJson);

                if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
                    return json({
                        {"jsonrpc", "2.0"},
                        {"error", {
                            {"code", -32600},
                            {"message", "Invalid Request: JSON-RPC version is missing or not '2.0'"}
                        }},
                        {"id", request.contains("id") ? request["id"] : nullptr}
                    }).dump(4);
                }

                messageId = request.contains("id") ? request["id"].get<int>() : -1;

                if (!request.contains("method") || !request["method"].is_string()) {
                     response = handleError("Invalid Request: 'method' is missing or not a string.", messageId, -32600);
                     return response.dump(4);
                }

                std::string methodName = request["method"].get<std::string>();

                if (m_commandHandlers.count(methodName)) {
                    json params = request.contains("params") ? request["params"] : json::object();
                    json result = m_commandHandlers[methodName](params);
                    response = {
                        {"jsonrpc", "2.0"},
                        {"result", result},
                        {"id", messageId}
                    };
                } else {
                    response = handleError("Method not found: " + methodName, messageId, -32601);
                }
            } catch (const json::parse_error& e) {
                // Log error
                Logger::error("MCP: JSON parse error: %s", e.what());
                response = handleError("Parse error: " + std::string(e.what()), messageId, -32700);
            } catch (const std::exception& e) {
                // Log error
                Logger::error("MCP: Exception during command processing: %s", e.what());
                response = handleError("Internal error: " + std::string(e.what()), messageId, -32603);
            } catch (...) {
                // Log error
                Logger::error("MCP: Unknown exception during command processing.");
                response = handleError("Internal error: Unknown exception occurred.", messageId, -32603);
            }

            return response.dump(4);
        }

        json CommandProcessor::handleError(const std::string& message, int id, int code) {
            return {
                {"jsonrpc", "2.0"},
                {"error", {
                    {"code", code},
                    {"message", message}
                }},
                {"id", id == -1 ? nullptr : json(id)}
            };
        }

        // Implement the handler methods
        json CommandProcessor::handleSearch(const json& params) {
            if (!params.contains("pattern") || !params["pattern"].is_string()) {
                return handleError("Missing or invalid 'pattern' parameter", -1, -32602);
            }
            std::string pattern_str = params["pattern"].get<std::string>();
            std::vector<u8> pattern_bytes = hexStringToBytes(pattern_str);

            if (pattern_bytes.empty() && !pattern_str.empty()) {
                return handleError("Invalid hex string for pattern", -1, -32602);
            }
            if (pattern_bytes.empty() && pattern_str.empty()) {
                 return handleError("Pattern cannot be empty", -1, -32602);
            }

            prv::Provider* provider = ImHexApi::Provider::get();
            if (!provider) {
                return handleError("No active data provider", -1, -32001);
            }

            u64 provider_size = provider->getSize();
            u64 start_offset = params.value("start_offset", static_cast<u64>(0));
            u64 end_offset = params.value("end_offset", provider_size);


            if (start_offset > provider_size) {
                 return handleError("'start_offset' is out of bounds.", -1, -32602);
            }
            if (end_offset > provider_size) { // Allow end_offset to be provider_size for inclusive search up to end
                end_offset = provider_size;
            }
            if (start_offset > end_offset) {
                 return handleError("'start_offset' cannot be greater than 'end_offset'.", -1, -32602);
            }


            std::vector<u64> found_offsets;

            // Ensure search does not go out of bounds, especially pattern_bytes.size()
            u64 searchable_end = end_offset;
            if (pattern_bytes.size() > 0) {
                 searchable_end = (end_offset >= pattern_bytes.size()) ? (end_offset - pattern_bytes.size() + 1) : 0;
            }


            for (u64 offset = start_offset; offset < searchable_end; ++offset) {
                 if (offset + pattern_bytes.size() > provider_size) break; // Final check

                std::vector<u8> data_chunk(pattern_bytes.size());
                // It's possible read returns false if offset is valid but read fails for other reasons.
                if (!provider->read(offset, data_chunk.data(), data_chunk.size())) {
                    Logger::error("MCP: Failed to read data at offset %s during search.", hex::formatAddress(offset).c_str());
                    // Decide if we should stop or continue. For now, continue.
                    // Potentially, this could be an error returned to the client.
                    continue;
                }
                if (data_chunk == pattern_bytes) {
                    found_offsets.push_back(offset);
                }
            }

            return { {"offsets", found_offsets} };
        }

        json CommandProcessor::handleGetOffsetInfo(const json& params) {
            if (!params.contains("offset") || !params["offset"].is_number_unsigned()) {
                return handleError("Missing or invalid 'offset' parameter", -1, -32602);
            }
            u64 offset = params["offset"].get<u64>();

            prv::Provider* provider = ImHexApi::Provider::get();
            if (provider && offset >= provider->getSize()) {
                 return handleError("'offset' is out of bounds.", -1, -32602);
            }

            return {
                {"offset", offset},
                {"address_str", hex::formatAddress(offset)}
            };
        }

        json CommandProcessor::handleReadBytes(const json& params) {
            if (!params.contains("offset") || !params["offset"].is_number_unsigned()) {
                return handleError("Missing or invalid 'offset' parameter", -1, -32602);
            }
            if (!params.contains("count") || !params["count"].is_number_unsigned()) {
                return handleError("Missing or invalid 'count' parameter", -1, -32602);
            }

            u64 offset = params["offset"].get<u64>();
            size_t count = params["count"].get<size_t>();

            if (count == 0) {
                return handleError("'count' must be greater than zero.", -1, -32602);
            }

            prv::Provider* provider = ImHexApi::Provider::get();
            if (!provider) {
                return handleError("No active data provider", -1, -32001);
            }

            if (offset >= provider->getSize()) {
                 return handleError("'offset' is out of bounds.", -1, -32602);
            }
            if (offset + count > provider->getSize()) {
                 // Read only up to the end of the provider
                count = provider->getSize() - offset;
                Logger::warning("MCP: Read request for %zu bytes at offset %s exceeds data size. Reading %zu bytes instead.", params["count"].get<size_t>(), hex::formatAddress(offset).c_str(), count);
                 if (count == 0) { // If offset is exactly provider size, nothing to read.
                    return {
                        {"offset", offset},
                        {"count", 0},
                        {"hex_data", ""},
                        {"bytes_read", 0}
                    };
                 }
            }

            std::vector<u8> buffer(count);
            size_t bytes_read = provider->read(offset, buffer.data(), count);
            // read() returns bytes read, not a boolean for success in the way it was assumed.
            // It might return less than `count` if end of file is reached.
            if (bytes_read == 0 && count > 0) { // If we expected to read bytes but got none (and it wasn't an EOF scenario handled by shrinking count)
                 return handleError("Failed to read bytes from provider. Read 0 bytes.", -1, -32002);
            }

            // If bytes_read is less than count, we should resize the buffer before converting to hex
            if (bytes_read < count) {
                buffer.resize(bytes_read);
            }

            return {
                {"offset", offset},
                {"count", params["count"].get<size_t>()}, // Report original requested count
                {"bytes_read", bytes_read},
                {"hex_data", bytesToHexString(buffer.data(), buffer.size())}
            };
        }

        json CommandProcessor::handleWriteBytes(const json& params) {
            if (!params.contains("offset") || !params["offset"].is_number_unsigned()) {
                return handleError("Missing or invalid 'offset' parameter", -1, -32602);
            }
            if (!params.contains("data") || !params["data"].is_string()) {
                return handleError("Missing or invalid 'data' (hex string) parameter", -1, -32602);
            }

            u64 offset = params["offset"].get<u64>();
            std::string hex_data = params["data"].get<std::string>();
            std::vector<u8> bytes_to_write = hexStringToBytes(hex_data);

            if (bytes_to_write.empty() && !hex_data.empty()) {
                return handleError("Invalid or empty hex data string provided", -1, -32602);
            }
             if (bytes_to_write.empty() && hex_data.empty()) {
                return handleError("No data provided to write", -1, -32602);
            }

            prv::Provider* provider = ImHexApi::Provider::get();
            if (!provider) {
                return handleError("No active data provider", -1, -32001);
            }

            if (!provider->isWritable()) {
                return handleError("Provider is not writable", -1, -32003);
            }

            if (offset > provider->getSize()) { // Allow writing at exact end to extend for some providers.
                 return handleError("'offset' is out of bounds for writing.", -1, -32602);
            }
            // This check might be too restrictive if provider supports extending.
            // For now, if offset + size > provider_size, it's an error unless provider can extend.
            // The prv::Provider::write function should handle this logic.
            // if (offset + bytes_to_write.size() > provider->getSize()) {
            //    return handleError("Write request exceeds data size and auto-extension not implemented or failed", -1, -32004);
            // }

            if (!provider->write(offset, bytes_to_write.data(), bytes_to_write.size())) {
                // This could be due to various reasons: out of space, offset invalid for underlying storage, etc.
                return handleError("Failed to write bytes to provider. Check provider size and offset.", -1, -32004);
            }

            ImHexApi::Provider::markDirty();
            EventManager::post<EventProviderDataChanged>();

            return {
                {"offset", offset},
                {"bytes_written", bytes_to_write.size()},
                {"status", "success"}
            };
        }

        json CommandProcessor::handleGetSelection(const json& params) {
            auto selection = ImHexApi::HexEditor::getSelection();
            if (selection.has_value()) {
                return {
                    {"start_offset", selection->getBaseAddress()},
                    {"size", selection->getSize()},
                    {"end_offset", selection->getBaseAddress() + selection->getSize() -1}
                };
            } else {
                // Return nulls or specific values indicating no selection, rather than an error
                // This is a valid state, not necessarily an error condition.
                return {
                    {"start_offset", nullptr},
                    {"size", 0},
                    {"end_offset", nullptr}
                };
            }
        }

        json CommandProcessor::handleSetSelection(const json& params) {
            if (!params.contains("start_offset") || !params["start_offset"].is_number_unsigned()) {
                return handleError("Missing or invalid 'start_offset' parameter", -1, -32602);
            }
            // Size can be 0 to clear selection, but must be present.
            if (!params.contains("size") || !params["size"].is_number_unsigned()) {
                return handleError("Missing or invalid 'size' parameter", -1, -32602);
            }

            u64 start_offset = params["start_offset"].get<u64>();
            size_t size = params["size"].get<size_t>();

            prv::Provider* provider = ImHexApi::Provider::get();
            if (provider) { // Only validate against provider if one is loaded
                if (start_offset > provider->getSize()) {
                     return handleError("'start_offset' exceeds data size.", -1, -32602);
                }
                if (size > 0 && start_offset + size > provider->getSize()) {
                    // If size is 0, start_offset can be provider->getSize() (caret at end)
                    // If size > 0, selection must be within bounds.
                     return handleError("Selection (start_offset + size) exceeds data size.", -1, -32602);
                }
            } else if (size > 0) { // If no provider, selection with size > 0 is not very meaningful
                 Logger::warning("MCP: Setting selection with no active provider.");
            }


            ImHexApi::HexEditor::setSelection(start_offset, size);

            return {
                {"status", "success"},
                {"start_offset", start_offset},
                {"size", size}
            };
        }

    }
}
