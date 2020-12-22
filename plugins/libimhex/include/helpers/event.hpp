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
        void post(Events eventType, const void *userData);
        void subscribe(Events eventType, void *owner, std::function<void(const void*)> callback);
        void unsubscribe(Events eventType, void *sender);

    private:
        std::vector<EventHandler> m_eventHandlers;
    };

}