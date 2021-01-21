#include <hex/api/event.hpp>

#include <hex/helpers/shared_data.hpp>

namespace hex {

    std::vector<std::any> EventManager::post(Events eventType, const std::any &userData) {
        std::vector<std::any> results;
        for (auto &handler : SharedData::eventHandlers)
            if (eventType == handler.eventType)
                results.push_back(handler.callback(userData));

        return results;
    }

    void EventManager::subscribe(Events eventType, void *owner, std::function<std::any(const std::any&)> callback) {
        for (auto &handler : SharedData::eventHandlers)
            if (eventType == handler.eventType && owner == handler.owner)
                return;

        SharedData::eventHandlers.push_back(EventHandler { owner, eventType, callback });
    }

    void EventManager::unsubscribe(Events eventType, void *sender) {
        std::erase_if(SharedData::eventHandlers, [&eventType, &sender](EventHandler handler) {
            return eventType == handler.eventType && sender == handler.owner;
        });
    }

}