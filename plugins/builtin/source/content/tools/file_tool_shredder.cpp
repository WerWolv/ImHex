#include <hex/helpers/fs.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/task_manager.hpp>

#include <algorithm>
#include <random>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <toasts/toast_notification.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    void drawFileToolShredder() {
        static std::u8string selectedFile;
        static bool fastMode     = false;
        static TaskHolder shredderTask;

        ImGui::TextUnformatted("hex.builtin.tools.file_tools.shredder.warning"_lang);
        ImGui::NewLine();

        if (ImGui::BeginChild("settings", { 0, ImGui::GetTextLineHeightWithSpacing() * 4 }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            ImGui::BeginDisabled(shredderTask.isRunning());
            {
                ImGui::TextUnformatted("hex.builtin.tools.file_tools.shredder.input"_lang);
                ImGui::SameLine();
                ImGui::InputText("##path", selectedFile);
                ImGui::SameLine();
                if (ImGui::Button("...")) {
                    fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                        selectedFile = path.u8string();
                    });
                }

                ImGui::Checkbox("hex.builtin.tools.file_tools.shredder.fast"_lang, &fastMode);
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();

        if (shredderTask.isRunning()) {
            ImGuiExt::TextSpinner("hex.builtin.tools.file_tools.shredder.shredding"_lang);
        } else {
            ImGui::BeginDisabled(selectedFile.empty());
            {
                if (ImGui::Button("hex.builtin.tools.file_tools.shredder.shred"_lang)) {
                    shredderTask = TaskManager::createTask("hex.builtin.tools.file_tools.shredder.shredding"_lang, 0, [](auto &task) {
                        ON_SCOPE_EXIT {
                            selectedFile.clear();
                        };
                        wolv::io::File file(selectedFile, wolv::io::File::Mode::Write);

                        if (!file.isValid()) {
                            ui::ToastError::open("hex.builtin.tools.file_tools.shredder.error.open"_lang);
                            return;
                        }

                        task.setMaxValue(file.getSize());

                        std::vector<std::array<u8, 3>> overwritePattern;
                        if (fastMode) {
                            /* Should be sufficient for modern disks */
                            overwritePattern.push_back({ 0x00, 0x00, 0x00 });
                            overwritePattern.push_back({ 0xFF, 0xFF, 0xFF });
                        } else {
                            /* Gutmann's method. Secure for magnetic storage */
                            std::random_device rd;
                            std::uniform_int_distribution<u8> dist(0x00, 0xFF);

                            /* Fill fixed patterns */
                            overwritePattern = {
                                    {    },
                                    {    },
                                    {},
                                    {},
                                    { 0x55, 0x55, 0x55 },
                                    { 0xAA, 0xAA, 0xAA },
                                    { 0x92, 0x49, 0x24 },
                                    { 0x49, 0x24, 0x92 },
                                    { 0x24, 0x92, 0x49 },
                                    { 0x00, 0x00, 0x00 },
                                    { 0x11, 0x11, 0x11 },
                                    { 0x22, 0x22, 0x22 },
                                    { 0x33, 0x33, 0x44 },
                                    { 0x55, 0x55, 0x55 },
                                    { 0x66, 0x66, 0x66 },
                                    { 0x77, 0x77, 0x77 },
                                    { 0x88, 0x88, 0x88 },
                                    { 0x99, 0x99, 0x99 },
                                    { 0xAA, 0xAA, 0xAA },
                                    { 0xBB, 0xBB, 0xBB },
                                    { 0xCC, 0xCC, 0xCC },
                                    { 0xDD, 0xDD, 0xDD },
                                    { 0xEE, 0xEE, 0xEE },
                                    { 0xFF, 0xFF, 0xFF },
                                    { 0x92, 0x49, 0x24 },
                                    { 0x49, 0x24, 0x92 },
                                    { 0x24, 0x92, 0x49 },
                                    { 0x6D, 0xB6, 0xDB },
                                    { 0xB6, 0xDB, 0x6D },
                                    { 0xBD, 0x6D, 0xB6 },
                                    {},
                                    {},
                                    {},
                                    {}
                            };

                            /* Fill random patterns */
                            for (u8 i = 0; i < 4; i++)
                                overwritePattern[i] = { dist(rd), dist(rd), dist(rd) };
                            for (u8 i = 0; i < 4; i++)
                                overwritePattern[overwritePattern.size() - 1 - i] = { dist(rd), dist(rd), dist(rd) };
                        }

                        size_t fileSize = file.getSize();

                        for (const auto &pattern : overwritePattern) {
                            for (u64 offset = 0; offset < fileSize; offset += 3) {
                                file.writeBuffer(pattern.data(), std::min<u64>(pattern.size(), fileSize - offset));
                                task.update(offset);
                            }

                            file.flush();
                        }

                        file.remove();

                        ui::ToastInfo::open("hex.builtin.tools.file_tools.shredder.success"_lang);
                    });
                }
            }
            ImGui::EndDisabled();
        }
    }

}
