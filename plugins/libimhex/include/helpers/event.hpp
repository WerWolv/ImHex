#pragma once

#include <hex.hpp>

#include <vector>
#include <functional>

namespace hex {

    enum class Events : u32 {
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
        ProjectFileLoad,

        SettingsChanged,


        /* This is not a real event but a flag to show all events after this one are plugin ones */
        Events_BuiltinEnd
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