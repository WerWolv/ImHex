#include <optional>

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>
#include <hex/helpers/logger.hpp>

#include "messaging.hpp"

namespace hex::messaging {

    static std::optional<bool> s_isMainInstance;

    void messageReceived(const std::string &evtName, const std::vector<u8> &evtData) {
        log::debug("Received event '{}' with size {}", evtName, evtData.size());
        ImHexApi::Messaging::impl::runHandler(evtName, evtData);
    }

    void setupEvents() {
        EventManager::subscribe<IsMainInstance>([](bool &ret){
            ret = *s_isMainInstance;
        });

        EventManager::subscribe<SendMessageToMainInstance>([](const std::string evtName, const std::vector<u8> &evtData) {
            log::debug("Forwarding message {} (maybe to us)", evtName);
            if (*s_isMainInstance) {
                EventManager::subscribe<EventImHexStartupFinished>([evtName, evtData](){
                    ImHexApi::Messaging::impl::runHandler(evtName, evtData);
                });
            } else {
                sendToOtherInstance(evtName, evtData);
            }
        });
    }

    void setupMessaging() {
        s_isMainInstance = setupNative();
        setupEvents();
    }
}
