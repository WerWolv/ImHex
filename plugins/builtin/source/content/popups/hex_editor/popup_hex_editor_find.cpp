#include "content/popups/hex_editor/popup_hex_editor_find.hpp"
#include "content/views/view_hex_editor.hpp"
#include "hex/api/content_registry.hpp"
#include "hex/providers/buffered_reader.hpp"
#include <hex/helpers/crypto.hpp>
#include <codecvt>

using namespace hex;
using namespace hex::plugin::builtin;

PopupFind::PopupFind(ViewHexEditor* editor) noexcept {
    EventRegionSelected::subscribe(this, [this](Region region) {
        m_searchFoundRegion = region;
    });

    m_searchFoundRegion = editor->getSelection();
}

PopupFind::~PopupFind() { 
    EventRegionSelected::unsubscribe(this);
}

void PopupFind::draw(ViewHexEditor *editor) {
    std::vector<u8> searchSequence;

    ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.file.search"_lang);
    if (ImGui::BeginTabBar("##find_tabs")) {
        if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.search.hex"_lang)) {
            m_isStringMode = false;
            if (ImGuiExt::InputTextIcon("##input", ICON_VS_SYMBOL_NUMERIC, m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsHexadecimal)) {
                if (!m_input.empty()) {
                    m_requestSearch = true;
                    m_backwards = false;
                }
            }
            this->drawButtons();
            if (m_requestSearch) {
                searchSequence = crypt::decode16(m_input);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.search.string"_lang)) {
            m_isStringMode = true;
            if (ImGuiExt::InputTextIcon("##input", ICON_VS_SYMBOL_KEY, m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                if (!m_input.empty()) {
                    m_requestSearch = true;
                    m_backwards = false;
                }
            }
            this->drawButtons();
            constexpr static std::array StringModeEncodingNames = {
                "UTF-8",
                "UTF-16",
                "UTF-32"
            };
            if (ImGui::BeginCombo("hex.builtin.view.hex_editor.search.string.encoding"_lang,
                StringModeEncodingNames[static_cast<u8>(m_stringModeEncoding.load())])) {
                    if (ImGui::Selectable(StringModeEncodingNames[0], m_stringModeEncoding == StringModeEncoding::UTF8))
                        m_stringModeEncoding = StringModeEncoding::UTF8;
                    if (ImGui::Selectable(StringModeEncodingNames[1], m_stringModeEncoding == StringModeEncoding::UTF16))
                        m_stringModeEncoding = StringModeEncoding::UTF16;
                    if (ImGui::Selectable(StringModeEncodingNames[2], m_stringModeEncoding == StringModeEncoding::UTF32))
                        m_stringModeEncoding = StringModeEncoding::UTF32;
                    ImGui::EndCombo();
            }
            constexpr static std::array StringModeEndianessNames = {
                    "Little endian",
                    "Big endian"
            };
            if (ImGui::BeginCombo("hex.builtin.view.hex_editor.search.string.endianess"_lang,
                StringModeEndianessNames[static_cast<u8>(m_stringModeEndianess.load())])) {
                    if (ImGui::Selectable(StringModeEndianessNames[0], m_stringModeEndianess == StringModeEndianness::Little))
                        m_stringModeEndianess = StringModeEndianness::Little;
                    if (ImGui::Selectable(StringModeEndianessNames[1], m_stringModeEndianess == StringModeEndianness::Big))
                        m_stringModeEndianess = StringModeEndianness::Big;
                    ImGui::EndCombo();
            }
            if (m_requestSearch) {
                searchSequence.clear();

                if (!m_isStringMode) // String
                {
                    std::endian nativeEndianess = std::endian::native;
                    if (m_stringModeEndianess == StringModeEndianness::Big)
                        nativeEndianess = std::endian::big;
                    else if (m_stringModeEndianess == StringModeEndianness::Little)
                        nativeEndianess = std::endian::little;
                        
                    auto swapEndianess = [](auto &value, std::endian endian, StringModeEncoding encoding) {
                            if (encoding == StringModeEncoding::UTF16 || encoding == StringModeEncoding::UTF32) {
                                if ((endian == std::endian::big && std::endian::big != std::endian::native) ||
                                    (endian == std::endian::little && std::endian::little != std::endian::native)) {
                                    if constexpr (sizeof(value) == 2)
                                        value = __builtin_bswap16(value);
                                    else if constexpr (sizeof(value) == 4)
                                        value = __builtin_bswap32(value);
                                }
                            }
                    };

                    switch (m_stringModeEncoding.load()) {
                        case StringModeEncoding::UTF8: {
                            std::copy(m_input.begin(), m_input.end(), std::back_inserter(searchSequence));
                        }
                            break;
                        case StringModeEncoding::UTF16: {
                            std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
                            std::u16string utf16 = convert.from_bytes(m_input);
                            for (auto &c : utf16)
                                swapEndianess(c, nativeEndianess, StringModeEncoding::UTF16);
                            std::copy(reinterpret_cast<const u8 *>(utf16.data()), reinterpret_cast<const u8 *>(utf16.data() + utf16.size()), std::back_inserter(searchSequence));
                        }
                            break;
                        case StringModeEncoding::UTF32: {
                            std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
                            std::u32string utf32 = convert.from_bytes(m_input);
                            for (auto &c : utf32)
                                swapEndianess(c, nativeEndianess, StringModeEncoding::UTF32);
                            std::copy(reinterpret_cast<const u8 *>(utf32.data()), reinterpret_cast<const u8 *>(utf32.data() + utf32.size()), std::back_inserter(searchSequence));
                        } 
                            break;
                    }
                }
                else // Bytes
                {
                    std::copy(m_input.begin(), m_input.end(), std::back_inserter(searchSequence));

                    if (!searchSequence.empty() && searchSequence.back() == 0x00)
                    {
                        searchSequence.pop_back();
                    }
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    auto canSearch = !m_searchTask.isRunning() && !searchSequence.empty() && m_requestSearch;
    if (canSearch) {
        m_searchTask = TaskManager::createTask("hex.ui.common.processing", ImHexApi::Provider::get()->getActualSize(), [this, editor, searchSequence](auto &) {
            auto region = this->findSequence(searchSequence, m_backwards);
            if (region.has_value()) {
                m_searchFoundRegion = region.value();
                if (editor->getSelection() != region) {
                    TaskManager::doLater([editor, region]{
                        editor->setSelection(region->getStartAddress(), region->getEndAddress());
                        editor->jumpToSelection();
                    });
                }
                m_reachedEnd = false;
            } else {
                m_reachedEnd = true;
            }
            m_requestSearch = false;
            m_requestFocus = true;
        });
    }
}

void PopupFind::drawButtons() {
    const auto ButtonSize = ImVec2(ImGui::CalcTextSize(ICON_VS_SEARCH).x, ImGui::GetTextLineHeight()) + ImGui::GetStyle().CellPadding * 2;
    const auto ButtonColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

    if (m_requestFocus) {
        ImGui::SetKeyboardFocusHere(-1);
        m_requestFocus = false;
    }
    ImGui::BeginDisabled(m_searchTask.isRunning());
    {
        ImGui::SameLine();
        if (ImGuiExt::IconButton(ICON_VS_ARROW_UP "##up", ButtonColor, ButtonSize)) {
            m_requestSearch = true;
            m_backwards = true;
        }
        ImGui::SameLine();
        if (ImGuiExt::IconButton(ICON_VS_ARROW_DOWN "##down", ButtonColor, ButtonSize)) {
            m_requestSearch = true;
            m_backwards = false;
        }


        const static auto SearchPositionFormat = "hex.builtin.view.hex_editor.search.position"_lang;

        // Status label
        if(m_reachedEnd)
        {
            auto statusString = "No more results";
            ImGui::SameLine();
            ImGui::TextUnformatted(statusString);
        }
    }
    ImGui::EndDisabled();
}

std::optional<Region> PopupFind::findSequence(const std::vector<u8> &sequence, bool backwards) {
    auto provider = ImHexApi::Provider::get();
    prv::ProviderReader reader(provider);
    
    auto sequenceSize = sequence.size();
    auto startAbsolutePosition = provider->getBaseAddress();
    auto endAbsolutePosition = provider->getBaseAddress() + provider->getActualSize() - 1;

    constexpr static auto searchFunction = [](const auto &haystackBegin, const auto &haystackEnd, const auto &needleBegin, const auto &needleEnd) {
        return std::search(haystackBegin, haystackEnd, needleBegin, needleEnd);
    };

    if (!backwards) { // Forwards
        reader.seek(m_searchFoundRegion->getStartAddress() + 1);
        reader.setEndAddress(endAbsolutePosition);
        auto occurrence = searchFunction(reader.begin(), reader.end(), sequence.begin(), sequence.end());
        if (occurrence != reader.end()) {
            return Region { occurrence.getAddress(), sequence.size() };
        }
    } else { // Backwardss
        reader.seek(startAbsolutePosition);
        reader.setEndAddress(m_searchFoundRegion->getEndAddress() - 1);
        auto occurrence = searchFunction(reader.rbegin(), reader.rend(), sequence.rbegin(), sequence.rend());
        if (occurrence != reader.rend()) {
            return Region { occurrence.getAddress() - (sequence.size() - 1), sequence.size() };
        }
    }

    return std::nullopt;
}

