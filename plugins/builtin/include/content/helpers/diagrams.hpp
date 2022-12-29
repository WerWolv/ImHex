#pragma once

#include <hex.hpp>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <random>

namespace hex {

    namespace {

        std::vector<u8> getSampleSelection(prv::Provider *provider, u64 address, size_t size, size_t sampleSize) {
            const size_t sequenceCount = std::ceil(std::sqrt(sampleSize));

            std::vector<u8> buffer;

            if (size < sampleSize) {
                buffer.resize(size);
                provider->read(address, buffer.data(), size);
            } else {
                std::random_device randomDevice;
                std::mt19937_64 random(randomDevice());

                std::map<u64, std::vector<u8>> orderedData;
                for (u32 i = 0; i < sequenceCount; i++) {
                    ssize_t offset = random() % size;

                    std::vector<u8> sequence;
                    sequence.resize(std::min<size_t>(sequenceCount, size - offset));
                    provider->read(address + offset, sequence.data(), sequence.size());

                    orderedData.insert({ offset, sequence });
                }

                buffer.reserve(sampleSize);

                u64 lastEnd = 0x00;
                for (const auto &[offset, sequence] : orderedData) {
                    if (offset < lastEnd)
                        buffer.resize(buffer.size() - (lastEnd - offset));

                    std::copy(sequence.begin(), sequence.end(), std::back_inserter(buffer));
                    lastEnd = offset + sequence.size();
                }
            }

            return buffer;
        }

        std::vector<u8> getSampleSelection(const std::vector<u8> &inputBuffer, size_t sampleSize) {
            const size_t sequenceCount = std::ceil(std::sqrt(sampleSize));

            std::vector<u8> buffer;

            if (inputBuffer.size() < sampleSize) {
                buffer = inputBuffer;
            } else {
                std::random_device randomDevice;
                std::mt19937_64 random(randomDevice());

                std::map<u64, std::vector<u8>> orderedData;
                for (u32 i = 0; i < sequenceCount; i++) {
                    ssize_t offset = random() % inputBuffer.size();

                    std::vector<u8> sequence;
                    sequence.reserve(sampleSize);
                    std::copy(inputBuffer.begin() + offset, inputBuffer.begin() + offset + std::min<size_t>(sequenceCount, inputBuffer.size() - offset), std::back_inserter(sequence));

                    orderedData.insert({ offset, sequence });
                }

                buffer.reserve(sampleSize);

                u64 lastEnd = 0x00;
                for (const auto &[offset, sequence] : orderedData) {
                    if (offset < lastEnd)
                        buffer.resize(buffer.size() - (lastEnd - offset));

                    std::copy(sequence.begin(), sequence.end(), std::back_inserter(buffer));
                    lastEnd = offset + sequence.size();
                }
            }

            return buffer;
        }

    }

    class DiagramDigram {
    public:
        DiagramDigram(size_t sampleSize = 0x9000) : m_sampleSize(sampleSize) { }

        void draw(ImVec2 size) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImU32(ImColor(0, 0, 0)));
            if (ImGui::BeginChild("##digram", size, true)) {
                auto drawList = ImGui::GetWindowDrawList();

                float xStep = (size.x * 0.95F) / 0xFF;
                float yStep = (size.y * 0.95F) / 0xFF;

                if (!this->m_processing)
                    for (size_t i = 0; i < (this->m_buffer.empty() ? 0 : this->m_buffer.size() - 1); i++) {
                        const auto &[x, y] = std::pair { this->m_buffer[i] * xStep, this->m_buffer[i + 1] * yStep };

                        auto color = ImLerp(ImColor(0xFF, 0x6D, 0x01).Value, ImColor(0x01, 0x93, 0xFF).Value, float(i) / this->m_buffer.size()) + ImVec4(this->m_glowBuffer[i], this->m_glowBuffer[i], this->m_glowBuffer[i], 0.0F);
                        color.w    = this->m_opacity;

                        auto pos = ImGui::GetWindowPos() + ImVec2(size.x * 0.025F, size.y * 0.025F) + ImVec2(x, y);
                        drawList->AddRectFilled(pos, pos + ImVec2(xStep, yStep), ImColor(color));
                    }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        void process(prv::Provider *provider, u64 address, size_t size) {
            this->m_processing = true;
            this->m_buffer = getSampleSelection(provider, address, size, this->m_sampleSize);
            processImpl();
            this->m_processing = false;
        }

        void process(const std::vector<u8> &buffer) {
            this->m_processing = true;
            this->m_buffer = getSampleSelection(buffer, this->m_sampleSize);
            processImpl();
            this->m_processing = false;
        }

    private:
        void processImpl() {
            this->m_glowBuffer.resize(this->m_buffer.size());

            std::map<u64, size_t> heatMap;
            for (size_t i = 0; i < (this->m_buffer.empty() ? 0 : this->m_buffer.size() - 1); i++) {
                auto count = ++heatMap[this->m_buffer[i] << 8 | heatMap[i + 1]];

                this->m_highestCount = std::max(this->m_highestCount, count);
            }

            for (size_t i = 0; i < (this->m_buffer.empty() ? 0 : this->m_buffer.size() - 1); i++) {
                this->m_glowBuffer[i] = std::min(0.2F + (float(heatMap[this->m_buffer[i] << 8 | this->m_buffer[i + 1]]) / float(this->m_highestCount / 1000)), 1.0F);
            }

            this->m_opacity = (log10(float(this->m_sampleSize)) / log10(float(m_highestCount))) / 10.0F;
        }

    private:
        size_t m_sampleSize;

        std::vector<u8> m_buffer;
        std::vector<float> m_glowBuffer;
        float m_opacity = 0.0F;
        size_t m_highestCount = 0;
        std::atomic<bool> m_processing = false;
    };


