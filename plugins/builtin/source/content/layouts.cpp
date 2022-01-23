#include <hex/api/content_registry.hpp>

#include <hex/views/view.hpp>

namespace hex::plugin::builtin {

    static void openViewAndDockTo(const std::string &unlocalizedName, ImGuiID dockId) {
        auto view = ContentRegistry::Views::getViewByName(unlocalizedName);

        if (view != nullptr) {
            view->getWindowOpenState() = true;
            ImGui::DockBuilderDockWindow(view->getName().c_str(), dockId);

        }
    }

    void registerLayouts() {

        ContentRegistry::Interface::addLayout("hex.builtin.layouts.default", [](ImGuiID dockMain) {
            ImGuiID hexEditor   = ImGui::DockBuilderSplitNode(dockMain,  ImGuiDir_Left,  0.7F, nullptr, &dockMain);
            ImGuiID utils       = ImGui::DockBuilderSplitNode(dockMain,  ImGuiDir_Right, 0.8F, nullptr, &dockMain);
            ImGuiID patternData = ImGui::DockBuilderSplitNode(hexEditor, ImGuiDir_Down,  0.3F, nullptr, &hexEditor);
            ImGuiID inspector   = ImGui::DockBuilderSplitNode(hexEditor, ImGuiDir_Right, 0.3F, nullptr, &hexEditor);

            openViewAndDockTo("hex.builtin.view.hexeditor.name",        hexEditor);
            openViewAndDockTo("hex.builtin.view.data_inspector.name",   inspector);
            openViewAndDockTo("hex.builtin.view.pattern_data.name",     patternData);

            openViewAndDockTo("hex.builtin.view.pattern_editor.name",   utils);
            openViewAndDockTo("hex.builtin.view.hashes.name",           utils);
            openViewAndDockTo("hex.builtin.view.data_information.name", utils);
            openViewAndDockTo("hex.builtin.view.strings.name",          utils);
            openViewAndDockTo("hex.builtin.view.bookmarks.name",        utils);

        });

    }

}