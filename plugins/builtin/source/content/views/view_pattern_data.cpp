#include <content/views/view_pattern_data.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/providers/memory_provider.hpp>

#include <fonts/vscode_icons.hpp>

#include <pl/patterns/pattern.hpp>
#include <wolv/utils/lock.hpp>

namespace hex::plugin::builtin {

    ViewPatternData::ViewPatternData() : View::Window("hex.builtin.view.pattern_data.name", ICON_VS_DATABASE) {
        // Handle tree style setting changes

        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.pattern_tree_style", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_treeStyle = ui::PatternDrawer::TreeStyle(value.get<int>(0));
            for (auto &drawer : m_patternDrawer.all())
                drawer->setTreeStyle(m_treeStyle);
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.pattern_data_row_bg", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_rowColoring = bool(value.get<int>(false));
            for (auto &drawer : m_patternDrawer.all())
                drawer->enableRowColoring(m_rowColoring);
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.general", "hex.builtin.setting.general.pattern_data_max_filter_items", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_maxFilterItems = value.get<u32>(128);
            for (auto &drawer : m_patternDrawer.all())
                drawer->setMaxFilterDisplayItems(m_maxFilterItems);
        });

        EventPatternEvaluating::subscribe(this, [this]{
            (*m_patternDrawer)->reset();
        });

        EventPatternExecuted::subscribe(this, [this](const auto&){
            (*m_patternDrawer)->reset();
        });

        RequestJumpToPattern::subscribe(this, [this](const pl::ptrn::Pattern *pattern) {
           (*m_patternDrawer)->jumpToPattern(pattern);
        });

        ImHexApi::HexEditor::addHoverHighlightProvider([this](const prv::Provider *, u64, size_t) -> std::set<Region> {
            return { m_hoveredPatternRegion };
        });

        m_patternDrawer.setOnCreateCallback([this](const prv::Provider *, auto &drawer) {
            drawer = std::make_unique<ui::PatternDrawer>();

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
            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

            const auto height = std::max(ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() - (ImGui::GetStyle().FramePadding.y * 2), ImGui::GetTextLineHeightWithSpacing() * 5);

            if (*m_patternDrawer != nullptr) {
                (*m_patternDrawer)->enablePatternEditing(ImHexApi::Provider::get()->isWritable());
                if (!runtime.arePatternsValid()) {
                    (*m_patternDrawer)->draw({ }, nullptr, height);
                } else {
                    // If the runtime has finished evaluating, draw the patterns
                    if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                        (*m_patternDrawer)->draw(runtime.getPatterns(), &runtime, height);
                    }
                }
            }
        }
    }

}
