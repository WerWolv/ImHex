#pragma once

#include <hex.hpp>
#include <hex/api/event_manager.hpp>

namespace hex {

    EVENT_DEF(RequestAddInitTask, std::string, bool, std::function<bool()>);
    EVENT_DEF(RequestAddExitTask, std::string, std::function<bool()>);
    EVENT_DEF(RequestOpenWindow, std::string);
    EVENT_DEF(RequestHexEditorSelectionChange, Region);
    EVENT_DEF(RequestPatternEditorSelectionChange, u32, u32);
    EVENT_DEF(RequestJumpToPattern, const pl::ptrn::Pattern*);
    EVENT_DEF(RequestAddBookmark, Region, std::string, std::string, color_t, u64*);
    EVENT_DEF(RequestRemoveBookmark, u64);
    EVENT_DEF(RequestSetPatternLanguageCode, std::string);
    EVENT_DEF(RequestRunPatternCode);
    EVENT_DEF(RequestLoadPatternLanguageFile, std::fs::path);
    EVENT_DEF(RequestSavePatternLanguageFile, std::fs::path);
    EVENT_DEF(RequestUpdateWindowTitle);
    EVENT_DEF(RequestCloseImHex, bool);
    EVENT_DEF(RequestRestartImHex);
    EVENT_DEF(RequestOpenFile, std::fs::path);
    EVENT_DEF(RequestChangeTheme, std::string);
    EVENT_DEF(RequestOpenPopup, std::string);
    EVENT_DEF(RequestAddVirtualFile, std::fs::path, std::vector<u8>, Region);

    /**
     * @brief Creates a provider from it's unlocalized name, and add it to the provider list
    */
    EVENT_DEF(RequestCreateProvider, std::string, bool, bool, hex::prv::Provider **);
    EVENT_DEF(RequestInitThemeHandlers);

    EVENT_DEF(RequestStartMigration);

}
