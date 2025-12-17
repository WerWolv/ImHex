#include <content/providers/ssh_provider.hpp>

#include <imgui.h>
#include <fonts/vscode_icons.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/helpers/utils.hpp>

#include <nlohmann/json.hpp>
#include <toasts/toast_notification.hpp>

namespace hex::plugin::remote {

    prv::Provider::OpenResult SSHProvider::open() {
        if (!m_sftpClient.isConnected()) {
            try {
                if (m_authMethod == AuthMethod::Password) {
                    SSHClient client(m_host, m_port, m_username, m_password);
                    m_sftpClient = std::move(client);
                } else if (m_authMethod == AuthMethod::KeyFile) {
                    SSHClient client(m_host, m_port, m_username, m_privateKeyPath, m_keyPassphrase);
                    m_sftpClient = std::move(client);
                }
            } catch (const std::exception& e) {
                return OpenResult::failure(e.what());
            }
        }

        try {
            if (m_accessFileOverSSH)
                m_remoteFile = m_sftpClient.openFileSSH(m_remoteFilePath, SSHClient::OpenMode::ReadWrite);
            else
                m_remoteFile = m_sftpClient.openFileSFTP(m_remoteFilePath, SSHClient::OpenMode::ReadWrite);
        } catch (const std::exception& e) {
            return OpenResult::failure(e.what());
        }

        if (!m_remoteFile->isOpen()) {
            return OpenResult::failure("hex.plugin.remote.ssh_provider.error.open_failed"_lang);
        }

        return {};
    }

    void SSHProvider::close() {
        if (m_remoteFile != nullptr)
            m_remoteFile->close();

        m_sftpClient.disconnect();
        m_remoteFilePath.clear();
    }

    void SSHProvider::save() {
        if (m_sftpClient.isConnected() && m_remoteFile->isOpen()) {
            m_remoteFile->flush();
        }
    }

    void SSHProvider::readFromSource(u64 offset, void* buffer, size_t size) {
        m_remoteFile->seek(offset);
        std::ignore = m_remoteFile->read({ static_cast<u8*>(buffer), size });
    }

    void SSHProvider::writeToSource(u64 offset, const void* buffer, size_t size) {
        m_remoteFile->seek(offset);
        std::ignore = m_remoteFile->write({ static_cast<const u8*>(buffer), size });
    }

    u64 SSHProvider::getSourceSize() const {
        auto size = m_remoteFile->size();
        if (size == 0)
            return std::numeric_limits<u32>::max();
        else
            return size;
    }

    std::string SSHProvider::getName() const {
        return fmt::format("{} [{}@{}:{}]", m_remoteFilePath.filename().string(), m_username, m_host, m_port);
    }


    bool SSHProvider::drawLoadInterface() {
        if (!m_sftpClient.isConnected()) {
            ImGui::InputText("hex.plugin.remote.ssh_provider.host"_lang, m_host);
            ImGui::InputInt("hex.plugin.remote.ssh_provider.port"_lang, &m_port, 0, 0);
            ImGui::InputText("hex.plugin.remote.ssh_provider.username"_lang, m_username);

            ImGui::NewLine();

            if (ImGui::BeginTabBar("##SSHProviderLoadInterface")) {
                if (ImGui::BeginTabItem("hex.plugin.remote.ssh_provider.password"_lang)) {
                    m_authMethod = AuthMethod::Password;
                    ImGui::InputText("hex.plugin.remote.ssh_provider.password"_lang, m_password, ImGuiInputTextFlags_Password);
                    ImGui::NewLine();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.plugin.remote.ssh_provider.key_file"_lang)) {
                    m_authMethod = AuthMethod::KeyFile;
                    ImGuiExt::InputFilePicker("hex.plugin.remote.ssh_provider.key_file"_lang, m_privateKeyPath, {});
                    ImGui::InputText("hex.plugin.remote.ssh_provider.passphrase"_lang, m_keyPassphrase, ImGuiInputTextFlags_Password);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }

            ImGui::NewLine();

            if (ImGui::Button("hex.plugin.remote.ssh_provider.connect"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                try {
                    if (m_authMethod == AuthMethod::Password) {
                        SSHClient client(m_host, m_port, m_username, m_password);
                        m_sftpClient = std::move(client);
                    } else if (m_authMethod == AuthMethod::KeyFile) {
                        SSHClient client(m_host, m_port, m_username, m_privateKeyPath, m_keyPassphrase);
                        m_sftpClient = std::move(client);
                    }
                } catch (const std::exception& e) {
                    ui::ToastError::open(fmt::format("Failed to connect to SSH server: {}", e.what()));
                    return false;
                }
            }
        } else {
            std::string pathString = wolv::util::toUTF8String(m_remoteFilePath);
            if (ImGui::InputText("##RemoteFilePath", pathString)) {
                m_remoteFilePath = pathString;
            }

            if (ImGui::BeginTable("##RemoteFileList", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 200_scaled))) {
                ImGui::TableSetupColumn("##Icon", ImGuiTableColumnFlags_WidthFixed, 20_scaled);
                ImGui::TableSetupColumn("##Name", ImGuiTableColumnFlags_WidthStretch);

                if (m_remoteFilePath.has_parent_path()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(ICON_VS_FOLDER);

                    ImGui::TableNextColumn();
                    ImGui::Selectable("..", false, ImGuiSelectableFlags_NoAutoClosePopups);
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        m_remoteFilePath = m_remoteFilePath.parent_path();
                    }
                }

                for (const auto &entry : m_sftpClient.listDirectory(m_selectedFile ? m_remoteFilePath.parent_path() : m_remoteFilePath)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.isDirectory() ? ICON_VS_FOLDER : ICON_VS_FILE);

                    ImGui::TableNextColumn();
                    ImGui::Selectable(entry.name.c_str(), m_remoteFilePath.filename() == entry.name, ImGuiSelectableFlags_NoAutoClosePopups);
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        m_selectedFile = !entry.isDirectory();
                        m_remoteFilePath /= entry.name;
                    }
                }
                ImGui::EndTable();
            }

            ImGui::NewLine();
            ImGui::Checkbox("hex.plugin.remote.ssh_provider.ssh_access"_lang, &m_accessFileOverSSH);
        }

        return m_selectedFile;
    }

    nlohmann::json SSHProvider::storeSettings(nlohmann::json settings) const {
        settings["host"] = m_host;
        settings["port"] = m_port;
        settings["username"] = m_username;
        settings["authMethod"] = m_authMethod == AuthMethod::Password ? "password" : "key_file";
        if (m_authMethod == AuthMethod::Password) {
            settings["password"] = m_password;
        } else {
            settings["privateKeyPath"] = m_privateKeyPath;
            settings["keyPassphrase"] = m_keyPassphrase;
        }

        settings["remoteFilePath"] = wolv::util::toUTF8String(m_remoteFilePath);
        settings["accessFileOverSSH"] = m_accessFileOverSSH;

        return Provider::storeSettings(settings);
    }

    void SSHProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        m_host = settings.value("host", "");
        m_port = settings.value("port", 22);
        m_username = settings.value("username", "");
        m_authMethod = settings.value("authMethod", "password") == "password" ? AuthMethod::Password : AuthMethod::KeyFile;
        if (m_authMethod == AuthMethod::Password) {
            m_password = settings.value("password", "");
        } else {
            m_privateKeyPath = settings.value("privateKeyPath", "");
            m_keyPassphrase = settings.value("keyPassphrase", "");
        }

        m_remoteFilePath = settings.value("remoteFilePath", "");
        m_accessFileOverSSH = settings.value("accessFileOverSSH", false);
    }

}
