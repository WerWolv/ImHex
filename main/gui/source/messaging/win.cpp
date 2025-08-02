#if defined(OS_WINDOWS)

#include "messaging.hpp"

#include <hex/helpers/logger.hpp>

#include <windows.h>

namespace hex::messaging {

    std::optional<HWND> getImHexWindow() {

        HWND imhexWindow = nullptr;
        
        ::EnumWindows([](HWND hWnd, LPARAM ret) -> BOOL {
            // Get the window name
            auto length = ::GetWindowTextLength(hWnd);
            std::string windowName(length + 1, '\x00');
            ::GetWindowTextA(hWnd, windowName.data(), windowName.size());

            // Check if the window is visible and if it's an ImHex window
            if (::IsWindowVisible(hWnd) == TRUE && length != 0) {
                if (windowName.starts_with("ImHex")) {
                    // It's our window, return it and stop iteration
                    *reinterpret_cast<HWND*>(ret) = hWnd;
                    return FALSE;
                }
            }

            // Continue iteration
            return TRUE;

        }, reinterpret_cast<LPARAM>(&imhexWindow));

        if (imhexWindow == nullptr)
            return { };
        else
            return imhexWindow;
    }

    void sendToOtherInstance(const std::string &eventName, const std::vector<u8> &args) {
        log::debug("Sending event {} to another instance (not us)", eventName);

        // Get the window we want to send it to
        auto potentialWindow = getImHexWindow();
        if (!potentialWindow.has_value())
            return;

        HWND imHexWindow = potentialWindow.value();

        // Create the message
        std::vector<u8> fullEventData(eventName.begin(), eventName.end());
        fullEventData.push_back('\0');

        fullEventData.insert(fullEventData.end(), args.begin(), args.end());
        
        u8 *data = &fullEventData[0];
        DWORD dataSize = fullEventData.size();

        COPYDATASTRUCT message = {
            .dwData = 0,
            .cbData = dataSize,
            .lpData = data
        };

        // Send the message
        SendMessage(imHexWindow, WM_COPYDATA, reinterpret_cast<WPARAM>(imHexWindow), reinterpret_cast<LPARAM>(&message));

    }

    bool setupNative() {
        constexpr static auto UniqueMutexId = L"ImHex/a477ea68-e334-4d07-a439-4f159c683763";

        // Check if an ImHex instance is already running by opening a global mutex
        static HANDLE globalMutex;
        AT_FINAL_CLEANUP {
            if (globalMutex != nullptr) {
                CloseHandle(globalMutex);
            }
        };

        globalMutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, UniqueMutexId);
        if (globalMutex == nullptr) {
            // If no ImHex instance is running, create a new global mutex
            globalMutex = CreateMutexW(nullptr, FALSE, UniqueMutexId);
            return true;
        } else {
            return false;
        }
    }
}

#endif
