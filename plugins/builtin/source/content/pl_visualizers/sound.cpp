#include <hex/helpers/utils.hpp>

#include <content/pl_visualizers/visualizer_helpers.hpp>

#include <implot.h>
#include <imgui.h>
#include <miniaudio.h>
#include <fonts/codicons_font.h>
#include <hex/api/task_manager.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    void drawSoundVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        auto wavePattern = arguments[0].toPattern();
        auto channels = arguments[1].toUnsigned();
        auto sampleRate = arguments[2].toUnsigned();

        static std::vector<i16> waveData, sampledData;
        static ma_device audioDevice;
        static ma_device_config deviceConfig;
        static bool shouldStop = false;
        static u64 index = 0;
        static TaskHolder resetTask;

        if (sampleRate == 0)
            throw std::logic_error(hex::format("Invalid sample rate: {}", sampleRate));
        else if (channels == 0)
            throw std::logic_error(hex::format("Invalid channel count: {}", channels));

        if (shouldReset) {
            waveData.clear();

            resetTask = TaskManager::createTask("Visualizing...", TaskManager::NoProgress, [=](Task &) {
                ma_device_stop(&audioDevice);
                waveData = patternToArray<i16>(wavePattern.get());
                sampledData = sampleData(waveData, 300_scaled * 4);
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
                    index += frameCount;
                };

                ma_device_init(nullptr, &deviceConfig, &audioDevice);
            });
        }

        ImGui::BeginDisabled(resetTask.isRunning());

        ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
        if (ImPlot::BeginPlot("##amplitude_plot", scaled(ImVec2(300, 80)), ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs)) {
            ImPlot::SetupAxes("##time", "##amplitude", ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoMenus, ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoMenus);
            ImPlot::SetupAxesLimits(0, waveData.size(), std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), ImGuiCond_Always);

            double dragPos = index;
            if (ImPlot::DragLineX(1, &dragPos, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                if (dragPos < 0) dragPos = 0;
                if (dragPos >= waveData.size()) dragPos = waveData.size() - 1;

                index = dragPos;
            }

            ImPlot::PlotLine("##audio", sampledData.data(), sampledData.size());

            ImPlot::EndPlot();
        }
        ImPlot::PopStyleVar();

        {
            const u64 min = 0, max = waveData.size();
            ImGui::PushItemWidth(300_scaled);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::SliderScalar("##index", ImGuiDataType_U64, &index, &min, &max, "");
            ImGui::PopStyleVar();
            ImGui::PopItemWidth();
        }

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
            ma_device_stop(&audioDevice);
        }

        ImGui::EndDisabled();

        ImGui::SameLine();

        if (resetTask.isRunning())
            ImGuiExt::TextSpinner("");
        else
            ImGuiExt::TextFormatted("{:02d}:{:02d} / {:02d}:{:02d}",
                                 (index / sampleRate) / 60, (index / sampleRate) % 60,
                                 (waveData.size() / sampleRate) / 60, (waveData.size() / sampleRate) % 60);
    }

}