#pragma once

#include <hex.hpp>

#include <any>
#include <functional>
#include <vector>

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
        std::function<std::any(const std::any&)> callback;
    };

    class EventManager {
    public:
        static std::vector<std::any> post(Events eventType, const std::any &userData);
        static void subscribe(Events eventType, void *owner, std::function<std::any(const std::any&)> callback);
        static void unsubscribe(Events eventType, void *sender);
    };

}