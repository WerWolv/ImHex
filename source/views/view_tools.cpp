#include "views/view_tools.hpp"

#include <hex/providers/provider.hpp>

namespace hex {

    ViewTools::ViewTools() : View("hex.view.tools.name") { }

    ViewTools::~ViewTools() { }

    void ViewTools::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.view.tools.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
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