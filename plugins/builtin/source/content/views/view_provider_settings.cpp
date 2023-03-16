#include "content/views/view_provider_settings.hpp"

namespace hex::plugin::builtin {

    ViewProviderSettings::ViewProviderSettings() : hex::View("hex.builtin.view.provider_settings.name") {
        EventManager::subscribe<EventProviderCreated>(this, [](hex::prv::Provider *provider) {
            if (provider->hasLoadInterface() && !provider->shouldSkipLoadInterface())
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
                bool settingsValid = provider->drawLoadInterface();

                ImGui::NewLine();
                ImGui::Separator();

                ImGui::BeginDisabled(!settingsValid);
                if (ImGui::Button("hex.builtin.common.open"_lang)) {
                    if (provider->open()) {
                        EventManager::post<EventProviderOpened>(provider);
                        ImGui::CloseCurrentPopup();
                    }
                    else {
                        View::showErrorPopup("hex.builtin.view.provider_settings.load_error"_lang);
                        TaskManager::doLater([=] { ImHexApi::Provider::remove(provider); });
                    }
                }
                ImGui::EndDisabled();

                ImGui::SameLine();

                if (ImGui::Button("hex.builtin.common.cancel"_lang)) {
                    ImGui::CloseCurrentPopup();
                    TaskManager::doLater([=] { ImHexApi::Provider::remove(provider); });
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

        return provider != nullptr && provider->hasInterface() && provider->isAvailable();
    }

}