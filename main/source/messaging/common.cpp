#include <optional>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include "messaging.hpp"

namespace hex::messaging {

    std::optional<bool> s_isMainInstance;

    void eventReceived(const std::string &evtName, const std::vector<u8> &evtData) {
        log::debug("Received event '{}' with size {}", evtName, evtData.size());
        ContentRegistry::ForwardEvent::impl::runHandler(evtName, evtData);
    }

    void setupEvents() {
        EventManager::subscribe<IsMainInstance>([](bool &ret){
            ret = *s_isMainInstance;
        });

        EventManager::subscribe<SendEventToMainInstance>([](const std::string evtName, const std::vector<u8> &evtData){
            log::debug("Forwarding event {} (maybe to us)", evtName);
            if (*s_isMainInstance) {
                EventManager::subscribe<EventImHexStartupFinished>([evtName, evtData](){
                    ContentRegistry::ForwardEvent::impl::runHandler(evtName, evtData);
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
