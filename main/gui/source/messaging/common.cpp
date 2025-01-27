#include <optional>

#include <hex/api/imhex_api.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/requests_lifecycle.hpp>
#include <hex/helpers/logger.hpp>

#include "messaging.hpp"

namespace hex::messaging {

    void messageReceived(const std::string &eventName, const std::vector<u8> &args) {
        log::debug("Received event '{}' with size {}", eventName, args.size());
        ImHexApi::Messaging::impl::runHandler(eventName, args);
    }

    void setupEvents() {
        SendMessageToMainInstance::subscribe([](const std::string &eventName, const std::vector<u8> &eventData) {
            if (ImHexApi::System::isMainInstance()) {
                log::debug("Executing message '{}' in current instance", eventName);
                EventImHexStartupFinished::subscribe([eventName, eventData]{
                    ImHexApi::Messaging::impl::runHandler(eventName, eventData);
                });
            } else {
                log::debug("Forwarding message '{}' to existing instance", eventName);
                sendToOtherInstance(eventName, eventData);
            }
        });

        EventNativeMessageReceived::subscribe([](const std::vector<u8> &rawData) {
            ssize_t nullIndex = -1;

            auto messageData = reinterpret_cast<const char*>(rawData.data());
            size_t messageSize = rawData.size();

            for (size_t i = 0; i < messageSize; i++) {
                if (messageData[i] == '\0') {
                    nullIndex = i;
                    break;
                }
            }

            if (nullIndex == -1) {
                log::warn("Received invalid forwarded event");
                return;
            }

            std::string eventName(messageData, nullIndex);
            std::vector<u8> eventData(messageData + nullIndex + 1, messageData + messageSize);

            messageReceived(eventName, eventData);
        });
    }

    void setupMessaging() {
        ImHexApi::System::impl::setMainInstanceStatus(setupNative());
        setupEvents();
    }
}
