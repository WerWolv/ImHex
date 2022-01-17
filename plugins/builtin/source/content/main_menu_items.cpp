#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <implot.h>

#include <hex/views/view.hpp>

namespace hex::plugin::builtin {

    static bool g_demoWindowOpen = false;

    void registerMainMenuEntries() {

        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.file");
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.edit");

        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.view", [] {
            for (auto &[name, view] : ContentRegistry::Views::getEntries()) {
                if (view->hasViewMenuItemEntry())
                    ImGui::MenuItem(LangEntry(view->getUnlocalizedName()), "", &view->getWindowOpenState());
            }

            #if defined(DEBUG)
                ImGui::Separator();
                ImGui::MenuItem("hex.builtin.menu.view.demo"_lang, "", &g_demoWindowOpen);
            #endif
        });

        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.layout", [] {
            for (auto &[layoutName, func] : ContentRegistry::Interface::getLayouts()) {
                if (ImGui::MenuItem(LangEntry(layoutName), "", false, ImHexApi::Provider::isValid())) {
                    auto dock = ContentRegistry::Interface::getDockSpaceId();

                    for (auto &[viewName, view] : ContentRegistry::Views::getEntries()) {
                        view->getWindowOpenState() = false;
                    }

                    ImGui::DockBuilderRemoveNode(dock);
                    ImGui::DockBuilderAddNode(dock);
                    func(dock);
                    ImGui::DockBuilderFinish(dock);
                }
            }
        });

        (void) EventManager::subscribe<EventFrameEnd>([]{
            if (g_demoWindowOpen) {
                ImGui::ShowDemoWindow(&g_demoWindowOpen);
                ImPlot::ShowDemoWindow(&g_demoWindowOpen);
            }
        });

        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.help");

    }

}