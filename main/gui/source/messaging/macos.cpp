#if defined(OS_MACOS)

#include <stdexcept>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/utils_macos.hpp>

#include "messaging.hpp"

namespace hex::messaging {

    void sendToOtherInstance(const std::string &eventName, const std::vector<u8> &args) {
        log::debug("Sending event {} to another instance (not us)", eventName);

        // Create the message
        std::vector<u8> fullEventData(eventName.begin(), eventName.end());
        fullEventData.push_back('\0');

        fullEventData.insert(fullEventData.end(), args.begin(), args.end());

        const u8 *data = &fullEventData[0];
        auto dataSize = fullEventData.size();

        macosSendMessageToMainInstance(data, dataSize);
    }

    bool setupNative() {
        macosInstallEventListener();
        return macosIsMainInstance();
    }
}

#endif
