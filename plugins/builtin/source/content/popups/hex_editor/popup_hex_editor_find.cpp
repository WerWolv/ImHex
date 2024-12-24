#include "content/popups/hex_editor/popup_hex_editor_find.hpp"

#include "content/views/view_hex_editor.hpp"

#include <hex/helpers/crypto.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <fonts/vscode_icons.hpp>

#include <bit>

namespace hex::plugin::builtin {

    PerProvider<std::string> PopupFind::s_inputString;
    PerProvider<PopupFind::SearchMode> PopupFind::s_searchMode;

    PopupFind::PopupFind(ViewHexEditor *editor) noexcept {
        EventRegionSelected::subscribe(this, [this](Region region) {
            m_foundRegion = region;
        });

        m_foundRegion = editor->getSelection();
    }

    PopupFind::~PopupFind() {
        EventRegionSelected::unsubscribe(this);
    }

    void PopupFind::draw(ViewHexEditor *editor) {
        auto lastMode = *s_searchMode;
        if (ImGui::BeginTabBar("##find_tabs")) {
            if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.search.hex"_lang)) {
                s_searchMode = SearchMode::ByteSequence;
                this->drawTabContents();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.search.string"_lang)) {
                s_searchMode = SearchMode::String;
                this->drawTabContents();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        if(ImGuiExt::IconHyperlink(ICON_VS_SEARCH, "hex.builtin.view.hex_editor.search.advanced"_lang)) {
            TaskManager::doLater([editor] {
                const auto& view = ContentRegistry::Views::getViewByName("hex.builtin.view.find.name");

                view->getWindowOpenState() = true;
                ImGui::SetWindowFocus(view->getName().c_str());
                editor->closePopup();
            });
        }

        if (lastMode != *s_searchMode) {
            m_requestFocus = true;
            s_inputString->clear();
        }

        const auto doSearch = [this, editor](auto &) {
            auto region = this->findByteSequence(m_searchByteSequence);

            if (region.has_value()) {
                m_foundRegion = region.value();

                if (editor->getSelection() != region) {
                    TaskManager::doLater([editor, region] {
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
        };

        if (m_requestSearch) {
            this->processInputString();

            if (!m_searchTask.isRunning() && !m_searchByteSequence.empty()) {
                m_searchTask = TaskManager::createTask("hex.ui.common.processing"_lang,
                                                       ImHexApi::Provider::get()->getActualSize(),
                                                       doSearch);
            }
            m_requestSearch = false;
        }

    }

    void PopupFind::drawSearchDirectionButtons() {
        const auto ButtonSize = ImVec2(ImGui::CalcTextSize(ICON_VS_SEARCH).x, ImGui::GetTextLineHeight()) +
                                ImGui::GetStyle().CellPadding * 2;
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
                m_searchBackwards = true;
            }

            ImGui::SameLine();

            if (ImGuiExt::IconButton(ICON_VS_ARROW_DOWN "##down", ButtonColor, ButtonSize)) {
                m_requestSearch = true;
                m_searchBackwards = false;
            }
        }
        ImGui::EndDisabled();
    }

    void PopupFind::drawTabContents() {
        constexpr static std::array EncodingNames = {
                "hex.builtin.view.hex_editor.search.string.encoding.utf8",
                "hex.builtin.view.hex_editor.search.string.encoding.utf16",
                "hex.builtin.view.hex_editor.search.string.encoding.utf32"
        };

        constexpr static std::array EndiannessNames = {
                "hex.builtin.view.hex_editor.search.string.endianness.little",
                "hex.builtin.view.hex_editor.search.string.endianness.big"
        };

        const char *searchInputIcon = nullptr;
        ImGuiInputTextFlags searchInputFlags = 0;

        // Set search input icon and flags
        switch (*s_searchMode) {
            case SearchMode::ByteSequence:
                searchInputIcon = ICON_VS_SYMBOL_NUMERIC;
                searchInputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll |
                                   ImGuiInputTextFlags_CharsHexadecimal;
                break;
            case SearchMode::String:
                searchInputIcon = ICON_VS_SYMBOL_KEY;
                searchInputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
                break;
            default:
                break;
        }

        // Draw search input
        if (ImGuiExt::InputTextIcon("##input", searchInputIcon, s_inputString, searchInputFlags)) {
            if (!s_inputString->empty()) {
                m_requestSearch = true;
                m_searchBackwards = ImGui::GetIO().KeyShift;
            }
        }

        // Draw search direction buttons
        ImGui::BeginDisabled(s_inputString->empty());
        this->drawSearchDirectionButtons();
        ImGui::EndDisabled();

        // Draw search options for string search
        if (*s_searchMode == SearchMode::String) {
            if (ImGui::BeginCombo("hex.builtin.view.hex_editor.search.string.encoding"_lang, Lang(EncodingNames[std::to_underlying<Encoding>(m_stringEncoding.load())]))) {
                if (ImGui::Selectable(Lang(EncodingNames[0]), m_stringEncoding == Encoding::UTF8)) {
                    m_stringEncoding = Encoding::UTF8;
                }

                if (ImGui::Selectable(Lang(EncodingNames[1]), m_stringEncoding == Encoding::UTF16)) {
                    m_stringEncoding = Encoding::UTF16;
                }

                if (ImGui::Selectable(Lang(EncodingNames[2]), m_stringEncoding == Encoding::UTF32)) {
                    m_stringEncoding = Encoding::UTF32;
                }

                ImGui::EndCombo();
            }

            ImGui::BeginDisabled(m_stringEncoding == Encoding::UTF8);
            {
                if (ImGui::BeginCombo("hex.builtin.view.hex_editor.search.string.endianness"_lang, Lang(EndiannessNames[std::to_underlying<Endianness>(m_stringEndianness.load())]))) {
                    if (ImGui::Selectable(Lang(EndiannessNames[0]), m_stringEndianness == Endianness::Little)) {
                        m_stringEndianness = Endianness::Little;
                    }

                    if (ImGui::Selectable(Lang(EndiannessNames[1]), m_stringEndianness == Endianness::Big)) {
                        m_stringEndianness = Endianness::Big;
                    }

                    ImGui::EndCombo();
                }
            }
            ImGui::EndDisabled();
        }

        if (m_reachedEnd) {
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.search.no_more_results"_lang);
        } else {
            ImGui::NewLine();
        }
    }

    std::optional<Region> PopupFind::findByteSequence(const std::vector<u8> &sequence) const {
        if (sequence.empty())
            return std::nullopt;

        auto provider = ImHexApi::Provider::get();
        if (provider == nullptr)
            return std::nullopt;

        const auto providerSize = provider->getActualSize();
        if (providerSize == 0x00)
            return std::nullopt;

        prv::ProviderReader reader(provider);

        auto startAbsolutePosition = provider->getBaseAddress();
        auto endAbsolutePosition = provider->getBaseAddress() + providerSize - 1;

        constexpr static auto searchFunction = [](const auto &haystackBegin, const auto &haystackEnd,
                                                  const auto &needleBegin, const auto &needleEnd) {
            return std::search(haystackBegin, haystackEnd, needleBegin, needleEnd);
        };

        if (!m_searchBackwards) {
            if (m_reachedEnd || !m_foundRegion.has_value()) {
                reader.seek(startAbsolutePosition);
            } else {
                reader.seek(m_foundRegion->getStartAddress() + 1);
            }
            reader.setEndAddress(endAbsolutePosition);

            auto occurrence = searchFunction(reader.begin(), reader.end(), sequence.begin(), sequence.end());
            if (occurrence != reader.end()) {
                return Region{occurrence.getAddress(), sequence.size()};
            }
        } else {
            if (m_reachedEnd || !m_foundRegion.has_value()) {
                reader.setEndAddress(endAbsolutePosition);
            } else {
                reader.setEndAddress(m_foundRegion->getEndAddress() - 1);
            }
            reader.seek(startAbsolutePosition);

            auto occurrence = searchFunction(reader.rbegin(), reader.rend(), sequence.rbegin(), sequence.rend());
            if (occurrence != reader.rend()) {
                return Region{occurrence.getAddress() - (sequence.size() - 1), sequence.size()};
            }
        }

        return std::nullopt;
    }

    void PopupFind::processInputString() {
        m_searchByteSequence.clear();

        constexpr auto swapEndianness = [](auto &value, Encoding encoding, Endianness endianness) {
            if (encoding == Encoding::UTF16 || encoding == Encoding::UTF32) {
                std::endian endian = (endianness == Endianness::Little)
                                     ? std::endian::little
                                     : std::endian::big;
                value = hex::changeEndianness(value, endian);
            }
        };

        switch (*s_searchMode) {
            case SearchMode::ByteSequence: {
                m_searchByteSequence = crypt::decode16(s_inputString);
                break;
            }
            case SearchMode::String: {
                switch (m_stringEncoding) {
                    case Encoding::UTF8: {
                        std::copy_n(s_inputString->data(), s_inputString->size(),
                                  std::back_inserter(m_searchByteSequence));
                        break;
                    }
                    case Encoding::UTF16: {
                        auto utf16 = wolv::util::utf8ToUtf16(s_inputString);

                        for (auto &c: utf16) {
                            swapEndianness(c, Encoding::UTF16, m_stringEndianness);
                        }

                        std::copy(reinterpret_cast<const u8 *>(utf16.data()),
                                  reinterpret_cast<const u8 *>(utf16.data() + utf16.size()),
                                  std::back_inserter(m_searchByteSequence));
                        break;
                    }
                    case Encoding::UTF32: {
                        auto utf32 = wolv::util::utf8ToUtf32(s_inputString);

                        for (auto &c: utf32) {
                            swapEndianness(c, Encoding::UTF32, m_stringEndianness);
                        }

                        std::copy(reinterpret_cast<const u8 *>(utf32.data()),
                                  reinterpret_cast<const u8 *>(utf32.data() + utf32.size()),
                                  std::back_inserter(m_searchByteSequence));
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
            default:
                break;
        }
    }

    [[nodiscard]] UnlocalizedString PopupFind::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.file.search";
    }
}