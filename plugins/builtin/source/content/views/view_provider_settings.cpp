#include "content/views/view_provider_settings.hpp"

#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/events/events_provider.hpp>
#include <hex/providers/provider.hpp>

#include <toasts/toast_notification.hpp>

namespace hex::plugin::builtin {

    ViewProviderSettings::ViewProviderSettings() : View::Modal("hex.builtin.view.provider_settings.name", ICON_VS_SETTINGS) {
        EventProviderCreated::subscribe(this, [this](std::shared_ptr<prv::Provider> provider) {
            if (dynamic_cast<const prv::IProviderLoadInterface*>(provider.get()) != nullptr && !provider->shouldSkipLoadInterface())
                this->getWindowOpenState() = true;
        });

        ContentRegistry::UserInterface::addSidebarItem(ICON_VS_SERVER_PROCESS, [] {
            auto provider = hex::ImHexApi::Provider::get();

            if (auto *sidebarInterfaceProvider = dynamic_cast<prv::IProviderSidebarInterface*>(provider); sidebarInterfaceProvider != nullptr)
                sidebarInterfaceProvider->drawSidebarInterface();
        },
        [] {
            auto provider = hex::ImHexApi::Provider::get();

            return provider != nullptr && dynamic_cast<prv::IProviderSidebarInterface*>(provider) != nullptr && provider->isAvailable();
        });
    }

    ViewProviderSettings::~ViewProviderSettings() {
        EventProviderCreated::unsubscribe(this);
    }

    void ViewProviderSettings::drawContent() {
        auto provider = hex::ImHexApi::Provider::get();
        if (auto *loadInterfaceProvider = dynamic_cast<prv::IProviderLoadInterface*>(provider); loadInterfaceProvider != nullptr) {
            bool settingsValid = loadInterfaceProvider->drawLoadInterface();

            ImGui::NewLine();
            ImGui::Separator();

            ImGui::BeginDisabled(!settingsValid);
            if (ImGui::Button("hex.ui.common.open"_lang)) {
                auto result = provider->open();
                if (result.isSuccess()) {
                    EventProviderOpened::post(provider);

                    this->getWindowOpenState() = false;
                    ImGui::CloseCurrentPopup();
                }
                else {
                    this->getWindowOpenState() = false;
                    ImGui::CloseCurrentPopup();
                    auto errorMessage = result.getErrorMessage();
                    if (errorMessage.empty()) {
                        ui::ToastError::open("hex.builtin.view.provider_settings.load_error"_lang);
                    } else {
                        ui::ToastError::open(fmt::format("hex.builtin.view.provider_settings.load_error_details"_lang, errorMessage));
                    }
                    TaskManager::doLater([=] { ImHexApi::Provider::remove(provider); });
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("hex.ui.common.cancel"_lang)) {
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
