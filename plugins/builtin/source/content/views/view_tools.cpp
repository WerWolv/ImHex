#include "content/views/view_tools.hpp"

#include <hex/api/content_registry.hpp>

namespace hex::plugin::builtin {

    ViewTools::ViewTools() : View("hex.builtin.view.tools.name") { }

    void ViewTools::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.tools.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            for (const auto &[name, function] : ContentRegistry::Tools::getEntries()) {
                if (ImGui::CollapsingHeader(LangEntry(name))) {
                    function();
                }
            }
        }
        ImGui::End();
    }

}