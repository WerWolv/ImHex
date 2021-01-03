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
        static void post(Events eventType, const void *userData);
        static void subscribe(Events eventType, void *owner, std::function<void(const void*)> callback);
        static void unsubscribe(Events eventType, void *sender);
    };

}