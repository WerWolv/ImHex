#include "helpers/event.hpp"

namespace hex {

    void EventManager::post(Events eventType, const void *userData) {
        for (auto &handler : this->m_eventHandlers)
            if (eventType == handler.eventType)
                handler.callback(userData);
    }

    void EventManager::subscribe(Events eventType, void *owner, std::function<void(const void*)> callback) {
        for (auto &handler : this->m_eventHandlers)
            if (eventType == handler.eventType && owner == handler.owner)
                return;

        this->m_eventHandlers.push_back(EventHandler { owner, eventType, callback });
    }

    void EventManager::unsubscribe(Events eventType, void *sender) {
        std::erase_if(this->m_eventHandlers, [&eventType, &sender](EventHandler handler) {
            return eventType == handler.eventType && sender == handler.owner;
        });
    }

}