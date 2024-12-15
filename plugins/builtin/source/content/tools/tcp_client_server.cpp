#include <hex/api/localization_manager.hpp>
#include <hex/helpers/utils.hpp>
#include <wolv/net/socket_client.hpp>
#include <wolv/net/socket_server.hpp>

#include <fonts/vscode_icons.hpp>
#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <jthread.hpp>
#include <string>
#include <vector>

namespace hex::plugin::builtin {

    using namespace std::literals::chrono_literals;

    void drawTCPClientServer() {
        if (ImGui::BeginTabBar("##tcpTransceiver", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("hex.builtin.tools.tcp_client_server.client"_lang)) {
                static wolv::net::SocketClient client;

                static std::string ipAddress;
                static int port;

                static std::vector<std::string> messages;
                static std::string input;
                static std::jthread receiverThread;
                static std::mutex receiverMutex;

                ImGuiExt::Header("hex.builtin.tools.tcp_client_server.settings"_lang, true);

                ImGui::BeginDisabled(client.isConnected());
                {
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.3F);
                    ImGui::InputText("##ipAddress", ipAddress);
                    ImGui::PopItemWidth();
                    ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted(":");
                    ImGui::SameLine(0, 0);
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.2F);
                    ImGui::InputInt("##port", &port, 0, 0);
                    ImGui::PopItemWidth();
                }
                ImGui::EndDisabled();

                ImGui::SameLine();

                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.2F);
                if (client.isConnected()) {
                    if (ImGuiExt::IconButton(ICON_VS_DEBUG_DISCONNECT, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                        client.disconnect();

                        receiverThread.request_stop();
                        receiverThread.join();
                    }
                } else {
                    if (ImGuiExt::IconButton(ICON_VS_PLAY, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen))) {
                        client.connect(ipAddress, port);

                        receiverThread = std::jthread([](const std::stop_token& stopToken) {
                            while (!stopToken.stop_requested()) {
                                auto message = client.readString();
                                if (!message.empty()) {
                                    std::scoped_lock lock(receiverMutex);
                                    messages.push_back(message);
                                }
                            }
                        });
                    }
                }
                ImGui::PopItemWidth();

                if (port < 1)
                    port = 1;
                else if (port > 65535)
                    port = 65535;

                ImGuiExt::Header("hex.builtin.tools.tcp_client_server.messages"_lang);
                if (ImGui::BeginTable("##response", 1, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders, ImVec2(0, 200_scaled))) {
                    {
                        std::scoped_lock lock(receiverMutex);
                        for (const auto &message : messages) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedSelectable("{}", message.c_str());
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::BeginDisabled(!client.isConnected());
                {
                    ImGui::PushItemWidth(-(50_scaled));
                    bool pressedEnter = ImGui::InputText("##input", input, ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::PopItemWidth();
                    ImGui::SameLine();

                    if (pressedEnter)
                        ImGui::SetKeyboardFocusHere(-1);

                    if (ImGuiExt::IconButton(ICON_VS_DEBUG_STACKFRAME, ImGui::GetStyleColorVec4(ImGuiCol_Text)) || pressedEnter) {
                        client.writeString(input);
                        input.clear();
                    }
                }
                ImGui::EndDisabled();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("hex.builtin.tools.tcp_client_server.server"_lang)) {
                static wolv::net::SocketServer server;
                static int port;

                static std::vector<std::string> messages;
                static std::mutex receiverMutex;
                static std::jthread receiverThread;

                ImGuiExt::Header("hex.builtin.tools.tcp_client_server.settings"_lang, true);

                ImGui::BeginDisabled(server.isActive());
                {
                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.2F);
                    ImGui::InputInt("##port", &port, 0, 0);
                    ImGui::PopItemWidth();
                }
                ImGui::EndDisabled();

                ImGui::SameLine();

                if (port < 1)
                    port = 1;
                else if (port > 65535)
                    port = 65535;

                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.2F);
                if (server.isActive()) {
                    if (ImGuiExt::IconButton(ICON_VS_DEBUG_DISCONNECT, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                        server.shutdown();

                        receiverThread.request_stop();
                        receiverThread.join();
                    }
                } else {
                    if (ImGuiExt::IconButton(ICON_VS_PLAY, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen))) {
                        receiverThread = std::jthread([](const std::stop_token& stopToken){
                            server = wolv::net::SocketServer(port);

                            while (!stopToken.stop_requested()) {
                                server.accept([](wolv::net::SocketHandle, const std::vector<u8> &data) -> std::vector<u8> {
                                    std::scoped_lock lock(receiverMutex);

                                    messages.emplace_back(data.begin(), data.end());

                                    return {};
                                }, nullptr, true);

                                std::this_thread::sleep_for(100ms);
                            }
                        });
                    }
                }
                ImGui::PopItemWidth();

                ImGuiExt::Header("hex.builtin.tools.tcp_client_server.messages"_lang);

                if (ImGui::BeginTable("##response", 1, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders, ImVec2(0, 200_scaled))) {
                    {
                        std::scoped_lock lock(receiverMutex);
                        u32 index = 0;
                        for (const auto &message : messages) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::PushID(index);
                            ImGuiExt::TextFormattedSelectable("{}", message.c_str());
                            ImGui::PopID();

                            index += 1;
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

}
