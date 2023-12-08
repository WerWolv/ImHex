#include <optional>

#include <hex/api/imhex_api.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/helpers/logger.hpp>

#include "messaging.hpp"

namespace hex::messaging {

    void messageReceived(const std::string &eventName, const std::vector<u8> &args) {
        log::debug("Received event '{}' with size {}", eventName, args.size());
        ImHexApi::Messaging::impl::runHandler(eventName, args);
    }

    void setupEvents() {
        SendMessageToMainInstance::subscribe([](const std::string &eventName, const std::vector<u8> &eventData) {
            log::debug("Forwarding message {} (maybe to us)", eventName);
            if (ImHexApi::System::isMainInstance()) {
                EventImHexStartupFinished::subscribe([eventName, eventData]{
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
