#include "content/views/view_pattern_data.hpp"

#include <hex/providers/provider.hpp>

#include <pl/patterns/pattern.hpp>

#include <content/helpers/provider_extra_data.hpp>

namespace hex::plugin::builtin {

    ViewPatternData::ViewPatternData() : View("hex.builtin.view.pattern_data.name") {

        EventManager::subscribe<EventHighlightingChanged>(this, [this]() {
            if (!ImHexApi::Provider::isValid()) return;

            this->m_sortedPatterns[ImHexApi::Provider::get()].clear();
        });

        EventManager::subscribe<EventProviderClosed>([this](auto *provider) {
            this->m_sortedPatterns[provider].clear();
        });
    }

    ViewPatternData::~ViewPatternData() {
        EventManager::unsubscribe<EventHighlightingChanged>(this);
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

                this->m_patternDrawer.draw(patterns);
            }
        }
        ImGui::End();
    }

}
