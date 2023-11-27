#include "content/views/view_provider_settings.hpp"

#include <content/popups/popup_notification.hpp>
#include <hex/api/content_registry.hpp>

namespace hex::plugin::builtin {

    ViewProviderSettings::ViewProviderSettings() : View::Modal("hex.builtin.view.provider_settings.name") {
        EventManager::subscribe<EventProviderCreated>(this, [this](const hex::prv::Provider *provider) {
            if (provider->hasLoadInterface() && !provider->shouldSkipLoadInterface())
                this->getWindowOpenState() = true;
        });

        ContentRegistry::Interface::addSidebarItem(ICON_VS_SERVER_PROCESS, [] {
            auto provider = hex::ImHexApi::Provider::get();

            if (provider != nullptr)
                provider->drawInterface();
        },
        [] {
            auto provider = hex::ImHexApi::Provider::get();

            return provider != nullptr && provider->hasInterface() && provider->isAvailable();
        });
    }

    ViewProviderSettings::~ViewProviderSettings() {
        EventManager::unsubscribe<EventProviderCreated>(this);
    }

    void ViewProviderSettings::drawContent() {
        auto provider = hex::ImHexApi::Provider::get();
        if (provider != nullptr) {
            bool settingsValid = provider->drawLoadInterface();

            ImGui::NewLine();
            ImGui::Separator();

            ImGui::BeginDisabled(!settingsValid);
            if (ImGui::Button("hex.builtin.common.open"_lang)) {
                if (provider->open()) {
                    EventManager::post<EventProviderOpened>(provider);

                    this->getWindowOpenState() = false;
                    ImGui::CloseCurrentPopup();
                }
                else {
                    this->getWindowOpenState() = false;
                    ImGui::CloseCurrentPopup();
                    auto errorMessage = provider->getErrorMessage();
                    if (errorMessage.empty()) {
                        PopupError::open("hex.builtin.view.provider_settings.load_error"_lang);
                    } else {
                        PopupError::open(hex::format("hex.builtin.view.provider_settings.load_error_details"_lang, errorMessage));
                    }
                    TaskManager::doLater([=] { ImHexApi::Provider::remove(provider); });
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("hex.builtin.common.cancel"_lang)) {
                ImGui::CloseCurrentPopup();
                this->getWindowOpenState() = false;
                TaskManager::doLater([=] { ImHexApi::Provider::remove(provider); });
            }
        }
    }

    bool ViewProviderSettings::hasViewMenuItemEntry() const {
        return false;
    }

}
