#include "views/view_tty_console.hpp"

#include <imgui_imhex_extensions.h>
#include <imgui_internal.h>

namespace hex {

    ViewTTYConsole::ViewTTYConsole() : View("hex.windows.view.tty_console.name") {
        this->m_comPorts = getAvailablePorts();
        this->m_transmitDataBuffer.resize(0xFFF, 0x00);
        this->m_receiveDataBuffer.reserve(0xFFF);
        this->m_receiveDataBuffer.push_back(0x00);
    }

    ViewTTYConsole::~ViewTTYConsole() {

    }

    void ViewTTYConsole::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.windows.view.tty_console.name").c_str(), &this->getWindowOpenState())) {

            ImGui::TextUnformatted("Configuration");
            ImGui::Separator();

            bool connected = this->m_portHandle != INVALID_HANDLE_VALUE;

            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, connected);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, connected ? 0.5F : 1.0F);

            ImGui::Combo("Port", &this->m_selectedPort, [](void* data, int idx, const char** out_text) -> bool {
                auto &ports = *static_cast<std::vector<std::pair<std::string, std::string>>*>(data);

                *out_text = ports[idx].first.c_str();
                return true;
            }, &this->m_comPorts, this->m_comPorts.size());

            ImGui::SameLine();
            if (ImGui::Button("Reload"))
                this->m_comPorts = getAvailablePorts();

            ImGui::Combo("Baud rate", &this->m_selectedBaudRate, [](void* data, int idx, const char** out_text) -> bool {
                *out_text = ViewTTYConsole::BaudRates[idx];
                return true;
            }, nullptr, ViewTTYConsole::BaudRates.size());

            ImGui::Combo("Data bits", &this->m_selectedNumBits, [](void* data, int idx, const char** out_text) -> bool {
                *out_text = ViewTTYConsole::NumBits[idx];
                return true;
            }, nullptr, ViewTTYConsole::NumBits.size());

            ImGui::Combo("Stop bits", &this->m_selectedStopBits, [](void* data, int idx, const char** out_text) -> bool {
                *out_text = ViewTTYConsole::StopBits[idx];
                return true;
            }, nullptr, ViewTTYConsole::StopBits.size());

            ImGui::Combo("Parity bit", &this->m_selectedParityBits, [](void* data, int idx, const char** out_text) -> bool {
                *out_text = ViewTTYConsole::ParityBits[idx];
                return true;
            }, nullptr, ViewTTYConsole::ParityBits.size());

            ImGui::Checkbox("CTS Flow control", &this->m_hasCTSFlowControl);

            ImGui::PopStyleVar();
            ImGui::PopItemFlag();

            ImGui::NewLine();

            if (this->m_portHandle == INVALID_HANDLE_VALUE) {
                if (ImGui::Button("Connect"))
                    if (!this->connect())
                        log::error("Connect error");
            } else {
                if (ImGui::Button("Disconnect"))
                    this->disconnect();
            }

            ImGui::NewLine();

            if (ImGui::Button("Clear")) {
                std::scoped_lock lock(this->m_receiveBufferMutex);

                this->m_receiveDataBuffer.clear();
                this->m_wrapPositions.clear();
            }

            ImGui::SameLine();

            ImGui::Checkbox("Auto Scroll", &this->m_shouldAutoScroll);

            ImGui::NewLine();
            ImGui::TextUnformatted("Console");
            ImGui::Separator();

            auto consoleSize = ImGui::GetContentRegionAvail();
            consoleSize.y -= ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 4;
            if (ImGui::BeginChild("##scrolling", consoleSize, true, ImGuiWindowFlags_HorizontalScrollbar)) {
                ImGuiListClipper clipper;
                clipper.Begin(this->m_wrapPositions.size(), ImGui::GetTextLineHeight());

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
                while (clipper.Step()) {
                    std::scoped_lock lock(this->m_receiveBufferMutex);

                    for (u32 i = clipper.DisplayStart + 1; i < clipper.DisplayEnd; i++) {
                        ImGui::TextUnformatted(this->m_receiveDataBuffer.data() + this->m_wrapPositions[i - 1], this->m_receiveDataBuffer.data() + this->m_wrapPositions[i]);
                    }

                    if (!this->m_receiveDataBuffer.empty() && !this->m_wrapPositions.empty())
                        if (clipper.DisplayEnd >= this->m_wrapPositions.size() - 1)
                            ImGui::TextUnformatted(this->m_receiveDataBuffer.data() + this->m_wrapPositions.back());

                    if (this->m_shouldAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                        ImGui::SetScrollHereY(0.0F);
                    }
                }
                ImGui::PopStyleVar();

                ImGui::EndChild();
            }

            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##transmit", this->m_transmitDataBuffer.data(), this->m_transmitDataBuffer.size() - 2, ImGuiInputTextFlags_EnterReturnsTrue)) {
                auto size = strlen(this->m_transmitDataBuffer.data());

                this->m_transmitDataBuffer[size + 0] = '\n';
                this->m_transmitDataBuffer[size + 1] = 0x00;

                this->transmitData(this->m_transmitDataBuffer);
                ImGui::SetKeyboardFocusHere(0);
            }
            ImGui::PopItemWidth();

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered() && this->m_portHandle != INVALID_HANDLE_VALUE && !this->m_transmitting)
                ImGui::OpenPopup("ConsoleMenu");

            if (ImGui::BeginPopup("ConsoleMenu")) {

                static std::vector<char> buffer(2, 0x00);
                if (ImGui::MenuItem("Send ETX", "CTRL + C")) {
                    buffer[0] = 0x03;
                    this->transmitData(buffer);
                }
                if (ImGui::MenuItem("Send EOT", "CTRL + D")) {
                    buffer[0] = 0x04;
                    this->transmitData(buffer);
                }
                if (ImGui::MenuItem("Send SUB", "CTRL + Z")) {
                    buffer[0] = 0x1A;
                    this->transmitData(buffer);
                }

                ImGui::EndPopup();
            }

        }
        ImGui::End();
    }

    void ViewTTYConsole::drawMenu() {

    }

    std::vector<std::pair<std::string, std::string>> ViewTTYConsole::getAvailablePorts() {
        std::vector<std::pair<std::string, std::string>> result;
        std::vector<char> buffer(0xFFF, 0x00);

        for (u16 portNumber = 0; portNumber <= 255; portNumber++) {
            std::string port = "COM" + std::to_string(portNumber);

            if (::QueryDosDevice(port.c_str(), buffer.data(), buffer.size()) != 0) {
                result.emplace_back(port, buffer.data());
            }
        }

        return result;
    }

    bool ViewTTYConsole::connect() {
        this->m_portHandle = ::CreateFile(("\\\\.\\" + this->m_comPorts[this->m_selectedPort].first).c_str(),
                                        GENERIC_READ | GENERIC_WRITE,
                                        0,
                                        nullptr,
                                        OPEN_EXISTING,
                                        FILE_FLAG_OVERLAPPED,
                                        nullptr);

        if (this->m_portHandle == INVALID_HANDLE_VALUE)
            return false;

        auto closeHandle = SCOPE_GUARD { CloseHandle(this->m_portHandle); };

        DCB serialParams;
        serialParams.DCBlength = sizeof(DCB);

        if (!::GetCommState(this->m_portHandle, &serialParams))
            return false;

        serialParams.BaudRate = std::stoi(ViewTTYConsole::BaudRates[this->m_selectedBaudRate]);
        serialParams.ByteSize = std::stoi(ViewTTYConsole::NumBits[this->m_selectedNumBits]);
        serialParams.StopBits = this->m_selectedStopBits;
        serialParams.Parity   = this->m_selectedParityBits;
        serialParams.fOutxCtsFlow = this->m_hasCTSFlowControl;

        if (!::SetCommState(this->m_portHandle, &serialParams))
            return false;

        COMMTIMEOUTS timeouts;
        timeouts.ReadIntervalTimeout = 500;
        timeouts.ReadTotalTimeoutConstant = 500;
        timeouts.ReadTotalTimeoutMultiplier = 100;
        timeouts.WriteTotalTimeoutConstant = 500;
        timeouts.WriteTotalTimeoutMultiplier = 100;

        if (!::SetCommTimeouts(this->m_portHandle, &timeouts))
            return false;

        if(!::SetupComm(this->m_portHandle, 10000, 10000))
            return false;

        closeHandle.release();

        this->m_receiveThread = std::jthread([this](const std::stop_token &token) {
            bool waitingOnRead = false;
            OVERLAPPED overlapped = { 0 };

            overlapped.hEvent = ::CreateEvent(nullptr, true, false, nullptr);
            ON_SCOPE_EXIT { ::CloseHandle(&overlapped); };

            auto addByte = [this](char byte) {
                std::scoped_lock lock(this->m_receiveBufferMutex);

                if (byte >= 0x20 && byte <= 0x7E) {
                    this->m_receiveDataBuffer.back() = byte;
                    this->m_receiveDataBuffer.push_back(0x00);
                } else if (byte == '\n' || byte == '\r') {
                    if (this->m_receiveDataBuffer.empty())
                        return;

                    u32 wrapPos = this->m_receiveDataBuffer.size() - 1;

                    if (this->m_wrapPositions.empty() || this->m_wrapPositions.back() != wrapPos)
                        this->m_wrapPositions.push_back(wrapPos);
                }
            };

            while (!token.stop_requested()) {
               DWORD bytesRead = 0;

               char byte = 0;
               if (!waitingOnRead) {
                   if (::ReadFile(this->m_portHandle, &byte, sizeof(char), &bytesRead, &overlapped)) {
                       addByte(byte);
                   } else if (::GetLastError() == ERROR_IO_PENDING) {
                       waitingOnRead = true;
                   }
               } else {
                   byte = 0;
                   auto res = ::WaitForSingleObject(overlapped.hEvent, 500);
                   switch (res) {
                       case WAIT_OBJECT_0:
                           if (::GetOverlappedResult(this->m_portHandle, &overlapped, &bytesRead, false)) {
                               addByte(byte);
                               waitingOnRead = false;
                           }
                       default: break;
                   }
               }
            }
        });

        return true;
    }

    bool ViewTTYConsole::disconnect() {
        ::SetCommMask(this->m_portHandle, EV_TXEMPTY);
        this->m_receiveThread.request_stop();
        this->m_receiveThread.join();

        ::CloseHandle(this->m_portHandle);
        this->m_portHandle = INVALID_HANDLE_VALUE;

        return true;
    }

    void ViewTTYConsole::transmitData(std::vector<char> &data) {
        if (this->m_transmitting)
            return;

        auto transmitThread = std::thread([&, this]{
            OVERLAPPED overlapped = { 0 };

            overlapped.hEvent = ::CreateEvent(nullptr, true, false, nullptr);
            ON_SCOPE_EXIT { ::CloseHandle(&overlapped); };

            this->m_transmitting = true;

            DWORD bytesWritten = 0;
            if (!::WriteFile(this->m_portHandle, data.data(), strlen(data.data()), &bytesWritten, &overlapped)) {
                if (::GetLastError() == ERROR_IO_PENDING) {
                    ::GetOverlappedResult(this->m_portHandle, &overlapped, &bytesWritten, true);
                }
            }

            if (bytesWritten > 0)
                data[0] = 0x00;

            this->m_transmitting = false;
        });
        transmitThread.detach();
    }

}