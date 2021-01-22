#include "views/view_tools.hpp"

#include <hex/providers/provider.hpp>

namespace hex {

    ViewTools::ViewTools() : View("Tools") { }

    ViewTools::~ViewTools() { }

    void ViewTools::drawContent() {
        if (ImGui::Begin("Tools", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            for (const auto& [name, function] : ContentRegistry::Tools::getEntries()) {
                if (ImGui::CollapsingHeader(name.c_str())) {
                    function();
                }
            }
        }
        ImGui::End();
    }

    void ViewTools::drawMenu() {

    }

}