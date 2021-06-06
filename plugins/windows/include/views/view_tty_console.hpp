#pragma once

#include <hex/views/view.hpp>

#include <windows.h>

#include <mutex>
#include <thread>
#include <vector>

namespace hex::plugin::windows {

    namespace prv { class Provider; }

    class ViewTTYConsole : public View {
    public:
        ViewTTYConsole();
        ~ViewTTYConsole() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        std::vector<std::pair<std::string, std::string>> m_comPorts;

        std::vector<std::pair<std::string, std::string>> getAvailablePorts();
        bool connect();
        bool disconnect();

        void transmitData(std::vector<char> &data);

        HANDLE m_portHandle = INVALID_HANDLE_VALUE;
        std::jthread m_receiveThread;

        int m_selectedPort = 0;
        int m_selectedBaudRate = 11;        // 115200
        int m_selectedNumBits = 3;          // 8
        int m_selectedStopBits = 0;         // 1
        int m_selectedParityBits = 0;       // None
        bool m_hasCTSFlowControl = false;   // No

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