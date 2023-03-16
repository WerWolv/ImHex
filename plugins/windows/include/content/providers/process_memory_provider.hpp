#pragma once

#include <hex/providers/provider.hpp>
#include <hex/api/localization.hpp>

#include <windows.h>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/ui/widgets.hpp>
#include <hex/helpers/utils.hpp>

#include <array>
#include <mutex>
#include <string_view>
#include <thread>

namespace hex::plugin::windows {

    class ProcessMemoryProvider : public hex::prv::Provider {
    public:
        ProcessMemoryProvider() = default;
        ~ProcessMemoryProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return this->m_processHandle != nullptr; }
        [[nodiscard]] bool isReadable() const override { return true; }
        [[nodiscard]] bool isWritable() const override { return true; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return false; }

        void read(u64 address, void *buffer, size_t size, bool) override { this->readRaw(address, buffer, size); }
        void write(u64 address, const void *buffer, size_t size) override { this->writeRaw(address, buffer, size); }

        void readRaw(u64 address, void *buffer, size_t size) override;
        void writeRaw(u64 address, const void *buffer, size_t size) override;
        [[nodiscard]] size_t getActualSize() const override { return 0xFFFF'FFFF'FFFF;  }

        void save() override {}
        void saveAs(const std::fs::path &) override {}

        [[nodiscard]] std::string getName() const override { return hex::format("hex.windows.provider.process_memory.name"_lang, this->m_selectedProcess != nullptr ? this->m_selectedProcess->name : ""); }
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataDescription() const override {
            return {
                    { "hex.windows.provider.process_memory.process_name"_lang, this->m_selectedProcess->name },
                    { "hex.windows.provider.process_memory.process_id"_lang, std::to_string(this->m_selectedProcess->id) }
            };
        }

        [[nodiscard]] bool open() override;
        void close() override;

        [[nodiscard]] bool hasLoadInterface() const override { return true; }
        [[nodiscard]] bool hasInterface() const override { return true; }
        bool drawLoadInterface() override;
        void drawInterface() override;

        void loadSettings(const nlohmann::json &) override {}
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json) const override { return { }; }

        [[nodiscard]] std::string getTypeName() const override {
            return "hex.windows.provider.process_memory";
        }

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64) const override;
        std::variant<std::string, i128> queryInformation(const std::string &category, const std::string &argument) override;

    private:
        void reloadProcessModules();

    private:
        struct Process {
            u32 id;
            std::string name;
            ImGui::Texture icon;
        };

        struct MemoryRegion {
            Region region;
            std::string name;

            constexpr bool operator<(const MemoryRegion &other) const {
                return this->region.getStartAddress() < other.region.getStartAddress();
            }
        };

        std::vector<Process> m_processes;
        const Process *m_selectedProcess = nullptr;

        std::set<MemoryRegion> m_memoryRegions;
        ui::SearchableWidget<Process> m_processSearchWidget = ui::SearchableWidget<Process>([](const std::string &search, const Process &process) {
            return hex::containsIgnoreCase(process.name, search);
        });
        ui::SearchableWidget<MemoryRegion> m_regionSearchWidget = ui::SearchableWidget<MemoryRegion>([](const std::string &search, const MemoryRegion &memoryRegion) {
            return hex::containsIgnoreCase(memoryRegion.name, search);
        });

        HANDLE m_processHandle = nullptr;

        bool m_enumerationFailed = false;
    };

}