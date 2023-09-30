#include <content/providers/process_memory_provider.hpp>

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#  include <shellapi.h>
#elif __linux__
#  include <filesystem>
#  include <fstream>
#  include <iostream>
#  include <cerrno>
#  include <sys/uio.h>
#endif

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/ui/view.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    bool ProcessMemoryProvider::open() {
#ifdef _WIN32
        this->m_processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, this->m_selectedProcess->id);
        if (this->m_processHandle == nullptr)
            return false;
#elif __linux__
        this->m_processId = this->m_selectedProcess->id;
#endif

        this->reloadProcessModules();

        return true;
    }

    void ProcessMemoryProvider::close() {
#ifdef _WIN32
        CloseHandle(this->m_processHandle);
        this->m_processHandle = nullptr;
#elif __linux__
        this->m_processId = -1;
#endif
    }

    void ProcessMemoryProvider::readRaw(u64 address, void *buffer, size_t size) {
#ifdef _WIN32
        ReadProcessMemory(this->m_processHandle, (LPCVOID)address, buffer, size, nullptr);
#elif __linux__
        const iovec local {
            .iov_base = buffer,
            .iov_len = size,
        };
        const iovec remote = {
            .iov_base = (void*) address,
            .iov_len = size,
        };

        auto read = process_vm_readv(this->m_processId, &local, 1, &remote, 1, 0);

        if (read == -1) {
            // TODO error handling strerror(errno)
        }
#endif
    }
    void ProcessMemoryProvider::writeRaw(u64 address, const void *buffer, size_t size) {
#ifdef _WIN32
        WriteProcessMemory(this->m_processHandle, (LPVOID)address, buffer, size, nullptr);
#elif __linux__
        const iovec local {
            .iov_base = (void*) buffer,
            .iov_len = size,
        };
        const iovec remote = {
            .iov_base = (void*) address,
            .iov_len = size,
        };

        auto read = process_vm_writev(this->m_processId, &local, 1, &remote, 1, 0);

        if (read == -1) {
            // TODO error handling strerror(errno)
        }
#endif
    }

    std::pair<Region, bool> ProcessMemoryProvider::getRegionValidity(u64 address) const {
        for (const auto &memoryRegion : this->m_memoryRegions) {
            if (memoryRegion.region.overlaps({ address, 1 }))
                return { memoryRegion.region, true };
        }

        Region lastRegion = Region::Invalid();
        for (const auto &memoryRegion : this->m_memoryRegions) {

            if (address < memoryRegion.region.getStartAddress())
                return { Region { lastRegion.getEndAddress() + 1, memoryRegion.region.getStartAddress() - lastRegion.getEndAddress() }, false };

            lastRegion = memoryRegion.region;
        }

        return { Region::Invalid(), false };
    }

    bool ProcessMemoryProvider::drawLoadInterface() {
        if (this->m_processes.empty() && !this->m_enumerationFailed) {
#ifdef _WIN32
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

            auto dc = GetDC(nullptr);
            ON_SCOPE_EXIT { ReleaseDC(nullptr, dc); };
            for (auto processId : processIds) {
                HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
                if (processHandle == nullptr)
                    continue;

                ON_SCOPE_EXIT { CloseHandle(processHandle); };

                char processName[MAX_PATH];
                if (GetModuleBaseNameA(processHandle, nullptr, processName, MAX_PATH) == 0)
                    continue;

                ImGui::Texture texture;
                {
                    HMODULE moduleHandle = nullptr;
                    DWORD numModules = 0;
                    if (EnumProcessModules(processHandle, &moduleHandle, sizeof(HMODULE), &numModules) != FALSE) {
                        char modulePath[MAX_PATH];
                        if (GetModuleFileNameExA(processHandle, moduleHandle, modulePath, MAX_PATH) != FALSE) {
                            SHFILEINFOA fileInfo;
                            if (SHGetFileInfoA(modulePath, 0, &fileInfo, sizeof(SHFILEINFOA), SHGFI_ICON | SHGFI_SMALLICON) > 0) {
                                ON_SCOPE_EXIT { DestroyIcon(fileInfo.hIcon); };

                                ICONINFO iconInfo;
                                if (GetIconInfo(fileInfo.hIcon, &iconInfo) != FALSE) {
                                    ON_SCOPE_EXIT { DeleteObject(iconInfo.hbmColor); DeleteObject(iconInfo.hbmMask); };

                                    BITMAP bitmap;
                                    if (GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bitmap) > 0) {
                                        BITMAPINFO bitmapInfo = { };
                                        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                                        bitmapInfo.bmiHeader.biWidth = bitmap.bmWidth;
                                        bitmapInfo.bmiHeader.biHeight = -bitmap.bmHeight;
                                        bitmapInfo.bmiHeader.biPlanes = 1;
                                        bitmapInfo.bmiHeader.biBitCount = 32;
                                        bitmapInfo.bmiHeader.biCompression = BI_RGB;

                                        std::vector<u32> pixels(bitmap.bmWidth * bitmap.bmHeight * 4);
                                        if (GetDIBits(dc, iconInfo.hbmColor, 0, bitmap.bmHeight, pixels.data(), &bitmapInfo, DIB_RGB_COLORS) > 0) {
                                            for (auto &pixel : pixels)
                                                pixel = (pixel & 0xFF00FF00) | ((pixel & 0xFF) << 16) | ((pixel & 0xFF0000) >> 16);

                                            texture = ImGui::Texture((u8*)pixels.data(), pixels.size(), bitmap.bmWidth, bitmap.bmHeight);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                this->m_processes.push_back({ processId, processName, std::move(texture) });
            }
#elif __linux__
            for (const auto& entry : std::fs::directory_iterator("/proc")) {
                if (!std::fs::is_directory(entry)) continue;

                auto path = entry.path();
                u32 processId;
                try {
                    processId = std::stoi(path.filename());
                } catch (...) {
                    continue; // not a PID
                }

                std::ifstream file(path / "cmdline");
                if (!file) continue;
                std::stringstream buffer;
                buffer << file.rdbuf();
                file.close();
                std::string processName = buffer.str();

                ImGui::Texture texture;

                this->m_processes.push_back({ processId, processName, std::move(texture) });
            }
#endif
        }

        if (this->m_enumerationFailed) {
            ImGui::TextUnformatted("hex.builtin.provider.process_memory.enumeration_failed"_lang);
        } else {
            ImGui::PushItemWidth(500_scaled);
            const auto &filtered = this->m_processSearchWidget.draw(this->m_processes);
            ImGui::PopItemWidth();
            if (ImGui::BeginTable("##process_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY, ImVec2(500_scaled, 500_scaled))) {
                ImGui::TableSetupColumn("##icon");
                ImGui::TableSetupColumn("hex.builtin.provider.process_memory.process_id"_lang);
                ImGui::TableSetupColumn("hex.builtin.provider.process_memory.process_name"_lang);
                ImGui::TableSetupScrollFreeze(0, 1);

                ImGui::TableHeadersRow();

                for (auto &process : filtered) {
                    ImGui::PushID(process);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Image(process->icon, process->icon.getSize());

                    ImGui::TableNextColumn();
                    ImGui::Text("%d", process->id);

                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(process->name.c_str(), this->m_selectedProcess != nullptr && process->id == this->m_selectedProcess->id, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, process->icon.getSize().y)))
                        this->m_selectedProcess = process;

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

        }

        return this->m_selectedProcess != nullptr;
    }

    void ProcessMemoryProvider::drawInterface() {
        ImGui::Header("hex.builtin.provider.process_memory.memory_regions"_lang, true);

        auto availableX = ImGui::GetContentRegionAvail().x;
        ImGui::PushItemWidth(availableX);
        const auto &filtered = this->m_regionSearchWidget.draw(this->m_memoryRegions);
        ImGui::PopItemWidth();
#ifdef _WIN32
        auto availableY = 400_scaled;
#else
        // take up full height on Linux since there are no DLL injection controls
        auto availableY = ImGui::GetContentRegionAvail().y;
#endif
        if (ImGui::BeginTable("##module_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY, ImVec2(availableX, availableY))) {
            ImGui::TableSetupColumn("hex.builtin.common.region"_lang);
            ImGui::TableSetupColumn("hex.builtin.common.size"_lang);
            ImGui::TableSetupColumn("hex.builtin.common.name"_lang);
            ImGui::TableSetupScrollFreeze(0, 1);

            ImGui::TableHeadersRow();

            for (auto &memoryRegion : filtered) {
                ImGui::PushID(memoryRegion->region.getStartAddress());

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("0x%016lX - 0x%016lX", memoryRegion->region.getStartAddress(), memoryRegion->region.getEndAddress());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(hex::toByteString(memoryRegion->region.getSize()).c_str());


                ImGui::TableNextColumn();
                if (ImGui::Selectable(memoryRegion->name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                    ImHexApi::HexEditor::setSelection(memoryRegion->region);

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

#ifdef _WIN32
        ImGui::Header("hex.builtin.provider.process_memory.utils"_lang);

        if (ImGui::Button("hex.builtin.provider.process_memory.utils.inject_dll"_lang)) {
            hex::fs::openFileBrowser(fs::DialogMode::Open, { { "DLL File", "dll" } }, [this](const std::fs::path &path) {
                const auto &dllPath = path.native();
                const auto dllPathLength = (dllPath.length() + 1) * sizeof(std::fs::path::value_type);

                if (auto pathAddress = VirtualAllocEx(this->m_processHandle, nullptr, dllPathLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); pathAddress != nullptr) {
                    if (WriteProcessMemory(this->m_processHandle, pathAddress, dllPath.c_str(), dllPathLength, nullptr) != FALSE) {
                        auto loadLibraryW = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW")));
                        if (loadLibraryW != nullptr) {
                            if (auto threadHandle = CreateRemoteThread(this->m_processHandle, nullptr, 0, loadLibraryW, pathAddress, 0, nullptr); threadHandle != nullptr) {
                                WaitForSingleObject(threadHandle, INFINITE);
                                EventManager::post<RequestOpenErrorPopup>(hex::format("hex.builtin.provider.process_memory.utils.inject_dll.success"_lang, path.filename().string()));
                                this->reloadProcessModules();
                                CloseHandle(threadHandle);
                                return;
                            }
                        }
                    }
                }

                EventManager::post<RequestOpenErrorPopup>(hex::format("hex.builtin.provider.process_memory.utils.inject_dll.failure"_lang, path.filename().string()));
            });
        }
#endif
    }

    void ProcessMemoryProvider::reloadProcessModules() {
        this->m_memoryRegions.clear();

#ifdef _WIN32
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

            this->m_memoryRegions.insert({ { u64(moduleInfo.lpBaseOfDll), size_t(moduleInfo.SizeOfImage) }, std::fs::path(moduleName).filename().string() });
        }

        MEMORY_BASIC_INFORMATION memoryInfo;
        for (u64 address = 0; address < this->getActualSize(); address += memoryInfo.RegionSize) {
            if (VirtualQueryEx(this->m_processHandle, (LPCVOID)address, &memoryInfo, sizeof(MEMORY_BASIC_INFORMATION)) == 0)
                break;

            std::string name;
            if (memoryInfo.State & MEM_IMAGE)   continue;
            if (memoryInfo.State & MEM_FREE)    continue;
            if (memoryInfo.State & MEM_COMMIT)  name += hex::format("{} ", "hex.builtin.provider.process_memory.region.commit"_lang);
            if (memoryInfo.State & MEM_RESERVE) name += hex::format("{} ", "hex.builtin.provider.process_memory.region.reserve"_lang);
            if (memoryInfo.State & MEM_PRIVATE) name += hex::format("{} ", "hex.builtin.provider.process_memory.region.private"_lang);
            if (memoryInfo.State & MEM_MAPPED)  name += hex::format("{} ", "hex.builtin.provider.process_memory.region.mapped"_lang);

            this->m_memoryRegions.insert({ { (u64)memoryInfo.BaseAddress, (u64)memoryInfo.BaseAddress + memoryInfo.RegionSize }, name });
        }

#elif __linux__

        std::ifstream file(std::fs::path("/proc") / std::to_string(this->m_processId) / "maps");

        if (!file) return;

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);

            std::vector<std::string> tokens;

            std::string token;
            while (tokens.size() < 5 && iss >> token)
                tokens.push_back(token);

            if (tokens.size() < 5)
                continue;

            std::string name;
            if (!std::getline(iss, name))
                continue;
            name = wolv::util::trim(name);

            std::istringstream rangeStream(tokens[0]);
            std::string startStr, endStr;

            std::getline(std::getline(rangeStream, startStr, '-'), endStr);

            u64 start = std::stoull(startStr, nullptr, 16);
            u64 end = std::stoull(endStr, nullptr, 16);

            this->m_memoryRegions.insert({ { start, end - start }, name });
        }

        file.close();
#endif
    }


    std::variant<std::string, i128> ProcessMemoryProvider::queryInformation(const std::string &category, const std::string &argument) {
        auto findRegionByName = [this](const std::string &name) {
            return std::find_if(this->m_memoryRegions.begin(), this->m_memoryRegions.end(),
                [name](const auto &region) {
                    return region.name == name;
                });
        };

        if (category == "region_address") {
            if (auto iter = findRegionByName(argument); iter != this->m_memoryRegions.end())
                return iter->region.getStartAddress();
            else
                return 0;
        } else if (category == "region_size") {
            if (auto iter = findRegionByName(argument); iter != this->m_memoryRegions.end())
                return iter->region.getSize();
            else
                return 0;
        } else if (category == "process_id") {
            return this->m_selectedProcess->id;
        } else if (category == "process_name") {
            return this->m_selectedProcess->name;
        } else
            return Provider::queryInformation(category, argument);
    }

}
