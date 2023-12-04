#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <implot.h>

#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/providers/provider.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <imgui_internal.h>

#include <random>

namespace hex {

    namespace impl {

        inline int IntegerAxisFormatter(double value, char* buffer, int size, void *userData) {
            u64 integer = static_cast<u64>(value);
            return snprintf(buffer, size, static_cast<const char*>(userData), integer);
        }

        inline std::vector<u8> getSampleSelection(prv::Provider *provider, u64 address, size_t size, size_t sampleSize) {
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

        inline std::vector<u8> getSampleSelection(const std::vector<u8> &inputBuffer, size_t sampleSize) {
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
                    std::copy_n(inputBuffer.begin() + offset, std::min<size_t>(sequenceCount, inputBuffer.size() - offset), std::back_inserter(sequence));

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
        explicit DiagramDigram(size_t sampleSize = 0x9000) : m_sampleSize(sampleSize) { }

        void draw(ImVec2 size) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImU32(ImColor(0, 0, 0)));
            if (ImGui::BeginChild("##digram", size, true)) {
                auto drawList = ImGui::GetWindowDrawList();

                float xStep = (size.x * 0.95F) / 0xFF;
                float yStep = (size.y * 0.95F) / 0xFF;

                if (!this->m_processing)
                    for (size_t i = 0; i < (this->m_buffer.empty() ? 0 : this->m_buffer.size() - 1); i++) {
                        auto x = this->m_buffer[i] * xStep;
                        auto y = this->m_buffer[i + 1] * yStep;

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
            this->m_buffer = impl::getSampleSelection(provider, address, size, this->m_sampleSize);
            processImpl();
            this->m_processing = false;
        }

        void process(const std::vector<u8> &buffer) {
            this->m_processing = true;
            this->m_buffer = impl::getSampleSelection(buffer, this->m_sampleSize);
            processImpl();
            this->m_processing = false;
        }

        void reset(u64 size) {
            this->m_processing = true;
            this->m_buffer.clear();
            this->m_buffer.reserve(this->m_sampleSize);
            this->m_byteCount = 0;
            this->m_fileSize  = size;
        }

        void update(u8 byte) {
            // Check if there is some space left
            if (this->m_byteCount < this->m_fileSize) {
                if ((this->m_byteCount % u64(std::ceil(double(this->m_fileSize) / double(this->m_sampleSize)))) == 0)
                    this->m_buffer.push_back(byte);
                ++this->m_byteCount;
                if (this->m_byteCount == this->m_fileSize) {
                    processImpl();
                    this->m_processing = false;
                }
            } 
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
                this->m_glowBuffer[i] = std::min<float>(0.2F + (float(heatMap[this->m_buffer[i] << 8 | this->m_buffer[i + 1]]) / float(this->m_highestCount / 1000)), 1.0F);
            }

            this->m_opacity = (log10(float(this->m_sampleSize)) / log10(float(m_highestCount))) / 10.0F;
        }

    private:
        size_t m_sampleSize = 0;

        // The number of bytes processed and the size of
        // the file to analyze (useful for iterative analysis)
        u64 m_byteCount = 0;
        u64 m_fileSize = 0;
        std::vector<u8> m_buffer;
        std::vector<float> m_glowBuffer;
        float m_opacity = 0.0F;
        size_t m_highestCount = 0;
        std::atomic<bool> m_processing = false;
    };

    class DiagramLayeredDistribution {
    public:
        explicit DiagramLayeredDistribution(size_t sampleSize = 0x9000) : m_sampleSize(sampleSize) { }

