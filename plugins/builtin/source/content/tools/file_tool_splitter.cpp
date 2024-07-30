#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/literals.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/task_manager.hpp>

#include <algorithm>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <toasts/toast_notification.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    void drawFileToolSplitter() {
        std::array sizeText = {
            static_cast<const char*>("hex.builtin.tools.file_tools.splitter.sizes.5_75_floppy"_lang),
            static_cast<const char*>("hex.builtin.tools.file_tools.splitter.sizes.3_5_floppy"_lang),
            static_cast<const char*>("hex.builtin.tools.file_tools.splitter.sizes.zip100"_lang),
            static_cast<const char*>("hex.builtin.tools.file_tools.splitter.sizes.zip200"_lang),
            static_cast<const char*>("hex.builtin.tools.file_tools.splitter.sizes.cdrom650"_lang),
            static_cast<const char*>("hex.builtin.tools.file_tools.splitter.sizes.cdrom700"_lang),
            static_cast<const char*>("hex.builtin.tools.file_tools.splitter.sizes.fat32"_lang),
            static_cast<const char*>("hex.builtin.tools.file_tools.splitter.sizes.custom"_lang)
        };
        std::array<u64, sizeText.size()> sizes = {
            1200_KiB,
            1400_KiB,
            100_MiB,
            200_MiB,
            650_MiB,
            700_MiB,
            4_GiB,
            1
        };

        static std::u8string selectedFile;
        static std::u8string baseOutputPath;
        static u64 splitSize = sizes[0];
        static int selectedItem = 0;
        static TaskHolder splitterTask;

        if (ImGui::BeginChild("split_settings", { 0, ImGui::GetTextLineHeightWithSpacing() * 7 }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            ImGui::BeginDisabled(splitterTask.isRunning());
            {
                ImGui::InputText("##path", selectedFile);
                ImGui::SameLine();
                if (ImGui::Button("...##input")) {
                    fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                        selectedFile = path.u8string();
                    });
                }
                ImGui::SameLine();
                ImGui::TextUnformatted("hex.builtin.tools.file_tools.splitter.input"_lang);

                ImGui::InputText("##base_path", baseOutputPath);
                ImGui::SameLine();
                if (ImGui::Button("...##output")) {
                    fs::openFileBrowser(fs::DialogMode::Save, {}, [](const auto &path) {
                        baseOutputPath = path.u8string();
                    });
                }
                ImGui::SameLine();
                ImGui::TextUnformatted("hex.builtin.tools.file_tools.splitter.output"_lang);

                ImGui::Separator();

                if (ImGui::Combo("###part_size", &selectedItem, sizeText.data(), sizeText.size())) {
                    splitSize = sizes[selectedItem];
                }
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(splitterTask.isRunning() || selectedItem != sizes.size() - 1);
            {
                ImGui::InputScalar("###custom_size", ImGuiDataType_U64, &splitSize);
                ImGui::SameLine();
                ImGui::TextUnformatted("Bytes");
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();

        ImGui::BeginDisabled(selectedFile.empty() || baseOutputPath.empty() || splitSize == 0);
        {
            if (splitterTask.isRunning()) {
                ImGuiExt::TextSpinner("hex.builtin.tools.file_tools.splitter.picker.splitting"_lang);
            } else {
                if (ImGui::Button("hex.builtin.tools.file_tools.splitter.picker.split"_lang)) {
                    splitterTask = TaskManager::createTask("hex.builtin.tools.file_tools.splitter.picker.splitting"_lang, 0, [](auto &task) {
                        ON_SCOPE_EXIT {
                            selectedFile.clear();
                            baseOutputPath.clear();
                        };

                        wolv::io::File file(selectedFile, wolv::io::File::Mode::Read);

                        if (!file.isValid()) {
                            ui::ToastError::open("hex.builtin.tools.file_tools.splitter.picker.error.open"_lang);
                            return;
                        }

                        if (file.getSize() < splitSize) {
                            ui::ToastError::open("hex.builtin.tools.file_tools.splitter.picker.error.size"_lang);
                            return;
                        }

                        task.setMaxValue(file.getSize());

                        u32 index = 1;
                        for (u64 offset = 0; offset < file.getSize(); offset += splitSize) {
                            task.update(offset);

                            std::fs::path path = baseOutputPath;
                            path += hex::format(".{:05}", index);

                            wolv::io::File partFile(path, wolv::io::File::Mode::Create);

                            if (!partFile.isValid()) {
                                ui::ToastError::open(hex::format("hex.builtin.tools.file_tools.splitter.picker.error.create"_lang, index));
                                return;
                            }

                            constexpr static auto BufferSize = 0xFF'FFFF;
                            for (u64 partOffset = 0; partOffset < splitSize; partOffset += BufferSize) {
                                partFile.writeVector(file.readVector(std::min<u64>(BufferSize, splitSize - partOffset)));
                                partFile.flush();
                            }

                            index++;
                        }

                        ui::ToastInfo::open("hex.builtin.tools.file_tools.splitter.picker.success"_lang);
                    });
                }
            }
        }
        ImGui::EndDisabled();
    }

}
