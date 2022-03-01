#include "content/views/view_hex_editor.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/paths.hpp>
#include <hex/helpers/patches.hpp>
#include <hex/helpers/project_file_handler.hpp>
#include <hex/helpers/loader_script_handler.hpp>

#include <content/providers/file_provider.hpp>


#include <GLFW/glfw3.h>

#include <nlohmann/json.hpp>

#include <thread>
#include <filesystem>

namespace hex::plugin::builtin {

    ViewHexEditor::ViewHexEditor() : View("hex.builtin.view.hex_editor.name"_lang) {

        this->m_searchStringBuffer.resize(0xFFF, 0x00);
        this->m_searchHexBuffer.resize(0xFFF, 0x00);

        ContentRegistry::FileHandler::add({ ".hexproj" }, [](const auto &path) {
            return ProjectFile::load(path);
        });

        ContentRegistry::FileHandler::add({ ".tbl" }, [this](const auto &path) {
            this->m_currEncodingFile = EncodingFile(EncodingFile::Type::Thingy, path);

            return true;
        });

        this->m_memoryEditor.ReadFn = [](const ImU8 *data, size_t off) -> ImU8 {
            auto provider = ImHexApi::Provider::get();
            if (!provider->isAvailable() || !provider->isReadable())
                return 0x00;

            ImU8 byte;
            provider->read(off + provider->getBaseAddress() + provider->getCurrentPageAddress(), &byte, sizeof(ImU8));

            return byte;
        };

        this->m_memoryEditor.WriteFn = [](ImU8 *data, size_t off, ImU8 d) -> void {
            auto provider = ImHexApi::Provider::get();
            if (!provider->isAvailable() || !provider->isWritable())
                return;

            provider->write(off + provider->getBaseAddress() + provider->getCurrentPageAddress(), &d, sizeof(ImU8));
            EventManager::post<EventDataChanged>();
            ProjectFile::markDirty();
        };

        this->m_memoryEditor.HighlightFn = [](const ImU8 *data, size_t off, bool next) -> bool {
            bool firstByte = off == 0x00;

            auto _this    = (ViewHexEditor *)(data);
            auto provider = ImHexApi::Provider::get();
            off += provider->getBaseAddress() + provider->getCurrentPageAddress();

            u32 alpha = static_cast<u32>(_this->m_highlightAlpha) << 24;

            std::optional<color_t> currColor, prevColor;

            for (auto &highlightBlock : _this->m_highlights) {
                if (off >= highlightBlock.base && off < (highlightBlock.base + HighlightBlock::Size)) {
                    currColor = highlightBlock.highlight[off - highlightBlock.base].color;
                }
                if ((off - 1) >= highlightBlock.base && (off - 1) < (highlightBlock.base + HighlightBlock::Size)) {
                    prevColor = highlightBlock.highlight[(off - 1) - highlightBlock.base].color;
                }

                if (currColor && prevColor) break;

                if (firstByte)
                    prevColor = 0x00;
            }

            if (!currColor || !prevColor) {
                if (_this->m_highlights.size() > 0x100)
                    _this->m_highlights.pop_front();

                auto blockStartOffset   = off - (off % HighlightBlock::Size);
                HighlightBlock newBlock = {
                    blockStartOffset,
                    {}
                };

                for (u32 i = 0; i < HighlightBlock::Size; i++) {
                    std::optional<color_t> highlightColor;
                    std::string highlightTooltip;

                    for (const auto &[id, highlight] : ImHexApi::HexEditor::impl::getHighlights()) {
                        auto &region  = highlight.getRegion();
                        auto &color   = highlight.getColor();
                        auto &tooltip = highlight.getTooltip();

                        if (blockStartOffset + i >= region.address && blockStartOffset + i < (region.address + region.size)) {
                            highlightColor   = (color & 0x00FFFFFF) | alpha;
                            highlightTooltip = tooltip;
                        }
                    }

                    for (const auto &[id, function] : ImHexApi::HexEditor::impl::getHighlightingFunctions()) {
                        auto highlight = function(blockStartOffset + i);
                        if (highlight.has_value()) {
                            highlightColor   = highlightColor.has_value() ? ImAlphaBlendColors(highlight->getColor(), highlightColor.value()) : highlight->getColor();
                            highlightTooltip = highlight->getTooltip();
                        }
                    }

                    auto &currHighlight = newBlock.highlight[i];
                    currHighlight.color = highlightColor.value_or(0x00);
                    if (!highlightTooltip.empty())
                        currHighlight.tooltips.push_back(highlightTooltip);
                }

                _this->m_highlights.push_back(std::move(newBlock));
            }

            if (next && prevColor != currColor) {
                return false;
            }

            if (currColor.has_value() && (currColor.value() & 0x00FFFFFF) != 0x00) {
                _this->m_memoryEditor.HighlightColor = (currColor.value() & 0x00FFFFFF) | alpha;
                return true;
            }

            _this->m_memoryEditor.HighlightColor = 0x60C08080;
            return false;
        };

        this->m_memoryEditor.HoverFn = [](const ImU8 *data, size_t off) {
            auto _this = (ViewHexEditor *)(data);

            bool tooltipShown = false;

            auto provider = ImHexApi::Provider::get();
            off += provider->getBaseAddress() + provider->getCurrentPageAddress();

            for (auto &highlightBlock : _this->m_highlights) {
                if (off >= highlightBlock.base && off < (highlightBlock.base + HighlightBlock::Size)) {
                    auto &highlight = highlightBlock.highlight[off - highlightBlock.base];
                    if (!tooltipShown && !highlight.tooltips.empty()) {
                        ImGui::BeginTooltip();
                        tooltipShown = true;
                    }

                    for (const auto &tooltip : highlight.tooltips) {
                        ImGui::ColorButton(tooltip.c_str(), ImColor(highlight.color).Value);
                        ImGui::SameLine(0, 10);
                        ImGui::TextUnformatted(tooltip.c_str());
                    }
                }
            }

            if (tooltipShown)
                ImGui::EndTooltip();
        };

        this->m_memoryEditor.DecodeFn = [](const ImU8 *data, size_t addr) -> MemoryEditor::DecodeData {
            auto *_this = (ViewHexEditor *)data;

            if (_this->m_currEncodingFile.getLongestSequence() == 0)
                return { ".", 1, 0xFFFF8000 };

            auto provider = ImHexApi::Provider::get();
            size_t size   = std::min<size_t>(_this->m_currEncodingFile.getLongestSequence(), provider->getActualSize() - addr);

            std::vector<u8> buffer(size);
            provider->read(addr + provider->getBaseAddress() + provider->getCurrentPageAddress(), buffer.data(), size);

            auto [decoded, advance] = _this->m_currEncodingFile.getEncodingFor(buffer);

            ImColor color;
            if (decoded.length() == 1 && std::isalnum(decoded[0])) color = 0xFFFF8000;
            else if (decoded.length() == 1 && advance == 1) color = 0xFF0000FF;
            else if (decoded.length() > 1 && advance == 1) color = 0xFF00FFFF;
            else if (advance > 1) color = 0xFFFFFFFF;
            else color = 0xFFFF8000;

            return { std::string(decoded), advance, color };
        };

        registerEvents();
        registerShortcuts();
        registerMenuItems();
    }

