#include <hex/plugin.hpp>

namespace hex::plugin::builtin {

    void addFooterItems() {

        ContentRegistry::Interface::addFooterItem([] {
            static float framerate = 0;
            if (ImGui::HasSecondPassed()) {
                framerate = 1.0F / ImGui::GetIO().DeltaTime;
            }

            ImGui::TextUnformatted(hex::format("FPS {0:.2f}", framerate).c_str());
        });

    }

}