        void draw(ImVec2 size) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImU32(ImColor(0, 0, 0)));
            if (ImGui::BeginChild("##layered_distribution", size, true)) {
                auto drawList = ImGui::GetWindowDrawList();

                float xStep = (size.x * 0.95F) / 0xFF;
                float yStep = (size.y * 0.95F) / 0xFF;

                if (!this->m_processing)
                    for (size_t i = 0; i < (this->m_buffer.empty() ? 0 : this->m_buffer.size()); i++) {
                        auto x = this->m_buffer[i] * xStep;
                        auto y = yStep * ((float(i) / this->m_buffer.size()) * 0xFF);

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
            this->m_buffer = impl::getSampleSelection(provider, address, size, this->m_sampleSize);
            processImpl();
            this->m_processing = false;
        }

        void process(const std::vector<u8> &buffer) {
            this->m_processing = true;
            this->m_buffer = impl::getSampleSelection(buffer, this->m_sampleSize);
            processImpl();
            this->m_processing = false;
        }

        void reset(u64 size) {
            this->m_processing = true;
            this->m_buffer.clear();
            this->m_buffer.reserve(this->m_sampleSize);
            this->m_byteCount = 0;
            this->m_fileSize  = size;
        }

        void update(u8 byte) {
            // Check if there is some space left
            if (this->m_byteCount < this->m_fileSize) {
                if ((this->m_byteCount % u64(std::ceil(double(this->m_fileSize) / double(this->m_sampleSize)))) == 0)
                    this->m_buffer.push_back(byte);
                ++this->m_byteCount;
                if (this->m_byteCount == this->m_fileSize) {
                    processImpl();
                    this->m_processing = false;
                }
            } 
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
                this->m_glowBuffer[i] = std::min<float>(0.2F + (float(heatMap[this->m_buffer[i] << 8 | this->m_buffer[i + 1]]) / float(this->m_highestCount / 1000)), 1.0F);
            }

            this->m_opacity = (log10(float(this->m_sampleSize)) / log10(float(m_highestCount))) / 10.0F;
        }
    private:
        size_t m_sampleSize = 0;
    
        // The number of bytes processed and the size of
        // the file to analyze (useful for iterative analysis)
        u64 m_byteCount = 0;
        u64 m_fileSize = 0;

        std::vector<u8> m_buffer;
        std::vector<float> m_glowBuffer;
        float m_opacity = 0.0F;
        size_t m_highestCount = 0;
        std::atomic<bool> m_processing = false;
    };

    class DiagramChunkBasedEntropyAnalysis {
    public:
        explicit DiagramChunkBasedEntropyAnalysis(u64 blockSize = 256, size_t sampleSize = 0x1000) : m_blockSize(blockSize), m_sampleSize(sampleSize) { }

