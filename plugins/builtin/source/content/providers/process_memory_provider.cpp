#if defined(OS_WINDOWS) || defined(OS_MACOS) || (defined(OS_LINUX) && !defined(OS_FREEBSD))

#include <algorithm>
#include <content/providers/process_memory_provider.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <winternl.h>
    #include <psapi.h>
    #include <shellapi.h>
#elif defined(OS_MACOS)
    #include <mach/mach_types.h>
    #include <mach/message.h>
    #include <mach/machine/kern_return.h>
    #include <mach/machine/vm_types.h>
    #include <mach/vm_map.h>
    #include <mach-o/dyld_images.h>
    #include <libproc.h>
#elif defined(OS_LINUX)
    #include <sys/uio.h>
#endif

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/ui/view.hpp>

#include <toasts/toast_notification.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/literals.hpp>

namespace hex::plugin::builtin {

    using namespace wolv::literals;

#if defined(OS_WINDOWS)

    using NtQueryInformationProcessFunc = NTSTATUS (NTAPI*)(
        HANDLE ProcessHandle,
        PROCESSINFOCLASS ProcessInformationClass,
        PVOID ProcessInformation,
        ULONG ProcessInformationLength,
        PULONG ReturnLength
    );

    std::string getProcessCommandLine(HANDLE processHandle) {
        // Get NtQueryInformationProcess function
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll) return "";

        auto funcNtQueryInformationProcess = (NtQueryInformationProcessFunc)
            (void*)GetProcAddress(ntdll, "NtQueryInformationProcess");
        if (!funcNtQueryInformationProcess) return "";

        // Query for PROCESS_BASIC_INFORMATION to get PEB address
        PROCESS_BASIC_INFORMATION pbi = {};
        ULONG len = 0;
        NTSTATUS status = funcNtQueryInformationProcess(
            processHandle,
            ProcessBasicInformation,
            &pbi,
            sizeof(pbi),
            &len
        );

        if (status != 0 || !pbi.PebBaseAddress) return "";

        // Read PEB to get ProcessParameters address
        PEB peb = {};
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(processHandle, pbi.PebBaseAddress, &peb, sizeof(peb), &bytesRead))
            return "";

        // Read RTL_USER_PROCESS_PARAMETERS
        RTL_USER_PROCESS_PARAMETERS params = {};
        if (!ReadProcessMemory(processHandle, peb.ProcessParameters, &params, sizeof(params), &bytesRead))
            return "";

        // Read the command line string
        std::vector<wchar_t> cmdLine(params.CommandLine.Length / sizeof(wchar_t) + 1, 0);
        if (!ReadProcessMemory(processHandle, params.CommandLine.Buffer, cmdLine.data(),
                              params.CommandLine.Length, &bytesRead))
            return "";

        // Convert wide string to narrow string
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, cmdLine.data(), -1, nullptr, 0, nullptr, nullptr);
        if (sizeNeeded <= 0) return "";

        std::string result(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, cmdLine.data(), -1, &result[0], sizeNeeded, nullptr, nullptr);

        // Remove trailing null terminator
        if (!result.empty() && result.back() == '\0')
            result.pop_back();

        return result;
    }

