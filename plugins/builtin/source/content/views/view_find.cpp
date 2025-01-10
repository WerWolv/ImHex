#include "content/views/view_find.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/providers/buffered_reader.hpp>

#include <fonts/vscode_icons.hpp>

#include <array>
#include <ranges>
#include <string>
#include <utility>

#include <content/helpers/demangle.hpp>
#include <boost/regex.hpp>

namespace hex::plugin::builtin {

    ViewFind::ViewFind() : View::Window("hex.builtin.view.find.name", ICON_VS_SEARCH) {
        const static auto HighlightColor = [] { return (ImGuiExt::GetCustomColorU32(ImGuiCustomCol_FindHighlight) & 0x00FFFFFF) | 0x70000000; };

        ImHexApi::HexEditor::addBackgroundHighlightingProvider([this](u64 address, const u8* data, size_t size, bool) -> std::optional<color_t> {
            std::ignore = data;
            std::ignore = size;

            if (m_searchTask.isRunning())
                return { };

            if (!m_occurrenceTree->overlapping({ address, address }).empty())
                return HighlightColor();
            else
                return std::nullopt;
        });

        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8* data, size_t size) {
            std::ignore = data;
            std::ignore = size;

            if (m_searchTask.isRunning())
                return;

            auto occurrences = m_occurrenceTree->overlapping({ address, address + size });
            if (occurrences.empty())
                return;

            ImGui::BeginTooltip();

            for (const auto &occurrence : occurrences) {
                ImGui::PushID(&occurrence);
                if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    {
                        auto region = occurrence.value.region;
                        const auto value = this->decodeValue(ImHexApi::Provider::get(), occurrence.value, 256);

                        ImGui::ColorButton("##color", ImColor(HighlightColor()));
                        ImGui::SameLine(0, 10);
                        ImGuiExt::TextFormatted("{} ", value);

                        if (ImGui::GetIO().KeyShift) {
                            ImGui::Indent();
                            if (ImGui::BeginTable("##extra_info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("{}: ", "hex.ui.common.region"_lang);
                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("[ 0x{:08X} - 0x{:08X} ]", region.getStartAddress(), region.getEndAddress());

                                auto demangledValue = hex::plugin::builtin::demangle(value);

                                if (value != demangledValue) {
                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGuiExt::TextFormatted("{}: ", "hex.builtin.view.find.demangled"_lang);
                                    ImGui::TableNextColumn();
                                    ImGuiExt::TextFormatted("{}", demangledValue);
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

        ShortcutManager::addShortcut(this, CTRLCMD + Keys::A, "hex.builtin.view.find.shortcut.select_all", [this] {
            if (m_filterTask.isRunning())
                return;
            if (m_searchTask.isRunning())
                return;

            for (auto &occurrence : *m_sortedOccurrences)
                occurrence.selected = true;
        });
    }

    template<typename Type, typename StorageType>
    static std::tuple<bool, std::variant<u64, i64, float, double>, size_t> parseNumericValue(const std::string &string) {
        static_assert(sizeof(StorageType) >= sizeof(Type));

        StorageType value;

        std::size_t processed = 0;
        try {
            if constexpr (std::floating_point<Type>)
                value = std::stod(string, &processed);
            else if constexpr (std::signed_integral<Type>)
                value = std::stoll(string, &processed, 0);
            else
                value = std::stoull(string, &processed, 0);
        } catch (std::exception &) {
            return { false, { }, 0 };
        }

        if (processed != string.size())
            return { false, { }, 0 };

        if (value < std::numeric_limits<Type>::lowest() || value > std::numeric_limits<Type>::max())
            return { false, { }, 0 };

        return { true, value, sizeof(Type) };
    }

    std::tuple<bool, std::variant<u64, i64, float, double>, size_t> ViewFind::parseNumericValueInput(const std::string &input, SearchSettings::Value::Type type) {
        switch (type) {
            using enum SearchSettings::Value::Type;

            case U8:    return parseNumericValue<u8,  u64>(input);
            case U16:   return parseNumericValue<u16, u64>(input);
            case U32:   return parseNumericValue<u32, u64>(input);
            case U64:   return parseNumericValue<u64, u64>(input);
            case I8:    return parseNumericValue<i8,  i64>(input);
            case I16:   return parseNumericValue<i16, i64>(input);
            case I32:   return parseNumericValue<i32, i64>(input);
            case I64:   return parseNumericValue<i64, i64>(input);
            case F32:   return parseNumericValue<float, float>(input);
            case F64:   return parseNumericValue<double, double>(input);
            default:    return { false, { }, 0 };
        }
    }

    template<typename T>
    static std::string formatBytes(const std::vector<u8> &bytes, std::endian endian) {
        if (bytes.size() > sizeof(T))
            return { };

        T value = 0x00;
        std::memcpy(&value, bytes.data(), bytes.size());

        value = hex::changeEndianness(value, bytes.size(), endian);

        if (std::signed_integral<T>)
            value = hex::signExtend(bytes.size() * 8, value);

        return hex::format("{}", value);
    }

    std::vector<hex::ContentRegistry::DataFormatter::impl::FindOccurrence> ViewFind::searchStrings(Task &task, prv::Provider *provider, hex::Region searchRegion, const SearchSettings::Strings &settings) {
        using enum SearchSettings::StringType;

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

        auto reader = prv::ProviderReader(provider);
        reader.seek(searchRegion.getStartAddress());
        reader.setEndAddress(searchRegion.getEndAddress());

        const auto [decodeType, endian] = [&] -> std::pair<Occurrence::DecodeType, std::endian> {
            if (settings.type == ASCII)
                return { Occurrence::DecodeType::ASCII, std::endian::native };
            if (settings.type == UTF8)
                return { Occurrence::DecodeType::UTF8, std::endian::native };
            else if (settings.type == SearchSettings::StringType::UTF16BE)
                return { Occurrence::DecodeType::UTF16, std::endian::big };
            else if (settings.type == SearchSettings::StringType::UTF16LE)
                return { Occurrence::DecodeType::UTF16, std::endian::little };
            else
                return { Occurrence::DecodeType::Binary, std::endian::native };
        }();

        i64 countedCharacters = 0;
        u64 startAddress = reader.begin().getAddress();
        u64 endAddress = reader.end().getAddress();

        u64 progress = 0;
        u64 codePointWidth = 0;
        i8 remainingCharacters = 0;
        for (u8 byte : reader) {
            bool validChar =
                (settings.lowerCaseLetters    && std::islower(byte))  ||
                (settings.upperCaseLetters    && std::isupper(byte))  ||
                (settings.numbers             && std::isdigit(byte))  ||
                (settings.spaces              && std::isspace(byte) && byte != '\r' && byte != '\n')  ||
                (settings.underscores         && byte == '_')             ||
                (settings.symbols             && std::ispunct(byte) && !std::isspace(byte))  ||
                (settings.lineFeeds           && (byte == '\r' || byte == '\n'));

            if (settings.type == UTF16LE) {
                // Check if second byte of UTF-16 encoded string is 0x00
                if (countedCharacters % 2 == 1)
                    validChar = byte == 0x00;
            } else if (settings.type == UTF16BE) {
                // Check if first byte of UTF-16 encoded string is 0x00
                if (countedCharacters % 2 == 0)
                    validChar = byte == 0x00;
            } else if (settings.type == UTF8) {
                if ((byte & 0b1000'0000) == 0b0000'0000) {
                    // ASCII range
                    codePointWidth = 1;
                    remainingCharacters = 0;
                    validChar = true;
                } else if ((byte & 0b1100'0000) == 0b1000'0000) {
                    // Continuation mark

                    if (remainingCharacters > 0) {
                        remainingCharacters -= 1;
                        validChar = true;
                    } else {
                        countedCharacters -= std::max<i64>(0, codePointWidth - (remainingCharacters + 1));
                        codePointWidth = 0;
                        remainingCharacters = 0;
                        validChar = false;
                    }
                } else if ((byte & 0b1110'0000) == 0b1100'0000) {
                    // Two bytes
                    codePointWidth = 2;
                    remainingCharacters = codePointWidth - 1;
                    validChar = true;
                } else if ((byte & 0b1111'0000) == 0b1110'0000) {
                    // Three bytes
                    codePointWidth = 3;
                    remainingCharacters = codePointWidth - 1;
                    validChar = true;
                } else if ((byte & 0b1111'1000) == 0b1111'0000) {
                    // Four bytes
                    codePointWidth = 4;
                    remainingCharacters = codePointWidth - 1;
                    validChar = true;
                } else {
                    validChar = false;
                }
            }

            task.update(progress);

            if (validChar)
                countedCharacters++;
            if (!validChar || startAddress + countedCharacters == endAddress) {
                if (countedCharacters >= settings.minLength) {
                    if (!settings.nullTermination || byte == 0x00) {
                        results.push_back(Occurrence { Region { startAddress, size_t(countedCharacters) }, decodeType, endian, false });
                    }
                }

                startAddress += countedCharacters + 1;
                countedCharacters = 0;
                progress = startAddress - searchRegion.getStartAddress();

            }
        }

        return results;
    }

    std::vector<hex::ContentRegistry::DataFormatter::impl::FindOccurrence> ViewFind::searchSequence(Task &task, prv::Provider *provider, hex::Region searchRegion, const SearchSettings::Sequence &settings) {
        std::vector<Occurrence> results;

        auto reader = prv::ProviderReader(provider);
        reader.seek(searchRegion.getStartAddress());
        reader.setEndAddress(searchRegion.getEndAddress());

        auto input = hex::decodeByteString(settings.sequence);
        if (input.empty())
            return { };

        std::vector<u8> bytes;
        auto decodeType = Occurrence::DecodeType::Binary;
        std::endian endian;
        switch (settings.type) {
            default:
            case SearchSettings::StringType::ASCII:
                bytes = input;
                decodeType = Occurrence::DecodeType::ASCII;
                endian = std::endian::native;
                break;
            case SearchSettings::StringType::UTF16LE: {
                auto wString = hex::utf8ToUtf16({ input.begin(), input.end() });

                bytes.resize(wString.size() * 2);
                std::memcpy(bytes.data(), wString.data(), bytes.size());
                decodeType = Occurrence::DecodeType::UTF16;
                endian = std::endian::little;

                break;
            }
            case SearchSettings::StringType::UTF16BE: {
                auto wString = hex::utf8ToUtf16({ input.begin(), input.end() });

                bytes.resize(wString.size() * 2);
                std::memcpy(bytes.data(), wString.data(), bytes.size());
                decodeType = Occurrence::DecodeType::UTF16;
                endian = std::endian::big;

                for (size_t i = 0; i < bytes.size(); i += 2)
                    std::swap(bytes[i], bytes[i + 1]);
                break;
            }
        }

        auto occurrence = reader.begin();
        u64 progress = 0;

        auto searchPredicate = [&] -> bool(*)(u8, u8) {
            if (!settings.ignoreCase)
                return [](u8 left, u8 right) -> bool {
                    return left == right;
                };
            else
                return [](u8 left, u8 right) -> bool {
                    if (std::isupper(left))
                        left = std::tolower(left);
                    if (std::isupper(right))
                        right = std::tolower(right);

                    return left == right;
                };
        }();


        while (true) {
            task.update(progress);

            occurrence = std::search(reader.begin(), reader.end(), std::default_searcher(bytes.begin(), bytes.end(), searchPredicate));
            if (occurrence == reader.end())
                break;

            auto address = occurrence.getAddress();
            reader.seek(address + 1);
            results.push_back(Occurrence{ Region { address, bytes.size() }, decodeType, endian, false });
            progress = address - searchRegion.getStartAddress();
        }

        return results;
    }

    std::vector<hex::ContentRegistry::DataFormatter::impl::FindOccurrence> ViewFind::searchRegex(Task &task, prv::Provider *provider, hex::Region searchRegion, const SearchSettings::Regex &settings) {
        auto stringOccurrences = searchStrings(task, provider, searchRegion, SearchSettings::Strings {
            .minLength          = settings.minLength,
            .nullTermination    = settings.nullTermination,
            .type               = settings.type,
            .lowerCaseLetters   = true,
            .upperCaseLetters   = true,
            .numbers            = true,
            .underscores        = true,
            .symbols            = true,
            .spaces             = true,
            .lineFeeds          = true
        });

        std::vector<Occurrence> result;
        boost::regex regex(settings.pattern);
        for (const auto &occurrence : stringOccurrences) {
            std::string string(occurrence.region.getSize(), '\x00');
            provider->read(occurrence.region.getStartAddress(), string.data(), occurrence.region.getSize());

            task.update();

            if (settings.fullMatch) {
                if (boost::regex_match(string, regex))
                    result.push_back(occurrence);
            } else {
                if (boost::regex_search(string, regex))
                    result.push_back(occurrence);
            }
        }

        return result;
    }

    std::vector<hex::ContentRegistry::DataFormatter::impl::FindOccurrence> ViewFind::searchBinaryPattern(Task &task, prv::Provider *provider, hex::Region searchRegion, const SearchSettings::BinaryPattern &settings) {
        std::vector<Occurrence> results;

        auto reader = prv::ProviderReader(provider);
        reader.seek(searchRegion.getStartAddress());
        reader.setEndAddress(searchRegion.getEndAddress());

        const size_t patternSize = settings.pattern.getSize();

        if (settings.alignment == 1) {
            u32 matchedBytes = 0;
            for (auto it = reader.begin(); it < reader.end(); it += 1) {
                auto byte = *it;

                task.update(it.getAddress());
                if (settings.pattern.matchesByte(byte, matchedBytes)) {
                    matchedBytes++;
                    if (matchedBytes == settings.pattern.getSize()) {
                        auto occurrenceAddress = it.getAddress() - (patternSize - 1);

                        results.push_back(Occurrence { Region { occurrenceAddress, patternSize }, Occurrence::DecodeType::Binary, std::endian::native, false });
                        it.setAddress(occurrenceAddress);
                        matchedBytes = 0;
                    }
                } else {
                    if (matchedBytes > 0)
                        it -= matchedBytes;
                    matchedBytes = 0;
                }
            }
        } else {
            std::vector<u8> data(patternSize);
            for (u64 address = searchRegion.getStartAddress(); address < searchRegion.getEndAddress(); address += settings.alignment) {
                reader.read(address, data.data(), data.size());

                task.update(address);

                bool match = true;
                for (u32 i = 0; i < patternSize; i++) {
                    if (!settings.pattern.matchesByte(data[i], i)) {
                        match = false;
                        break;
                    }
                }

                if (match)
                    results.push_back(Occurrence { Region { address, patternSize }, Occurrence::DecodeType::Binary, std::endian::native, false });
            }
        }

        return results;
    }

    std::vector<hex::ContentRegistry::DataFormatter::impl::FindOccurrence> ViewFind::searchValue(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Value &settings) {
        std::vector<Occurrence> results;

        auto reader = prv::ProviderReader(provider);
        reader.seek(searchRegion.getStartAddress());
        reader.setEndAddress(searchRegion.getEndAddress());

        auto inputMin = settings.inputMin;
        auto inputMax = settings.inputMax;

        if (inputMax.empty())
            inputMax = inputMin;

        const auto [validMin, min, sizeMin] = parseNumericValueInput(inputMin, settings.type);
        const auto [validMax, max, sizeMax] = parseNumericValueInput(inputMax, settings.type);

        if (!validMin || !validMax || sizeMin != sizeMax)
            return { };

        const auto size = sizeMin;

        const auto advance = settings.aligned ? size : 1;

        for (u64 address = searchRegion.getStartAddress(); address < searchRegion.getEndAddress(); address += advance) {
            task.update(address);

            auto result = std::visit([&]<typename T>(T) {
                using DecayedType = std::remove_cvref_t<std::decay_t<T>>;

                auto minValue = std::get<DecayedType>(min);
                auto maxValue = std::get<DecayedType>(max);

                DecayedType value = 0;
                reader.read(address, reinterpret_cast<u8*>(&value), size);
                value = hex::changeEndianness(value, size, settings.endian);

                return value >= minValue && value <= maxValue;
            }, min);

            if (result) {
                Occurrence::DecodeType decodeType = [&]{
                    switch (settings.type) {
                        using enum SearchSettings::Value::Type;
                        using enum Occurrence::DecodeType;

                        case U8:
                        case U16:
                        case U32:
                        case U64:
                            return Unsigned;
                        case I8:
                        case I16:
                        case I32:
                        case I64:
                            return Signed;
                        case F32:
                            return Float;
                        case F64:
                            return Double;
                        default:
                            return Binary;
                    }
                }();

                results.push_back(Occurrence { Region { address, size }, decodeType, settings.endian, false });
            }
        }

        return results;
    }

    void ViewFind::runSearch() {
        Region searchRegion = m_searchSettings.region;

        if (m_searchSettings.mode == SearchSettings::Mode::Strings) {
            AchievementManager::unlockAchievement("hex.builtin.achievement.find", "hex.builtin.achievement.find.find_strings.name");
        } else if (m_searchSettings.mode == SearchSettings::Mode::Sequence) {
            AchievementManager::unlockAchievement("hex.builtin.achievement.find", "hex.builtin.achievement.find.find_specific_string.name");
        } else if (m_searchSettings.mode == SearchSettings::Mode::Value) {
            if (m_searchSettings.value.inputMin == "250" && m_searchSettings.value.inputMax == "1000")
                AchievementManager::unlockAchievement("hex.builtin.achievement.find", "hex.builtin.achievement.find.find_numeric.name");
        }

        m_occurrenceTree->clear();
        EventHighlightingChanged::post();

        m_searchTask = TaskManager::createTask("hex.builtin.view.find.searching"_lang, searchRegion.getSize(), [this, settings = m_searchSettings, searchRegion](auto &task) {
            auto provider = ImHexApi::Provider::get();

            switch (settings.mode) {
                using enum SearchSettings::Mode;
                case Strings:
                    m_foundOccurrences.get(provider) = searchStrings(task, provider, searchRegion, settings.strings);
                    break;
                case Sequence:
                    m_foundOccurrences.get(provider) = searchSequence(task, provider, searchRegion, settings.bytes);
                    break;
                case Regex:
                    m_foundOccurrences.get(provider) = searchRegex(task, provider, searchRegion, settings.regex);
                    break;
                case BinaryPattern:
                    m_foundOccurrences.get(provider) = searchBinaryPattern(task, provider, searchRegion, settings.binaryPattern);
                    break;
                case Value:
                    m_foundOccurrences.get(provider) = searchValue(task, provider, searchRegion, settings.value);
                    break;
            }

            m_sortedOccurrences.get(provider) = m_foundOccurrences.get(provider);
            m_lastSelectedOccurrence = nullptr;

            for (const auto &occurrence : m_foundOccurrences.get(provider))
                m_occurrenceTree->insert({ occurrence.region.getStartAddress(), occurrence.region.getEndAddress() }, occurrence);

            TaskManager::doLater([] {
                EventHighlightingChanged::post();
            });
        });
    }

    std::string ViewFind::decodeValue(prv::Provider *provider, const Occurrence &occurrence, size_t maxBytes) const {
        std::vector<u8> bytes(std::min<size_t>(occurrence.region.getSize(), maxBytes));
        provider->read(occurrence.region.getStartAddress(), bytes.data(), bytes.size());

        std::string result;
        switch (m_decodeSettings.mode) {
            using enum SearchSettings::Mode;

            case Value:
            case Strings:
            case Sequence:
            case Regex:
            {
                switch (occurrence.decodeType) {
                    using enum Occurrence::DecodeType;
                    case Binary:
                    case ASCII:
                        result = hex::encodeByteString(bytes);
                        break;
                    case UTF8:
                        result = std::string(bytes.begin(), bytes.end());
                        result = wolv::util::replaceStrings(result, "\n", "");
                        result = wolv::util::replaceStrings(result, "\r", "");
                        break;
                    case UTF16:
                        for (size_t i = occurrence.endian == std::endian::little ? 0 : 1; i < bytes.size(); i += 2)
                            result += hex::encodeByteString({ bytes[i] });
                        break;
                    case Unsigned:
                        result += formatBytes<u64>(bytes, occurrence.endian);
                        break;
                    case Signed:
                        result += formatBytes<i64>(bytes, occurrence.endian);
                        break;
                    case Float:
                        result += formatBytes<float>(bytes, occurrence.endian);
                        break;
                    case Double:
                        result += formatBytes<double>(bytes, occurrence.endian);
                        break;
                }
            }
                break;
            case BinaryPattern:
                result = hex::encodeByteString(bytes);
                break;
        }

        if (occurrence.region.getSize() > maxBytes)
            result += "...";

        return result;
    }

    void ViewFind::drawContextMenu(Occurrence &target, const std::string &value) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsItemHovered()) {
            ImGui::OpenPopup("FindContextMenu");
            target.selected = true;
            m_replaceBuffer.clear();
        }

        if (ImGui::BeginPopup("FindContextMenu")) {
            if (ImGui::MenuItemEx("hex.builtin.view.find.context.copy"_lang, ICON_VS_COPY))
                ImGui::SetClipboardText(value.c_str());
            if (ImGui::MenuItemEx("hex.builtin.view.find.context.copy_demangle"_lang, ICON_VS_FILES))
                ImGui::SetClipboardText(hex::plugin::builtin::demangle(value).c_str());
            if (ImGui::BeginMenuEx("hex.builtin.view.find.context.replace"_lang, ICON_VS_REPLACE)) {
                if (ImGui::BeginTabBar("##replace_tabs")) {
                    if (ImGui::BeginTabItem("hex.builtin.view.find.context.replace.hex"_lang)) {
                        ImGuiExt::InputTextIcon("##replace_input", ICON_VS_SYMBOL_NAMESPACE, m_replaceBuffer);

                        ImGui::BeginDisabled(m_replaceBuffer.empty());
                        if (ImGui::Button("hex.builtin.view.find.context.replace"_lang)) {
                            auto provider = ImHexApi::Provider::get();
                            auto bytes = parseHexString(m_replaceBuffer);

                            for (const auto &occurrence : *m_sortedOccurrences) {
                                if (occurrence.selected) {
                                    size_t size = std::min<size_t>(occurrence.region.size, bytes.size());
                                    provider->write(occurrence.region.getStartAddress(), bytes.data(), size);
                                }
                            }
                        }
                        ImGui::EndDisabled();

                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("hex.builtin.view.find.context.replace.ascii"_lang)) {
                        ImGuiExt::InputTextIcon("##replace_input", ICON_VS_SYMBOL_KEY, m_replaceBuffer);

                        ImGui::BeginDisabled(m_replaceBuffer.empty());
                        if (ImGui::Button("hex.builtin.view.find.context.replace"_lang)) {
                            auto provider = ImHexApi::Provider::get();
                            auto bytes = decodeByteString(m_replaceBuffer);

                            for (const auto &occurrence : *m_sortedOccurrences) {
                                if (occurrence.selected) {
                                    size_t size = std::min<size_t>(occurrence.region.size, bytes.size());
                                    provider->write(occurrence.region.getStartAddress(), bytes.data(), size);
                                }
                            }
                        }
                        ImGui::EndDisabled();

                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
    }

    void ViewFind::drawContent() {
        auto provider = ImHexApi::Provider::get();

        ImGui::BeginDisabled(m_searchTask.isRunning());
        {
            ui::regionSelectionPicker(&m_searchSettings.region, provider, &m_searchSettings.range, true, true);

            ImGui::NewLine();

            if (ImGui::BeginTabBar("SearchMethods")) {
                const std::array<std::string, 6> StringTypes = {
                        "hex.ui.common.encoding.ascii"_lang,
                        "hex.ui.common.encoding.utf8"_lang,
                        "hex.ui.common.encoding.utf16le"_lang,
                        "hex.ui.common.encoding.utf16be"_lang,
                        hex::format("{} + {}", "hex.ui.common.encoding.ascii"_lang, "hex.ui.common.encoding.utf16le"_lang),
                        hex::format("{} + {}", "hex.ui.common.encoding.ascii"_lang, "hex.ui.common.encoding.utf16be"_lang)
                };

                auto &mode = m_searchSettings.mode;
                if (ImGui::BeginTabItem("hex.builtin.view.find.strings"_lang)) {
                    auto &settings = m_searchSettings.strings;
                    mode = SearchSettings::Mode::Strings;

                    ImGui::InputInt("hex.builtin.view.find.strings.min_length"_lang, &settings.minLength, 1, 1);
                    if (settings.minLength < 1)
                        settings.minLength = 1;

                    if (ImGui::BeginCombo("hex.ui.common.type"_lang, StringTypes[std::to_underlying(settings.type)].c_str())) {
                        for (size_t i = 0; i < StringTypes.size(); i++) {
                            auto type = static_cast<SearchSettings::StringType>(i);

                            if (ImGui::Selectable(StringTypes[i].c_str(), type == settings.type))
                                settings.type = type;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::NewLine();

                    if (ImGui::CollapsingHeader("hex.builtin.view.find.strings.match_settings"_lang)) {
                        ImGui::Checkbox("hex.builtin.view.find.strings.null_term"_lang, &settings.nullTermination);

                        ImGuiExt::Header("hex.builtin.view.find.strings.chars"_lang);
                        ImGui::Checkbox(hex::format("{} [a-z]", "hex.builtin.view.find.strings.lower_case"_lang.get()).c_str(), &settings.lowerCaseLetters);
                        ImGui::Checkbox(hex::format("{} [A-Z]", "hex.builtin.view.find.strings.upper_case"_lang.get()).c_str(), &settings.upperCaseLetters);
                        ImGui::Checkbox(hex::format("{} [0-9]", "hex.builtin.view.find.strings.numbers"_lang.get()).c_str(), &settings.numbers);
                        ImGui::Checkbox(hex::format("{} [_]", "hex.builtin.view.find.strings.underscores"_lang.get()).c_str(), &settings.underscores);
                        ImGui::Checkbox(hex::format("{} [!\"#$%...]", "hex.builtin.view.find.strings.symbols"_lang.get()).c_str(), &settings.symbols);
                        ImGui::Checkbox(hex::format("{} [ \\f\\t\\v]", "hex.builtin.view.find.strings.spaces"_lang.get()).c_str(), &settings.spaces);
                        ImGui::Checkbox(hex::format("{} [\\r\\n]", "hex.builtin.view.find.strings.line_feeds"_lang.get()).c_str(), &settings.lineFeeds);
                    }

                    m_settingsValid = true;

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.find.sequences"_lang)) {
                    auto &settings = m_searchSettings.bytes;

                    mode = SearchSettings::Mode::Sequence;

                    ImGuiExt::InputTextIconHint("hex.ui.common.value"_lang, ICON_VS_SYMBOL_KEY, "String", settings.sequence);

                    if (ImGui::BeginCombo("hex.ui.common.type"_lang, StringTypes[std::to_underlying(settings.type)].c_str())) {
                        for (size_t i = 0; i < StringTypes.size() - 2; i++) {
                            auto type = static_cast<SearchSettings::StringType>(i);

                            if (ImGui::Selectable(StringTypes[i].c_str(), type == settings.type))
                                settings.type = type;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Checkbox("hex.builtin.view.find.sequences.ignore_case"_lang, &settings.ignoreCase);

                    m_settingsValid = !settings.sequence.empty() && !hex::decodeByteString(settings.sequence).empty();

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.find.regex"_lang)) {
                    auto &settings = m_searchSettings.regex;

                    mode = SearchSettings::Mode::Regex;

                    ImGui::InputInt("hex.builtin.view.find.strings.min_length"_lang, &settings.minLength, 1, 1);
                    if (settings.minLength < 1)
                        settings.minLength = 1;

                    if (ImGui::BeginCombo("hex.ui.common.type"_lang, StringTypes[std::to_underlying(settings.type)].c_str())) {
                        for (size_t i = 0; i < StringTypes.size(); i++) {
                            auto type = static_cast<SearchSettings::StringType>(i);

                            if (ImGui::Selectable(StringTypes[i].c_str(), type == settings.type))
                                settings.type = type;
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Checkbox("hex.builtin.view.find.strings.null_term"_lang, &settings.nullTermination);

                    ImGui::NewLine();

                    ImGuiExt::InputTextIconHint("hex.builtin.view.find.regex.pattern"_lang, ICON_VS_REGEX, "[A-Za-z]{2}\\d{3}", settings.pattern);

                    try {
                        boost::regex regex(settings.pattern);
                        m_settingsValid = true;
                    } catch (const boost::regex_error &) {
                        m_settingsValid = false;
                    }

                    if (settings.pattern.empty())
                        m_settingsValid = false;

                    ImGui::Checkbox("hex.builtin.view.find.regex.full_match"_lang, &settings.fullMatch);

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.find.binary_pattern"_lang)) {
                    auto &settings = m_searchSettings.binaryPattern;

                    mode = SearchSettings::Mode::BinaryPattern;

                    ImGuiExt::InputTextIconHint("hex.builtin.view.find.binary_pattern"_lang, ICON_VS_SYMBOL_NAMESPACE, "AA BB ?? ?D \"XYZ\"", settings.input);

                    constexpr static u32 min = 1, max = 0x1000;
                    ImGui::SliderScalar("hex.builtin.view.find.binary_pattern.alignment"_lang, ImGuiDataType_U32, &settings.alignment, &min, &max);

                    settings.pattern = hex::BinaryPattern(settings.input);
                    m_settingsValid = settings.pattern.isValid() && settings.alignment > 0;

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.find.value"_lang)) {
                    auto &settings = m_searchSettings.value;

                    mode = SearchSettings::Mode::Value;

                    bool edited = false;

                    if (settings.range) {
                        if (ImGuiExt::InputTextIcon("hex.builtin.view.find.value.min"_lang, ICON_VS_SYMBOL_NUMERIC, settings.inputMin)) edited = true;
                        if (ImGuiExt::InputTextIcon("hex.builtin.view.find.value.max"_lang, ICON_VS_SYMBOL_NUMERIC, settings.inputMax)) edited = true;
                    } else {
                        if (ImGuiExt::InputTextIcon("hex.ui.common.value"_lang, ICON_VS_SYMBOL_NUMERIC, settings.inputMin)) {
                            edited = true;
                            settings.inputMax = settings.inputMin;
                        }

                        ImGui::BeginDisabled();
                        ImGuiExt::InputTextIcon("##placeholder_value", ICON_VS_SYMBOL_NUMERIC, settings.inputMax);
                        ImGui::EndDisabled();
                    }

                    if (ImGui::Checkbox("hex.builtin.view.find.value.range"_lang, &settings.range)) {
                        settings.inputMax = settings.inputMin;
                    }
                    ImGui::NewLine();

                    const std::array<std::string, 10> InputTypes = {
                            "hex.ui.common.type.u8"_lang,
                            "hex.ui.common.type.u16"_lang,
                            "hex.ui.common.type.u32"_lang,
                            "hex.ui.common.type.u64"_lang,
                            "hex.ui.common.type.i8"_lang,
                            "hex.ui.common.type.i16"_lang,
                            "hex.ui.common.type.i32"_lang,
                            "hex.ui.common.type.i64"_lang,
                            "hex.ui.common.type.f32"_lang,
                            "hex.ui.common.type.f64"_lang
                    };

                    if (ImGui::BeginCombo("hex.ui.common.type"_lang, InputTypes[std::to_underlying(settings.type)].c_str())) {
                        for (size_t i = 0; i < InputTypes.size(); i++) {
                            auto type = static_cast<SearchSettings::Value::Type>(i);

                            if (ImGui::Selectable(InputTypes[i].c_str(), type == settings.type)) {
                                settings.type = type;
                                edited = true;
                            }
                        }
                        ImGui::EndCombo();
                    }

                    {
                        int selection = [&] {
                            switch (settings.endian) {
                                default:
                                case std::endian::little:    return 0;
                                case std::endian::big:       return 1;
                            }
                        }();

                        std::array options = { "hex.ui.common.little"_lang, "hex.ui.common.big"_lang };
                        if (ImGui::SliderInt("hex.ui.common.endian"_lang, &selection, 0, options.size() - 1, options[selection], ImGuiSliderFlags_NoInput)) {
                            edited = true;
                            switch (selection) {
                                default:
                                case 0: settings.endian = std::endian::little;   break;
                                case 1: settings.endian = std::endian::big;      break;
                            }
                        }
                    }

                    ImGui::Checkbox("hex.builtin.view.find.value.aligned"_lang, &settings.aligned);

                    if (edited) {
                        auto [minValid, min, minSize] = parseNumericValueInput(settings.inputMin, settings.type);
                        auto [maxValid, max, maxSize] = parseNumericValueInput(settings.inputMax, settings.type);

                        m_settingsValid = minValid && maxValid && minSize == maxSize;
                    }

                    if (settings.inputMin.empty())
                        m_settingsValid = false;

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::NewLine();

            ImGui::BeginDisabled(!m_settingsValid);
            {
                if (ImGui::Button("hex.builtin.view.find.search"_lang)) {
                    this->runSearch();

                    m_decodeSettings = m_searchSettings;
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGui::BeginDisabled(m_foundOccurrences->empty());
            {
                if (ImGui::Button("hex.builtin.view.find.search.reset"_lang)) {
                    m_foundOccurrences->clear();
                    m_sortedOccurrences->clear();
                    m_occurrenceTree->clear();
                    m_lastSelectedOccurrence = nullptr;

                    EventHighlightingChanged::post();
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGuiExt::TextFormatted("hex.builtin.view.find.search.entries"_lang, m_foundOccurrences->size());
        }
        ImGui::EndDisabled();


        ImGui::Separator();
        ImGui::NewLine();

        auto &currOccurrences = *m_sortedOccurrences;

        ImGui::PushItemWidth(-30_scaled);
        auto prevFilterLength = m_currFilter->length();
        if (ImGuiExt::InputTextIcon("##filter", ICON_VS_FILTER, *m_currFilter)) {
            if (prevFilterLength > m_currFilter->length())
                *m_sortedOccurrences = *m_foundOccurrences;

            if (m_filterTask.isRunning())
                m_filterTask.interrupt();

            if (!m_currFilter->empty()) {
                m_filterTask = TaskManager::createTask("hex.builtin.task.filtering_data"_lang, currOccurrences.size(), [this, provider, &currOccurrences](Task &task) {
                    u64 progress = 0;
                    std::erase_if(currOccurrences, [this, provider, &task, &progress](const auto &region) {
                        task.update(progress);
                        progress += 1;

                        return !hex::containsIgnoreCase(this->decodeValue(provider, region), m_currFilter.get(provider));
                    });
                });
            }
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();

        const auto startPos = ImGui::GetCursorPos();
        ImGui::BeginDisabled(m_sortedOccurrences->empty());
        if (ImGuiExt::DimmedIconButton(ICON_VS_EXPORT, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            ImGui::OpenPopup("ExportResults");
        }
        ImGui::EndDisabled();

        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImVec2(startPos.x, ImGui::GetCursorPosY()));
        if (ImGui::BeginPopup("ExportResults")) {
            for (const auto &formatter : ContentRegistry::DataFormatter::impl::getFindExporterEntries()) {
                const auto formatterName = formatter.unlocalizedName;
                const auto name = toUpper(formatterName);

                const auto &extension = formatter.fileExtension;

                if (ImGui::MenuItem(name.c_str())) {
                    fs::openFileBrowser(fs::DialogMode::Save, { { name.c_str(), extension.c_str() } }, [&](const std::fs::path &path) {
                        wolv::io::File file(path, wolv::io::File::Mode::Create);
                        if (!file.isValid())
                            return;

                        auto result = formatter.callback(
                                m_sortedOccurrences.get(provider),
                                [&](Occurrence o){ return this->decodeValue(provider, o); });

                        file.writeVector(result);
                        file.close();
                    });
                }
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginTable("##entries", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImMax(ImGui::GetContentRegionAvail(), ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5)))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.ui.common.offset"_lang, 0, -1, ImGui::GetID("offset"));
            ImGui::TableSetupColumn("hex.ui.common.size"_lang, 0, -1, ImGui::GetID("size"));
            ImGui::TableSetupColumn("hex.ui.common.value"_lang, 0, -1, ImGui::GetID("value"));

            auto sortSpecs = ImGui::TableGetSortSpecs();

            if (sortSpecs->SpecsDirty) {
                std::sort(currOccurrences.begin(), currOccurrences.end(), [this, &sortSpecs, provider](const Occurrence &left, const Occurrence &right) -> bool {
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

                    ImGuiExt::TextFormatted("0x{:08X}", foundItem.region.getStartAddress());
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{}", hex::toByteString(foundItem.region.getSize()));
                    ImGui::TableNextColumn();

                    ImGui::PushID(i);

                    auto value = this->decodeValue(provider, foundItem, 256);
                    ImGuiExt::TextFormatted("{}", value);
                    ImGui::SameLine();
                    if (ImGui::Selectable("##line", foundItem.selected, ImGuiSelectableFlags_SpanAllColumns)) {
                        if (ImGui::GetIO().KeyShift && m_lastSelectedOccurrence != nullptr) {
                            for (auto start = std::min(&foundItem, m_lastSelectedOccurrence.get(provider)); start <= std::max(&foundItem, m_lastSelectedOccurrence.get(provider)); start += 1)
                                start->selected = true;

                        } else if (ImGui::GetIO().KeyCtrl) {
                            foundItem.selected = !foundItem.selected;
                        } else {
                            for (auto &occurrence : *m_sortedOccurrences)
                                occurrence.selected = false;
                            foundItem.selected = true;
                            ImHexApi::HexEditor::setSelection(foundItem.region.getStartAddress(), foundItem.region.getSize());
                        }

                        m_lastSelectedOccurrence = &foundItem;
                    }
                    drawContextMenu(foundItem, value);

                    ImGui::PopID();
                }
            }
            clipper.End();

            ImGui::EndTable();
        }
    }

}