    class DiagramLayeredDistribution {
    public:
        DiagramLayeredDistribution(size_t sampleSize = 0x9000) : m_sampleSize(sampleSize) { }

        void draw(ImVec2 size) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImU32(ImColor(0, 0, 0)));
            if (ImGui::BeginChild("##layered_distribution", size, true)) {
                auto drawList = ImGui::GetWindowDrawList();

                float xStep = (size.x * 0.95F) / 0xFF;
                float yStep = (size.y * 0.95F) / 0xFF;

                if (!this->m_processing)
                    for (size_t i = 0; i < (this->m_buffer.empty() ? 0 : this->m_buffer.size()); i++) {
                        const auto &[x, y] = std::pair { this->m_buffer[i] * xStep, yStep * ((float(i) / this->m_buffer.size()) * 0xFF) };

                        auto color = ImLerp(ImColor(0xFF, 0x6D, 0x01).Value, ImColor(0x01, 0x93, 0xFF).Value, float(i) / this->m_buffer.size()) + ImVec4(this->m_glowBuffer[i], this->m_glowBuffer[i], this->m_glowBuffer[i], 0.0F);
                        color.w    = this->m_opacity;

                        auto pos = ImGui::GetWindowPos() + ImVec2(size.x * 0.025F, size.y * 0.025F) + ImVec2(x, y);
                        drawList->AddRectFilled(pos, pos + ImVec2(xStep, yStep), ImColor(color));
                    }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        void process(prv::Provider *provider, u64 address, size_t size) {
            this->m_processing = true;
            this->m_buffer = getSampleSelection(provider, address, size, this->m_sampleSize);
            processImpl();
            this->m_processing = false;
        }

        void process(const std::vector<u8> &buffer) {
            this->m_processing = true;
            this->m_buffer = getSampleSelection(buffer, this->m_sampleSize);
            processImpl();
            this->m_processing = false;
        }

    private:
        void processImpl() {
            this->m_glowBuffer.resize(this->m_buffer.size());

            std::map<u64, size_t> heatMap;
            for (size_t i = 0; i < (this->m_buffer.empty() ? 0 : this->m_buffer.size() - 1); i++) {
                auto count = ++heatMap[this->m_buffer[i] << 8 | heatMap[i + 1]];

                this->m_highestCount = std::max(this->m_highestCount, count);
            }

            for (size_t i = 0; i < (this->m_buffer.empty() ? 0 : this->m_buffer.size() - 1); i++) {
                this->m_glowBuffer[i] = std::min(0.2F + (float(heatMap[this->m_buffer[i] << 8 | this->m_buffer[i + 1]]) / float(this->m_highestCount / 1000)), 1.0F);
            }

            this->m_opacity = (log10(float(this->m_sampleSize)) / log10(float(m_highestCount))) / 10.0F;
        }
    private:
        size_t m_sampleSize;

        std::vector<u8> m_buffer;
        std::vector<float> m_glowBuffer;
        float m_opacity = 0.0F;
        size_t m_highestCount = 0;
        std::atomic<bool> m_processing = false;
    };

}