        void draw(ImVec2 size, ImPlotFlags flags, bool updateHandle = false) {

            if (!this->m_processing && ImPlot::BeginPlot("##ChunkBasedAnalysis", size, flags)) {
                ImPlot::SetupAxes("hex.builtin.common.address"_lang, "hex.builtin.view.information.entropy"_lang,
                                  ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch,
                                  ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch);
                ImPlot::SetupAxisFormat(ImAxis_X1, impl::IntegerAxisFormatter, (void*)("0x%04llX"));
                ImPlot::SetupMouseText(ImPlotLocation_NorthEast);

                // Set the axis limit to [first block : last block]
                ImPlot::SetupAxesLimits(
                        this->m_xBlockEntropy.empty() ? 0 : this->m_xBlockEntropy.front(),
                        this->m_xBlockEntropy.empty() ? 0 : this->m_xBlockEntropy.back(),
                        -0.1F,
                        1.1F,
                        ImGuiCond_Always);

                // Draw the plot
                ImPlot::PlotLine("##ChunkBasedAnalysisLine", this->m_xBlockEntropy.data(), this->m_yBlockEntropySampled.data(), this->m_xBlockEntropy.size());

                // The parameter updateHandle is used when using the pattern language since we don't have a provider 
                // but just a set of bytes, we won't be able to use the drag bar correctly.
                if (updateHandle) {
                    // Set a draggable line on the plot
                    if (ImPlot::DragLineX(1, &this->m_handlePosition, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        // The line was dragged, update the position in the hex editor

                        // Clamp the value between the start/end of the region to analyze
                        this->m_handlePosition = std::clamp<double>(
                                this->m_handlePosition,
                                this->m_startAddress,
                                this->m_endAddress);

                        // Compute the position inside hex editor 
                        u64 address = u64(std::max<double>(this->m_handlePosition, 0)) + this->m_baseAddress;
                        address     = std::min<u64>(address, this->m_baseAddress + this->m_fileSize - 1);
                        ImHexApi::HexEditor::setSelection(address, 1);
                    }
                }
                ImPlot::EndPlot();
            }
        }

        void process(prv::Provider *provider, u64 chunkSize, u64 startAddress, u64 endAddress) {
            this->m_processing = true;

            // Update attributes  
            this->m_chunkSize    = chunkSize;
            this->m_startAddress = startAddress;
            this->m_endAddress   = endAddress;

            this->m_baseAddress  = provider->getBaseAddress();
            this->m_fileSize     = provider->getSize();

            // Get a file reader
            auto reader = prv::ProviderReader(provider);
            std::vector<u8> bytes = reader.read(this->m_startAddress, this->m_endAddress - this->m_startAddress);

            this->processImpl(bytes);

            // Set the diagram handle position to the start of the plot
            this->m_handlePosition = this->m_startAddress;

            this->m_processing = false;
        }

        void process(const std::vector<u8> &buffer, u64 chunkSize) {
            this->m_processing = true;

            // Update attributes (use buffer size as end address) 
            this->m_chunkSize    = chunkSize;
            this->m_startAddress = 0;
            this->m_endAddress   = buffer.size();

            this->m_baseAddress  = 0;
            this->m_fileSize     = buffer.size();

            this->processImpl(buffer);

            // Set the diagram handle position to the start of the plot
            this->m_handlePosition = this->m_startAddress;

            this->m_processing = false;
        }

        // Reset the entropy analysis 
        void reset(u64 chunkSize, u64 startAddress, u64 endAddress, u64 baseAddress, u64 size) {
            this->m_processing = true;

            // Update attributes  
            this->m_chunkSize    = chunkSize;
            this->m_startAddress = startAddress;
            this->m_endAddress   = endAddress;
            this->m_baseAddress  = baseAddress; 
            this->m_fileSize     = size;

            this->m_blockValueCounts = { 0 };

            // Reset and resize the array
            this->m_yBlockEntropy.clear();

            this->m_byteCount = 0;
            this->m_blockCount = 0;

            // Set the diagram handle position to the start of the plot
            this->m_handlePosition = this->m_startAddress;
        }

        // Process one byte at the time
        void update(u8 byte) {
            u64 totalBlock = std::ceil((this->m_endAddress - this->m_startAddress) / this->m_chunkSize);

            // Check if there is still some 
            if (this->m_blockCount < totalBlock) {
                // Increment the occurrence of the current byte
                this->m_blockValueCounts[byte]++;

                this->m_byteCount++;
                // Check if we processed one complete chunk, if so compute the entropy and start analysing the next chunk
                if (((this->m_byteCount % this->m_chunkSize) == 0) || this->m_byteCount == (this->m_endAddress - this->m_startAddress)) [[unlikely]] {
                    this->m_yBlockEntropy.push_back(calculateEntropy(this->m_blockValueCounts, this->m_chunkSize));

                    this->m_blockCount += 1;
                    this->m_blockValueCounts = { 0 };
                }
               
                // Check if we processed the last block, if so setup the X axis part of the data
                if (this->m_blockCount == totalBlock) {
                    processFinalize();
                    this->m_processing = false;           
                }
            }
        }

        // Method used to compute the entropy of a block of size `blockSize`
        // using the byte occurrences from `valueCounts` array.
        double calculateEntropy(const std::array<ImU64, 256> &valueCounts, size_t blockSize) const {
            double entropy = 0;

            u8 processedValueCount = 0;
            for (const auto count : valueCounts) {
                if (count == 0) [[unlikely]]
                    continue;

                processedValueCount += 1;

                double probability = static_cast<double>(count) / blockSize;

                entropy += probability * std::log2(probability);
            }

            if (processedValueCount == 1)
                return 0.0;

            return std::min<double>(1.0, (-entropy) / 8);    // log2(256) = 8
        }

        // Return the highest entropy value among all of the blocks
        double getHighestEntropyBlockValue() {
            double result = 0.0f;
            if (!this->m_yBlockEntropy.empty())
                result = *std::max_element(this->m_yBlockEntropy.begin(), this->m_yBlockEntropy.end());
            return result;
        }

        // Return the highest entropy value among all of the blocks
        u64 getHighestEntropyBlockAddress() {
            u64 address = 0x00;
            if (!this->m_yBlockEntropy.empty())
                address = (std::max_element(this->m_yBlockEntropy.begin(), this->m_yBlockEntropy.end()) - this->m_yBlockEntropy.begin()) * this->m_blockSize;
            return this->m_startAddress + address;
        }

        // Return the highest entropy value among all of the blocks
        double getLowestEntropyBlockValue() {
            double result = 0.0f;
            if (this->m_yBlockEntropy.size() > 1)
                result = *std::min_element(this->m_yBlockEntropy.begin(), this->m_yBlockEntropy.end() - 1);
            return result;
        }

        // Return the highest entropy value among all of the blocks
        u64 getLowestEntropyBlockAddress() {
            u64 address = 0x00;
            if (this->m_yBlockEntropy.size() > 1)
                address = (std::min_element(this->m_yBlockEntropy.begin(), this->m_yBlockEntropy.end() - 1) - this->m_yBlockEntropy.begin()) * this->m_blockSize;
            return this->m_startAddress + address;
        }

        // Return the number of blocks that have been processed
        u64 getSize() const {
            return this->m_yBlockEntropySampled.size();
        }
    
        // Return the size of the chunk used for this analysis
        u64 getChunkSize() const {
            return this->m_chunkSize;
        } 

        void setHandlePosition(u64 filePosition) {
            this->m_handlePosition = filePosition;
        }

    private: 
        // Private method used to factorize the process public method 
        void processImpl(const std::vector<u8> &bytes) {
            this->m_blockValueCounts = { 0 };

            // Reset and resize the array
            this->m_yBlockEntropy.clear();

            this->m_byteCount = 0;
            this->m_blockCount = 0;

            // Loop over each byte of the file (or a part of it)
            for (u8 byte: bytes) {
                // Increment the occurrence of the current byte
                this->m_blockValueCounts[byte]++;

                this->m_byteCount++;
                // Check if we processed one complete chunk, if so compute the entropy and start analysing the next chunk
                if (((this->m_byteCount % this->m_chunkSize) == 0) || this->m_byteCount == bytes.size() * 8) [[unlikely]] {
                    this->m_yBlockEntropy.push_back(calculateEntropy(this->m_blockValueCounts, this->m_chunkSize));

                    this->m_blockCount += 1;
                    this->m_blockValueCounts = { 0 };
                }
            }
            processFinalize();
        }

        void processFinalize() {
            // Only save at most m_sampleSize elements of the result
            this->m_yBlockEntropySampled = sampleData(this->m_yBlockEntropy, std::min<size_t>(this->m_blockCount + 1, this->m_sampleSize));

            if (!this->m_yBlockEntropySampled.empty())
                this->m_yBlockEntropySampled.push_back(this->m_yBlockEntropySampled.back());

            double stride = std::max(1.0, double(
                double(std::ceil((this->m_endAddress - this->m_startAddress)) / this->m_blockSize) / this->m_yBlockEntropySampled.size()));

            this->m_blockCount = this->m_yBlockEntropySampled.size() - 1;

            // The m_xBlockEntropy attribute is used to specify the position of entropy values 
            // in the plot when the Y axis doesn't start at 0
            this->m_xBlockEntropy.clear();
            this->m_xBlockEntropy.resize(this->m_blockCount);
            for (u64 i = 0; i < this->m_blockCount; ++i)
                this->m_xBlockEntropy[i] = ((this->m_startAddress / this->m_blockSize) + stride * i) * this->m_blockSize;
            this->m_xBlockEntropy.push_back(this->m_endAddress);
        }

    private:
        // Variables used to store the parameters to process 
        
        // Chunk's size for entropy analysis
        u64 m_chunkSize = 0;
        u64 m_startAddress = 0x00;
        u64 m_endAddress = 0x00;
        // Start / size of the file
        u64 m_baseAddress = 0x00;
        u64 m_fileSize = 0;
        // The size of the blocks (for diagram drawing) 
        u64 m_blockSize = 0;

        // Position of the handle inside the plot
        double m_handlePosition = 0.0; 

        // Hold the number of blocks that have been processed
        // during the chunk-based entropy analysis
        u64 m_blockCount = 0;
        
        // Hold the number of bytes that have been processed 
        // during the analysis (useful for the iterative analysis)
        u64 m_byteCount = 0;
        
        // Array used to hold the occurrences of each byte
        // (useful for the iterative analysis)
        std::array<ImU64, 256> m_blockValueCounts = {};

        // Variable to hold the result of the chunk-based
        // entropy analysis
        std::vector<double> m_xBlockEntropy;
        std::vector<double> m_yBlockEntropy, m_yBlockEntropySampled;

        // Sampling size, number of elements displayed in the plot,
        // avoid showing to many data because it decreased the frame rate
        size_t m_sampleSize = 0;

        std::atomic<bool> m_processing = false;
    };

