#include "views/view_tty_console.hpp"

#include <imgui_internal.h>

#include <hex/api/task_manager.hpp>
#include <hex/helpers/utils.hpp>

#include <toasts/toast_notification.hpp>

#include <wolv/utils/guards.hpp>

namespace hex::plugin::windows {

    ViewTTYConsole::ViewTTYConsole() : View::Window("hex.windows.view.tty_console.name", ICON_VS_TERMINAL) {
        m_comPorts = getAvailablePorts();
        m_selectedPortIndex = 0;
        m_transmitDataBuffer.resize(0xFFF, 0x00);
    }

    void ViewTTYConsole::drawContent() {
        ImGuiExt::Header("hex.windows.view.tty_console.config"_lang, true);

        bool connected = m_portHandle != INVALID_HANDLE_VALUE;

        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, connected);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, connected ? 0.5F : 1.0F);

        if (ImGui::BeginCombo("hex.windows.view.tty_console.port"_lang, m_comPorts.empty() ? "" : m_comPorts[m_selectedPortIndex].name.c_str())) {
            for (u32 i = 0; i < m_comPorts.size(); i++) {
                const auto &port = m_comPorts[i];
                if (ImGui::Selectable(port.name.c_str(), m_selectedPortIndex == i)) {
                    m_selectedPortIndex = i;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("hex.windows.view.tty_console.reload"_lang)) {
            m_comPorts = getAvailablePorts();
            m_selectedPortIndex = 0;
        }

        if (ImGui::BeginCombo("hex.windows.view.tty_console.baud"_lang, std::to_string(m_selectedBaudRate).c_str())) {
            for (auto baudRate : BaudRates) {
                if (ImGui::Selectable(std::to_string(baudRate).c_str(), m_selectedBaudRate == baudRate)) {
                    m_selectedBaudRate = baudRate;
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginCombo("hex.windows.view.tty_console.num_bits"_lang, std::to_string(m_selectedNumBits).c_str())) {
            for (auto numBits : NumBits) {
                if (ImGui::Selectable(std::to_string(numBits).c_str(), m_selectedBaudRate == numBits)) {
                    m_selectedBaudRate = numBits;
                }
            }
            ImGui::EndCombo();
        }

        constexpr static std::array StopBitsStrings = { "1", "1.5", "2" };
        if (ImGui::BeginCombo( "hex.windows.view.tty_console.stop_bits"_lang, StopBitsStrings[u32(m_selectedStopBits)])) {
            for (u32 i = 0; i < StopBitsStrings.size(); i++) {
                if (ImGui::Selectable(StopBitsStrings[i], m_selectedStopBits == StopBits(i))) {
                    m_selectedStopBits = StopBits(i);
                }
            }
            ImGui::EndCombo();
        }
        constexpr static std::array ParityBitsStrings = { "None", "Odd", "Even", "Mark", "Space" };
        if (ImGui::BeginCombo( "hex.windows.view.tty_console.parity_bits"_lang, ParityBitsStrings[u32(m_selectedParityBits)])) {
            for (u32 i = 0; i < ParityBitsStrings.size(); i++) {
                if (ImGui::Selectable(ParityBitsStrings[i], m_selectedParityBits == ParityBits(i))) {
                    m_selectedParityBits = ParityBits(i);
                }
            }
            ImGui::EndCombo();
        }

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

            m_receiveLines.clear();
        }

        ImGui::SameLine();

        ImGui::Checkbox("hex.windows.view.tty_console.auto_scroll"_lang, &m_shouldAutoScroll);

        this->drawConsole();

        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##transmit", m_transmitDataBuffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            this->transmitData(m_transmitDataBuffer + "\r\n");
            m_transmitDataBuffer.clear();
            ImGui::SetKeyboardFocusHere(0);
        }
        ImGui::PopItemWidth();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsItemHovered() && m_portHandle != INVALID_HANDLE_VALUE && !m_transmitting)
            ImGui::OpenPopup("ConsoleMenu");

        if (ImGui::BeginPopup("ConsoleMenu")) {

            if (ImGui::MenuItem("hex.windows.view.tty_console.send_etx"_lang, "CTRL + C")) {
                this->transmitData({ 0x03 });
            }
            if (ImGui::MenuItem("hex.windows.view.tty_console.send_eot"_lang, "CTRL + D")) {
                this->transmitData({ 0x04 });
            }
            if (ImGui::MenuItem("hex.windows.view.tty_console.send_sub"_lang, "CTRL + Z")) {
                this->transmitData({ 0x1A });
            }

            ImGui::EndPopup();
        }
    }

    void ViewTTYConsole::drawConsole() {
        ImGuiExt::Header("hex.windows.view.tty_console.console"_lang);

        auto consoleSize = ImMax(ImGui::GetContentRegionAvail(), ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5));
        consoleSize.y -= ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 4;
        if (ImGui::BeginChild("##scrolling", consoleSize, true, ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGuiListClipper clipper;
            clipper.Begin(m_receiveLines.size(), ImGui::GetTextLineHeight());

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
            while (clipper.Step()) {
                std::scoped_lock lock(m_receiveBufferMutex);

                for (int i = clipper.DisplayStart + 1; i < clipper.DisplayEnd; i++) {
                    ImGui::TextUnformatted(m_receiveLines[i].c_str());
                }

                if (m_shouldAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(0.0F);
                }
            }
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
    }


    std::vector<ViewTTYConsole::Port> ViewTTYConsole::getAvailablePorts() const {
        std::vector<Port> result;
        std::vector<wchar_t> buffer(0xFFF, 0x00);

        for (u16 portNumber = 0; portNumber <= 255; portNumber++) {
            std::wstring port = L"COM" + std::to_wstring(portNumber);

            if (::QueryDosDeviceW(port.c_str(), buffer.data(), buffer.size()) != 0) {
                result.emplace_back(utf16ToUtf8(port), L"\\\\.\\" + port);
            }
        }

        return result;
    }

    bool ViewTTYConsole::connect() {
        if (m_comPorts.empty() || static_cast<size_t>(m_selectedPortIndex) >= m_comPorts.size()) {
            ui::ToastError::open("hex.windows.view.tty_console.no_available_port"_lang);
            return true;
        }
        m_portHandle = ::CreateFileW(m_comPorts[m_selectedPortIndex].path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (m_portHandle == INVALID_HANDLE_VALUE)
            return false;

        auto closeHandle = SCOPE_GUARD {
            CloseHandle(m_portHandle);
            m_portHandle = INVALID_HANDLE_VALUE;
        };

        if (!::SetupComm(m_portHandle, 10000, 10000))
            return false;

        DCB serialParams;
        serialParams.DCBlength = sizeof(DCB);

        if (!::GetCommState(m_portHandle, &serialParams))
            return false;

        serialParams.BaudRate     = m_selectedBaudRate;
        serialParams.ByteSize     = m_selectedNumBits;
        serialParams.StopBits     = u32(m_selectedStopBits);
        serialParams.Parity       = u32(m_selectedParityBits);
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
            ON_SCOPE_EXIT { ::CloseHandle(overlapped.hEvent); };

            auto addByte = [&, this](char byte) {
                std::scoped_lock lock(m_receiveBufferMutex);

                if (m_receiveLines.empty())
                    m_receiveLines.emplace_back();
                if (std::isprint(byte)) {
                    m_receiveLines.back().push_back(byte);
                } else if (byte == '\n') {
                    m_receiveLines.emplace_back();
                }  else if (byte == '\r') {
                    // Ignore carriage return
                } else {
                    m_receiveLines.back() += hex::format("<{:02X}>", static_cast<unsigned char>(byte));
                }

            };

            while (!token.stop_requested()) {
                DWORD bytesRead = 0;

                char byte = 0;
                if (!waitingOnRead) {
                    if (::ReadFile(m_portHandle, &byte, sizeof(char), &bytesRead, &overlapped) && bytesRead > 0) {
                        addByte(byte);
                    } else if (::GetLastError() == ERROR_IO_PENDING) {
                        waitingOnRead = true;
                    }
                } else {
                    byte = 0;
                    switch (::WaitForSingleObject(overlapped.hEvent, 500)) {
                        case WAIT_OBJECT_0:
                            if (::GetOverlappedResult(m_portHandle, &overlapped, &bytesRead, false) && bytesRead > 0) {
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

        ::CloseHandle(m_portHandle);
        m_portHandle = INVALID_HANDLE_VALUE;

        if (m_receiveThread.joinable()) {
            m_receiveThread.request_stop();
            m_receiveThread.join();
        }

        return true;
    }

    void ViewTTYConsole::transmitData(const std::string &data) {
        if (m_transmitting)
            return;

        m_transmitting = true;

        DWORD bytesWritten = 0;
        if (!::WriteFile(m_portHandle, data.data(), data.size(), &bytesWritten, nullptr)) {
            log::error("Failed to write data to serial port: {}", formatSystemError(::GetLastError()));
        }

        m_transmitting = false;
    }

}