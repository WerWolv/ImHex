#pragma once

#if defined(OS_WINDOWS) || defined(OS_MACOS) || (defined(OS_LINUX) && !defined(OS_FREEBSD))

#include <hex/providers/provider.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/ui/widgets.hpp>
#include <hex/helpers/utils.hpp>

#include <set>
#include <thread>
#include <fonts/vscode_icons.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <nlohmann/json.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
#elif defined(OS_LINUX)
    #include <sys/types.h>
#endif

namespace hex::plugin::builtin {

    class ProcessMemoryProvider : public prv::Provider,
                                  public prv::IProviderDataDescription,
                                  public prv::IProviderLoadInterface,
                                  public prv::IProviderSidebarInterface {
    public:
        ProcessMemoryProvider() = default;
        ~ProcessMemoryProvider() override = default;

        [[nodiscard]] bool isAvailable() const override {
            #if defined(OS_WINDOWS)
                return m_processHandle != nullptr;
            #else
                return m_processId != -1;
            #endif
        }
        [[nodiscard]] bool isReadable() const override { return true; }
        [[nodiscard]] bool isWritable() const override { return true; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return false; }
        [[nodiscard]] bool isDumpable() const override { return false; }

        void readRaw(u64 address, void *buffer, size_t size) override;
        void writeRaw(u64 address, const void *buffer, size_t size) override;
        [[nodiscard]] u64 getActualSize() const override { return 0xFFFF'FFFF'FFFF;  }

        void save() override {}

        [[nodiscard]] std::string getName() const override { return fmt::format("hex.builtin.provider.process_memory.name"_lang, m_selectedProcess != nullptr ? m_selectedProcess->name : ""); }
        [[nodiscard]] std::vector<Description> getDataDescription() const override {
            return {
                    { "hex.builtin.provider.process_memory.process_name"_lang, m_selectedProcess->name },
                    { "hex.builtin.provider.process_memory.process_id"_lang, std::to_string(m_selectedProcess->id) }
            };
        }

        [[nodiscard]] OpenResult open() override;
        void close() override;

        bool drawLoadInterface() override;
        void drawSidebarInterface() override;

        void loadSettings(const nlohmann::json &) override {}
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json) const override { return { }; }

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.process_memory";
        }

        [[nodiscard]] const char* getIcon() const override {
            return ICON_VS_SERVER_PROCESS;
        }

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64) const override;
        std::variant<std::string, i128> queryInformation(const std::string &category, const std::string &argument) override;

    private:
        void reloadProcessModules();

    private:
        struct Process {
            u32 id;
            std::string name;
            std::string commandLine;
            ImGuiExt::Texture icon;
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

#if defined(OS_WINDOWS)
        HANDLE m_processHandle = nullptr;
#else
        pid_t m_processId = -1;
#endif

        bool m_enumerationFailed = false;
    };

}

#endif
