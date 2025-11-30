#pragma once

#include <hex/ui/view.hpp>

#include <mutex>
#include <thread>
#include <jthread.hpp>
#include <vector>
#include <windows.h>
#include <wolv/container/interval_tree.hpp>

namespace hex::plugin::windows {

    class ViewTTYConsole : public View::Window {
    public:
        ViewTTYConsole();
        ~ViewTTYConsole() override = default;

        void drawContent() override;
        void drawHelpText() override;

    private:
        void drawSettings();
        void drawConsole();

    private:
        struct Port {
            std::string name;
            std::wstring path;
        };

        constexpr static std::array BaudRates = {
            110, 150, 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
        };

        constexpr static std::array NumBits = {
            5,
            6,
            7,
            8
        };

        enum class StopBits : u8 {
            _1_0 = ONESTOPBIT,
            _1_5 = ONE5STOPBITS,
            _2_0 = TWOSTOPBITS
        };

        enum class ParityBits {
            None  = NOPARITY,
            Odd   = ODDPARITY,
            Even  = EVENPARITY,
            Mark  = MARKPARITY,
            Space = SPACEPARITY
        };

        std::vector<Port> getAvailablePorts() const;
        bool connect();
        bool disconnect();

        void transmitData(const std::string &data);

        void* m_portHandle = INVALID_HANDLE_VALUE;
        std::jthread m_receiveThread;

        u32 m_selectedPortIndex         = 0;
        i32 m_selectedBaudRate          = 115200;
        i32 m_selectedNumBits           = 8;
        StopBits m_selectedStopBits     = StopBits::_1_0;
        ParityBits m_selectedParityBits = ParityBits::None;
        bool m_hasCTSFlowControl = false;

        bool m_shouldAutoScroll = true;
        bool m_settingsCollapsed = false;

        std::mutex m_receiveBufferMutex;
        std::vector<std::string> m_receiveLines;
        std::vector<ImColor> m_receiveLinesColor;
        std::string m_transmitDataBuffer;
        bool m_transmitting = false;
        std::vector<Port> m_comPorts;
    };

}
