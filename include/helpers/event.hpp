#pragma once

#include <vector>
#include <functional>

namespace hex {

    enum class Events {
        FileLoaded,
        DataChanged,
        PatternChanged,
        FileDropped,
        WindowClosing,
        RegionSelected,

        SelectionChangeRequest,

        AddBookmark,
        AppendPatternLanguageCode,

        ProjectFileStore,
        ProjectFileLoad
    };

    struct EventHandler {
        void *owner;
        Events eventType;
        std::function<void(const void*)> callback;
    };

    class EventManager {
    public:

        void post(Events eventType, const void *userData) {
            for (auto &handler : this->m_eventHandlers)
                if (eventType == handler.eventType)
                    handler.callback(userData);
        }

        void subscribe(Events eventType, void *owner, std::function<void(const void*)> callback) {
            for (auto &handler : this->m_eventHandlers)
                if (eventType == handler.eventType && owner == handler.owner)
                    return;

            this->m_eventHandlers.push_back(EventHandler { owner, eventType, callback });
        }

        void unsubscribe(Events eventType, void *sender) {
            std::erase_if(this->m_eventHandlers, [&eventType, &sender](EventHandler handler) {
                return eventType == handler.eventType && sender == handler.owner;
            });
        }

    private:
        std::vector<EventHandler> m_eventHandlers;
    };

}