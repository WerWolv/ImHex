#include <hex/plugin.hpp>

#include <windows.h>
#include <psapi.h>

namespace hex::plugin::builtin {

    void addFooterItems() {

        ContentRegistry::Interface::addFooterItem([] {
            static float framerate = 0;
            if (ImGui::HasSecondPassed()) {
                framerate = ImGui::GetIO().Framerate;
            }

            ImGui::TextUnformatted(hex::format("FPS {0:.2f}", framerate).c_str());
        });

    }

}