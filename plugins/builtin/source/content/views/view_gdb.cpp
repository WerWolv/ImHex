#include "content/views/view_gdb.hpp"

#include "content/providers/gdb_provider.hpp"

namespace hex::plugin::builtin {

    ViewGDB::ViewGDB() : hex::View("hex.builtin.view.gdb.name") {
        this->m_address.reserve(3 * 4 + 4);
        this->m_port = 0;
    }

    void ViewGDB::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.gdb.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            ImGui::Header("hex.builtin.view.gdb.settings"_lang);
            ImGui::InputText("hex.builtin.view.gdb.ip"_lang, this->m_address.data(), this->m_address.capacity(), ImGuiInputTextFlags_CallbackEdit, ImGui::UpdateStringSizeCallback, &this->m_address);
            ImGui::InputInt("hex.builtin.view.gdb.port"_lang, &this->m_port, 1, 0xFFFF);

            ImGui::NewLine();

            auto provider = dynamic_cast<prv::GDBProvider*>(hex::ImHexApi::Provider::get());
            if (provider != nullptr) {
                if (!provider->isConnected()) {
                    if (ImGui::Button("hex.builtin.view.gdb.connect"_lang)) {
                        provider->connect(this->m_address, this->m_port);
                    }
                } else {
                    if (ImGui::Button("hex.builtin.view.gdb.disconnect"_lang)) {
                        provider->disconnect();
                    }
                }
            }
        }
        ImGui::End();
    }

    bool ViewGDB::hasViewMenuItemEntry() {
        return this->isAvailable();
    }

    bool ViewGDB::isAvailable() {
        auto provider = hex::ImHexApi::Provider::get();

        return provider != nullptr && dynamic_cast<prv::GDBProvider*>(provider) != nullptr;
    }

}