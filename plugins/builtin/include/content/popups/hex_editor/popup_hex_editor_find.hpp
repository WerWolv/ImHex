#pragma once

#include "content/views/view_hex_editor.hpp"
#include "hex/api/task_manager.hpp"
#include <atomic>
#include <optional>
#include <string>

using namespace hex;

namespace hex::plugin::builtin {
    
class PopupFind : public ViewHexEditor::Popup {
public:
    PopupFind(ViewHexEditor *editor) noexcept;
    ~PopupFind() override;

    void draw(ViewHexEditor *editor) override;
    
private:
    void drawButtons();
    std::optional<Region> findSequence(const std::vector<u8> &sequence, bool backwards);

    std::string m_input;
    std::optional<u64> m_searchPosition, m_nextSearchPosition;

    std::optional<Region> m_searchFoundRegion = Region::Invalid();

    bool m_requestFocus = true;
    std::atomic<bool> m_requestSearch = false;
    std::atomic<bool> m_backwards    = false;
    std::atomic<bool> m_reachedEnd   = false;
    std::atomic<bool> m_isStringMode = false;

    enum class StringModeEncoding : u8 {
        UTF8,
        UTF16,
        UTF32
    };

    enum class StringModeEndianness : u8 {
        Little,
        Big
    };

    std::atomic<StringModeEncoding> m_stringModeEncoding = StringModeEncoding::UTF8;
    std::atomic<StringModeEndianness> m_stringModeEndianess = StringModeEndianness::Little;

    TaskHolder m_searchTask;
};

} // namespace hex::plugin::builtin