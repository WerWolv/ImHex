#include "content/views/view_provider_settings.hpp"

namespace hex::plugin::builtin {

    ViewProviderSettings::ViewProviderSettings() : hex::View("hex.builtin.view.provider_settings.name") {
        EventManager::subscribe<EventProviderCreated>(this, [](hex::prv::Provider *provider) {
            if (provider->hasLoadInterface())
                EventManager::post<RequestOpenPopup>(View::toWindowName("hex.builtin.view.provider_settings.load_popup"));
        });
    }

    ViewProviderSettings::~ViewProviderSettings() {
        EventManager::unsubscribe<EventProviderCreated>(this);
    }

    void ViewProviderSettings::drawContent() {
        if (ImGui::Begin(this->getName().c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            auto provider = hex::ImHexApi::Provider::get();

            if (provider != nullptr)
                provider->drawInterface();
        }
        ImGui::End();
    }

    void ViewProviderSettings::drawAlwaysVisible() {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        if (ImGui::BeginPopupModal(View::toWindowName("hex.builtin.view.provider_settings.load_popup").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

            auto provider = hex::ImHexApi::Provider::get();

            if (provider != nullptr) {
                provider->drawLoadInterface();

                ImGui::NewLine();
                ImGui::Separator();

                if (ImGui::Button("hex.common.open"_lang)) {
                    if (provider->open())
                        ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();

                if (ImGui::Button("hex.common.cancel"_lang)) {
                    ImHexApi::Provider::remove(provider);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }
    }

    bool ViewProviderSettings::hasViewMenuItemEntry() const {
        return this->isAvailable();
    }

    bool ViewProviderSettings::isAvailable() const {
        auto provider = hex::ImHexApi::Provider::get();

        return provider != nullptr && provider->hasInterface();
    }

}