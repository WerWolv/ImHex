#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/task_manager.hpp>

#include <algorithm>
#include <random>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <toasts/toast_notification.hpp>

#include <wolv/io/file.hpp>

namespace hex::plugin::builtin {

    void drawFileToolCombiner() {
        static std::vector<std::fs::path> files;
        static std::u8string outputPath;
        static u32 selectedIndex;
        static TaskHolder combinerTask;

        if (ImGui::BeginTable("files_table", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("file list", ImGuiTableColumnFlags_NoHeaderLabel, 10);
            ImGui::TableSetupColumn("buttons", ImGuiTableColumnFlags_NoHeaderLabel, 1);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (ImGui::BeginListBox("##files", { -FLT_MIN, 10 * ImGui::GetTextLineHeightWithSpacing() })) {
                u32 index = 0;
                for (auto &file : files) {
                    if (ImGui::Selectable(wolv::util::toUTF8String(file).c_str(), index == selectedIndex))
                        selectedIndex = index;
                    index++;
                }

                ImGui::EndListBox();
            }

            ImGui::TableNextColumn();

            ImGui::BeginDisabled(selectedIndex <= 0);
            {
                if (ImGui::ArrowButton("move_up", ImGuiDir_Up)) {
                    std::iter_swap(files.begin() + selectedIndex, files.begin() + selectedIndex - 1);
                    selectedIndex--;
                }
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(files.empty() || selectedIndex >= files.size() - 1);
            {
                if (ImGui::ArrowButton("move_down", ImGuiDir_Down)) {
                    std::iter_swap(files.begin() + selectedIndex, files.begin() + selectedIndex + 1);
                    selectedIndex++;
                }
            }
            ImGui::EndDisabled();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::BeginDisabled(combinerTask.isRunning());
            {
                if (ImGui::Button("hex.builtin.tools.file_tools.combiner.add"_lang)) {
                    fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                        files.push_back(path);
                    }, "", true);
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(files.empty() || selectedIndex >= files.size());
                if (ImGui::Button("hex.builtin.tools.file_tools.combiner.delete"_lang)) {
                    files.erase(files.begin() + selectedIndex);

                    if (selectedIndex > 0)
                        selectedIndex--;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(files.empty());
                if (ImGui::Button("hex.builtin.tools.file_tools.combiner.clear"_lang)) {
                    files.clear();
                }
                ImGui::EndDisabled();
            }
            ImGui::EndDisabled();

            ImGui::EndTable();
        }

        ImGui::BeginDisabled(combinerTask.isRunning());
        {
            ImGui::InputText("##output_path", outputPath);
            ImGui::SameLine();
            if (ImGui::Button("...")) {
                fs::openFileBrowser(fs::DialogMode::Save, {}, [](const auto &path) {
                    outputPath = path.u8string();
                });
            }
            ImGui::SameLine();
            ImGui::TextUnformatted("hex.builtin.tools.file_tools.combiner.output"_lang);
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(files.empty() || outputPath.empty());
        {
            if (combinerTask.isRunning()) {
                ImGuiExt::TextSpinner("hex.builtin.tools.file_tools.combiner.combining"_lang);
            } else {
                if (ImGui::Button("hex.builtin.tools.file_tools.combiner.combine"_lang)) {
                    combinerTask = TaskManager::createTask("hex.builtin.tools.file_tools.combiner.combining"_lang, 0, [](auto &task) {
                        wolv::io::File output(outputPath, wolv::io::File::Mode::Create);

                        if (!output.isValid()) {
                            ui::ToastError::open("hex.builtin.tools.file_tools.combiner.error.open_output"_lang);
                            return;
                        }

                        task.setMaxValue(files.size());

                        for (const auto &file : files) {
                            task.increment();

                            wolv::io::File input(file, wolv::io::File::Mode::Read);
                            if (!input.isValid()) {
                                ui::ToastError::open(hex::format("hex.builtin.tools.file_tools.combiner.open_input"_lang, wolv::util::toUTF8String(file)));
                                return;
                            }

                            constexpr static auto BufferSize = 0xFF'FFFF;
                            auto inputSize = input.getSize();
                            for (u64 inputOffset = 0; inputOffset < inputSize; inputOffset += BufferSize) {
                                output.writeVector(input.readVector(std::min<u64>(BufferSize, inputSize - inputOffset)));
                                output.flush();
                            }
                        }

                        files.clear();
                        selectedIndex = 0;
                        outputPath.clear();

                        ui::ToastInfo::open("hex.builtin.tools.file_tools.combiner.success"_lang);
                    });
                }
            }
        }
        ImGui::EndDisabled();
    }

}