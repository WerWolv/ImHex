#include "views/view_tty_console.hpp"

#include <imgui_internal.h>
#include <implot.h>

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
        this->drawSettings();
        this->drawConsole();
    }

    template<size_t N>
     struct SignalPart {
        const char *name;
        std::array<double, N> values;
    };

    void ViewTTYConsole::drawSettings() {
        const auto configWidth = 200_scaled;
        if (ImGuiExt::BeginSubWindow("hex.windows.view.tty_console.config"_lang, &m_settingsCollapsed, m_settingsCollapsed ? ImVec2(0, 1) : ImVec2(0, 0))) {
            const bool connected = m_portHandle != INVALID_HANDLE_VALUE;

            if (ImGui::BeginTable("##config_table", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("##config");
                ImGui::TableSetupColumn("##visualization");
                ImGui::TableNextRow();

                {
                    ImGui::BeginDisabled(connected);
                    {
                        ImGui::TableNextColumn();

                        ImGui::PushItemWidth(configWidth - ImGui::GetStyle().ItemSpacing.x - ImGui::GetStyle().FramePadding.x * 2 - ImGui::CalcTextSize(ICON_VS_REFRESH).x);
                        if (ImGui::BeginCombo("##port", m_comPorts.empty() ? "" : m_comPorts[m_selectedPortIndex].name.c_str())) {
                            for (u32 i = 0; i < m_comPorts.size(); i++) {
                                const auto &port = m_comPorts[i];
                                if (ImGui::Selectable(port.name.c_str(), m_selectedPortIndex == i)) {
                                    m_selectedPortIndex = i;
                                }
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        if (ImGuiExt::DimmedIconButton(ICON_VS_REFRESH, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                            m_comPorts = getAvailablePorts();
                            m_selectedPortIndex = 0;
                        }
                        ImGui::SetItemTooltip("%s", "hex.windows.view.tty_console.reload"_lang.get());

                        ImGui::SameLine();

                        ImGui::TextUnformatted("hex.windows.view.tty_console.port"_lang);
                    }

                    ImGui::PushItemWidth(configWidth);

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
                                m_selectedNumBits = numBits;
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

                    ImGui::Separator();

                    ImGui::EndDisabled();

                    {
                        if (m_portHandle == INVALID_HANDLE_VALUE) {
                            if (ImGuiExt::DimmedButton("hex.windows.view.tty_console.connect"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                                if (!this->connect()) {
                                    ui::ToastError::open("hex.windows.view.tty_console.connect_error"_lang);
                                } else {
                                    m_settingsCollapsed = true;
                                }
                            }
                        } else {
                            if (ImGuiExt::DimmedButton("hex.windows.view.tty_console.disconnect"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                                this->disconnect();
                            }
                        }
                    }

                }

                {
                    ImGui::TableNextColumn();

                    if (ImPlot::BeginPlot("##visualization", ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 1).GetSize(), ImPlotFlags_NoFrame | ImPlotFlags_CanvasOnly)) {
                        ImPlot::SetupAxis(ImAxis_X1, "X", ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_AutoFit);
                        ImPlot::SetupAxis(ImAxis_Y1, "Y", ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_LockMax);

                        std::vector<double> signal;

                        struct Annotation { double x; const char* text; };
                        std::vector<Annotation> annotations;

                        constexpr static auto Low = 0.3;
                        constexpr static auto High = 0.6;

                        annotations.emplace_back(0, "Idle");

                        // Idle
                        signal.push_back(High);
                        signal.push_back(High);
                        signal.push_back(High);
                        signal.push_back(High);

                        // Start bit
                        annotations.emplace_back(signal.size(), "Start");
                        signal.push_back(Low);

                        // Data bits
                        annotations.emplace_back(signal.size(), "Data");
                        for (u8 i = 0; i < m_selectedNumBits; i += 1) {
                            signal.push_back((i & 1) ? Low : High);
                        }

                        // Parity
                        if (m_selectedParityBits != ParityBits::None)
                            annotations.emplace_back(signal.size(), "Parity");
                        switch (m_selectedParityBits) {
                            case ParityBits::Even:
                                signal.push_back(Low);
                                break;
                            case ParityBits::Odd:
                                signal.push_back(High);
                                break;
                            case ParityBits::Mark:
                                signal.push_back(High);
                                break;
                            case ParityBits::Space:
                                signal.push_back(Low);
                                break;
                            case ParityBits::None:
                                break;
                        }

                        // Stop bits
                        annotations.emplace_back(signal.size(), "Stop");
                        switch (m_selectedStopBits) {
                            case StopBits::_1_0:
                                signal.push_back(High);
                                break;
                            case StopBits::_1_5:
                                signal.push_back(High);
                                signal.push_back(High);
                                break;
                            case StopBits::_2_0:
                                signal.push_back(High);
                                signal.push_back(High);
                                signal.push_back(High);
                                break;
                        }

                        // Idle
                        annotations.emplace_back(signal.size(), "Idle");
                        signal.push_back(High);
                        signal.push_back(High);
                        signal.push_back(High);
                        signal.push_back(High);

                        annotations.emplace_back(signal.size(), "");

                        const auto scale = 1.0 / (signal.size() - 1);
                        ImPlot::PlotStairs("Signal", signal.data(), signal.size(), scale, 0);

                        for (u32 index = 0; index < annotations.size() - 1; index++) {
                            const auto &x1 = annotations[index];
                            const auto &x2 = annotations[index + 1];

                            ImPlot::Annotation((x2.x - (x2.x - x1.x) / 2) * scale, index & 1 ? 0.77 : 0.90, ImGui::GetStyleColorVec4(ImGuiCol_Text), ImVec2(0, 0), false, "%s", x1.text);

                            const auto lineX = (x1.x - 0.1) * scale;
                            const double xs[] = { lineX, lineX };
                            const double ys[] = { 0.0, 1.0 };
                            ImPlot::PlotLine("##line", xs, ys, 2);

                        }

                        ImPlot::EndPlot();
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGuiExt::EndSubWindow();
    }


    void ViewTTYConsole::drawConsole() {
        ImGui::BeginDisabled(m_portHandle == INVALID_HANDLE_VALUE);

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

        if (ImGuiExt::DimmedIconButton(ICON_VS_CLEAR_ALL, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            std::scoped_lock lock(m_receiveBufferMutex);

            m_receiveLines.clear();
        }
        ImGui::SetItemTooltip("%s", "hex.windows.view.tty_console.clear"_lang.get());

        ImGui::SameLine();

        ImGuiExt::DimmedIconToggle(ICON_VS_GIT_FETCH, &m_shouldAutoScroll);
        ImGui::SetItemTooltip("%s", "hex.windows.view.tty_console.auto_scroll"_lang.get());

        ImGui::SameLine();

        ImGui::PushItemWidth(-ImGui::GetStyle().ItemSpacing.x - ImGui::GetStyle().FramePadding.x * 2 - ImGui::CalcTextSize(ICON_VS_SEND).x);
        if (ImGui::InputText("##transmit", m_transmitDataBuffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            this->transmitData(m_transmitDataBuffer + "\r\n");
            m_transmitDataBuffer.clear();
            ImGui::SetKeyboardFocusHere(0);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGuiExt::DimmedIconButton(ICON_VS_SEND, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            this->transmitData(m_transmitDataBuffer + "\r\n");
            m_transmitDataBuffer.clear();
            ImGui::SetKeyboardFocusHere(-1);
        }

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

        ImGui::EndDisabled();
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
                    m_receiveLines.back() += fmt::format("<{:02X}>", static_cast<unsigned char>(byte));
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

    void ViewTTYConsole::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This view can send and receive data over a Serial (TTY) port.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("Connect your device to a Serial Port (or a USB port with a Serial adapter) and configure the connection settings on the left side. Once connected, you can send and receive data using the console below.");
    }
}
