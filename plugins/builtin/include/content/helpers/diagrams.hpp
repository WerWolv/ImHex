#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <implot.h>

#include <hex/providers/provider.hpp>
#include <hex/providers/buffered_reader.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <hex/helpers/logger.hpp>
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

    class ChunkBasedEntropyAnalysis {
    public:

        void draw(ImVec2 size, ImPlotFlags flags) {

            if (ImPlot::BeginPlot("##ChunkBasedAnalysis", size, flags)) {
                ImPlot::SetupAxes("hex.builtin.common.address"_lang, "hex.builtin.view.information.entropy"_lang, ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock);

                // set the axis limit to [first block : last block]
                ImPlot::SetupAxesLimits(
                    this->m_startAddress / this->m_chunkSize,
                    this->m_endAddress / this->m_chunkSize,
                    -0.1F, 1.1F, ImGuiCond_Always
                );

                // draw the plot
                ImPlot::PlotLine("##ChunkBasedAnalysisLine", this->m_xBlockEntropy.data(), this->m_yBlockEntropy.data(), this->m_blockCount);
                ImPlot::EndPlot();
            }
        }

        void process(prv::Provider *provider, u64 chunkSize, u64 startAddress, u64 endAddress) {
            this->m_processing = true;

            // update attributes  
            this->m_chunkSize    = chunkSize;
            this->m_startAddress = startAddress;
            this->m_endAddress   = endAddress;

            // get a file reader
            auto reader = prv::BufferedReader(provider);
            std::vector<u8> bytes = reader.read(this->m_startAddress, this->m_endAddress - this->m_startAddress);

            this->processImpl(bytes);

            this->m_processing = false;
        }

        void process(std::vector<u8> buffer, u64 chunkSize) {
            this->m_processing = true;

            // update attributes (use buffer size as end address) 
            this->m_chunkSize    = chunkSize;
            this->m_startAddress = 0;
            this->m_endAddress   = buffer.size();

            this->processImpl(buffer);

            this->m_processing = false;
        }

    private:
        // private method used to compute the entropy of a block of size `blockSize`
        // using the bytes occurrences from `valueCounts` array.
        double calculateEntropy(std::array<ImU64, 256> &valueCounts, size_t blockSize) {
            double entropy = 0;

            for (auto count : valueCounts) {
                if (count == 0) [[unlikely]]
                    continue;

                double probability = static_cast<double>(count) / blockSize;

                entropy += probability * std::log2(probability);
            }

            return std::min(1.0, (-entropy) / 8);    // log2(256) = 8
        }

        // private methode used to factorize the process public method 
        void processImpl(std::vector<u8> bytes) {
            // array used to hold the occurrences of each byte
            std::array<ImU64, 256> blockValueCounts;

            u64 blockCount = ((this->m_endAddress - this->m_startAddress) / this->m_chunkSize) + 1;

            // reset and resize the array
            this->m_yBlockEntropy.clear();
            this->m_yBlockEntropy.resize(blockCount);

            u64 count = 0;
            this->m_blockCount = 0;

            // loop over each byte of the file (or a part of it)
            for (u64 i = 0; i < bytes.size(); ++i) {
                u8 byte = bytes[i];
                // increment the occurrence of the current byte
                blockValueCounts[byte]++;

                count++;
                // check if we processed one complete chunk, if so compute the entropy and start analysing the next chunk
                if (((count % this->m_chunkSize) == 0) || count == bytes.size() * 8) [[unlikely]] {
                    this->m_yBlockEntropy[this->m_blockCount] = calculateEntropy(blockValueCounts, this->m_chunkSize);

                    this->m_blockCount += 1;
                    blockValueCounts = { 0 };
                }
            }

            // m_xBlockEntropy is used to specify the position of entropy values in the plot
            // when the Y axis doesn't start at 0
            this->m_xBlockEntropy.clear();
            this->m_xBlockEntropy.resize(this->m_blockCount);
            for (u64 i = 0; i < this->m_blockCount; ++i)
                this->m_xBlockEntropy[i] = (this->m_startAddress / this->m_chunkSize) + i;
        }

    private:
        // variables used to store the parameters to process 
        u64 m_chunkSize;
        u64 m_startAddress;
        u64 m_endAddress;

        // hold the number of block that have been processed
        // during the chunk based entropy analysis
        u64 m_blockCount;
        // variable to hold the result of the chunk based
        // entropy analysis
        std::vector<double> m_xBlockEntropy;
        std::vector<double> m_yBlockEntropy;

        std::atomic<bool> m_processing = false;
    };
}
