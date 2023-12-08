#include <content/views/view_pattern_data.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>

#include <pl/patterns/pattern.hpp>
#include <wolv/utils/lock.hpp>

namespace hex::plugin::builtin {

    ViewPatternData::ViewPatternData() : View::Window("hex.builtin.view.pattern_data.name") {
        this->m_patternDrawer = std::make_unique<ui::PatternDrawer>();

        // Handle tree style setting changes
        EventSettingsChanged::subscribe(this, [this] {
            auto patternStyle = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.pattern_tree_style", 0);
            this->m_patternDrawer->setTreeStyle(patternStyle);

            auto rowColoring = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.pattern_data_row_bg", false);
            this->m_patternDrawer->enableRowColoring(rowColoring);
        });

        // Reset the pattern drawer when the provider changes
        EventProviderChanged::subscribe(this, [this](auto, auto) {
            this->m_patternDrawer->reset();
        });

        EventPatternEvaluating::subscribe(this, [this]{
            this->m_patternDrawer->reset();
        });

        EventPatternExecuted::subscribe(this, [this](auto){
            this->m_patternDrawer->reset();
        });

        // Handle jumping to a pattern's location when it is clicked
        this->m_patternDrawer->setSelectionCallback([](Region region){ ImHexApi::HexEditor::setSelection(region); });
    }

    ViewPatternData::~ViewPatternData() {
        EventSettingsChanged::unsubscribe(this);
        EventProviderChanged::unsubscribe(this);
        EventPatternEvaluating::unsubscribe(this);
        EventPatternExecuted::unsubscribe(this);
    }

    void ViewPatternData::drawContent() {
        // Draw the pattern tree if the provider is valid
        if (ImHexApi::Provider::isValid()) {
            // Make sure the runtime has finished evaluating and produced valid patterns
            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

            const auto height = std::max(ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().FramePadding.y * 2, ImGui::GetTextLineHeightWithSpacing() * 5);
            if (!runtime.arePatternsValid()) {
                this->m_patternDrawer->draw({ }, nullptr, height);
            } else {
                // If the runtime has finished evaluating, draw the patterns
                if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                    this->m_patternDrawer->draw(runtime.getPatterns(), &runtime, height);
                }
            }
        }
    }

}
