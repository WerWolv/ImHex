#include <content/visualizer_helpers.hpp>

#include <implot.h>
#include <imgui.h>
#include <miniaudio.h>
#include <fonts/vscode_icons.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/api/events/events_gui.hpp>

#include <atomic>

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/helpers/scaling.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::plugin::visualizers {

    void drawSoundVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        auto wavePattern = arguments[0].toPattern();
        auto channels    = u64(arguments[1].toUnsigned());
        auto sampleRate  = u64(arguments[2].toUnsigned());
        u32 downSampling = wavePattern->getSize() /  300_scaled / 8 / channels;

        static std::vector<i16> waveData;
        static std::vector<std::vector<i16>> sampledData;
        sampledData.resize(channels);
        static ma_device audioDevice;
        static ma_device_config deviceConfig;
        static std::atomic<bool> deviceReady = false;
        static bool shouldStop = false;
        static std::atomic<u64> index = 0;
        static TaskHolder resetTask;
        static int lastDrawnFrame = 0;
        static bool seekHeld = false;
        static bool resumeAfterSeek = false;
        static u64 heldSampledIndex = 0;
        static bool frameEndSubscribed = false;

        lastDrawnFrame = ImGui::GetFrameCount();
        if (!frameEndSubscribed) {
            frameEndSubscribed = true;

            // Stop playback in a frame that did not draw the visualizer, which means its popup was closed.
            EventFrameEnd::subscribe([] {
                if (lastDrawnFrame == ImGui::GetFrameCount() || !deviceReady || resetTask.isRunning())
                    return;
                seekHeld = false;
                resumeAfterSeek = false;
                if (!ma_device_is_started(&audioDevice))
                    return;

                ma_device_stop(&audioDevice);
                index = 0;
            });
        }

        if (sampleRate == 0)
            throw std::logic_error(fmt::format("Invalid sample rate: {}", sampleRate));
        if (channels == 0)
            throw std::logic_error(fmt::format("Invalid channel count: {}", channels));
        if (downSampling == 0)
            throw std::logic_error(fmt::format("Invalid down sampling factor: {} / 2400 / {} = {}", wavePattern->getSize(), channels, downSampling));
        u64 sampledIndex;
        if (shouldReset) {
            // A second ma_device_init without uninit leaks a worker thread that shares this device, and ma_device_stop then hangs.
            if (deviceReady.exchange(false))
                ma_device_uninit(&audioDevice);
            seekHeld = false;
            resumeAfterSeek = false;

            waveData.clear();

            resetTask = TaskManager::createTask("hex.visualizers.pl_visualizer.task.visualizing"_unlocalized, ProgressValue::None(), [=](Task &) {
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
                    // Play silence until the UI stops the device, so the clip does not loop when the UI is not drawn.
                    const u64 position = index;
                    if (position >= waveData.size()) {
                        shouldStop = true;
                        return;
                    }

                    const u64 framesLeft = (waveData.size() - position) / device->playback.channels;
                    const u64 frames = std::min<u64>(frameCount, framesLeft);
                    ma_copy_pcm_frames(pOutput, waveData.data() + position, frames, device->playback.format, device->playback.channels);
                    index = frames < frameCount ? waveData.size() : position + frames * device->playback.channels;
                };

                deviceReady = ma_device_init(nullptr, &deviceConfig, &audioDevice) == MA_SUCCESS;
            });
        }
        sampledIndex = seekHeld ? heldSampledIndex : index / downSampling;
        bool held = false;
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
                    bool lineHeld = false;
                    if (ImPlot::DragLineX(1, &dragPos, ImGui::GetStyleColorVec4(ImGuiCol_Text), 1, ImPlotDragToolFlags_None, nullptr, nullptr, &lineHeld)) {
                        if (dragPos < 0) dragPos = 0;
                        if (dragPos >= sampledDataSize) dragPos = sampledDataSize - 1;

                        sampledIndex = dragPos;
                    }
                    held |= lineHeld;
                    ImPlot::PlotLine("##audio", sampledData[i].data(), sampledDataSize);

                    ImPlot::EndPlot();
                }
            }
            ImPlot::PopStyleVar();

            {
                const u64 min = 0, max = sampledDataSize-1;
                ImGui::PushItemWidth(300_scaled);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::SliderScalar("##index", ImGuiDataType_U64, &sampledIndex, &min, &max, "");
                held |= ImGui::IsItemActive();
                ImGui::PopStyleVar();
                ImGui::PopItemWidth();
            }

            // Pause while the drag line or the slider is held, and seek once on release.
            const bool ready = deviceReady && !resetTask.isRunning();
            if (held) {
                if (!seekHeld) {
                    seekHeld = true;
                    resumeAfterSeek = ready && ma_device_is_started(&audioDevice);
                    if (resumeAfterSeek)
                        ma_device_stop(&audioDevice);
                }
                heldSampledIndex = sampledIndex;
            } else if (seekHeld) {
                seekHeld = false;
                index = heldSampledIndex * downSampling;
                if (resumeAfterSeek && ready)
                    ma_device_start(&audioDevice);
            }

            if (shouldStop) {
                shouldStop = false;
                if (ready)
                    ma_device_stop(&audioDevice);
                index = 0;
                sampledIndex = 0;
            }

            bool playing = ready && ma_device_is_started(&audioDevice);

            if (ImGuiExt::IconButton(playing ? ICON_VS_DEBUG_PAUSE : ICON_VS_PLAY, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen)) && ready) {
                if (playing)
                    ma_device_stop(&audioDevice);
                else
                    ma_device_start(&audioDevice);
            }

            ImGui::SameLine();

            if (ImGuiExt::IconButton(ICON_VS_DEBUG_STOP, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                index = 0;
                sampledIndex = 0;
                if (ready)
                    ma_device_stop(&audioDevice);
            }

            ImGui::EndDisabled();

            ImGui::SameLine();

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