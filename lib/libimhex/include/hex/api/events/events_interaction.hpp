#pragma once

#include <hex/api/event_manager.hpp>

namespace hex {

    EVENT_DEF(EventFileLoaded, std::fs::path);
    EVENT_DEF(EventDataChanged, prv::Provider *);
    EVENT_DEF(EventHighlightingChanged);
    EVENT_DEF(EventRegionSelected, ImHexApi::HexEditor::ProviderRegion);
    EVENT_DEF(EventThemeChanged);
    EVENT_DEF(EventOSThemeChanged);

    EVENT_DEF(EventBookmarkCreated, ImHexApi::Bookmarks::Entry&);

    /**
     * @brief Called upon creation of an IPS patch.
     * As for now, the event only serves a purpose for the achievement unlock.
     */
    EVENT_DEF(EventPatchCreated, const u8*, u64, const PatchKind);
    EVENT_DEF(EventPatternEvaluating);
    EVENT_DEF(EventPatternExecuted, const std::string&);
    EVENT_DEF(EventPatternEditorChanged, const std::string&);
    EVENT_DEF(EventStoreContentDownloaded, const std::fs::path&);
    EVENT_DEF(EventStoreContentRemoved, const std::fs::path&);
    EVENT_DEF(EventAchievementUnlocked, const Achievement&);
    EVENT_DEF(EventSearchBoxClicked, u32);

    EVENT_DEF(EventFileDragged, bool);
    EVENT_DEF(EventFileDropped, std::fs::path);

}