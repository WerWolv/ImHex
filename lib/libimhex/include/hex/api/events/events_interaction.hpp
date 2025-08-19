#pragma once

#include <hex/api/imhex_api/bookmarks.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/helpers/patches.hpp>

/* Forward declarations */
namespace hex { class Achievement; }

/* Interaction events definitions */
namespace hex {

    /**
     * @brief Signals a file was loaded
     *
     * FIXME: this event is unused and should be scrapped.
     *
     * @param path the loaded file's path
     */
    EVENT_DEF(EventFileLoaded, std::fs::path);

    /**
     * @brief Signals a change in the current data
     *
     * Enables provider reaction to data change, especially the data inspector.
     *
     * This is caused by the following:
     * - an explicit provider reload, requested by the user (Ctrl+R)
     * - any user action that results in the creation of an "undo" stack action (generally a data modification)
     *
     * @param provider the Provider subject to the data change
     */
    EVENT_DEF(EventDataChanged, prv::Provider *);

    /**
     * @brief Signals a change in highlighting
     *
     * The event's only purpose is for the Hex editor to clear highlights when receiving this event.
     */
    EVENT_DEF(EventHighlightingChanged);

    /**
     * @brief Informs of a provider region being selected
     *
     * This is very generally used to signal user actions that select a specific region within the provider.
     * It is also used to pass on regions when the provider changes.
     *
     * @param providerRegion the provider-aware region being selected
     */
    EVENT_DEF(EventRegionSelected, ImHexApi::HexEditor::ProviderRegion);

    /**
     * @brief Signals a theme change
     *
     * On Windows OS, this is used to reflect the theme color onto the window frame.
     */
    EVENT_DEF(EventThemeChanged);

    /**
     * @brief Signals that a bookmark was created
     *
     * For now, this event's only purpose is to unlock an achievement.
     *
     * @param entry the new bookmark
     */
    EVENT_DEF(EventBookmarkCreated, ImHexApi::Bookmarks::Entry&);

    /**
     * @brief Called upon creation of an IPS patch.
     * As for now, the event only serves a purpose for the achievement unlock.
     *
     * @param data the pointer to the patch content's start
     * @param size the patch data size
     * @param kind the patch's kind
     */
    EVENT_DEF(EventPatchCreated, const u8*, u64, const PatchKind);

    /**
     * @brief Signals the beginning of evaluation of the current pattern
     *
     * This allows resetting the drawer view for the pattern data while we wait for the execution completion.
     */
    EVENT_DEF(EventPatternEvaluating);

    /**
     * @brief Signals the completion of the pattern evaluation
     *
     * This causes another reset in the drawer view, to refresh the table displayed to the user.
     *
     * @param code the execution's status code
     */
    EVENT_DEF(EventPatternExecuted, const std::string&);

    /**
     * @brief Denotes when pattern editor has changed
     *
     * FIXME: this event is unused and should be scrapped.
     */
    EVENT_DEF(EventPatternEditorChanged, const std::string&);

    /**
     * @brief Signals that a Content Store item was downloaded
     *
     * FIXME: this event is unused and should be scrapped.
     *
     * @param path the item's path on the filesystem
     */
    EVENT_DEF(EventStoreContentDownloaded, const std::fs::path&);

    /**
     * @brief Signals the removal of a Content Store item
     *
     * Note: at the time of the event firing, the item has already been removed from the filesystem.
     *
     * FIXME: this event is unused and should be scrapped.
     *
     * @param path the item's old file path where it used to be in the filesystem
     */
    EVENT_DEF(EventStoreContentRemoved, const std::fs::path&);

    /**
     * @brief Signals the unlocking of an achievement
     *
     * This is used by the achievement manager to refresh the achievement display, as well as store progress to
     * the appropriate storage file.
     *
     * @param achievement the achievement that was unlocked
     */
    EVENT_DEF(EventAchievementUnlocked, const Achievement&);

    /**
     * @brief Signals a click on the search box
     *
     * As there are different behaviours depending on the click (left or right) done by the user,
     * this allows the consequences of said click to be registered in their own components.
     *
     * @param button the ImGuiMouseButton's value
     */
    EVENT_DEF(EventSearchBoxClicked, u32);

    /**
     * @brief Updates on whether a file is being dragged into ImHex
     *
     * Allows ImGUi to display a file dragging information on screen when a file is being dragged.
     *
     * @param isFileDragged true if a file is being dragged
     */
    EVENT_DEF(EventFileDragged, bool);

    /**
     * @brief Triggers loading when a file is dropped
     *
     * The event fires when a file is dropped into ImHex, which passes it to file handlers to load it.
     *
     * @param path the dropped file's path
     */
    EVENT_DEF(EventFileDropped, std::fs::path);

}