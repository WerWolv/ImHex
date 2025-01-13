#pragma once

#include <hex.hpp>
#include <hex/api/event_manager.hpp>

/* Forward declarations */
namespace pl::ptrn { class Pattern; }

namespace hex {

    EVENT_DEF(RequestHexEditorSelectionChange, Region);
    EVENT_DEF(RequestPatternEditorSelectionChange, u32, u32);
    EVENT_DEF(RequestJumpToPattern, const pl::ptrn::Pattern*);
    EVENT_DEF(RequestAddBookmark, Region, std::string, std::string, color_t, u64*);
    EVENT_DEF(RequestRemoveBookmark, u64);
    EVENT_DEF(RequestSetPatternLanguageCode, std::string);
    EVENT_DEF(RequestRunPatternCode);
    EVENT_DEF(RequestLoadPatternLanguageFile, std::fs::path);
    EVENT_DEF(RequestSavePatternLanguageFile, std::fs::path);

    EVENT_DEF(RequestOpenFile, std::fs::path);

    EVENT_DEF(RequestAddVirtualFile, std::fs::path, std::vector<u8>, Region);
}