    class DiagramByteDistribution {
    public:
        DiagramByteDistribution() = default;

        void draw(ImVec2 size, ImPlotFlags flags) {

            if (!this->m_processing && ImPlot::BeginPlot("##distribution", size, flags)) {
                ImPlot::SetupAxes("hex.builtin.common.value"_lang, "hex.builtin.common.count"_lang,
                                  ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch,
                                  ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch);
                ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                ImPlot::SetupAxesLimits(-1, 256, 1, double(*std::max_element(this->m_valueCounts.begin(), this->m_valueCounts.end())) * 1.1F, ImGuiCond_Always);
                ImPlot::SetupAxisFormat(ImAxis_X1, impl::IntegerAxisFormatter, (void*)("0x%02llX"));
                ImPlot::SetupAxisTicks(ImAxis_X1, 0, 255, 17);
                ImPlot::SetupMouseText(ImPlotLocation_NorthEast);

                constexpr static auto x = [] {
                    std::array<ImU64, 256> result { 0 };
                    std::iota(result.begin(), result.end(), 0);
                    return result;
                }();

                ImPlot::PlotBars<ImU64>("##bytes", x.data(), this->m_valueCounts.data(), x.size(), 1);
                ImPlot::EndPlot();
            }
        }

        void process(prv::Provider *provider, u64 startAddress, u64 endAddress) {
            this->m_processing = true;

            // Update attributes  
            this->m_startAddress = startAddress;
            this->m_endAddress   = endAddress;

            // Get a file reader
            auto reader = prv::ProviderReader(provider);
            std::vector<u8> bytes = reader.read(this->m_startAddress, this->m_endAddress - this->m_startAddress);

            this->processImpl(bytes);

            this->m_processing = false;
        }

