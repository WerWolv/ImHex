#if defined(OS_WINDOWS)

#include "messaging.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/helpers/logger.hpp>

#include <windows.h>

namespace hex::messaging {

    std::optional<HWND> getImHexWindow() {

        HWND imhexWindow = 0;
        
        ::EnumWindows([](HWND hWnd, LPARAM ret) -> BOOL {
            // Get the window name
            auto length = ::GetWindowTextLength(hWnd);
            std::string windowName(length + 1, '\x00');
            ::GetWindowText(hWnd, windowName.data(), windowName.size());

            // Check if the window is visible and if it's an ImHex window
            if (::IsWindowVisible(hWnd) == TRUE && length != 0) {
                if (windowName.starts_with("ImHex")) {
                    // it's our window, return it and stop iteration
                    *reinterpret_cast<HWND*>(ret) = hWnd;
                    return FALSE;
                }
            }

            // continue iteration
            return TRUE;

        }, reinterpret_cast<LPARAM>(&imhexWindow));

        if (imhexWindow == 0) return { };
        else return imhexWindow;
    }

    void sendToOtherInstance(const std::string &evtName, const std::vector<u8> &evtData) {
        log::debug("Sending event {} to another instance (not us)", evtName);
        hex::unused(evtName);
        hex::unused(evtData);

        // Get the window we want to send it to
        HWND imHexWindow = *getImHexWindow();

        // Create the message
        // TODO actually send all arguments and not just the evtName
        
        std::vector<u8> fullEvtData(evtName.begin(), evtName.end());
        fullEvtData.push_back('\0');

        fullEvtData.insert(fullEvtData.end(), evtData.begin(), evtData.end());
        
        u8 *data = &fullEvtData[0];
        DWORD dataSize = static_cast<DWORD>(fullEvtData.size());

        COPYDATASTRUCT message = {
            .dwData = 0,
            .cbData = dataSize,
            .lpData = data
        };

        // Send the message
        SendMessage(imHexWindow, WM_COPYDATA, reinterpret_cast<WPARAM>(imHexWindow), reinterpret_cast<LPARAM>(&message));

    }

    bool setupNative() {

        constexpr static auto UniqueMutexId = "ImHex/a477ea68-e334-4d07-a439-4f159c683763";

        // check if an ImHex instance is already running by opening a global mutex
        HANDLE globalMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, UniqueMutexId);
        if (globalMutex == nullptr) {
            // If no ImHex instance is running, create a new global mutex
            globalMutex = CreateMutex(nullptr, FALSE, UniqueMutexId);
            return true;
        } else {
            return false;
        }
    }
}

#endif
