#include <hex/api/content_registry.hpp>
#include <hex/helpers/shared_data.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#include <codicons_font.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    void addFooterItems() {

        ContentRegistry::Interface::addFooterItem([] {
            static float framerate = 0;
            if (ImGui::HasSecondPassed()) {
                framerate = 1.0F / ImGui::GetIO().DeltaTime;
            }

            ImGui::TextUnformatted(hex::format("FPS {0:2}.{1:02}", u32(framerate), u32(framerate * 100) % 100).c_str());
        });

    }

    void addToolbarItems() {
        ContentRegistry::Interface::addToolbarItem([] {
            const static auto buttonSize = ImVec2(ImGui::GetCurrentWindow()->MenuBarHeight(), ImGui::GetCurrentWindow()->MenuBarHeight());

            auto provider = ImHexApi::Provider::get();

            // Undo
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_DISCARD, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue), buttonSize))
                    provider->undo();
            }, !ImHexApi::Provider::isValid() || !provider->canUndo());

            // Redo
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_REDO, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue), buttonSize))
                    provider->redo();
            }, !ImHexApi::Provider::isValid() || !provider->canRedo());


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            // Create new file
            if (ImGui::ToolBarButton(ICON_VS_FILE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGray), buttonSize))
                EventManager::post<RequestOpenWindow>("Create File");

            // Open file
            if (ImGui::ToolBarButton(ICON_VS_FOLDER_OPENED, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBrown), buttonSize))
                EventManager::post<RequestOpenWindow>("Open File");


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            // Save file
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_SAVE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue), buttonSize))
                    provider->save();
            }, !ImHexApi::Provider::isValid() || !provider->isWritable() || !provider->isSavable());

            // Save file as
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_SAVE_AS, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue), buttonSize))
                    hex::openFileBrowser("hex.view.hexeditor.save_as"_lang, DialogMode::Save, { }, [&provider](auto path) {
                        provider->saveAs(path);
                    });
            }, !ImHexApi::Provider::isValid() || !provider->isSavable());


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);


            // Create bookmark
            ImGui::Disabled([] {
                if (ImGui::ToolBarButton(ICON_VS_BOOKMARK, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen), buttonSize)) {
                    Region region = { 0 };
                    EventManager::post<QuerySelection>(region);

                    ImHexApi::Bookmarks::add(region.address, region.size, { }, { });
                }
            }, !ImHexApi::Provider::isValid() || !provider->isReadable());


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::Spacing();

            // Provider switcher
            ImGui::Disabled([] {
                auto &providers = ImHexApi::Provider::getProviders();

                std::string preview;
                if (ImHexApi::Provider::isValid())
                    preview = providers[SharedData::currentProvider]->getName();

                ImGui::SetNextItemWidth(200 * SharedData::globalScale);
                if (ImGui::BeginCombo("", preview.c_str())) {

                    for (int i = 0; i < providers.size(); i++) {
                        if (ImGui::Selectable(providers[i]->getName().c_str())) {
                            SharedData::currentProvider = i;
                        }
                    }

                    ImGui::EndCombo();
                }

            }, !ImHexApi::Provider::isValid());
        });

    }

}