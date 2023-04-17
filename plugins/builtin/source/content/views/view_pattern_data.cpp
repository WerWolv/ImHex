#include <content/views/view_pattern_data.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>

#include <pl/patterns/pattern.hpp>

namespace hex::plugin::builtin {

    ViewPatternData::ViewPatternData() : View("hex.builtin.view.pattern_data.name") {

        EventManager::subscribe<EventSettingsChanged>(this, [this]() {
            auto patternStyle = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.pattern_tree_style", 0);
            this->m_patternDrawer.setTreeStyle(static_cast<ui::PatternDrawer::TreeStyle>(patternStyle));
        });

        EventManager::subscribe<EventProviderChanged>(this, [this](auto, auto) {
            this->m_patternDrawer.reset();
        });

        this->m_patternDrawer.setSelectionCallback([](Region region){ ImHexApi::HexEditor::setSelection(region); });

        EventManager::subscribe<EventPatternExecuted>([this](const auto&){
            this->m_shouldReset = true;
        });
    }

    ViewPatternData::~ViewPatternData() {
        EventManager::unsubscribe<EventSettingsChanged>(this);
        EventManager::unsubscribe<EventProviderChanged>(this);
    }

    void ViewPatternData::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.pattern_data.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImHexApi::Provider::isValid()) {
                auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
                if (!runtime.arePatternsValid()) {
                    this->m_patternDrawer.draw({});
                } else {
                    auto lock = ContentRegistry::PatternLanguage::getRuntimeLock();

                    if (this->m_shouldReset) {
                        this->m_patternDrawer.reset();
                        this->m_shouldReset = false;
                    }

                    this->m_patternDrawer.draw(runtime.getAllPatterns());
                }
            }
        }
        ImGui::End();
    }

}
