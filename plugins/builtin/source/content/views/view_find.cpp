#include "content/views/view_find.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <array>
#include <regex>
#include <string>
#include <utility>

#include <llvm/Demangle/Demangle.h>

namespace hex::plugin::builtin {

    ViewFind::ViewFind() : View("hex.builtin.view.find.name") {
        const static auto HighlightColor = [] { return (ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarPurple) & 0x00FFFFFF) | 0x70000000; };

        ImHexApi::HexEditor::addBackgroundHighlightingProvider([this](u64 address, const u8* data, size_t size) -> std::optional<color_t> {
            hex::unused(data, size);

            auto provider = ImHexApi::Provider::get();

            if (!this->m_occurrenceTree[provider].findOverlapping(address, address).empty())
                return HighlightColor();
            else
                return std::nullopt;
        });

        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8* data, size_t size) {
            hex::unused(data, size);

            auto provider = ImHexApi::Provider::get();

            auto occurrences = this->m_occurrenceTree[provider].findOverlapping(address, address);
            if (occurrences.empty())
                return;

            ImGui::BeginTooltip();

            for (const auto &occurrence : occurrences) {
                ImGui::PushID(&occurrence);
                if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    {
                        const auto value = this->decodeValue(ImHexApi::Provider::get(), occurrence.value);

                        ImGui::ColorButton("##color", ImColor(HighlightColor()));
                        ImGui::SameLine(0, 10);
                        ImGui::TextFormatted("{}", value);

                        if (ImGui::GetIO().KeyShift) {
                            ImGui::Indent();
                            if (ImGui::BeginTable("##extra_info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("{}: ", "hex.builtin.common.region"_lang.get());
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("[ 0x{:08X} - 0x{:08X} ]", occurrence.value.region.getStartAddress(), occurrence.value.region.getEndAddress());

                                auto demangledValue = llvm::demangle(value);

                                if (value != demangledValue) {
                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::TextFormatted("{}: ", "hex.builtin.view.find.demangled"_lang.get());
                                    ImGui::TableNextColumn();
                                    ImGui::TextFormatted("{}", demangledValue);
                                }

                                ImGui::EndTable();
                            }
                            ImGui::Unindent();
                        }
                    }


                    ImGui::PushStyleColor(ImGuiCol_TableRowBg, HighlightColor());
                    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, HighlightColor());
                    ImGui::EndTable();
                    ImGui::PopStyleColor(2);
                }
                ImGui::PopID();
            }

            ImGui::EndTooltip();
        });
    }


    std::vector<ViewFind::BinaryPattern> ViewFind::parseBinaryPatternString(std::string string) {
        std::vector<BinaryPattern> result;

        if (string.length() < 2)
            return { };

        bool inString = false;
        while (string.length() > 0) {
            BinaryPattern pattern = { 0, 0 };
            if (string.starts_with("\"")) {
                inString = !inString;
                string = string.substr(1);
                continue;
            } else if (inString) {
                pattern = { 0xFF, u8(string.front()) };
                string = string.substr(1);
            } else if (string.starts_with("??")) {
                pattern = { 0x00, 0x00 };
                string = string.substr(2);
            } else if ((std::isxdigit(string.front()) || string.front() == '?') && string.length() >= 2) {
                const auto hex = string.substr(0, 2);

                for (const auto &c : hex) {
                    pattern.mask  <<= 4;
                    pattern.value <<= 4;

                    if (std::isxdigit(c)) {
                        pattern.mask |= 0x0F;

                        if (auto hexValue = hex::hexCharToValue(c); hexValue.has_value())
                            pattern.value |= hexValue.value();
                        else
                            return { };
                    } else if (c != '?') {
                        return { };
                    }
                }

                string = string.substr(2);
            } else if (std::isspace(string.front())) {
                string = string.substr(1);
                continue;
            } else {
                return { };
            }

            result.push_back(pattern);
        }

        if (inString)
            return { };

        return result;
    }


    std::vector<ViewFind::Occurrence> ViewFind::searchStrings(Task &task, prv::Provider *provider, hex::Region searchRegion, SearchSettings::Strings settings) {
        using enum SearchSettings::Strings::Type;

        std::vector<Occurrence> results;

        if (settings.type == ASCII_UTF16BE || settings.type == ASCII_UTF16LE) {
            auto newSettings = settings;

            newSettings.type = ASCII;
            auto asciiResults = searchStrings(task, provider, searchRegion, newSettings);
            std::copy(asciiResults.begin(), asciiResults.end(), std::back_inserter(results));

            if (settings.type == ASCII_UTF16BE) {
                newSettings.type = UTF16BE;
                auto utf16Results = searchStrings(task, provider, searchRegion, newSettings);
                std::copy(utf16Results.begin(), utf16Results.end(), std::back_inserter(results));
            } else if (settings.type == ASCII_UTF16LE) {
                newSettings.type = UTF16LE;
                auto utf16Results = searchStrings(task, provider, searchRegion, newSettings);
                std::copy(utf16Results.begin(), utf16Results.end(), std::back_inserter(results));
            }

            return results;
        }

        auto reader = prv::BufferedReader(provider);
        reader.seek(searchRegion.getStartAddress());
        reader.setEndAddress(searchRegion.getEndAddress());

        const Occurrence::DecodeType decodeType = [&]{
            if (settings.type == ASCII)
                return Occurrence::DecodeType::ASCII;
            else if (settings.type == SearchSettings::Strings::Type::UTF16BE)
                return Occurrence::DecodeType::UTF16BE;
            else if (settings.type == SearchSettings::Strings::Type::UTF16LE)
                return Occurrence::DecodeType::UTF16LE;
            else
                return Occurrence::DecodeType::Binary;
        }();

        size_t countedCharacters = 0;
        u64 startAddress = reader.begin().getAddress();
        for (u8 byte : reader) {
            bool validChar =
                (settings.m_lowerCaseLetters    && std::islower(byte))  ||
                (settings.m_upperCaseLetters    && std::isupper(byte))  ||
                (settings.m_numbers             && std::isdigit(byte))  ||
                (settings.m_spaces              && std::isspace(byte))  ||
                (settings.m_underscores         && byte == '_')             ||
                (settings.m_symbols             && std::ispunct(byte))  ||
                (settings.m_lineFeeds           && byte == '\n');

            if (settings.type == UTF16LE) {
                // Check if second byte of UTF-16 encoded string is 0x00
                if (countedCharacters % 2 == 1)
                    validChar =  byte == 0x00;
            } else if (settings.type == UTF16BE) {
                // Check if first byte of UTF-16 encoded string is 0x00
                if (countedCharacters % 2 == 0)
                    validChar =  byte == 0x00;
            }

            if (validChar)
                countedCharacters++;
            else {
                if (countedCharacters >= size_t(settings.minLength)) {
                    if (!(settings.nullTermination && byte != 0x00)) {
                        results.push_back(Occurrence { Region { startAddress, countedCharacters }, decodeType });
                    }
                }

                startAddress += countedCharacters + 1;
                countedCharacters = 0;
                task.update(startAddress - searchRegion.getStartAddress());
            }
        }

        return results;
    }

    std::vector<ViewFind::Occurrence> ViewFind::searchSequence(Task &task, prv::Provider *provider, hex::Region searchRegion, SearchSettings::Bytes settings) {
        std::vector<Occurrence> results;

        auto reader = prv::BufferedReader(provider);
        reader.seek(searchRegion.getStartAddress());
        reader.setEndAddress(searchRegion.getEndAddress());

        auto sequence = hex::decodeByteString(settings.sequence);
        auto occurrence = reader.begin();
        while (true) {
            occurrence = std::search(reader.begin(), reader.end(), std::boyer_moore_horspool_searcher(sequence.begin(), sequence.end()));
            if (occurrence == reader.end())
                break;

            auto address = occurrence.getAddress();
            reader.seek(address + 1);
            results.push_back(Occurrence{ Region { address, sequence.size() }, Occurrence::DecodeType::Binary });
            task.update(address - searchRegion.getStartAddress());
        }

        return results;
    }

    std::vector<ViewFind::Occurrence> ViewFind::searchRegex(Task &task, prv::Provider *provider, hex::Region searchRegion, SearchSettings::Regex settings) {
        auto stringOccurrences = searchStrings(task, provider, searchRegion, SearchSettings::Strings {
            .minLength          = 1,
            .type               = SearchSettings::Strings::Type::ASCII,
            .m_lowerCaseLetters = true,
            .m_upperCaseLetters = true,
            .m_numbers          = true,
            .m_underscores      = true,
            .m_symbols          = true,
            .m_spaces           = true,
            .m_lineFeeds        = true
        });

        std::vector<Occurrence> result;
        std::regex regex(settings.pattern);
        for (const auto &occurrence : stringOccurrences) {
            std::string string(occurrence.region.getSize(), '\x00');
            provider->read(occurrence.region.getStartAddress(), string.data(), occurrence.region.getSize());

            if (std::regex_match(string, regex))
                result.push_back(occurrence);
        }

        return result;
    }

    std::vector<ViewFind::Occurrence> ViewFind::searchBinaryPattern(Task &task, prv::Provider *provider, hex::Region searchRegion, SearchSettings::BinaryPattern settings) {
        std::vector<Occurrence> results;

        auto reader = prv::BufferedReader(provider);
        reader.seek(searchRegion.getStartAddress());
        reader.setEndAddress(searchRegion.getEndAddress());

        u32 matchedBytes = 0;
        const size_t patternSize = settings.pattern.size();

        for (auto it = reader.begin(); it != reader.end(); ++it) {
            auto byte = *it;

            if ((byte & settings.pattern[matchedBytes].mask) == settings.pattern[matchedBytes].value) {
                matchedBytes++;
                if (matchedBytes == settings.pattern.size()) {
                    auto occurrenceAddress = it.getAddress() - (patternSize - 1);

                    results.push_back(Occurrence { Region { occurrenceAddress, patternSize }, Occurrence::DecodeType::Binary });
                    task.update(occurrenceAddress);
                    it.setAddress(occurrenceAddress);
                    matchedBytes = 0;
                }
            } else {
                if (matchedBytes > 0)
                    it -= matchedBytes;
                matchedBytes = 0;
            }
        }

        return results;
    }

    void ViewFind::runSearch() {
        Region searchRegion = [this]{
            if (this->m_searchSettings.range == ui::SelectedRegion::EntireData || !ImHexApi::HexEditor::isSelectionValid()) {
                auto provider = ImHexApi::Provider::get();
                return Region { provider->getBaseAddress(), provider->getActualSize() };
            } else {
                return ImHexApi::HexEditor::getSelection().value();
            }
        }();

        this->m_searchTask = TaskManager::createTask("hex.builtin.view.find.searching", searchRegion.getSize(), [this, settings = this->m_searchSettings, searchRegion](auto &task) {
            auto provider = ImHexApi::Provider::get();

            switch (settings.mode) {
                using enum SearchSettings::Mode;
                case Strings:
                    this->m_foundOccurrences[provider] = searchStrings(task, provider, searchRegion, settings.strings);
                    break;
                case Sequence:
                    this->m_foundOccurrences[provider] = searchSequence(task, provider, searchRegion, settings.bytes);
                    break;
                case Regex:
                    this->m_foundOccurrences[provider] = searchRegex(task, provider, searchRegion, settings.regex);
                    break;
                case BinaryPattern:
                    this->m_foundOccurrences[provider] = searchBinaryPattern(task, provider, searchRegion, settings.binaryPattern);
                    break;
            }

            this->m_sortedOccurrences[provider] = this->m_foundOccurrences[provider];

            OccurrenceTree::interval_vector intervals;
            for (const auto &occurrence : this->m_foundOccurrences[provider])
                intervals.push_back(OccurrenceTree::interval(occurrence.region.getStartAddress(), occurrence.region.getEndAddress(), occurrence));
            this->m_occurrenceTree[provider] = std::move(intervals);
        });
    }

    std::string ViewFind::decodeValue(prv::Provider *provider, Occurrence occurrence) const {
        std::vector<u8> bytes(std::min<size_t>(occurrence.region.getSize(), 128));
        provider->read(occurrence.region.getStartAddress(), bytes.data(), bytes.size());

        std::string result;
        switch (this->m_decodeSettings.mode) {
            using enum SearchSettings::Mode;

            case Strings:
            {
                switch (occurrence.decodeType) {
                    using enum Occurrence::DecodeType;
                    case Binary:
                    case ASCII:
                        result = hex::encodeByteString(bytes);
                        break;
                    case UTF16LE:
                        for (size_t i = 0; i < bytes.size(); i += 2)
                            result += hex::encodeByteString({ bytes[i] });
                        break;
                    case UTF16BE:
                        for (size_t i = 1; i < bytes.size(); i += 2)
                            result += hex::encodeByteString({ bytes[i] });
                        break;

                }
            }
                break;
            case Sequence:
            case Regex:
            case BinaryPattern:
                result = hex::encodeByteString(bytes);
                break;
        }

        return result;
    }

    static void drawContextMenu(const std::string &value) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered()) {
            ImGui::OpenPopup("FindContextMenu");
        }

        if (ImGui::BeginPopup("FindContextMenu")) {
            if (ImGui::MenuItem("hex.builtin.view.find.context.copy"_lang))
                ImGui::SetClipboardText(value.c_str());
            if (ImGui::MenuItem("hex.builtin.view.find.context.copy_demangle"_lang))
                ImGui::SetClipboardText(llvm::demangle(value).c_str());

            ImGui::EndPopup();
        }
    }

    void ViewFind::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.find.name").c_str(), &this->getWindowOpenState())) {
            auto provider = ImHexApi::Provider::get();

            ImGui::BeginDisabled(this->m_searchTask.isRunning());
            {
                ui::regionSelectionPicker(&this->m_searchSettings.range, true, true);

                ImGui::NewLine();

                if (ImGui::BeginTabBar("SearchMethods")) {
                    auto &mode = this->m_searchSettings.mode;
                    if (ImGui::BeginTabItem("hex.builtin.view.find.strings"_lang)) {
                        auto &settings = this->m_searchSettings.strings;
                        mode = SearchSettings::Mode::Strings;

                        ImGui::InputInt("hex.builtin.view.find.strings.min_length"_lang, &settings.minLength, 1, 1);
                        if (settings.minLength < 1)
                            settings.minLength = 1;

                        const std::array<std::string, 5> StringTypes = {
                            "hex.builtin.common.encoding.ascii"_lang,
                            "hex.builtin.common.encoding.utf16le"_lang,
                            "hex.builtin.common.encoding.utf16be"_lang,
                            hex::format("{} + {}", "hex.builtin.common.encoding.ascii"_lang, "hex.builtin.common.encoding.utf16le"_lang),
                            hex::format("{} + {}", "hex.builtin.common.encoding.ascii"_lang, "hex.builtin.common.encoding.utf16be"_lang)
                        };

                        if (ImGui::BeginCombo("hex.builtin.common.type"_lang, StringTypes[std::to_underlying(settings.type)].c_str())) {
                            for (size_t i = 0; i < StringTypes.size(); i++) {
                                auto type = static_cast<SearchSettings::Strings::Type>(i);

                                if (ImGui::Selectable(StringTypes[i].c_str(), type == settings.type))
                                    settings.type = type;
                            }
                            ImGui::EndCombo();
                        }

                        if (ImGui::CollapsingHeader("hex.builtin.view.find.strings.match_settings"_lang)) {
                            ImGui::Checkbox("hex.builtin.view.find.strings.null_term"_lang, &settings.nullTermination);

                            ImGui::Header("hex.builtin.view.find.strings.chars"_lang);
                            ImGui::Checkbox(hex::format("{} [a-z]", "hex.builtin.view.find.strings.lower_case"_lang.get()).c_str(), &settings.m_lowerCaseLetters);
                            ImGui::Checkbox(hex::format("{} [A-Z]", "hex.builtin.view.find.strings.upper_case"_lang.get()).c_str(), &settings.m_upperCaseLetters);
                            ImGui::Checkbox(hex::format("{} [0-9]", "hex.builtin.view.find.strings.numbers"_lang.get()).c_str(), &settings.m_numbers);
                            ImGui::Checkbox(hex::format("{} [_]", "hex.builtin.view.find.strings.underscores"_lang.get()).c_str(), &settings.m_underscores);
                            ImGui::Checkbox(hex::format("{} [!\"#$%...]", "hex.builtin.view.find.strings.symbols"_lang.get()).c_str(), &settings.m_symbols);
                            ImGui::Checkbox(hex::format("{} [ ]", "hex.builtin.view.find.strings.spaces"_lang.get()).c_str(), &settings.m_spaces);
                            ImGui::Checkbox(hex::format("{} [\\n]", "hex.builtin.view.find.strings.line_feeds"_lang.get()).c_str(), &settings.m_lineFeeds);
                        }

                        this->m_settingsValid = true;

                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("hex.builtin.view.find.sequences"_lang)) {
                        auto &settings = this->m_searchSettings.bytes;

                        mode = SearchSettings::Mode::Sequence;

                        ImGui::InputText("hex.builtin.common.value"_lang, settings.sequence);

                        this->m_settingsValid = !settings.sequence.empty();

                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("hex.builtin.view.find.regex"_lang)) {
                        auto &settings = this->m_searchSettings.regex;

                        mode = SearchSettings::Mode::Regex;

                        ImGui::InputText("hex.builtin.view.find.regex"_lang, settings.pattern);

                        try {
                            std::regex regex(settings.pattern);
                            this->m_settingsValid = true;
                        } catch (std::regex_error &e) {
                            this->m_settingsValid = false;
                        }

                        if (settings.pattern.empty())
                            this->m_settingsValid = false;

                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("hex.builtin.view.find.binary_pattern"_lang)) {
                        auto &settings = this->m_searchSettings.binaryPattern;

                        mode = SearchSettings::Mode::BinaryPattern;

                        ImGui::InputText("hex.builtin.view.find.binary_pattern"_lang, settings.input);

                        settings.pattern = parseBinaryPatternString(settings.input);
                        this->m_settingsValid = !settings.pattern.empty();

                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }

                ImGui::NewLine();

                ImGui::BeginDisabled(!this->m_settingsValid);
                {
                    if (ImGui::Button("hex.builtin.view.find.search"_lang)) {
                        this->runSearch();

                        this->m_decodeSettings = this->m_searchSettings;
                    }
                }
                ImGui::EndDisabled();

                ImGui::SameLine();
                ImGui::TextFormatted("hex.builtin.view.find.search.entries"_lang, this->m_foundOccurrences[provider].size());
            }
            ImGui::EndDisabled();


            ImGui::Separator();
            ImGui::NewLine();

            auto &currOccurrences = this->m_sortedOccurrences[provider];

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::InputTextWithHint("##filter", "hex.builtin.common.filter"_lang, this->m_currFilter[provider])) {
                this->m_sortedOccurrences[provider] = this->m_foundOccurrences[provider];

                currOccurrences.erase(std::remove_if(currOccurrences.begin(), currOccurrences.end(), [this, provider](const auto &region) {
                    return !this->decodeValue(provider, region).contains(this->m_currFilter[provider]);
                }), currOccurrences.end());
            }
            ImGui::PopItemWidth();

            if (ImGui::BeginTable("##entries", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.common.offset"_lang, 0, -1, ImGui::GetID("offset"));
                ImGui::TableSetupColumn("hex.builtin.common.size"_lang, 0, -1, ImGui::GetID("size"));
                ImGui::TableSetupColumn("hex.builtin.common.value"_lang, 0, -1, ImGui::GetID("value"));

                auto sortSpecs = ImGui::TableGetSortSpecs();

                if (sortSpecs->SpecsDirty) {
                    std::sort(currOccurrences.begin(), currOccurrences.end(), [this, &sortSpecs, provider](Occurrence &left, Occurrence &right) -> bool {
                        if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("offset")) {
                            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                return left.region.getStartAddress() > right.region.getStartAddress();
                            else
                                return left.region.getStartAddress() < right.region.getStartAddress();
                        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size")) {
                            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                return left.region.getSize() > right.region.getSize();
                            else
                                return left.region.getSize() < right.region.getSize();
                        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("value")) {
                            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                return this->decodeValue(provider, left) > this->decodeValue(provider, right);
                            else
                                return this->decodeValue(provider, left) < this->decodeValue(provider, right);
                        }

                        return false;
                    });

                    sortSpecs->SpecsDirty = false;
                }

                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(currOccurrences.size(), ImGui::GetTextLineHeightWithSpacing());

                while (clipper.Step()) {
                    for (size_t i = clipper.DisplayStart; i < std::min<size_t>(clipper.DisplayEnd, currOccurrences.size()); i++) {
                        auto &foundItem = currOccurrences[i];

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::TextFormatted("0x{:08X}", foundItem.region.getStartAddress());
                        ImGui::TableNextColumn();
                        ImGui::TextFormatted("{}", hex::toByteString(foundItem.region.getSize()));
                        ImGui::TableNextColumn();

                        ImGui::PushID(i);

                        auto value = this->decodeValue(provider, foundItem);
                        ImGui::TextFormatted("{}", value);
                        ImGui::SameLine();
                        if (ImGui::Selectable("##line", false, ImGuiSelectableFlags_SpanAllColumns))
                            ImHexApi::HexEditor::setSelection(foundItem.region.getStartAddress(), foundItem.region.getSize());
                        drawContextMenu(value);

                        ImGui::PopID();
                    }
                }
                clipper.End();

                ImGui::EndTable();
            }

        }
        ImGui::End();
    }

}