#endif

    prv::Provider::OpenResult ProcessMemoryProvider::open() {
        if (m_selectedProcess == nullptr)
            return OpenResult::failure("hex.builtin.provider.process_memory.error.no_process_selected"_lang);

        #if defined(OS_WINDOWS)
            m_processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_selectedProcess->id);
            if (m_processHandle == nullptr)
                return OpenResult::failure("hex.builtin.provider.process_memory.error.open_process"_lang);
        #else
            m_processId = pid_t(m_selectedProcess->id);
        #endif

        this->reloadProcessModules();

        return {};
    }

    void ProcessMemoryProvider::close() {
        #if defined(OS_WINDOWS)
            CloseHandle(m_processHandle);
            m_processHandle = nullptr;
        #else
            m_processId = -1;
        #endif
    }

    void ProcessMemoryProvider::readRaw(u64 address, void *buffer, size_t size) {
        #if defined(OS_WINDOWS)
            ReadProcessMemory(m_processHandle, reinterpret_cast<LPCVOID>(address), buffer, size, nullptr);
        #elif defined(OS_MACOS)
            task_t t;
            task_for_pid(mach_task_self(), m_processId, &t);

            vm_size_t dataSize = 0;
            vm_read_overwrite(t,
                 address,
                 size,
                 reinterpret_cast<vm_address_t>(buffer),
                 &dataSize
            );

        #elif defined(OS_LINUX)
            const iovec local {
                .iov_base = buffer,
                .iov_len = size,
            };
            const iovec remote = {
                .iov_base = reinterpret_cast<void*>(address),
                .iov_len = size,
            };

            auto read = process_vm_readv(m_processId, &local, 1, &remote, 1, 0);
            std::ignore = read;
        #endif
    }
    void ProcessMemoryProvider::writeRaw(u64 address, const void *buffer, size_t size) {
        #if defined(OS_WINDOWS)
            WriteProcessMemory(m_processHandle, reinterpret_cast<LPVOID>(address), buffer, size, nullptr);
        #elif defined(OS_MACOS)
            task_t t;
            task_for_pid(mach_task_self(), m_processId, &t);

            vm_write(t,
                 address,
                 reinterpret_cast<vm_address_t>(buffer),
                 size
            );
        #elif defined(OS_LINUX)
            const iovec local {
                .iov_base = const_cast<void*>(buffer),
                .iov_len = size,
            };
            const iovec remote = {
                .iov_base = reinterpret_cast<void*>(address),
                .iov_len = size,
            };

            auto write = process_vm_writev(m_processId, &local, 1, &remote, 1, 0);
            std::ignore = write;
        #endif
    }

    std::pair<Region, bool> ProcessMemoryProvider::getRegionValidity(u64 address) const {
        for (const auto &memoryRegion : m_memoryRegions) {
            if (memoryRegion.region.overlaps({ .address=address, .size=1LLU }))
                return { memoryRegion.region, true };
        }

        Region lastRegion = Region::Invalid();
        for (const auto &memoryRegion : m_memoryRegions) {

            if (address < memoryRegion.region.getStartAddress())
                return { Region { .address=lastRegion.getEndAddress(), .size=memoryRegion.region.getStartAddress() - lastRegion.getEndAddress() }, false };

            lastRegion = memoryRegion.region;
        }

        return { Region::Invalid(), false };
    }

    bool ProcessMemoryProvider::drawLoadInterface() {
        if (m_processes.empty() && !m_enumerationFailed) {
            #if defined(OS_WINDOWS)
                DWORD numProcesses = 0;
                std::vector<DWORD> processIds;

                do {
                    processIds.resize(processIds.size() + 1024);
                    if (EnumProcesses(processIds.data(), processIds.size() * sizeof(DWORD), &numProcesses) == FALSE) {
                        processIds.clear();
                        m_enumerationFailed = true;
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

                    ImGuiExt::Texture texture;
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

                                                texture = ImGuiExt::Texture::fromBitmap(reinterpret_cast<const u8*>(pixels.data()), pixels.size(), bitmap.bmWidth, bitmap.bmHeight, ImGuiExt::Texture::Filter::Linear);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    m_processes.push_back({ u32(processId), processName, getProcessCommandLine(processHandle), std::move(texture) });
                }
            #elif defined(OS_MACOS)
                std::array<pid_t, 2048> pids;
                const auto bytes = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), sizeof(pids));
                const auto processCount = bytes / sizeof(pid_t);
                for (u32 i = 0; i < processCount; i += 1) {
                    proc_bsdinfo proc;
                    const auto result = proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0,
                                         &proc, PROC_PIDTBSDINFO_SIZE);
                    if (result == PROC_PIDTBSDINFO_SIZE) {
                        m_processes.emplace_back(pids[i], proc.pbi_name, proc.pbi_comm, ImGuiExt::Texture());
                    }
                }
            #elif defined(OS_LINUX)
                for (const auto& entry : std::fs::directory_iterator("/proc")) {
                    if (!std::fs::is_directory(entry)) continue;

                    const auto &path = entry.path();
                    u32 processId = 0;
                    try {
                        processId = std::stoi(path.filename());
                    } catch (...) {
                        continue; // not a PID
                    }

                    // Parse status file
                    wolv::io::File file(path / "status", wolv::io::File::Mode::Read);
                    std::map<std::string, std::string> statusInfo;
                    if (file.isValid()) {
                        const auto statusContent = file.readString(1_MiB);
                        for (const auto &line : wolv::util::splitString(statusContent, "\n")) {
                            const auto delimiterPos = line.find(':');
                            if (delimiterPos != std::string::npos) {
                                const auto key = line.substr(0, delimiterPos);
                                const auto value = line.substr(delimiterPos + 1);
                                statusInfo[key] = wolv::util::trim(value);
                            }
                        }
                    }

                    // Skip kernel threads
                    if (statusInfo.contains("Kthread") && statusInfo["Kthread"] == "1")
                        continue;

                    // Parse process name
                    std::string processName;
                    if (statusInfo.contains("Name")) {
                        processName = statusInfo["Name"];
                    }

                    wolv::io::File cmdlineFile(path / "cmdline", wolv::io::File::Mode::Read);
                    if (!file.isValid())
                        continue;

                    auto commandLine = cmdlineFile.readString(1_MiB);
                    if (processName.empty())
                        processName = commandLine;

                    m_processes.emplace_back(processId, processName, commandLine, ImGuiExt::Texture());
                }
            #endif
        }

        if (m_enumerationFailed) {
            ImGui::TextUnformatted("hex.builtin.provider.process_memory.enumeration_failed"_lang);
        } else {
            #if defined(OS_MACOS)
                ImGuiExt::TextFormattedWrapped("{}", "hex.builtin.provider.process_memory.macos_limitations"_lang);
                ImGui::NewLine();
            #endif

            ImGui::PushItemWidth(500_scaled);
            const auto &filtered = m_processSearchWidget.draw(m_processes);
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

                    auto height = ImGui::GetTextLineHeight();
                    if (process->icon.isValid()) {
                        ImGui::Image(process->icon, { height, height });
                    } else {
                        ImGui::Dummy({ height, height });
                    }

                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{}", process->id);

                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(process->name.c_str(), m_selectedProcess != nullptr && process->id == m_selectedProcess->id, ImGuiSelectableFlags_SpanAllColumns))
                        m_selectedProcess = process;

                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary | ImGuiHoveredFlags_DelayNormal)) {
                        if (ImGui::BeginTooltip()) {
                            ImGui::PushTextWrapPos(200_scaled);
                            ImGui::TextWrapped("%s", process->commandLine.c_str());
                            ImGui::PopTextWrapPos();
                            ImGui::EndTooltip();
                        }
                    }

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

        }

        return m_selectedProcess != nullptr;
    }

    void ProcessMemoryProvider::drawSidebarInterface() {
        ImGuiExt::Header("hex.builtin.provider.process_memory.memory_regions"_lang, true);

        auto availableX = ImGui::GetContentRegionAvail().x;
        ImGui::PushItemWidth(availableX);
        const auto &filtered = m_regionSearchWidget.draw(m_memoryRegions);
        ImGui::PopItemWidth();

        #if defined(OS_WINDOWS)
            auto availableY = 400_scaled;
        #else
            // Take up full height on Linux since there are no DLL injection controls
            auto availableY = ImGui::GetContentRegionAvail().y;
        #endif

        if (ImGui::BeginTable("##module_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY, ImVec2(availableX, availableY))) {
            ImGui::TableSetupColumn("hex.ui.common.region"_lang);
            ImGui::TableSetupColumn("hex.ui.common.size"_lang);
            ImGui::TableSetupColumn("hex.ui.common.name"_lang);
            ImGui::TableSetupScrollFreeze(0, 1);

            ImGui::TableHeadersRow();

            for (const auto &memoryRegion : filtered) {
                ImGui::PushID(&memoryRegion);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("0x{0:016X} - 0x{1:016X}", memoryRegion->region.getStartAddress(), memoryRegion->region.getEndAddress());

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(hex::toByteString(memoryRegion->region.getSize()).c_str());


                ImGui::TableNextColumn();
                if (ImGui::Selectable(memoryRegion->name.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                    ImHexApi::HexEditor::setSelection(memoryRegion->region);

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        #if defined(OS_WINDOWS)
            ImGuiExt::Header("hex.builtin.provider.process_memory.utils"_lang);

            if (ImGui::Button("hex.builtin.provider.process_memory.utils.inject_dll"_lang)) {
                hex::fs::openFileBrowser(fs::DialogMode::Open, { { "DLL File", "dll" } }, [this](const std::fs::path &path) {
                    const auto &dllPath = path.native();
                    const auto dllPathLength = (dllPath.length() + 1) * sizeof(std::fs::path::value_type);

                    if (auto pathAddress = VirtualAllocEx(m_processHandle, nullptr, dllPathLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); pathAddress != nullptr) {
                        if (WriteProcessMemory(m_processHandle, pathAddress, dllPath.c_str(), dllPathLength, nullptr) != FALSE) {
                            auto loadLibraryW = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW")));
                            if (loadLibraryW != nullptr) {
                                if (auto threadHandle = CreateRemoteThread(m_processHandle, nullptr, 0, loadLibraryW, pathAddress, 0, nullptr); threadHandle != nullptr) {
                                    WaitForSingleObject(threadHandle, INFINITE);
                                    ui::ToastInfo::open(fmt::format("hex.builtin.provider.process_memory.utils.inject_dll.success"_lang, path.filename().string()));
                                    this->reloadProcessModules();
                                    CloseHandle(threadHandle);
                                    return;
                                }
                            }
                        }
                    }

                    ui::ToastError::open(fmt::format("hex.builtin.provider.process_memory.utils.inject_dll.failure"_lang, path.filename().string()));
                });
            }
        #endif
    }

    void ProcessMemoryProvider::reloadProcessModules() {
        m_memoryRegions.clear();

        #if defined(OS_WINDOWS)
            DWORD numModules = 0;
            std::vector<HMODULE> modules;

            do {
                modules.resize(modules.size() + 1024);
                if (EnumProcessModules(m_processHandle, modules.data(), modules.size() * sizeof(HMODULE), &numModules) == FALSE) {
                    modules.clear();
                    break;
                }
            } while (numModules == modules.size() * sizeof(HMODULE));

            modules.resize(numModules / sizeof(HMODULE));

            for (auto &module : modules) {
                MODULEINFO moduleInfo;
                if (GetModuleInformation(m_processHandle, module, &moduleInfo, sizeof(MODULEINFO)) == FALSE)
                    continue;

                char moduleName[MAX_PATH];
                if (GetModuleFileNameExA(m_processHandle, module, moduleName, MAX_PATH) == FALSE)
                    continue;

                m_memoryRegions.insert({ { u64(moduleInfo.lpBaseOfDll), size_t(moduleInfo.SizeOfImage) }, std::fs::path(moduleName).filename().string() });
            }

            MEMORY_BASIC_INFORMATION memoryInfo;
            for (u64 address = 0; address < this->getActualSize(); address += memoryInfo.RegionSize) {
                if (VirtualQueryEx(m_processHandle, reinterpret_cast<LPCVOID>(address), &memoryInfo, sizeof(MEMORY_BASIC_INFORMATION)) == 0)
                    break;

                std::string name;
                if (memoryInfo.State & MEM_IMAGE)   continue;
                if (memoryInfo.State & MEM_FREE)    continue;
                if (memoryInfo.State & MEM_COMMIT)  name += fmt::format("{} ", "hex.builtin.provider.process_memory.region.commit"_lang);
                if (memoryInfo.State & MEM_RESERVE) name += fmt::format("{} ", "hex.builtin.provider.process_memory.region.reserve"_lang);
                if (memoryInfo.State & MEM_PRIVATE) name += fmt::format("{} ", "hex.builtin.provider.process_memory.region.private"_lang);
                if (memoryInfo.State & MEM_MAPPED)  name += fmt::format("{} ", "hex.builtin.provider.process_memory.region.mapped"_lang);

                m_memoryRegions.insert({ { reinterpret_cast<u64>(memoryInfo.BaseAddress), reinterpret_cast<u64>(memoryInfo.BaseAddress) + memoryInfo.RegionSize }, name });
            }

        #elif defined(OS_MACOS)
            vm_region_submap_info_64 info;
            mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
            vm_address_t address = 0;
            vm_size_t size = 0;
            natural_t depth = 0;

            while (true) {
                if (vm_region_recurse_64(mach_task_self(), &address, &size, &depth, reinterpret_cast<vm_region_info_64_t>(&info), &count) != KERN_SUCCESS)
                    break;

                // Get region name
                std::array<char, 1024> name;
                if (proc_regionfilename(m_processId, address, name.data(), name.size()) != 0) {
                    std::strcpy(name.data(), "???");
                }

                m_memoryRegions.insert({ { address, size }, name.data() });
                address += size;
            }
        #elif defined(OS_LINUX)

            wolv::io::File file(std::fs::path("/proc") / std::to_string(m_processId) / "maps", wolv::io::File::Mode::Read);

            if (!file.isValid())
                return;

            // procfs files don't have a defined size, so we have to just keep reading until we stop getting data
            std::string data;
            while (true) {
                auto chunk = file.readString(0xFFFF);
                if (chunk.empty())
                    break;
                data.append(chunk);
            }

            for (const auto &line : wolv::util::splitString(data, "\n")) {
                const auto &split = wolv::util::splitString(line, " ");
                if (split.size() < 5)
                    continue;

                const u64 start = std::stoull(split[0].substr(0, split[0].find('-')), nullptr, 16);
                const u64 end   = std::stoull(split[0].substr(split[0].find('-') + 1), nullptr, 16);

                std::string name;
                if (split.size() > 5)
                    name = wolv::util::trim(wolv::util::combineStrings(std::vector(split.begin() + 5, split.end()), " "));

                m_memoryRegions.insert({ { .address=start, .size=end - start }, name });
            }
        #endif
    }


    std::variant<std::string, i128> ProcessMemoryProvider::queryInformation(const std::string &category, const std::string &argument) {
        auto findRegionByName = [this](const std::string &name) {
            return std::ranges::find_if(m_memoryRegions,
                [name](const auto &region) {
                    return region.name == name;
                });
        };

        if (category == "region_address") {
            if (auto iter = findRegionByName(argument); iter != m_memoryRegions.end())
                return iter->region.getStartAddress();
            else
                return 0;
        } else if (category == "region_size") {
            if (auto iter = findRegionByName(argument); iter != m_memoryRegions.end())
                return iter->region.getSize();
            else
                return 0;
        } else if (category == "process_id") {
            return m_selectedProcess->id;
        } else if (category == "process_name") {
            return m_selectedProcess->name;
        } else {
            return Provider::queryInformation(category, argument);
        }
    }

}

#endif