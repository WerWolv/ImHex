#include <imgui.h>
#include <content/providers/udp_provider.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fmt/chrono.h>

namespace hex::plugin::builtin {

    prv::Provider::OpenResult UDPProvider::open() {
        m_udpServer = UDPServer(m_port, [this](std::span<const u8> data) {
            this->receive(data);
        });
        m_udpServer.start();

        return {};
    }

    void UDPProvider::close() {
        m_udpServer.stop();
    }

    void UDPProvider::receive(std::span<const u8> data) {
        std::scoped_lock lock(m_mutex);

        m_messages.emplace_back(
            std::vector(data.begin(), data.end()),
            std::chrono::system_clock::now()
        );

        this->markDirty();
    }

    u64 UDPProvider::getActualSize() const {
        std::scoped_lock lock(m_mutex);

        if (m_messages.empty())
            return 0;

        if (m_selectedMessage >= m_messages.size())
            return 0;

        return m_messages[m_selectedMessage].data.size();
    }

    void UDPProvider::readRaw(u64 offset, void* buffer, size_t size) {
        std::scoped_lock lock(m_mutex);

        if (m_selectedMessage >= m_messages.size())
            return;

        const auto &message = m_messages[m_selectedMessage];

        if (offset >= message.data.size())
            return;

        if (offset + size > message.data.size()) {
            if (offset >= message.data.size())
                size = offset - message.data.size();
            else
                return;
        }

        std::memcpy(buffer, &message.data[offset], size);
    }

    void UDPProvider::writeRaw(u64, const void*, size_t) {
        /* Not supported */
    }

    void UDPProvider::drawSidebarInterface() {
        std::scoped_lock lock(m_mutex);

        if (ImGui::BeginTable("##Messages", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImGui::GetContentRegionAvail())) {
            ImGui::TableSetupColumn("hex.builtin.provider.udp.timestamp"_lang, ImGuiTableColumnFlags_WidthFixed, 32 * ImGui::CalcTextSize(" ").x);
            ImGui::TableSetupColumn("hex.ui.common.size"_lang, ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            ImGuiListClipper clipper;
            clipper.Begin(m_messages.size());
            while (clipper.Step())
                for (u64 i = clipper.DisplayStart; i != u64(clipper.DisplayEnd); i += 1) {
                    ImGui::PushID(i + 1);
                    const auto &message = m_messages[i];

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{}", message.timestamp);
                    ImGui::SameLine();
                    if (ImGui::Selectable("##selectable", i == m_selectedMessage, ImGuiSelectableFlags_SpanAllColumns))
                        m_selectedMessage = i;

                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{}", hex::toByteString(message.data.size()));

                    ImGui::PopID();
                }

            ImGui::EndTable();
        }
    }

    bool UDPProvider::drawLoadInterface() {
        ImGui::InputInt("hex.builtin.provider.udp.port"_lang, &m_port, 0, 0);

        if (m_port < 0)
            m_port = 0;
        else if (m_port > 0xFFFF)
            m_port = 0xFFFF;

        return m_port != 0;
    }


    void UDPProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        m_port      = settings.at("port").get<int>();
    }

    nlohmann::json UDPProvider::storeSettings(nlohmann::json settings) const {
        settings["port"] = m_port;

        return Provider::storeSettings(settings);
    }

}
