#pragma once
#if !defined(OS_WEB)

#include <hex/providers/provider.hpp>

#include <set>
#include <string>
#include <vector>
#include <fonts/vscode_icons.hpp>
#include <wolv/io/handle.hpp>

namespace hex::plugin::builtin {

    class DiskProvider : public prv::Provider,
                         public prv::IProviderDataDescription,
                         public prv::IProviderLoadInterface {
    public:
        DiskProvider() = default;
        ~DiskProvider() override = default;

        [[nodiscard]] bool isAvailable() const override;
        [[nodiscard]] bool isReadable() const override;
        [[nodiscard]] bool isWritable() const override;
        [[nodiscard]] bool isResizable() const override;
        [[nodiscard]] bool isSavable() const override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] u64 getActualSize() const override;

        void setPath(const std::fs::path &path);

        [[nodiscard]] OpenResult open() override;
        void close() override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<Description> getDataDescription() const override;

        bool drawLoadInterface() override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.disk";
        }

        [[nodiscard]] const char* getIcon() const override {
            return ICON_VS_SAVE;
        }

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64 address) const override;
        std::variant<std::string, i128> queryInformation(const std::string &category, const std::string &argument) override;

    protected:
        void reloadDrives();

        struct DriveInfo {
            std::string path;
            std::string friendlyName;

            auto operator<=>(const DriveInfo &other) const {
                return this->path <=> other.path;
            }
        };

        std::set<DriveInfo> m_availableDrives;
        std::fs::path m_path;
        std::string m_friendlyName;
        bool m_elevated = false;

        wolv::io::NativeHandle m_diskHandle;
        [[maybe_unused]] std::string m_pathBuffer;


        size_t m_diskSize   = 0;
        size_t m_sectorSize = 0;

        bool m_readable = false;
        bool m_writable = false;
    };

}
#endif
