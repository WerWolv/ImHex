#pragma once

#include <hex/ui/view.hpp>

#include <mutex>
#include <thread>
#include <jthread.hpp>
#include <vector>

namespace hex::plugin::windows {

    class ViewTTYConsole : public View::Window {
    public:
        ViewTTYConsole();
        ~ViewTTYConsole() override = default;

        void drawContent() override;

    private:
        std::vector<std::pair<std::wstring, std::wstring>> m_comPorts;

        std::vector<std::pair<std::wstring, std::wstring>> getAvailablePorts() const;
        bool connect();
        bool disconnect();

        void transmitData(std::vector<char> &data);

        void* m_portHandle = reinterpret_cast<void*>(-1);
        std::jthread m_receiveThread;

        int m_selectedPort       = 0;
        int m_selectedBaudRate   = 11;       // 115200
        int m_selectedNumBits    = 3;        // 8
        int m_selectedStopBits   = 0;        // 1
        int m_selectedParityBits = 0;        // None
        bool m_hasCTSFlowControl = false;    // No

        bool m_shouldAutoScroll = true;

        std::mutex m_receiveBufferMutex;
        std::vector<char> m_receiveDataBuffer, m_transmitDataBuffer;
        std::vector<u32> m_wrapPositions;
        bool m_transmitting = false;

        constexpr static std::array BaudRates = {
            "110",
            "300",
            "600",
            "1200",
            "2400",
            "4800",
            "9600",
            "14400",
            "19200",
            "38400",
            "57600",
            "115200",
            "128000",
            "256000"
        };

        constexpr static std::array NumBits = {
            "5",
            "6",
            "7",
            "8"
        };

        constexpr static std::array StopBits = {
            "1",
            "1.5",
            "2.0"
        };

        constexpr static std::array ParityBits = {
            "None",
            "Odd",
            "Even",
            "Mark",
            "Space"
        };
    };

}