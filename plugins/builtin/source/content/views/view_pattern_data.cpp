#include <content/views/view_pattern_data.hpp>

#include <hex/api/content_registry/settings.hpp>
#include <hex/providers/memory_provider.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/api/events/events_interaction.hpp>

#include <fonts/vscode_icons.hpp>
#include <fonts/tabler_icons.hpp>

#include <imgui_internal.h>

#include <pl/patterns/pattern.hpp>
#include <wolv/utils/lock.hpp>

#include <ranges>

namespace hex::plugin::builtin {

    ViewPatternData::ViewPatternData() : View::Window("hex.builtin.view.pattern_data.name", ICON_VS_DATABASE) {
        // Handle tree style setting changes

        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.pattern_tree_style", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_treeStyle = ui::PatternDrawer::TreeStyle(value.get<int>(0));
            for (auto &drawers : m_patternDrawer.all())
                for (auto &[id, drawer] : drawers)
                    drawer->setTreeStyle(m_treeStyle);
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.pattern_data_row_bg", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_rowColoring = bool(value.get<int>(false));
            for (auto &drawers : m_patternDrawer.all())
                for (auto &[id, drawer] : drawers)
                    drawer->enableRowColoring(m_rowColoring);
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.general", "hex.builtin.setting.general.pattern_data_max_filter_items", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_maxFilterItems = value.get<u32>(128);
            for (auto &drawers : m_patternDrawer.all())
                for (auto &[id, drawer] : drawers)
                    drawer->setMaxFilterDisplayItems(m_maxFilterItems);
        });

        EventPatternEvaluating::subscribe(this, [this]{
            m_virtualFiles->clear();
            for (auto &drawers : m_patternDrawer.all())
                for (auto &[id, drawer] : drawers)
                    drawer->reset();
        });

        EventPatternExecuted::subscribe(this, [this](const auto&){
            for (auto &drawers : m_patternDrawer.all())
                for (auto &[id, drawer] : drawers)
                    drawer->reset();

            const auto createDefaultDrawer = [this]() {
                auto drawer = std::make_unique<ui::PatternDrawer>();

                drawer->setSelectionCallback([](const pl::ptrn::Pattern *pattern) {
                    ImHexApi::HexEditor::setSelection(Region(pattern->getOffset(), pattern->getSize()));
                    RequestPatternEditorSelectionChange::post(pattern->getLine(), 0);
                });

                drawer->setHoverCallback([this](const pl::ptrn::Pattern *pattern) {
                    if (pattern == nullptr)
                        m_hoveredPatternRegion = Region::Invalid();
                    else
                        m_hoveredPatternRegion = Region(pattern->getOffset(), pattern->getSize());
                });

                drawer->setTreeStyle(m_treeStyle);
                drawer->enableRowColoring(m_rowColoring);

                return drawer;
            };

            const auto &sections = ContentRegistry::PatternLanguage::getRuntime().getSections();

            (*m_patternDrawer)[0] = createDefaultDrawer();
            for (const auto &[id, section] : sections) {
                (*m_patternDrawer)[id] = createDefaultDrawer();
            }
        });

        RequestJumpToPattern::subscribe(this, [this](const pl::ptrn::Pattern *pattern) {
           (*m_patternDrawer)[0]->jumpToPattern(pattern);
        });

        RequestAddVirtualFile::subscribe(this, [this](const std::fs::path &path, const std::vector<u8> &data, Region region) {
            m_virtualFiles->emplace_back(path, data, region);
        });

        ImHexApi::HexEditor::addHoverHighlightProvider([this](const prv::Provider *, u64, size_t) -> std::set<Region> {
            return { m_hoveredPatternRegion };
        });
    }

    ViewPatternData::~ViewPatternData() {
        EventPatternEvaluating::unsubscribe(this);
        EventPatternExecuted::unsubscribe(this);
    }


    static void loadPatternAsMemoryProvider(const VirtualFile *file) {
        ImHexApi::Provider::add<prv::MemoryProvider>(file->data, wolv::util::toUTF8String(file->path.filename()));
    }

    static void drawVirtualFileTree(const std::vector<const VirtualFile*> &virtualFiles, u32 level = 0) {
        static int levelId = 0;
        if (level == 0)
            levelId = 1;

        ImGui::PushID(level + 1);
        ON_SCOPE_EXIT { ImGui::PopID(); };

        std::map<std::string, std::vector<const VirtualFile*>> currFolderEntries;
        for (const auto &file : virtualFiles) {
            const auto &path = file->path;

            auto currSegment = wolv::io::fs::toNormalizedPathString(*std::next(path.begin(), level));
            if (std::distance(path.begin(), path.end()) == i32(level + 1)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextUnformatted(ICON_VS_FILE);
                ImGui::PushID(levelId);
                ImGui::SameLine();

                ImGui::TreeNodeEx(currSegment.c_str(), ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup("##virtual_files_context_menu");
                }
                if (ImGui::BeginPopup("##virtual_files_context_menu")) {
                    if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.open_in_new_provider"_lang, nullptr, false)) {
                        loadPatternAsMemoryProvider(file);
                    }
                    ImGui::EndPopup();
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) {
                    loadPatternAsMemoryProvider(file);
                }
                ImGui::PopID();
                levelId += 1;
                continue;
            }

            currFolderEntries[currSegment].emplace_back(file);
        }

        int id = 1;
        for (const auto &[segment, entries] : currFolderEntries) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, 0.0F);

            if (level == 0) {
                ImGui::TextUnformatted(ICON_VS_DATABASE);
            } else {
                ImGui::TextUnformatted(ICON_VS_FOLDER);
            }

            ImGui::PushID(id);

            ImGui::SameLine(0, 20_scaled);

            const auto open = ImGui::TreeNodeEx("##Segment", ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_SpanLabelWidth | ImGuiTreeNodeFlags_OpenOnArrow);
            ImGui::SameLine();
            ImGui::TextUnformatted(segment.c_str());
            ImGui::PopStyleVar();
            if (open) {
                drawVirtualFileTree(entries, level + 1);
                ImGui::TreePop();
            }

            ImGui::PopID();
            id += 1;
        }
    }

    static void setItemInfoTooltip(const char *title, const char *description) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
            ImGui::SetNextWindowSize(scaled(300, 0), ImGuiCond_Always);
            if (ImGui::BeginTooltipEx(ImGuiTooltipFlags_OverridePrevious, ImGuiWindowFlags_None)) {
                if (ImGuiExt::BeginSubWindow(title)) {
                    ImGuiExt::TextFormattedWrapped("{}", description);
                }
                ImGuiExt::EndSubWindow();
                ImGui::EndTooltip();
            }
        }
    }

    static void selectFirstTabItem() {
        auto *tabBar = ImGui::GetCurrentTabBar();
        if (tabBar != nullptr) {
            for (const auto &tab : tabBar->Tabs) {
                if (tab.Flags & ImGuiTabItemFlags_Button) {
                    continue;
                }

                tabBar->SelectedTabId = tab.ID;
                break;
            }
        }
    }


    void ViewPatternData::drawContent() {
        // Draw the pattern tree if the provider is valid
        if (ImHexApi::Provider::isValid()) {
            // Make sure the runtime has finished evaluating and produced valid patterns
            bool patternsValid = false;
            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                patternsValid = runtime.getCreatedPatternCount() > 0 && runtime.arePatternsValid();
            }

            if (ImGui::BeginTabBar("##SectionSelector")) {

                const auto height = std::max(ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() - (ImGui::GetStyle().FramePadding.y * 2), ImGui::GetTextLineHeightWithSpacing() * 5);

                if (!patternsValid) {
                    ImGui::BeginDisabled();
                    ImGui::PushID(1);
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_data.section.main"_lang)) {
                        static ui::PatternDrawer emptyDrawer;
                        emptyDrawer.draw({ }, nullptr, height);
                        ImGui::EndTabItem();
                    }
                    ImGui::PopID();
                    ImGui::EndDisabled();
                } else {
                    static i32 selectedSection = -1;
                    for (auto &[id, drawer] : *m_patternDrawer) {
                        ImGui::PushID(id + 1);
                        ON_SCOPE_EXIT { ImGui::PopID(); };
                        drawer->enablePatternEditing(ImHexApi::Provider::get()->isWritable());

                        // If the runtime has finished evaluating, draw the patterns
                        if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                            const auto &sections = runtime.getSections();
                            if (id != 0 && !sections.contains(id))
                                continue;

                            const bool open = ImGui::BeginTabItem(id == 0 ? "hex.builtin.view.pattern_data.section.main"_lang : sections.at(id).name.c_str());
                            const bool hovered = ImGui::IsItemHovered();
                            if (open) {
                                drawer->draw(runtime.getPatterns(id), &runtime, height);
                                ImGui::EndTabItem();
                            }

                            if (id != 0) {
                                if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                                    ImGui::OpenPopup("##PatternDataContextMenu");
                                    selectedSection = id;
                                }

                                if (ImGui::BeginPopup("##PatternDataContextMenu")) {
                                    if (ImGui::MenuItemEx("hex.builtin.view.pattern_data.section.view_raw"_lang, ICON_VS_OPEN_PREVIEW)) {
                                        if (auto it = sections.find(selectedSection); it != sections.end()) {
                                            const auto &[sectionId, section] = *it;
                                            ImHexApi::Provider::add<prv::MemoryProvider>(section.data, section.name);
                                        }
                                    }
                                    ImGui::EndPopup();
                                }
                            }
                        }
                    }
                }

                constexpr static auto SimplifiedEditorAttribute = "hex::editor_export";
                if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                    constexpr static auto SimplifiedEditorTabName = "hex.builtin.view.pattern_data.simplified_editor"_lang;
                    const auto simplifiedEditorPatterns = [&] {
                        const auto &patternSet = runtime.getPatternsWithAttribute(SimplifiedEditorAttribute);

                        std::vector<pl::ptrn::Pattern*> result = { patternSet.begin(), patternSet.end() };
                        std::ranges::sort(result, [](const pl::ptrn::Pattern *a, const pl::ptrn::Pattern *b) {
                            return a->getOffset() < b->getOffset() || a->getDisplayName() < b->getDisplayName();
                        });

                        return result;
                    }();

                    constexpr static auto VirtualFilesTabName = "hex.builtin.view.pattern_data.virtual_files"_lang;

                    float spacingWidth = ImGui::GetContentRegionAvail().x;

                    spacingWidth -= ImGui::TabItemCalcSize(ICON_TA_ACCESSIBLE, false).x;
                    spacingWidth -= ImGui::TabItemCalcSize(ICON_TA_BINARY_TREE, false).x;

                    ImGui::TabItemSpacing("##spacing", ImGuiTabItemFlags_None, spacingWidth);

                    ImGui::BeginDisabled(simplifiedEditorPatterns.empty());
                    if (ImGui::BeginTabItem(ICON_TA_ACCESSIBLE, nullptr, ImGuiTabItemFlags_Trailing)) {
                        if (simplifiedEditorPatterns.empty())
                            selectFirstTabItem();

                        if (ImGui::BeginChild("##editor")) {
                            for (const auto &pattern : simplifiedEditorPatterns) {
                                ImGui::PushID(pattern);
                                try {
                                    const auto attribute = pattern->getAttributeArguments(SimplifiedEditorAttribute);

                                    const auto name = !attribute.empty() ? attribute[0].toString() : pattern->getDisplayName();
                                    const auto description = attribute.size() >= 2 ? attribute[1].toString() : pattern->getComment();

                                    const auto widgetPos = 200_scaled;
                                    ImGui::TextUnformatted(name.c_str());
                                    ImGui::SameLine(0, 20_scaled);
                                    if (ImGui::GetCursorPosX() < widgetPos)
                                        ImGui::SetCursorPosX(widgetPos);

                                    ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 0);
                                    ImGui::PushItemWidth(-50_scaled);
                                    pattern->accept(m_patternValueEditor);
                                    ImGui::PopItemWidth();
                                    ImGui::PopStyleVar();

                                    if (!description.empty()) {
                                        ImGui::PushFont(nullptr, ImGui::GetFontSize() * 0.8F);
                                        ImGui::BeginDisabled();
                                        ImGui::Indent();
                                        ImGui::TextWrapped("%s", description.c_str());
                                        ImGui::Unindent();
                                        ImGui::EndDisabled();
                                        ImGui::PopFont();
                                    }

                                    ImGui::Separator();

                                } catch (const std::exception &e) {
                                    ImGui::TextUnformatted(pattern->getDisplayName().c_str());
                                    ImGui::TextUnformatted(e.what());
                                }

                                ImGui::PopID();
                            }

                            ImGui::EndChild();
                        }

                        ImGui::EndTabItem();
                    }
                    ImGui::EndDisabled();

                    if (simplifiedEditorPatterns.empty())
                        setItemInfoTooltip(SimplifiedEditorTabName, "hex.builtin.view.pattern_data.simplified_editor.no_patterns"_lang);

                    ImGui::BeginDisabled(m_virtualFiles->empty());
                    if (ImGui::BeginTabItem(ICON_TA_BINARY_TREE, nullptr, ImGuiTabItemFlags_Trailing)) {
                        if (m_virtualFiles->empty())
                            selectFirstTabItem();

                        std::vector<const VirtualFile*> virtualFilePointers;
                        for (const auto &file : *m_virtualFiles)
                            virtualFilePointers.emplace_back(&file);

                        if (ImGui::BeginTable("##virtual_file_tree", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImGui::GetContentRegionAvail())) {
                            ImGui::TableSetupColumn("##path", ImGuiTableColumnFlags_WidthStretch);
                            drawVirtualFileTree(virtualFilePointers);

                            ImGui::EndTable();
                        }

                        ImGui::EndTabItem();
                    }
                    ImGui::EndDisabled();

                    if (m_virtualFiles->empty())
                        setItemInfoTooltip(VirtualFilesTabName, "hex.builtin.view.pattern_data.virtual_files.no_virtual_files"_lang);
                }

                ImGui::EndTabBar();
            }

        }
    }

    void ViewPatternData::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This view displays the pattern tree generated by the previously executed pattern script. It only becomes active once a pattern has been successfully executed.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("Clicking on a pattern in the tree will select the corresponding bytes in the Hex Editor view and jump to its definition in the Pattern Editor view. Additionally you can edit the values of patterns by double clicking them which will automatically update the bytes in your loaded data source.");
    }

}