    ViewHexEditor::~ViewHexEditor() {
        EventManager::unsubscribe<RequestOpenFile>(this);
        EventManager::unsubscribe<RequestSelectionChange>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);
        EventManager::unsubscribe<EventWindowClosing>(this);
        EventManager::unsubscribe<RequestOpenWindow>(this);
        EventManager::unsubscribe<EventSettingsChanged>(this);
        EventManager::unsubscribe<EventHighlightingChanged>(this);
    }

    void ViewHexEditor::drawContent() {
        auto provider = ImHexApi::Provider::get();

        size_t dataSize = (!ImHexApi::Provider::isValid() || !provider->isReadable()) ? 0x00 : provider->getSize();

        this->m_memoryEditor.DrawWindow(View::toWindowName("hex.builtin.view.hex_editor.name").c_str(), &this->getWindowOpenState(), this, dataSize, dataSize == 0 ? 0x00 : provider->getBaseAddress() + provider->getCurrentPageAddress());

        if (dataSize != 0x00) {
            this->m_memoryEditor.OptShowAdvancedDecoding = this->m_advancedDecodingEnabled && this->m_currEncodingFile.valid();

            if (ImGui::Begin(View::toWindowName("hex.builtin.view.hex_editor.name").c_str())) {

                if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
                    ImGui::OpenPopup("hex.builtin.menu.edit"_lang);

                if (ImGui::BeginPopup("hex.builtin.menu.edit"_lang)) {
                    this->drawEditPopup();
                    ImGui::EndPopup();
                }

                if (provider->getPageCount() > 1) {
                    ImGui::NewLine();

                    auto linePos = ImGui::GetCursorPosY() - 15_scaled;

                    ImGui::SetCursorPosY(linePos);

                    if (ImGui::ArrowButton("prevPage", ImGuiDir_Left)) {
                        provider->setCurrentPage(provider->getCurrentPage() - 1);

                        EventManager::post<EventRegionSelected>(Region { std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd), 1 });
                    }

                    ImGui::SameLine();

                    if (ImGui::ArrowButton("nextPage", ImGuiDir_Right)) {
                        provider->setCurrentPage(provider->getCurrentPage() + 1);

                        EventManager::post<EventRegionSelected>(Region { std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd), 1 });
                    }

                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();
                    ImGui::SetCursorPosY(linePos);

                    ImGui::TextFormatted("hex.builtin.view.hex_editor.page"_lang, provider->getCurrentPage() + 1, provider->getPageCount());
                }

                this->drawSearchPopup();
                this->drawGotoPopup();
            }
            ImGui::End();
        }
    }

    static void save() {
        ImHexApi::Provider::get()->save();
    }

    static void saveAs() {
        hex::openFileBrowser("hex.builtin.view.hex_editor.save_as"_lang, DialogMode::Save, {}, [](const auto &path) {
            ImHexApi::Provider::get()->saveAs(path);
        });
    }

    void ViewHexEditor::drawAlwaysVisible() {
        auto provider = ImHexApi::Provider::get();

        if (ImGui::BeginPopupModal("hex.builtin.view.hex_editor.exit_application.title"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::NewLine();
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.exit_application.desc"_lang);
            ImGui::NewLine();

            confirmButtons(
                "hex.builtin.common.yes"_lang, "hex.builtin.common.no"_lang, [] { ImHexApi::Common::closeImHex(true); }, [] { ImGui::CloseCurrentPopup(); });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("hex.builtin.view.hex_editor.script.title"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetCursorPosX(10);
            ImGui::TextFormattedWrapped("{}", static_cast<const char *>("hex.builtin.view.hex_editor.script.desc"_lang));

            ImGui::NewLine();
            ImGui::InputText("##nolabel", this->m_loaderScriptScriptPath.data(), this->m_loaderScriptScriptPath.length(), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("hex.builtin.view.hex_editor.script.script"_lang)) {
                hex::openFileBrowser("hex.builtin.view.hex_editor.script.script.title"_lang, DialogMode::Open, {
                                                                                                                   {"Python Script", "py"}
                },
                    [this](const auto &path) {
                        this->m_loaderScriptScriptPath = path.string();
                    });
            }
            ImGui::InputText("##nolabel", this->m_loaderScriptFilePath.data(), this->m_loaderScriptFilePath.length(), ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("hex.builtin.view.hex_editor.script.file"_lang)) {
                hex::openFileBrowser("hex.builtin.view.hex_editor.script.file.title"_lang, DialogMode::Open, {}, [this](const auto &path) {
                    this->m_loaderScriptFilePath = path.string();
                });
            }
            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::NewLine();

            confirmButtons(
                "hex.builtin.common.load"_lang, "hex.builtin.common.cancel"_lang, [this, &provider] {
                   if (!this->m_loaderScriptScriptPath.empty() && !this->m_loaderScriptFilePath.empty()) {
                       EventManager::post<RequestOpenFile>(this->m_loaderScriptFilePath);
                       LoaderScript::setFilePath(this->m_loaderScriptFilePath);
                       LoaderScript::setDataProvider(provider);
                       LoaderScript::processFile(this->m_loaderScriptScriptPath);
                       ImGui::CloseCurrentPopup();
                   } }, [] { ImGui::CloseCurrentPopup(); });

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("hex.builtin.view.hex_editor.menu.edit.set_base"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("hex.builtin.common.address"_lang, this->m_baseAddressBuffer, 16, ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::NewLine();

            confirmButtons(
                "hex.builtin.common.set"_lang, "hex.builtin.common.cancel"_lang, [this, &provider] {
                               provider->setBaseAddress(strtoull(this->m_baseAddressBuffer, nullptr, 16));
                               ImGui::CloseCurrentPopup(); }, [] { ImGui::CloseCurrentPopup(); });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("hex.builtin.view.hex_editor.menu.edit.resize"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("0x");
            ImGui::SameLine();
            ImGui::InputScalar("hex.builtin.common.size"_lang, ImGuiDataType_U64, &this->m_resizeSize, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::NewLine();

            confirmButtons(
                "hex.builtin.common.set"_lang, "hex.builtin.common.cancel"_lang, [this, &provider] {
                    provider->resize(this->m_resizeSize);
                    ImGui::CloseCurrentPopup(); }, [] { ImGui::CloseCurrentPopup(); });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("hex.builtin.view.hex_editor.menu.edit.insert"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("0x");
            ImGui::SameLine();
            ImGui::InputScalar("hex.builtin.common.size"_lang, ImGuiDataType_U64, &this->m_resizeSize, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::NewLine();

            confirmButtons(
                "hex.builtin.common.set"_lang, "hex.builtin.common.cancel"_lang, [this, &provider] {
                    provider->insert(std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd), this->m_resizeSize);
                    ImGui::CloseCurrentPopup(); }, [] { ImGui::CloseCurrentPopup(); });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void ViewHexEditor::openFile(const fs::path &path) {
        hex::prv::Provider *provider = nullptr;
        EventManager::post<RequestCreateProvider>("hex.builtin.provider.file", &provider);

        if (auto fileProvider = dynamic_cast<prv::FileProvider *>(provider)) {
            fileProvider->setPath(path);
            if (!fileProvider->open()) {
                View::showErrorPopup("hex.builtin.view.hex_editor.error.open"_lang);
                ImHexApi::Provider::remove(provider);

                return;
            }
        }

        if (!provider->isWritable()) {
            this->m_memoryEditor.ReadOnly = true;
            View::showErrorPopup("hex.builtin.view.hex_editor.error.read_only"_lang);
        } else {
            this->m_memoryEditor.ReadOnly = false;
        }

        if (!provider->isAvailable()) {
            View::showErrorPopup("hex.builtin.view.hex_editor.error.open"_lang);
            ImHexApi::Provider::remove(provider);

            return;
        }

        ProjectFile::setFilePath(path);

        this->getWindowOpenState() = true;

        EventManager::post<EventFileLoaded>(path);
        EventManager::post<EventDataChanged>();
        EventManager::post<EventHighlightingChanged>();
    }

    void ViewHexEditor::copyBytes() const {
        auto provider = ImHexApi::Provider::get();

        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end   = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::vector<u8> buffer(copySize, 0x00);
        provider->read(start + provider->getBaseAddress() + provider->getCurrentPageAddress(), buffer.data(), buffer.size());

        std::string str;
        for (const auto &byte : buffer)
            str += hex::format("{0:02X} ", byte);
        str.pop_back();

        ImGui::SetClipboardText(str.c_str());
    }

    void ViewHexEditor::pasteBytes() const {
        auto provider = ImHexApi::Provider::get();

        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end   = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        std::string clipboard = ImGui::GetClipboardText();

        // Check for non-hex characters
        bool isValidHexString = std::find_if(clipboard.begin(), clipboard.end(), [](char c) {
            return !std::isxdigit(c) && !std::isspace(c);
        }) == clipboard.end();

        if (!isValidHexString) return;

        // Remove all whitespace
        clipboard.erase(std::remove_if(clipboard.begin(), clipboard.end(), [](char c) { return std::isspace(c); }), clipboard.end());

        // Only paste whole bytes
        if (clipboard.length() % 2 != 0) return;

        // Convert hex string to bytes
        std::vector<u8> buffer(clipboard.length() / 2, 0x00);
        u32 stringIndex = 0;
        for (u8 &byte : buffer) {
            for (u8 i = 0; i < 2; i++) {
                byte <<= 4;

                char c = clipboard[stringIndex];

                if (c >= '0' && c <= '9') byte |= (c - '0');
                else if (c >= 'a' && c <= 'f') byte |= (c - 'a') + 0xA;
                else if (c >= 'A' && c <= 'F') byte |= (c - 'A') + 0xA;

                stringIndex++;
            }
        }

        // Write bytes
        provider->write(start + provider->getBaseAddress() + provider->getCurrentPageAddress(), buffer.data(), std::min(end - start + 1, buffer.size()));
    }

    void ViewHexEditor::copyString() const {
        auto provider = ImHexApi::Provider::get();

        size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
        size_t end   = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

        size_t copySize = (end - start) + 1;

        std::string buffer(copySize, 0x00);
        buffer.reserve(copySize);
        provider->read(start + provider->getBaseAddress() + provider->getCurrentPageAddress(), buffer.data(), copySize);

        ImGui::SetClipboardText(buffer.c_str());
    }

    static std::vector<std::pair<u64, u64>> findString(hex::prv::Provider *&provider, std::string string) {
        std::vector<std::pair<u64, u64>> results;

        if (string.empty())
            return {};

        u32 foundCharacters = 0;

        std::vector<u8> buffer(1024, 0x00);
        size_t dataSize = provider->getSize();
        for (u64 offset = 0; offset < dataSize; offset += 1024) {
            size_t usedBufferSize = std::min<size_t>({ buffer.size(), dataSize - offset, string.size() });
            provider->read(offset + provider->getBaseAddress() + provider->getCurrentPageAddress(), buffer.data(), usedBufferSize);

            for (u64 i = 0; i < usedBufferSize; i++) {
                if (buffer[i] == string[foundCharacters])
                    foundCharacters++;
                else
                    foundCharacters = 0;

                if (foundCharacters == string.size()) {
                    results.emplace_back(offset + i - foundCharacters + 1, offset + i);
                    foundCharacters = 0;
                }
            }
        }

        return results;
    }

    static std::vector<std::pair<u64, u64>> findHex(hex::prv::Provider *&provider, std::string string) {
        std::vector<std::pair<u64, u64>> results;

        if ((string.size() % 2) == 1)
            string = "0" + string;

        if (string.empty())
            return {};

        std::vector<u8> hex;
        hex.reserve(string.size() / 2);

        for (u32 i = 0; i < string.size(); i += 2) {
            char byte[3] = { string[i], string[i + 1], 0 };
            hex.push_back(strtoul(byte, nullptr, 16));
        }

        u32 foundCharacters = 0;

        std::vector<u8> buffer(1024, 0x00);
        size_t dataSize = provider->getSize();
        for (u64 offset = 0; offset < dataSize; offset += 1024) {
            size_t usedBufferSize = std::min<size_t>({ buffer.size(), dataSize - offset, hex.size() });
            provider->read(offset + provider->getBaseAddress() + provider->getCurrentPageAddress(), buffer.data(), usedBufferSize);

            for (u64 i = 0; i < usedBufferSize; i++) {
                if (buffer[i] == hex[foundCharacters])
                    foundCharacters++;
                else
                    foundCharacters = 0;

                if (foundCharacters == hex.size()) {
                    results.emplace_back(offset + i - foundCharacters + 1, offset + i);
                    foundCharacters = 0;
                }
            }
        }

        return results;
    }

    void ViewHexEditor::drawSearchInput(std::vector<char> *currBuffer, ImGuiInputTextFlags flags) {
        if (this->m_searchRequested) {
            ImGui::SetKeyboardFocusHere();
            this->m_searchRequested = false;
        }

        if (ImGui::InputText("##nolabel", currBuffer->data(), currBuffer->size(), flags, ViewHexEditor::inputCallback, this)) {
            this->m_searchRequested = true;
            if (this->m_lastSearchBuffer == nullptr || this->m_lastSearchBuffer->empty())
                performSearch(currBuffer->data());
            else
                performSearchNext();
        }

        ImGui::EndTabItem();
    }

    void ViewHexEditor::performSearch(const char *buffer) {
        auto provider = ImHexApi::Provider::get();

        *this->m_lastSearchBuffer = this->m_searchFunction(provider, buffer);
        this->m_lastSearchIndex   = 0;

        if (!this->m_lastSearchBuffer->empty())
            this->m_memoryEditor.GotoAddrAndSelect((*this->m_lastSearchBuffer)[0].first, (*this->m_lastSearchBuffer)[0].second);
    }

    void ViewHexEditor::performSearchNext() {
        if (this->m_lastSearchBuffer->empty()) {
            return;
        }

        ++this->m_lastSearchIndex %= this->m_lastSearchBuffer->size();
        this->m_memoryEditor.GotoAddrAndSelect((*this->m_lastSearchBuffer)[this->m_lastSearchIndex].first,
            (*this->m_lastSearchBuffer)[this->m_lastSearchIndex].second);
    }

    void ViewHexEditor::performSearchPrevious() {
        if (this->m_lastSearchBuffer->empty()) {
            return;
        }

        this->m_lastSearchIndex--;

        if (this->m_lastSearchIndex < 0)
            this->m_lastSearchIndex = this->m_lastSearchBuffer->size() - 1;

        this->m_lastSearchIndex %= this->m_lastSearchBuffer->size();

        this->m_memoryEditor.GotoAddrAndSelect((*this->m_lastSearchBuffer)[this->m_lastSearchIndex].first,
            (*this->m_lastSearchBuffer)[this->m_lastSearchIndex].second);
    }

    int ViewHexEditor::inputCallback(ImGuiInputTextCallbackData *data) {
        auto _this    = static_cast<ViewHexEditor *>(data->UserData);
        auto provider = ImHexApi::Provider::get();

        *_this->m_lastSearchBuffer = _this->m_searchFunction(provider, data->Buf);
        _this->m_lastSearchIndex   = 0;

        if (!_this->m_lastSearchBuffer->empty())
            _this->m_memoryEditor.GotoAddrAndSelect((*_this->m_lastSearchBuffer)[0].first, (*_this->m_lastSearchBuffer)[0].second);

        return 0;
    }

    void ViewHexEditor::drawSearchPopup() {
        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin() - ImGui::GetStyle().WindowPadding);
        if (ImGui::BeginPopup("hex.builtin.view.hex_editor.menu.file.search"_lang)) {
            if (ImGui::BeginTabBar("searchTabs")) {
                std::vector<char> *currBuffer = nullptr;
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.search.string"_lang)) {
                    this->m_searchFunction   = findString;
                    this->m_lastSearchBuffer = &this->m_lastStringSearch;
                    currBuffer               = &this->m_searchStringBuffer;

                    drawSearchInput(currBuffer, ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_EnterReturnsTrue);
                }

                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.search.hex"_lang)) {
                    this->m_searchFunction   = findHex;
                    this->m_lastSearchBuffer = &this->m_lastHexSearch;
                    currBuffer               = &this->m_searchHexBuffer;

                    drawSearchInput(currBuffer, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_EnterReturnsTrue);
                }

                if (currBuffer != nullptr) {
                    if (ImGui::Button("hex.builtin.view.hex_editor.search.find"_lang))
                        performSearch(currBuffer->data());

                    if (!this->m_lastSearchBuffer->empty()) {
                        if ((ImGui::Button("hex.builtin.view.hex_editor.search.find_next"_lang)))
                            performSearchNext();

                        ImGui::SameLine();

                        if ((ImGui::Button("hex.builtin.view.hex_editor.search.find_prev"_lang)))
                            performSearchPrevious();
                    }
                }

                ImGui::EndTabBar();
            }

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void ViewHexEditor::drawGotoPopup() {
        auto provider    = ImHexApi::Provider::get();
        auto baseAddress = provider->getBaseAddress();
        auto dataSize    = provider->getActualSize();

        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin() - ImGui::GetStyle().WindowPadding);
        if (ImGui::BeginPopup("hex.builtin.view.hex_editor.menu.file.goto"_lang)) {
            bool runGoto = false;

            if (ImGui::BeginTabBar("gotoTabs")) {
                u64 newOffset = 0;
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.absolute"_lang)) {
                    if (this->m_gotoRequested) {
                        ImGui::SetKeyboardFocusHere();
                        this->m_gotoRequested = false;
                    }

                    runGoto = ImGui::InputHexadecimal("##goto", &this->m_gotoAddressAbsolute, ImGuiInputTextFlags_EnterReturnsTrue);

                    if (this->m_gotoAddressAbsolute < baseAddress || this->m_gotoAddressAbsolute > baseAddress + dataSize)
                        this->m_gotoAddressAbsolute = baseAddress;

                    newOffset = this->m_gotoAddressAbsolute;

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.begin"_lang)) {
                    if (this->m_gotoRequested) {
                        ImGui::SetKeyboardFocusHere();
                        this->m_gotoRequested = false;
                    }
                    runGoto = ImGui::InputScalar("##goto", ImGuiDataType_U64, &this->m_gotoAddressAbsolute, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);

                    if (this->m_gotoAddressAbsolute < 0 || this->m_gotoAddressAbsolute > dataSize)
                        this->m_gotoAddressAbsolute = 0;

                    newOffset = this->m_gotoAddressAbsolute + baseAddress;

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.current"_lang)) {
                    ImGui::SetKeyboardFocusHere();
                    runGoto = ImGui::InputScalar("##goto", ImGuiDataType_S64, &this->m_gotoAddressRelative, nullptr, nullptr, "%lld", ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue);

                    i64 currSelectionOffset = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

                    if (currSelectionOffset + this->m_gotoAddressRelative < 0)
                        this->m_gotoAddressRelative = -currSelectionOffset;
                    else if (currSelectionOffset + this->m_gotoAddressRelative > dataSize)
                        this->m_gotoAddressRelative = dataSize - currSelectionOffset;

                    newOffset = currSelectionOffset + this->m_gotoAddressRelative + baseAddress;

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.end"_lang)) {
                    ImGui::SetKeyboardFocusHere();
                    runGoto = ImGui::InputHexadecimal("##goto", &this->m_gotoAddressAbsolute, ImGuiInputTextFlags_EnterReturnsTrue);

                    if (this->m_gotoAddressAbsolute < 0 || this->m_gotoAddressAbsolute > dataSize)
                        this->m_gotoAddressAbsolute = 0;

                    newOffset = (baseAddress + dataSize) - this->m_gotoAddressAbsolute - 1;

                    ImGui::EndTabItem();
                }

                if (ImGui::Button("hex.builtin.view.hex_editor.menu.file.goto"_lang) || runGoto) {
                    this->m_gotoRequested = true;
                    provider->setCurrentPage(std::floor(double(newOffset - baseAddress) / hex::prv::Provider::PageSize));
                    ImHexApi::HexEditor::setSelection(newOffset, 1);
                }

                ImGui::EndTabBar();
            }

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void ViewHexEditor::drawEditPopup() {
        auto provider      = ImHexApi::Provider::get();
        bool providerValid = ImHexApi::Provider::isValid();
        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.undo"_lang, "CTRL + Z", false, providerValid))
            provider->undo();
        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.redo"_lang, "CTRL + Y", false, providerValid))
            provider->redo();

        ImGui::Separator();

        bool bytesSelected = this->m_memoryEditor.DataPreviewAddr != -1 && this->m_memoryEditor.DataPreviewAddrEnd != -1;

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.copy"_lang, "CTRL + C", false, bytesSelected))
            this->copyBytes();

        if (ImGui::BeginMenu("hex.builtin.view.hex_editor.menu.edit.copy_as"_lang, bytesSelected)) {
            if (ImGui::MenuItem("hex.builtin.view.hex_editor.copy.hex"_lang, "CTRL + SHIFT + C"))
                this->copyString();

            ImGui::Separator();

            for (const auto &[unlocalizedName, callback] : ContentRegistry::DataFormatter::getEntries()) {
                if (ImGui::MenuItem(LangEntry(unlocalizedName))) {
                    size_t start = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
                    size_t end   = std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

                    size_t copySize = (end - start) + 1;

                    ImGui::SetClipboardText(callback(provider, start + provider->getBaseAddress() + provider->getCurrentPageAddress(), copySize).c_str());
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.paste"_lang, "CTRL + V", false, bytesSelected))
            this->pasteBytes();

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.select_all"_lang, "CTRL + A", false, providerValid))
            ImHexApi::HexEditor::setSelection(provider->getBaseAddress(), provider->getActualSize());

        ImGui::Separator();

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.bookmark"_lang, nullptr, false, this->m_memoryEditor.DataPreviewAddr != -1 && this->m_memoryEditor.DataPreviewAddrEnd != -1)) {
            auto base = ImHexApi::Provider::get()->getBaseAddress();

            size_t start = base + std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
            size_t end   = base + std::max(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);

            ImHexApi::Bookmarks::add(start, end - start + 1, {}, {});
        }

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.set_base"_lang, nullptr, false, providerValid && provider->isReadable())) {
            std::memset(this->m_baseAddressBuffer, 0x00, sizeof(this->m_baseAddressBuffer));
            ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.view.hex_editor.menu.edit.set_base"_lang); });
        }

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.resize"_lang, nullptr, false, providerValid && provider->isResizable())) {
            ImHexApi::Tasks::doLater([this] {
                this->m_resizeSize = ImHexApi::Provider::get()->getActualSize();
                ImGui::OpenPopup("hex.builtin.view.hex_editor.menu.edit.resize"_lang);
            });
        }

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.insert"_lang, nullptr, false, providerValid && provider->isResizable())) {
            ImHexApi::Tasks::doLater([this] {
                this->m_resizeSize = 0;
                ImGui::OpenPopup("hex.builtin.view.hex_editor.menu.edit.insert"_lang);
            });
        }
    }

    void ViewHexEditor::registerEvents() {
        EventManager::subscribe<RequestOpenFile>(this, [this](const auto &path) {
            this->openFile(path);
            this->getWindowOpenState() = true;
        });

        EventManager::subscribe<RequestSelectionChange>(this, [this](Region region) {
            auto provider = ImHexApi::Provider::get();
            auto page     = provider->getPageOfAddress(region.address);

            if (!page.has_value())
                return;

            if (region.size != 0) {
                provider->setCurrentPage(page.value());
                u64 start = region.address - provider->getBaseAddress() - provider->getCurrentPageAddress();
                this->m_memoryEditor.GotoAddrAndSelect(start, start + region.size - 1);
            }

            EventManager::post<EventRegionSelected>(Region { this->m_memoryEditor.DataPreviewAddr, (this->m_memoryEditor.DataPreviewAddrEnd - this->m_memoryEditor.DataPreviewAddr) + 1 });
        });

        EventManager::subscribe<EventProjectFileLoad>(this, []() {
            EventManager::post<RequestOpenFile>(ProjectFile::getFilePath());
        });

        EventManager::subscribe<EventWindowClosing>(this, [](GLFWwindow *window) {
            if (ProjectFile::hasUnsavedChanges()) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.view.hex_editor.exit_application.title"_lang); });
            }
        });

        EventManager::subscribe<RequestOpenWindow>(this, [this](std::string name) {
            if (name == "Create File") {
                hex::openFileBrowser("hex.builtin.view.hex_editor.create_file"_lang, DialogMode::Save, {}, [this](const auto &path) {
                    File file(path, File::Mode::Create);

                    if (!file.isValid()) {
                        View::showErrorPopup("hex.builtin.view.hex_editor.error.create"_lang);
                        return;
                    }

                    file.setSize(1);

                    EventManager::post<RequestOpenFile>(path);
                    this->getWindowOpenState() = true;
                });
            } else if (name == "Open File") {
                hex::openFileBrowser("hex.builtin.view.hex_editor.open_file"_lang, DialogMode::Open, {}, [this](const auto &path) {
                    EventManager::post<RequestOpenFile>(path);
                    this->getWindowOpenState() = true;
                });
            } else if (name == "Open Project") {
                hex::openFileBrowser("hex.builtin.view.hex_editor.open_project"_lang, DialogMode::Open, {
                                                                                                            {"Project File", "hexproj"}
                },
                    [this](const auto &path) {
                        ProjectFile::load(path);
                        this->getWindowOpenState() = true;
                    });
            }
        });

        EventManager::subscribe<EventSettingsChanged>(this, [this] {
            {
                auto alpha = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.highlight_alpha");

                if (alpha.is_number())
                    this->m_highlightAlpha = alpha;
            }

            {
                auto columnCount = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.column_count");

                if (columnCount.is_number())
                    this->m_memoryEditor.Cols = static_cast<int>(columnCount);
            }

            {
                auto hexii = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.hexii");

                if (hexii.is_number())
                    this->m_memoryEditor.OptShowHexII = static_cast<int>(hexii);
            }

            {
                auto ascii = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.ascii");

                if (ascii.is_number())
                    this->m_memoryEditor.OptShowAscii = static_cast<int>(ascii);
            }

            {
                auto advancedDecoding = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.advanced_decoding");

                if (advancedDecoding.is_number())
                    this->m_advancedDecodingEnabled = static_cast<int>(advancedDecoding);
            }

            {
                auto greyOutZeros = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.grey_zeros");

                if (greyOutZeros.is_number())
                    this->m_memoryEditor.OptGreyOutZeroes = static_cast<int>(greyOutZeros);
            }

            {
                auto upperCaseHex = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.uppercase_hex");

                if (upperCaseHex.is_number())
                    this->m_memoryEditor.OptUpperCaseHex = static_cast<int>(upperCaseHex);
            }

            {
                auto showExtraInfo = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.extra_info");

                if (showExtraInfo.is_number())
                    this->m_memoryEditor.OptShowExtraInfo = static_cast<int>(showExtraInfo);
            }
        });

        EventManager::subscribe<QuerySelection>(this, [this](auto &region) {
            u64 address = std::min(this->m_memoryEditor.DataPreviewAddr, this->m_memoryEditor.DataPreviewAddrEnd);
            size_t size = std::abs(i64(this->m_memoryEditor.DataPreviewAddrEnd) - i64(this->m_memoryEditor.DataPreviewAddr)) + 1;

            region = Region { address, size };
        });

        EventManager::subscribe<EventHighlightingChanged>(this, [this] {
            this->m_highlights.clear();
        });
    }

    void ViewHexEditor::registerShortcuts() {
        ShortcutManager::addGlobalShortcut(CTRL + Keys::S, [] {
            save();
        });

        ShortcutManager::addGlobalShortcut(CTRL + SHIFT + Keys::S, [] {
            saveAs();
        });


        ShortcutManager::addShortcut(this, CTRL + Keys::Z, [] {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::get()->undo();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Y, [] {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::get()->redo();
        });


        ShortcutManager::addShortcut(this, CTRL + Keys::F, [this] {
            this->m_searchRequested = true;
            ImGui::OpenPopupInWindow(View::toWindowName("hex.builtin.view.hex_editor.name").c_str(),
                "hex.builtin.view.hex_editor.menu.file.search"_lang);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::G, [this] {
            this->m_gotoRequested = true;
            ImGui::OpenPopupInWindow(View::toWindowName("hex.builtin.view.hex_editor.name").c_str(),
                "hex.builtin.view.hex_editor.menu.file.goto"_lang);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::O, [] {
            hex::openFileBrowser("hex.builtin.view.hex_editor.open_file"_lang, DialogMode::Open, {}, [](const auto &path) {
                EventManager::post<RequestOpenFile>(path);
            });
        });


        ShortcutManager::addShortcut(this, CTRL + Keys::C, [this] {
            this->copyBytes();
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::C, [this] {
            this->copyString();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::V, [this] {
            this->pasteBytes();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::A, [] {
            auto provider = ImHexApi::Provider::get();
            ImHexApi::HexEditor::setSelection(provider->getBaseAddress(), provider->getActualSize());
        });
    }

    void ViewHexEditor::registerMenuItems() {

        /* Basic operations */

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1100, [&] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();


            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.save"_lang, "CTRL + S", false, providerValid && provider->isWritable())) {
                save();
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.save_as"_lang, "CTRL + SHIFT + S", false, providerValid && provider->isWritable())) {
                saveAs();
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.close"_lang, "", false, providerValid)) {
                EventManager::post<EventFileUnloaded>();
                ImHexApi::Provider::remove(ImHexApi::Provider::get());
                providerValid = false;
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.quit"_lang, "", false)) {
                ImHexApi::Common::closeImHex();
            }
        });


        /* Metadata save/load */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1200, [&, this] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.open_project"_lang, "")) {
                hex::openFileBrowser("hex.builtin.view.hex_editor.menu.file.open_project"_lang, DialogMode::Open, {
                                                                                                                      {"Project File", "hexproj"}
                },
                    [](const auto &path) {
                        ProjectFile::load(path);
                    });
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.save_project"_lang, "", false, providerValid && provider->isWritable())) {
                if (ProjectFile::getProjectFilePath() == "") {
                    hex::openFileBrowser("hex.builtin.view.hex_editor.save_project"_lang, DialogMode::Save, {
                                                                                                                {"Project File", "hexproj"}
                    },
                        [](fs::path path) {
                            if (path.extension() != ".hexproj") {
                                path.replace_extension(".hexproj");
                            }

                            ProjectFile::store(path);
                        });
                } else
                    ProjectFile::store();
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.load_encoding_file"_lang)) {
                std::vector<fs::path> paths;
                for (const auto &path : hex::getPath(ImHexPath::Encodings)) {
                    for (const auto &entry : fs::recursive_directory_iterator(path)) {
                        if (!entry.is_regular_file()) continue;

                        paths.push_back(entry);
                    }
                }

                View::showFileChooserPopup(paths, {
                                                      {"Thingy Table File", "tbl"}
                },
                    [this](const auto &path) {
                        this->m_currEncodingFile = EncodingFile(EncodingFile::Type::Thingy, path);
                    });
            }
        });


        /* Import / Export */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1300, [&, this] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();

            /* Import */
            if (ImGui::BeginMenu("hex.builtin.view.hex_editor.menu.file.import"_lang)) {
                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.import.base64"_lang)) {

                    hex::openFileBrowser("hex.builtin.view.hex_editor.menu.file.import.base64"_lang, DialogMode::Open, {}, [this](const auto &path) {
                        File file(path, File::Mode::Read);
                        if (!file.isValid()) {
                            View::showErrorPopup("hex.builtin.view.hex_editor.error.open"_lang);
                            return;
                        }

                        auto base64 = file.readBytes();

                        if (!base64.empty()) {
                            this->m_dataToSave = crypt::decode64(base64);

                            if (this->m_dataToSave.empty())
                                View::showErrorPopup("hex.builtin.view.hex_editor.base64.import_error"_lang);
                            else
                                ImGui::OpenPopup("hex.builtin.view.hex_editor.save_data"_lang);
                            this->getWindowOpenState() = true;
                        } else View::showErrorPopup("hex.builtin.view.hex_editor.file_open_error"_lang);
                    });
                }

                ImGui::Separator();

                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.import.ips"_lang, nullptr, false, !this->m_processingImportExport)) {

                    hex::openFileBrowser("hex.builtin.view.hex_editor.open_file"_lang, DialogMode::Open, {}, [this](const auto &path) {
                        this->m_processingImportExport = true;
                        std::thread([this, path] {
                            auto task = ImHexApi::Tasks::createTask("hex.builtin.view.hex_editor.processing", 0);

                            auto patchData = File(path, File::Mode::Read).readBytes();
                            auto patch     = hex::loadIPSPatch(patchData);

                            task.setMaxValue(patch.size());

                            auto provider = ImHexApi::Provider::get();

                            u64 progress = 0;
                            for (auto &[address, value] : patch) {
                                provider->addPatch(address, &value, 1);
                                progress++;
                                task.update(progress);
                            }

                            provider->createUndoPoint();
                            this->m_processingImportExport = false;
                        }).detach();

                        this->getWindowOpenState() = true;
                    });
                }

                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.import.ips32"_lang, nullptr, false, !this->m_processingImportExport)) {
                    hex::openFileBrowser("hex.builtin.view.hex_editor.open_file"_lang, DialogMode::Open, {}, [this](const auto &path) {
                        this->m_processingImportExport = true;
                        std::thread([this, path] {
                            auto task = ImHexApi::Tasks::createTask("hex.builtin.view.hex_editor.processing", 0);

                            auto patchData = File(path, File::Mode::Read).readBytes();
                            auto patch     = hex::loadIPS32Patch(patchData);

                            task.setMaxValue(patch.size());

                            auto provider = ImHexApi::Provider::get();

                            u64 progress = 0;
                            for (auto &[address, value] : patch) {
                                provider->addPatch(address, &value, 1);
                                progress++;
                                task.update(progress);
                            }

                            provider->createUndoPoint();
                            this->m_processingImportExport = false;
                        }).detach();

                        this->getWindowOpenState() = true;
                    });
                }

                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.import.script"_lang)) {
                    this->m_loaderScriptFilePath.clear();
                    this->m_loaderScriptScriptPath.clear();
                    ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.view.hex_editor.script.title"_lang); });
                }

                ImGui::EndMenu();
            }


            /* Export */
            if (ImGui::BeginMenu("hex.builtin.view.hex_editor.menu.file.export"_lang, providerValid && provider->isWritable())) {
                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.export.ips"_lang, nullptr, false, !this->m_processingImportExport)) {
                    Patches patches = provider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x00454F46)) {
                        u8 value = 0;
                        provider->read(0x00454F45, &value, sizeof(u8));
                        patches[0x00454F45] = value;
                    }

                    this->m_processingImportExport = true;
                    std::thread([this, patches] {
                        auto task = ImHexApi::Tasks::createTask("hex.builtin.view.hex_editor.processing", 0);

                        this->m_dataToSave             = generateIPSPatch(patches);
                        this->m_processingImportExport = false;

                        ImHexApi::Tasks::doLater([this] {
                            hex::openFileBrowser("hex.builtin.view.hex_editor.menu.file.export.title"_lang, DialogMode::Save, {}, [this](const auto &path) {
                                auto file = File(path, File::Mode::Create);
                                if (!file.isValid()) {
                                    View::showErrorPopup("hex.builtin.view.hex_editor.error.create"_lang);
                                    return;
                                }

                                file.write(this->m_dataToSave);
                            });
                        });
                    }).detach();
                }

                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.export.ips32"_lang, nullptr, false, !this->m_processingImportExport)) {
                    Patches patches = provider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x45454F46)) {
                        u8 value = 0;
                        provider->read(0x45454F45, &value, sizeof(u8));
                        patches[0x45454F45] = value;
                    }

                    this->m_processingImportExport = true;
                    std::thread([this, patches] {
                        auto task = ImHexApi::Tasks::createTask("hex.builtin.view.hex_editor.processing", 0);

                        this->m_dataToSave             = generateIPS32Patch(patches);
                        this->m_processingImportExport = false;

                        ImHexApi::Tasks::doLater([this] {
                            hex::openFileBrowser("hex.builtin.view.hex_editor.menu.file.export.title"_lang, DialogMode::Save, {}, [this](const auto &path) {
                                auto file = File(path, File::Mode::Create);
                                if (!file.isValid()) {
                                    View::showErrorPopup("hex.builtin.view.hex_editor.error.create"_lang);
                                    return;
                                }

                                file.write(this->m_dataToSave);
                            });
                        });
                    }).detach();
                }

                ImGui::EndMenu();
            }
        });


        /* Search / Goto */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1400, [&, this] {
            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.search"_lang, "CTRL + F")) {
                this->getWindowOpenState() = true;
                this->m_searchRequested    = true;
                ImGui::OpenPopupInWindow(View::toWindowName("hex.builtin.view.hex_editor.name").c_str(), "hex.builtin.view.hex_editor.menu.file.search"_lang);
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.goto"_lang, "CTRL + G")) {
                this->getWindowOpenState() = true;
                this->m_gotoRequested      = true;
                ImGui::OpenPopupInWindow(View::toWindowName("hex.builtin.view.hex_editor.name").c_str(), "hex.builtin.view.hex_editor.menu.file.goto"_lang);
            }
        });


        /* Edit menu */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.edit", 1000, [&, this] {
            this->drawEditPopup();
        });
    }

}