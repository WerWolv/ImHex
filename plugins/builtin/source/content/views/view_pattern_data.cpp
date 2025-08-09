#include <content/views/view_pattern_data.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/providers/memory_provider.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/api/events/events_interaction.hpp>

#include <fonts/vscode_icons.hpp>

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
            for (const auto &id : sections | std::views::keys) {
                (*m_patternDrawer)[id] = createDefaultDrawer();
            }
        });

        RequestJumpToPattern::subscribe(this, [this](const pl::ptrn::Pattern *pattern) {
           (*m_patternDrawer)[0]->jumpToPattern(pattern);
        });

        ImHexApi::HexEditor::addHoverHighlightProvider([this](const prv::Provider *, u64, size_t) -> std::set<Region> {
            return { m_hoveredPatternRegion };
        });
    }

    ViewPatternData::~ViewPatternData() {
        EventPatternEvaluating::unsubscribe(this);
        EventPatternExecuted::unsubscribe(this);
    }

    void ViewPatternData::drawContent() {
        // Draw the pattern tree if the provider is valid
        if (ImHexApi::Provider::isValid()) {
            // Make sure the runtime has finished evaluating and produced valid patterns
            bool patternsValid = false;
            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                patternsValid = runtime.arePatternsValid();
            }

            if (ImGui::BeginTabBar("##SectionSelector")) {

                const auto height = std::max(ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() - (ImGui::GetStyle().FramePadding.y * 2), ImGui::GetTextLineHeightWithSpacing() * 5);

                if (!patternsValid) {
                    ImGui::BeginDisabled();
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_data.section.main"_lang)) {
                        static ui::PatternDrawer emptyDrawer;
                        emptyDrawer.draw({ }, nullptr, height);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndDisabled();
                } else {
                    static i32 selectedSection = -1;
                    for (auto &[id, drawer] : *m_patternDrawer) {
                        drawer->enablePatternEditing(ImHexApi::Provider::get()->isWritable());

                        // If the runtime has finished evaluating, draw the patterns
                        if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                            const auto &sections = runtime.getSections();
                            if (id != 0 && !sections.contains(id))
                                continue;

                            if (ImGui::BeginTabItem(id == 0 ? "hex.builtin.view.pattern_data.section.main"_lang : sections.at(id).name.c_str())) {
                                drawer->draw(runtime.getPatterns(id), &runtime, height);
                                ImGui::EndTabItem();
                            }

                            if (id != 0) {
                                if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                                    ImGui::OpenPopup("##PatternDataContextMenu");
                                    selectedSection = id;
                                }
                            }
                        }
                    }

                    if (ImGui::BeginPopup("##PatternDataContextMenu")) {
                        if (ImGui::MenuItemEx("hex.builtin.view.pattern_data.section.view_raw"_lang, ICON_VS_OPEN_PREVIEW)) {
                            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                                const auto &sections = runtime.getSections();
                                if (auto it = sections.find(selectedSection); it != sections.end()) {
                                    const auto &[id, section] = *it;
                                    ImHexApi::Provider::add<prv::MemoryProvider>(section.data, section.name);
                                }
                            }
                        }
                        ImGui::EndPopup();
                    }
                }

                constexpr static auto SimplifiedEditorAttribute = "hex::editor_export";
                if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock()) && patternsValid) {
                    const auto &patternSet = runtime.getPatternsWithAttribute(SimplifiedEditorAttribute);
                    std::vector<pl::ptrn::Pattern*> patterns = { patternSet.begin(), patternSet.end() };
                    std::ranges::sort(patterns, [](const pl::ptrn::Pattern *a, const pl::ptrn::Pattern *b) {
                        return a->getOffset() < b->getOffset() || a->getDisplayName() < b->getDisplayName();
                    });

                    if (!patterns.empty()) {
                        constexpr auto TabName = "hex.builtin.view.pattern_data.simplified_editor"_lang;

                        ImGui::TabItemSpacing("##spacing", 0, ImGui::GetContentRegionAvail().x - ImGui::TabItemCalcSize(TabName, false).x);
                        if (ImGui::BeginTabItem(TabName, nullptr, ImGuiTabItemFlags_Trailing)) {
                            if (ImGui::BeginChild("##editor")) {
                                for (const auto &pattern : patterns) {
                                    ImGui::PushID(pattern);
                                    try {
                                        const auto attribute = pattern->getAttributeArguments(SimplifiedEditorAttribute);

                                        const auto name = attribute.size() >= 1 ? attribute[0].toString() : pattern->getDisplayName();
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
                    }
                }

                ImGui::EndTabBar();
            }

        }
    }

}
