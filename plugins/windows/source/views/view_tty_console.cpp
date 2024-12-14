#include "views/view_tty_console.hpp"

#include <imgui_internal.h>

#include <hex/api/task_manager.hpp>
#include <hex/helpers/utils.hpp>

#include <toasts/toast_notification.hpp>

#include <wolv/utils/guards.hpp>

#include <windows.h>

namespace hex::plugin::windows {

    ViewTTYConsole::ViewTTYConsole() : View::Window("hex.windows.view.tty_console.name", ICON_VS_TERMINAL) {
        m_comPorts = getAvailablePorts();
        m_transmitDataBuffer.resize(0xFFF, 0x00);
        m_receiveDataBuffer.reserve(0xFFF);
        m_receiveDataBuffer.push_back(0x00);
    }

    void ViewTTYConsole::drawContent() {
        ImGuiExt::Header("hex.windows.view.tty_console.config"_lang, true);

        bool connected = m_portHandle != INVALID_HANDLE_VALUE;

        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, connected);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, connected ? 0.5F : 1.0F);

        ImGui::Combo(
            "hex.windows.view.tty_console.port"_lang, &m_selectedPort, [](void *data, int idx) {
                auto &ports = *static_cast<std::vector<std::pair<std::string, std::string>> *>(data);

                return ports[idx].first.c_str();
            },
            &m_comPorts,
            m_comPorts.size());

        ImGui::SameLine();
        if (ImGui::Button("hex.windows.view.tty_console.reload"_lang))
            m_comPorts = getAvailablePorts();

        ImGui::Combo(
            "hex.windows.view.tty_console.baud"_lang, &m_selectedBaudRate, [](void *data, int idx) {
                std::ignore = data;

                return ViewTTYConsole::BaudRates[idx];
            },
            nullptr,
            ViewTTYConsole::BaudRates.size());

        ImGui::Combo(
            "hex.windows.view.tty_console.num_bits"_lang, &m_selectedNumBits, [](void *data, int idx) {
                std::ignore = data;

                return ViewTTYConsole::NumBits[idx];
            },
            nullptr,
            ViewTTYConsole::NumBits.size());

        ImGui::Combo(
            "hex.windows.view.tty_console.stop_bits"_lang, &m_selectedStopBits, [](void *data, int idx) {
                std::ignore = data;

                return ViewTTYConsole::StopBits[idx];
            },
            nullptr,
            ViewTTYConsole::StopBits.size());

        ImGui::Combo(
            "hex.windows.view.tty_console.parity_bits"_lang, &m_selectedParityBits, [](void *data, int idx) {
                std::ignore = data;

                return ViewTTYConsole::ParityBits[idx];
            },
            nullptr,
            ViewTTYConsole::ParityBits.size());

        ImGui::Checkbox("hex.windows.view.tty_console.cts"_lang, &m_hasCTSFlowControl);

        ImGui::PopStyleVar();
        ImGui::PopItemFlag();

        ImGui::NewLine();

        if (m_portHandle == INVALID_HANDLE_VALUE) {
            if (ImGui::Button("hex.windows.view.tty_console.connect"_lang))
                if (!this->connect())
                    ui::ToastError::open("hex.windows.view.tty_console.connect_error"_lang);
        } else {
            if (ImGui::Button("hex.windows.view.tty_console.disconnect"_lang))
                this->disconnect();
        }

        ImGui::NewLine();

        if (ImGui::Button("hex.windows.view.tty_console.clear"_lang)) {
            std::scoped_lock lock(m_receiveBufferMutex);

            m_receiveDataBuffer.clear();
            m_wrapPositions.clear();
        }

        ImGui::SameLine();

        ImGui::Checkbox("hex.windows.view.tty_console.auto_scroll"_lang, &m_shouldAutoScroll);

        ImGuiExt::Header("hex.windows.view.tty_console.console"_lang);

        auto consoleSize = ImMax(ImGui::GetContentRegionAvail(), ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5));
        consoleSize.y -= ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 4;
        if (ImGui::BeginChild("##scrolling", consoleSize, true, ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGuiListClipper clipper;
            clipper.Begin(m_wrapPositions.size(), ImGui::GetTextLineHeight());

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
            while (clipper.Step()) {
                std::scoped_lock lock(m_receiveBufferMutex);

                for (int i = clipper.DisplayStart + 1; i < clipper.DisplayEnd; i++) {
                    ImGui::TextUnformatted(m_receiveDataBuffer.data() + m_wrapPositions[i - 1], m_receiveDataBuffer.data() + m_wrapPositions[i]);
                }

                if (!m_receiveDataBuffer.empty() && !m_wrapPositions.empty())
                    if (static_cast<size_t>(clipper.DisplayEnd) >= m_wrapPositions.size() - 1)
                        ImGui::TextUnformatted(m_receiveDataBuffer.data() + m_wrapPositions.back());

                if (m_shouldAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(0.0F);
                }
            }
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();

        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##transmit", m_transmitDataBuffer.data(), m_transmitDataBuffer.size() - 2, ImGuiInputTextFlags_EnterReturnsTrue)) {
            auto size = strlen(m_transmitDataBuffer.data());

            m_transmitDataBuffer[size + 0] = '\n';
            m_transmitDataBuffer[size + 1] = 0x00;

            this->transmitData(m_transmitDataBuffer);
            ImGui::SetKeyboardFocusHere(0);
        }
        ImGui::PopItemWidth();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsItemHovered() && m_portHandle != INVALID_HANDLE_VALUE && !m_transmitting)
            ImGui::OpenPopup("ConsoleMenu");

        if (ImGui::BeginPopup("ConsoleMenu")) {

            static std::vector<char> buffer(2, 0x00);
            if (ImGui::MenuItem("hex.windows.view.tty_console.send_etx"_lang, "CTRL + C")) {
                buffer[0] = 0x03;
                this->transmitData(buffer);
            }
            if (ImGui::MenuItem("hex.windows.view.tty_console.send_eot"_lang, "CTRL + D")) {
                buffer[0] = 0x04;
                this->transmitData(buffer);
            }
            if (ImGui::MenuItem("hex.windows.view.tty_console.send_sub"_lang, "CTRL + Z")) {
                buffer[0] = 0x1A;
                this->transmitData(buffer);
            }

            ImGui::EndPopup();
        }
    }

    std::vector<std::pair<std::wstring, std::wstring>> ViewTTYConsole::getAvailablePorts() const {
        std::vector<std::pair<std::wstring, std::wstring>> result;
        std::vector<wchar_t> buffer(0xFFF, 0x00);

        for (u16 portNumber = 0; portNumber <= 255; portNumber++) {
            std::wstring port = L"COM" + std::to_wstring(portNumber);

            if (::QueryDosDeviceW(port.c_str(), buffer.data(), buffer.size()) != 0) {
                result.emplace_back(port, buffer.data());
            }
        }

        return result;
    }

    bool ViewTTYConsole::connect() {
        if (m_comPorts.empty() || static_cast<size_t>(m_selectedPort) >= m_comPorts.size()) {
            ui::ToastError::open("hex.windows.view.tty_console.no_available_port"_lang);
            return true;    // If false, connect_error error popup will override this error popup
        }
        m_portHandle = ::CreateFileW((LR"(\\.\)" + m_comPorts[m_selectedPort].first).c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr);

        if (m_portHandle == INVALID_HANDLE_VALUE)
            return false;

        auto closeHandle = SCOPE_GUARD { CloseHandle(m_portHandle); };

        if (!::SetupComm(m_portHandle, 10000, 10000))
            return false;

        DCB serialParams;
        serialParams.DCBlength = sizeof(DCB);

        if (!::GetCommState(m_portHandle, &serialParams))
            return false;

        serialParams.BaudRate     = std::stoi(ViewTTYConsole::BaudRates[m_selectedBaudRate]);
        serialParams.ByteSize     = std::stoi(ViewTTYConsole::NumBits[m_selectedNumBits]);
        serialParams.StopBits     = m_selectedStopBits;
        serialParams.Parity       = m_selectedParityBits;
        serialParams.fOutxCtsFlow = m_hasCTSFlowControl;

        if (!::SetCommState(m_portHandle, &serialParams))
            return false;

        COMMTIMEOUTS timeouts;
        timeouts.ReadIntervalTimeout         = 500;
        timeouts.ReadTotalTimeoutConstant    = 500;
        timeouts.ReadTotalTimeoutMultiplier  = 100;
        timeouts.WriteTotalTimeoutConstant   = 500;
        timeouts.WriteTotalTimeoutMultiplier = 100;

        if (!::SetCommTimeouts(m_portHandle, &timeouts))
            return false;

        closeHandle.release();

        m_receiveThread = std::jthread([this](const std::stop_token &token) {
            bool waitingOnRead    = false;
            OVERLAPPED overlapped = { };

            overlapped.hEvent = ::CreateEvent(nullptr, true, false, nullptr);
            ON_SCOPE_EXIT { ::CloseHandle(&overlapped); };

            auto addByte = [this](char byte) {
                std::scoped_lock lock(m_receiveBufferMutex);

                if (byte >= 0x20 && byte <= 0x7E) {
                    m_receiveDataBuffer.back() = byte;
                    m_receiveDataBuffer.push_back(0x00);
                } else if (byte == '\n' || byte == '\r') {
                    if (m_receiveDataBuffer.empty())
                        return;

                    u32 wrapPos = m_receiveDataBuffer.size() - 1;

                    if (m_wrapPositions.empty() || m_wrapPositions.back() != wrapPos)
                        m_wrapPositions.push_back(wrapPos);
                }
            };

            while (!token.stop_requested()) {
                DWORD bytesRead = 0;

                char byte = 0;
                if (!waitingOnRead) {
                    if (::ReadFile(m_portHandle, &byte, sizeof(char), &bytesRead, &overlapped)) {
                        addByte(byte);
                    } else if (::GetLastError() == ERROR_IO_PENDING) {
                        waitingOnRead = true;
                    }
                } else {
                    byte     = 0;
                    switch (::WaitForSingleObject(overlapped.hEvent, 500)) {
                        case WAIT_OBJECT_0:
                            if (::GetOverlappedResult(m_portHandle, &overlapped, &bytesRead, false)) {
                                addByte(byte);
                                waitingOnRead = false;
                            }
                        default:
                            break;
                    }
                }
            }
        });

        return true;
    }

    bool ViewTTYConsole::disconnect() {
        ::SetCommMask(m_portHandle, EV_TXEMPTY);
        m_receiveThread.request_stop();
        m_receiveThread.join();

        ::CloseHandle(m_portHandle);
        m_portHandle = INVALID_HANDLE_VALUE;

        return true;
    }

    void ViewTTYConsole::transmitData(std::vector<char> &data) {
        if (m_transmitting)
            return;

        TaskManager::createBackgroundTask("hex.windows.view.tty_console.task.transmitting"_lang, [&, this](auto&) {
            OVERLAPPED overlapped = { };

            overlapped.hEvent = ::CreateEvent(nullptr, true, false, nullptr);
            ON_SCOPE_EXIT { ::CloseHandle(&overlapped); };

            m_transmitting = true;

            DWORD bytesWritten = 0;
            if (!::WriteFile(m_portHandle, data.data(), strlen(data.data()), &bytesWritten, &overlapped)) {
                if (::GetLastError() == ERROR_IO_PENDING) {
                    ::GetOverlappedResult(m_portHandle, &overlapped, &bytesWritten, true);
                }
            }

            if (bytesWritten > 0)
                data[0] = 0x00;

            m_transmitting = false;
        });
    }

}