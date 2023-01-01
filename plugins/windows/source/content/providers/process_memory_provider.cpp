#include <content/providers/process_memory_provider.hpp>

#include <windows.h>
#include <psapi.h>

#include <imgui.h>
#include <hex/helpers/utils.hpp>

namespace hex::plugin::windows {

    bool ProcessMemoryProvider::open() {
        this->m_processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, this->m_selectedProcess->id);
        if (this->m_processHandle == nullptr)
            return false;

        DWORD numModules = 0;
        std::vector<HMODULE> modules;

        do {
            modules.resize(modules.size() + 1024);
            if (EnumProcessModules(this->m_processHandle, modules.data(), modules.size() * sizeof(HMODULE), &numModules) == FALSE) {
                modules.clear();
                break;
            }
        } while (numModules == modules.size() * sizeof(HMODULE));

        modules.resize(numModules / sizeof(HMODULE));

        for (auto &module : modules) {
            MODULEINFO moduleInfo;
            if (GetModuleInformation(this->m_processHandle, module, &moduleInfo, sizeof(MODULEINFO)) == FALSE)
                continue;

            char moduleName[MAX_PATH];
            if (GetModuleFileNameExA(this->m_processHandle, module, moduleName, MAX_PATH) == FALSE)
                continue;

            this->m_modules.push_back({ { (u64)moduleInfo.lpBaseOfDll, (u64)moduleInfo.lpBaseOfDll + moduleInfo.SizeOfImage }, std::fs::path(moduleName).filename().string() });
        }

        return true;
    }

    void ProcessMemoryProvider::close() {
        CloseHandle(this->m_processHandle);
        this->m_processHandle = nullptr;
    }

    void ProcessMemoryProvider::readRaw(u64 address, void *buffer, size_t size) {
        ReadProcessMemory(this->m_processHandle, (LPCVOID)address, buffer, size, nullptr);
    }
    void ProcessMemoryProvider::writeRaw(u64 address, const void *buffer, size_t size) {
        WriteProcessMemory(this->m_processHandle, (LPVOID)address, buffer, size, nullptr);
    }

    std::pair<Region, bool> ProcessMemoryProvider::getRegionValidity(u64 address) const {
        for (const auto &module : this->m_modules) {
            if (module.region.overlaps({ address, 1 }))
                return { module.region, true };
        }

        return { Region::Invalid(), false };
    }

    void ProcessMemoryProvider::drawLoadInterface() {
        if (this->m_processes.empty() && !this->m_enumerationFailed) {
            DWORD numProcesses = 0;
            std::vector<DWORD> processIds;

            do {
                processIds.resize(processIds.size() + 1024);
                if (EnumProcesses(processIds.data(), processIds.size() * sizeof(DWORD), &numProcesses) == FALSE) {
                    processIds.clear();
                    this->m_enumerationFailed = true;
                    break;
                }
            } while (numProcesses == processIds.size() * sizeof(DWORD));

            processIds.resize(numProcesses / sizeof(DWORD));

            for (auto processId : processIds) {
                HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
                if (processHandle == nullptr)
                    continue;

                ON_SCOPE_EXIT { CloseHandle(processHandle); };

                char processName[MAX_PATH];
                if (GetModuleBaseNameA(processHandle, nullptr, processName, MAX_PATH) == 0)
                    continue;

                this->m_processes.push_back({ processId, processName });
            }
        }

        if (this->m_enumerationFailed) {
            ImGui::TextUnformatted("hex.windows.provider.process_memory.enumeration_failed"_lang);
        } else {
            if (ImGui::BeginTable("##process_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY, ImVec2(0, 500))) {
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("Name");
                ImGui::TableHeadersRow();

                for (auto &process : this->m_processes) {
                    ImGui::PushID(process.id);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", process.id);

                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(process.name.c_str(), this->m_selectedProcess.has_value() && process.id == this->m_selectedProcess->id, ImGuiSelectableFlags_SpanAllColumns))
                        this->m_selectedProcess = process;

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

        }
    }

    void ProcessMemoryProvider::drawInterface() {
        if (ImGui::BeginTable("##module_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Base");
            ImGui::TableSetupColumn("Name");
            ImGui::TableHeadersRow();

            for (auto &module : this->m_modules) {
                ImGui::PushID(module.region.getStartAddress());

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("0x%016llX - 0x%016llX", module.region.getStartAddress(), module.region.getEndAddress());

                ImGui::TableNextColumn();
                if (ImGui::Selectable(module.name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                    ImHexApi::HexEditor::setSelection(module.region);

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

}