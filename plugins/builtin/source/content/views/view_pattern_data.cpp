#include <content/views/view_pattern_data.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>

#include <pl/patterns/pattern.hpp>

#include <content/helpers/provider_extra_data.hpp>

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
    }

    ViewPatternData::~ViewPatternData() {
        EventManager::unsubscribe<EventSettingsChanged>(this);
        EventManager::unsubscribe<EventProviderChanged>(this);
    }

    void ViewPatternData::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.pattern_data.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImHexApi::Provider::isValid()) {
                auto provider = ImHexApi::Provider::get();
                auto &patternLanguage = ProviderExtraData::get(provider).patternLanguage;

                const auto &patterns = [&] -> const auto& {
                    if (provider->isReadable() && patternLanguage.runtime != nullptr && patternLanguage.executionDone)
                        return ProviderExtraData::get(provider).patternLanguage.runtime->getAllPatterns();
                    else {
                        static const std::vector<std::shared_ptr<pl::ptrn::Pattern>> empty;
                        return empty;
                    }
                }();

                if (!patternLanguage.executionDone)
                    this->m_patternDrawer.reset();

                this->m_patternDrawer.draw(patterns);
            }
        }
        ImGui::End();
    }

}
