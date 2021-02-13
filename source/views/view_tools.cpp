#include "views/view_tools.hpp"

#include <hex/providers/provider.hpp>

namespace hex {

    ViewTools::ViewTools() : View("hex.view.tools.title"_lang) { }

    ViewTools::~ViewTools() { }

    void ViewTools::drawContent() {
        if (ImGui::Begin("hex.view.tools.title"_lang, &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            for (const auto& [name, function] : ContentRegistry::Tools::getEntries()) {
                if (ImGui::CollapsingHeader(LangEntry(name))) {
                    function();
                }
            }
        }
        ImGui::End();
    }

    void ViewTools::drawMenu() {

    }

}