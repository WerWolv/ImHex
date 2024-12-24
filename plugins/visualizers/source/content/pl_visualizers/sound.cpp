#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <implot.h>
#include <imgui.h>
#include <miniaudio.h>
#include <fonts/vscode_icons.hpp>
#include <hex/api/task_manager.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::visualizers {

    void drawSoundVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        auto wavePattern = arguments[0].toPattern();
        auto channels = arguments[1].toUnsigned();
        auto sampleRate = arguments[2].toUnsigned();
        u32 downSampling = wavePattern->getSize() /  300_scaled / 8 / channels;

        static std::vector<i16> waveData;
        static std::vector<std::vector<i16>> sampledData;
        sampledData.resize(channels);
        static ma_device audioDevice;
        static ma_device_config deviceConfig;
        static bool shouldStop = false;
        static u64 index = 0;
        static TaskHolder resetTask;

        if (sampleRate == 0)
            throw std::logic_error(hex::format("Invalid sample rate: {}", sampleRate));
        else if (channels == 0)
            throw std::logic_error(hex::format("Invalid channel count: {}", channels));
        u64 sampledIndex;
        if (shouldReset) {
            waveData.clear();

            resetTask = TaskManager::createTask("hex.visualizers.pl_visualizer.task.visualizing"_lang, TaskManager::NoProgress, [=](Task &) {
                ma_device_stop(&audioDevice);
                waveData = patternToArray<i16>(wavePattern.get());
                if (waveData.empty())
                    return;
                sampledData = sampleChannels(waveData, 300_scaled * 4, channels);
                index = 0;

                deviceConfig = ma_device_config_init(ma_device_type_playback);
                deviceConfig.playback.format   = ma_format_s16;
                deviceConfig.playback.channels = channels;
                deviceConfig.sampleRate        = sampleRate;
                deviceConfig.pUserData         = &waveData;
                deviceConfig.dataCallback      = [](ma_device *device, void *pOutput, const void *, ma_uint32 frameCount) {
                    if (index >= waveData.size()) {
                        index = 0;
                        shouldStop = true;
                        return;
                    }

                    ma_copy_pcm_frames(pOutput, waveData.data() + index, frameCount, device->playback.format, device->playback.channels);
                    index += static_cast<u64>(frameCount) * device->playback.channels;
                };

                ma_device_init(nullptr, &deviceConfig, &audioDevice);
            });
        }
        sampledIndex = index / downSampling;
        ImGui::BeginDisabled(resetTask.isRunning());
        u32 waveDataSize = waveData.size();
        u32 sampledDataSize = sampledData[0].size();
        auto subplotFlags = ImPlotSubplotFlags_LinkAllX | ImPlotSubplotFlags_LinkCols | ImPlotSubplotFlags_NoResize;
        auto plotFlags = ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs;
        auto axisFlags = ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_AutoFit;
        ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));

        if (ImPlot::BeginSubplots("##AxisLinking", channels, 1, scaled(ImVec2(300, 80 * channels)), subplotFlags)) {
            for (u32 i = 0; i < channels; i++) {
                if (ImPlot::BeginPlot("##amplitude_plot", scaled(ImVec2(300, 80)), plotFlags)) {

                    ImPlot::SetupAxes("##time", "##amplitude", axisFlags, axisFlags);
                    double dragPos = sampledIndex;
                    if (ImPlot::DragLineX(1, &dragPos, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        if (dragPos < 0) dragPos = 0;
                        if (dragPos >= sampledDataSize) dragPos = sampledDataSize - 1;

                        sampledIndex = dragPos;
                    }
                    ImPlot::PlotLine("##audio", sampledData[i].data(), sampledDataSize);

                    ImPlot::EndPlot();
                }
            }
            ImPlot::PopStyleVar();

            index = sampledIndex * downSampling;
            {
                const u64 min = 0, max = sampledDataSize-1;
                ImGui::PushItemWidth(300_scaled);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::SliderScalar("##index", ImGuiDataType_U64, &sampledIndex, &min, &max, "");
                ImGui::PopStyleVar();
                ImGui::PopItemWidth();
            }
            index = sampledIndex * downSampling;


            if (shouldStop) {
                shouldStop = false;
                ma_device_stop(&audioDevice);
            }

            bool playing = ma_device_is_started(&audioDevice);

            if (ImGuiExt::IconButton(playing ? ICON_VS_DEBUG_PAUSE : ICON_VS_PLAY, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen))) {
                if (playing)
                    ma_device_stop(&audioDevice);
                else
                    ma_device_start(&audioDevice);
            }

            ImGui::SameLine();

            if (ImGuiExt::IconButton(ICON_VS_DEBUG_STOP, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                index = 0;
                sampledIndex = 0;
                ma_device_stop(&audioDevice);
            }

            ImGui::EndDisabled();

            ImGui::SameLine();
            index = sampledIndex * downSampling;

            if (resetTask.isRunning())
                ImGuiExt::TextSpinner("");
            else
                ImGuiExt::TextFormatted("{:02d}:{:02d}:{:03d} / {:02d}:{:02d}:{:03d}",
                                        (index / sampleRate / channels) / 60, (index / sampleRate / channels) % 60, (index * 1000 / sampleRate / channels ) % 1000,
                                        ((waveDataSize-1) / sampleRate / channels) / 60, ((waveDataSize-1) / sampleRate / channels) % 60, ((waveDataSize-1) * 1000 / sampleRate / channels) % 1000);
            ImPlot::EndSubplots();
        }
    }

}