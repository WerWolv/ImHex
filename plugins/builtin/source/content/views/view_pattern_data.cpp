#include <content/views/view_pattern_data.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>

#include <pl/patterns/pattern.hpp>
#include <wolv/utils/lock.hpp>

namespace hex::plugin::builtin {

    ViewPatternData::ViewPatternData() : View("hex.builtin.view.pattern_data.name") {
        this->m_patternDrawer = std::make_unique<ui::PatternDrawer>();

        // Handle tree style setting changes
        EventManager::subscribe<EventSettingsChanged>(this, [this]() {
            auto patternStyle = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.pattern_tree_style", 0);
            this->m_patternDrawer->setTreeStyle(static_cast<ui::PatternDrawer::TreeStyle>(patternStyle));
        });

        // Reset the pattern drawer when the provider changes
        EventManager::subscribe<EventProviderChanged>(this, [this](auto, auto) {
            this->m_patternDrawer->reset();
        });

        EventManager::subscribe<EventPatternEvaluating>(this, [this]{
            this->m_patternDrawer->reset();
        });

        EventManager::subscribe<EventPatternExecuted>(this, [this](auto){
            this->m_patternDrawer->reset();
        });

        // Handle jumping to a pattern's location when it is clicked
        this->m_patternDrawer->setSelectionCallback([](Region region){ ImHexApi::HexEditor::setSelection(region); });
    }

    ViewPatternData::~ViewPatternData() {
        EventManager::unsubscribe<EventSettingsChanged>(this);
        EventManager::unsubscribe<EventProviderChanged>(this);
        EventManager::unsubscribe<EventPatternEvaluating>(this);
        EventManager::unsubscribe<EventPatternExecuted>(this);
    }

    void ViewPatternData::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.pattern_data.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            // Draw the pattern tree if the provider is valid
            if (ImHexApi::Provider::isValid()) {
                // Make sure the runtime has finished evaluating and produced valid patterns
                auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
                if (!runtime.arePatternsValid()) {
                    this->m_patternDrawer->draw({ });
                } else {
                    // If the runtime has finished evaluating, draw the patterns
                    if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                        this->m_patternDrawer->draw(runtime.getPatterns(), &runtime);
                    }
                }
            }
        }
        ImGui::End();
    }

}
