#pragma once

#include <hex.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/event_manager.hpp>

/* Forward declarations */
namespace pl::ptrn { class Pattern; }

/* Interaction requests definitions */
namespace hex {

    /**
     * @brief Requests a selection change in the Hex editor
     *
     * This request is handled by the Hex editor, which proceeds to check if the selection is valid.
     * If it is invalid, the Hex editor fires the `EventRegionSelected` event with nullptr region info.
     *
     * @param region the region that should be selected
     */
    EVENT_DEF(RequestHexEditorSelectionChange, ImHexApi::HexEditor::ProviderRegion);

    /**
     * @brief Requests the Pattern editor to move selection
     *
     * Requests the Pattern editor to move the cursor's position to reflect the user's click or movement.
     *
     * @param line the target line
     * @param column the target column
     */
    EVENT_DEF(RequestPatternEditorSelectionChange, u32, u32);

    /**
     * @brief Requests a jump to a given pattern
     *
     * This request is fired by the Hex editor when the user asks to jump to the pattern.
     * It is then caught and reflected by the Pattern data component.
     *
     * @param pattern the pattern to jump to
     */
    EVENT_DEF(RequestJumpToPattern, const pl::ptrn::Pattern*);

    /**
     * @brief Requests to add a bookmark
     *
     * @param region the region to be bookmarked
     * @param name the bookmark's name
     * @param comment a comment
     * @param color the color
     * @param id the bookmark's unique ID
     */
    EVENT_DEF(RequestAddBookmark, Region, std::string, std::string, color_t, u64*);

    /**
     * @brief Requests a bookmark removal
     *
     * @param id the bookmark's unique ID
     */
    EVENT_DEF(RequestRemoveBookmark, u64);

    /**
     * @brief Request the Pattern editor to set its code
     *
     * This request allows the rest of ImHex to interface with the Pattern editor component, by setting its code.
     * This allows for `.hexpat` file loading, and more.
     *
     * @param code the code's string
     */
    EVENT_DEF(RequestSetPatternLanguageCode, std::string);

    /**
     * @brief Requests the Pattern editor to run the current code
     *
     */
    EVENT_DEF(RequestTriggerPatternEvaluation);

    /**
     * @brief Requests ImHex to open and process a file
     *
     * @param path the file's path
     */
    EVENT_DEF(RequestOpenFile, std::fs::path);

    /**
     * @brief Adds a virtual file in the Pattern editor
     *
     * @param path the file's path
     * @param data the file's data
     * @param region the impacted region
     */
    EVENT_DEF(RequestAddVirtualFile, std::fs::path, std::vector<u8>, Region);

    /**
     * @brief Requests the command palette to be opened
     */
    EVENT_DEF(RequestOpenCommandPalette);

}