        void process(const std::vector<u8> &buffer) {
            this->m_processing = true;

            // Update attributes  
            this->m_startAddress = 0;
            this->m_endAddress   = buffer.size();

            this->processImpl(buffer);

            this->m_processing = false;
        }

    // Reset the byte distribution array
    void reset() {
        this->m_processing = true;
        this->m_valueCounts.fill(0);
        this->m_processing = false;          
    }

    // Process one byte at the time
    void update(u8 byte) {
        this->m_processing = true;
        this->m_valueCounts[byte]++;
        this->m_processing = false;           
    }

    // Return byte distribution array in it's current state 
    std::array<ImU64, 256> & get() {
        return this->m_valueCounts;
    }

    private:
        // Private method used to factorize the process public method 
        void processImpl(const std::vector<u8> &bytes) {
            // Reset the array
            this->m_valueCounts.fill(0);
            // Loop over each byte of the file (or a part of it)
            // Increment the occurrence of the current byte
            for (u8 byte : bytes) 
                this->m_valueCounts[byte]++;
        }

    private:
        // Variables used to store the parameters to process 
        u64 m_startAddress = 0;
        u64 m_endAddress = 0;

        // Hold the result of the byte distribution analysis 
        std::array<ImU64, 256> m_valueCounts;
        std::atomic<bool> m_processing = false;
    };

