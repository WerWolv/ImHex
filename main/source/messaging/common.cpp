#include <optional>

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>
#include <hex/helpers/logger.hpp>

#include "messaging.hpp"

namespace hex::messaging {

    void messageReceived(const std::string &eventName, const std::vector<u8> &eventData) {
        log::debug("Received event '{}' with size {}", eventName, eventData.size());
        ImHexApi::Messaging::impl::runHandler(eventName, eventData);
    }

    void setupEvents() {
        EventManager::subscribe<SendMessageToMainInstance>([](const std::string eventName, const std::vector<u8> &eventData) {
            log::debug("Forwarding message {} (maybe to us)", eventName);
            if (ImHexApi::System::isMainInstance()) {
                EventManager::subscribe<EventImHexStartupFinished>([eventName, eventData](){
                    ImHexApi::Messaging::impl::runHandler(eventName, eventData);
                });
            } else {
                sendToOtherInstance(eventName, eventData);
            }
        });
    }

    void setupMessaging() {
        ImHexApi::System::impl::setMainInstanceStatus(setupNative());
        setupEvents();
    }
}
