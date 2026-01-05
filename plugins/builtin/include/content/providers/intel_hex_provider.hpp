#pragma once

#include <fonts/vscode_icons.hpp>
#include <hex/providers/provider.hpp>
#include <hex/ui/widgets.hpp>

#include <wolv/container/interval_tree.hpp>
#include <wolv/utils/expected.hpp>

namespace hex::plugin::builtin {
    struct MemoryRegion {
        Region region;
        std::string name;

        constexpr bool operator<(const MemoryRegion &other) const {
            return this->region.getStartAddress() < other.region.getStartAddress();
        }
    };

    class IntelHexProvider : public hex::prv::Provider,
                             public hex::prv::IProviderDataDescription,
                             public hex::prv::IProviderFilePicker,
                             public hex::prv::IProviderSidebarInterface {
    public:
        IntelHexProvider() = default;
        ~IntelHexProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return m_dataValid; }
        [[nodiscard]] bool isReadable() const override { return true; }
        [[nodiscard]] bool isWritable() const override { return false; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return false; }

        void setBaseAddress(u64 address) override;
        void drawSidebarInterface() override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] u64 getActualSize() const override;
        void processMemoryRegions(wolv::util::Expected<std::map<u64, std::vector<u8>>, std::string> data);
        static bool memoryRegionFilter(const std::string &search, const MemoryRegion &memoryRegion);
        OpenResult open() override;
        void close() override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<Description> getDataDescription() const override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.intel_hex";
        }

        [[nodiscard]] const char* getIcon() const override {
            return ICON_VS_TABLE;
        }

        [[nodiscard]] bool handleFilePicker() override;

        std::pair<Region, bool> getRegionValidity(u64 address) const override;

    protected:
        bool m_dataValid = false;
        size_t m_dataSize = 0x00;
        wolv::container::IntervalTree<std::vector<u8>> m_data;

        ui::SearchableWidget<MemoryRegion> m_regionSearchWidget = ui::SearchableWidget<MemoryRegion>(memoryRegionFilter);
        std::vector<MemoryRegion> m_memoryRegions;
        std::fs::path m_sourceFilePath;
    };
}
