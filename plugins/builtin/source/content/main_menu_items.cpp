#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <implot.h>

#include <hex/views/view.hpp>

namespace hex::plugin::builtin {

    static bool g_demoWindowOpen = false;

    void registerMainMenuEntries() {

        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.file",   1000);
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.edit",   2000);
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.view",   3000);
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.layout", 4000);
        ContentRegistry::Interface::registerMainMenuItem("hex.builtin.menu.help",   5000);

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.view", 1000, []{
            for (auto &[name, view] : ContentRegistry::Views::getEntries()) {
                if (view->hasViewMenuItemEntry())
                    ImGui::MenuItem(LangEntry(view->getUnlocalizedName()), "", &view->getWindowOpenState());
            }
        });

        #if defined(DEBUG)
            ContentRegistry::Interface::addMenuItem("hex.builtin.menu.view", 2000, []{
                ImGui::MenuItem("hex.builtin.menu.view.demo"_lang, "", &g_demoWindowOpen);
            });
        #endif

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.layout", 1000, [] {
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
    }

}