    class DiagramByteTypesDistribution {
    public:
        explicit DiagramByteTypesDistribution(u64 blockSize = 256, size_t sampleSize = 0x1000) : m_blockSize(blockSize), m_sampleSize(sampleSize){ }

        void draw(ImVec2 size, ImPlotFlags flags, bool updateHandle = false) {
            // Draw the result of the analysis
            if (!this->m_processing && ImPlot::BeginPlot("##byte_types", size, flags)) {
                ImPlot::SetupAxes("hex.builtin.common.address"_lang, "hex.builtin.common.percentage"_lang,
                                  ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch,
                                  ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_NoSideSwitch);
                ImPlot::SetupAxesLimits(
                        this->m_xBlockTypeDistributions.empty() ? 0 : this->m_xBlockTypeDistributions.front(),
                        this->m_xBlockTypeDistributions.empty() ? 0 : this->m_xBlockTypeDistributions.back(),
                        -0.1F,
                        100.1F,
                        ImGuiCond_Always);
                ImPlot::SetupLegend(ImPlotLocation_South, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
                ImPlot::SetupAxisFormat(ImAxis_X1, impl::IntegerAxisFormatter, (void*)("0x%04llX"));
                ImPlot::SetupMouseText(ImPlotLocation_NorthEast);

                constexpr static std::array Names = { "iscntrl", "isprint", "isspace", "isblank", 
                                                      "isgraph", "ispunct", "isalnum", "isalpha", 
                                                      "isupper", "islower", "isdigit", "isxdigit" 
                                                    };

                for (u32 i = 0; i < Names.size(); i++) {
                    ImPlot::PlotLine(Names[i], this->m_xBlockTypeDistributions.data(), this->m_yBlockTypeDistributionsSampled[i].data(), this->m_xBlockTypeDistributions.size());
                }

                // The parameter updateHandle is used when using the pattern language since we don't have a provider 
                // but just a set of bytes, we won't be able to use the drag bar correctly.
                if (updateHandle) {
                    // Set a draggable line on the plot
                    if (ImPlot::DragLineX(1, &this->m_handlePosition, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        // The line was dragged, update the position in the hex editor

                        // Clamp the value between the start/end of the region to analyze
                        this->m_handlePosition = std::clamp<double>(
                                this->m_handlePosition,
                                this->m_startAddress,
                                this->m_endAddress);

                        // Compute the position inside hex editor 
                        u64 address = u64(std::max<double>(this->m_handlePosition, 0)) + this->m_baseAddress;
                        address     = std::min<u64>(address, this->m_baseAddress + this->m_fileSize - 1);
                        ImHexApi::HexEditor::setSelection(address, 1);
                    }
                }
                ImPlot::EndPlot();
            }
        }

        void process(prv::Provider *provider, u64 startAddress, u64 endAddress) {
            this->m_processing = true;

            // Update attributes  
            this->m_startAddress = startAddress;
            this->m_endAddress   = endAddress;
            this->m_baseAddress  = provider->getBaseAddress();
            this->m_fileSize     = provider->getSize();

            // Get a file reader
            auto reader = prv::ProviderReader(provider);
            std::vector<u8> bytes = reader.read(this->m_startAddress, this->m_endAddress - this->m_startAddress);

            this->processImpl(bytes);

            // Set the diagram handle position to the start of the plot
            this->m_handlePosition = this->m_startAddress;

            this->m_processing = false;
        }

        void process(const std::vector<u8> &buffer, u64 baseAddress, u64 fileSize) {
            this->m_processing = true;

            // Update attributes  
            this->m_startAddress = 0;
            this->m_endAddress   = buffer.size();
            this->m_baseAddress  = baseAddress;
            this->m_fileSize     = fileSize;

            this->processImpl(buffer);

            // Set the diagram handle position to the start of the plot
            this->m_handlePosition = this->m_startAddress;

            this->m_processing = false;
        }

        // Reset the byte type distribution analysis
        void reset(u64 startAddress, u64 endAddress, u64 baseAddress, u64 size) {
            this->m_processing = true;

            // Update attributes  
            this->m_startAddress = startAddress;
            this->m_endAddress   = endAddress;
            this->m_baseAddress  = baseAddress; 
            this->m_fileSize     = size;

            this->m_byteCount = 0;
            this->m_blockCount = 0;
            this->m_blockValueCounts = { 0 };

            // Reset and resize the array
            this->m_yBlockTypeDistributions.fill({});

            // Set the diagram handle position to the start of the plot
            this->m_handlePosition = this->m_startAddress;
        }

        // Process one byte at the time
        void update(u8 byte) {
            u64 totalBlock = std::ceil((this->m_endAddress - this->m_startAddress) / this->m_blockSize);
            // Check if there is still some block to process 
            if (this->m_blockCount < totalBlock) {

                this->m_blockValueCounts[byte]++;

                this->m_byteCount++;
                if (((this->m_byteCount % this->m_blockSize) == 0) || this->m_byteCount == (this->m_endAddress - this->m_startAddress)) [[unlikely]] {
                    auto typeDist = calculateTypeDistribution(this->m_blockValueCounts, this->m_blockSize);
                    for (size_t i = 0; i < typeDist.size(); i++)
                        this->m_yBlockTypeDistributions[i].push_back(typeDist[i] * 100);

                    this->m_blockCount += 1;
                    this->m_blockValueCounts = { 0 };
                }
                
                // Check if we processed the last block, if so setup the X axis part of the data
                if (this->m_blockCount == totalBlock) {

                    processFinalize();
                    this->m_processing = false;           
                }
            }
        }

        // Return the percentage of plain text character inside the analyzed region
        double getPlainTextCharacterPercentage() {
            if (this->m_yBlockTypeDistributions[2].empty() || this->m_yBlockTypeDistributions[4].empty())
                return -1.0;


            double plainTextPercentage = std::reduce(this->m_yBlockTypeDistributions[2].begin(), this->m_yBlockTypeDistributions[2].end()) / this->m_yBlockTypeDistributions[2].size();
            return plainTextPercentage + std::reduce(this->m_yBlockTypeDistributions[4].begin(), this->m_yBlockTypeDistributions[4].end()) / this->m_yBlockTypeDistributions[4].size();
        }

        void setHandlePosition(u64 filePosition) {
            this->m_handlePosition = filePosition;
        }

    private:
        std::array<float, 12> calculateTypeDistribution(const std::array<ImU64, 256> &valueCounts, size_t blockSize) const {
            std::array<ImU64, 12> counts = {};

            for (u16 value = 0x00; value < u16(valueCounts.size()); value++) {
                const auto &count = valueCounts[value];

                if (count == 0) [[unlikely]]
                    continue;

                if (std::iscntrl(value))
                    counts[0] += count;
                if (std::isprint(value))
                    counts[1] += count;
                if (std::isspace(value))
                    counts[2] += count;
                if (std::isblank(value))
                    counts[3] += count;
                if (std::isgraph(value))
                    counts[4] += count;
                if (std::ispunct(value))
                    counts[5] += count;
                if (std::isalnum(value))
                    counts[6] += count;
                if (std::isalpha(value))
                    counts[7] += count;
                if (std::isupper(value))
                    counts[8] += count;
                if (std::islower(value))
                    counts[9] += count;
                if (std::isdigit(value))
                    counts[10] += count;
                if (std::isxdigit(value))
                    counts[11] += count;
            }

            std::array<float, 12> distribution = {};
            for (u32 i = 0; i < distribution.size(); i++)
                distribution[i] = static_cast<float>(counts[i]) / blockSize;

            return distribution;
        }

        // Private method used to factorize the process public method 
        void processImpl(const std::vector<u8> &bytes) {
            this->m_blockValueCounts = { 0 };

            this->m_yBlockTypeDistributions.fill({});
            this->m_byteCount = 0;
            this->m_blockCount = 0;

            // Loop over each byte of the file (or a part of it)
            for (u8 byte : bytes) {
                this->m_blockValueCounts[byte]++;

                this->m_byteCount++;
                if (((this->m_byteCount % this->m_blockSize) == 0) || this->m_byteCount == (this->m_endAddress - this->m_startAddress)) [[unlikely]] {
                    auto typeDist = calculateTypeDistribution(this->m_blockValueCounts, this->m_blockSize);
                    for (size_t i = 0; i < typeDist.size(); i++)
                        this->m_yBlockTypeDistributions[i].push_back(typeDist[i] * 100);

                    this->m_blockCount += 1;
                    this->m_blockValueCounts = { 0 };
                }
            }

            processFinalize();
        }

        void processFinalize() {
            // Only save at most m_sampleSize elements of the result
            for (size_t i = 0; i < this->m_yBlockTypeDistributions.size(); ++i) {
                this->m_yBlockTypeDistributionsSampled[i] = sampleData(this->m_yBlockTypeDistributions[i], std::min<size_t>(this->m_blockCount + 1, this->m_sampleSize));

                if (!this->m_yBlockTypeDistributionsSampled[i].empty())
                    this->m_yBlockTypeDistributionsSampled[i].push_back(this->m_yBlockTypeDistributionsSampled[i].back());
            }

            double stride = std::max(1.0, double(this->m_blockCount) / this->m_yBlockTypeDistributionsSampled[0].size());
            this->m_blockCount = this->m_yBlockTypeDistributionsSampled[0].size() - 1;

            // The m_xBlockTypeDistributions attribute is used to specify the position of entropy 
            // values in the plot when the Y axis doesn't start at 0
            this->m_xBlockTypeDistributions.clear();
            this->m_xBlockTypeDistributions.resize(this->m_blockCount);
            for (u64 i = 0; i < this->m_blockCount; ++i)
                this->m_xBlockTypeDistributions[i] = this->m_startAddress + (stride * i * this->m_blockSize);
            this->m_xBlockTypeDistributions.push_back(this->m_endAddress);
        }

    private:
        // Variables used to store the parameters to process

        // The size of the block we are considering for the analysis
        u64 m_blockSize = 0;
        u64 m_startAddress = 0;
        u64 m_endAddress = 0;
        // Start / size of the file
        u64 m_baseAddress = 0;
        u64 m_fileSize = 0;
 
        // Position of the handle inside the plot
        double m_handlePosition = 0.0;    

        // Hold the number of blocks that have been processed
        // during the chunk-based entropy analysis
        u64 m_blockCount = 0;

        // Hold the number of bytes that have been processed 
        // during the analysis (useful for the iterative analysis)
        u64 m_byteCount = 0;
 
        // Sampling size, number of elements displayed in the plot,
        // avoid showing to many data because it decreased the frame rate
        size_t m_sampleSize = 0;

        // Array used to hold the occurrences of each byte
        // (useful for the iterative analysis)
        std::array<ImU64, 256> m_blockValueCounts = {};

        // The m_xBlockTypeDistributions attributes are used to specify the position of
        // the values in the plot when the Y axis doesn't start at 0 
        std::vector<float> m_xBlockTypeDistributions;
        // Hold the result of the byte distribution analysis 
        std::array<std::vector<float>, 12> m_yBlockTypeDistributions, m_yBlockTypeDistributionsSampled;
        std::atomic<bool> m_processing = false;
    };
}
