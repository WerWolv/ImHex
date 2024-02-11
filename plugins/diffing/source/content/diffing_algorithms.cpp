#include <hex.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/literals.hpp>

#include <edlib.h>
#include <imgui.h>
#include <hex/api/task_manager.hpp>

namespace hex::plugin::diffing {

    using namespace ContentRegistry::Diffing;
    using namespace wolv::literals;

    class AlgorithmSimple : public Algorithm {
    public:
        AlgorithmSimple() : Algorithm("hex.diffing.algorithm.simple.name", "hex.diffing.algorithm.simple.description") {}

        [[nodiscard]] std::vector<DiffTree> analyze(prv::Provider *providerA, prv::Provider *providerB) const override {
            wolv::container::IntervalTree<DifferenceType> differences;

            // Set up readers for both providers
            auto readerA = prv::ProviderReader(providerA);
            auto readerB = prv::ProviderReader(providerB);

            auto &task = TaskManager::getCurrentTask();

            // Iterate over both providers and compare the bytes
            for (auto itA = readerA.begin(), itB = readerB.begin(); itA < readerA.end() && itB < readerB.end(); ++itA, ++itB) {
                // Stop comparing if the diff task was canceled
                if (task.wasInterrupted())
                    break;

                // If the bytes are different, find the end of the difference
                if (*itA != *itB) {
                    u64 start = itA.getAddress();
                    size_t size = 0;

                    while (itA != readerA.end() && itB != readerB.end() && *itA != *itB) {
                        ++itA;
                        ++itB;
                        ++size;
                    }

                    // Add the difference to the list
                    differences.emplace({ start, (start + size) - 1 }, DifferenceType::Mismatch);
                }

                // Update the progress bar
                task.update(itA.getAddress());
            }

            auto otherDifferences = differences;

            // If one provider is larger than the other, add the extra bytes to the list
            if (providerA->getActualSize() != providerB->getActualSize()) {
                auto endA = providerA->getActualSize() + 1;
                auto endB = providerB->getActualSize() + 1;

                if (endA > endB) {
                    differences.emplace({ endB, endA }, DifferenceType::Insertion);
                    otherDifferences.emplace({ endB, endA }, DifferenceType::Deletion);
                }
                else {
                    differences.emplace({ endA, endB }, DifferenceType::Insertion);
                    otherDifferences.emplace({ endB, endA }, DifferenceType::Insertion);
                }
            }

            return { differences, otherDifferences };
        }
    };

    class AlgorithmMyers : public Algorithm {
    public:
        AlgorithmMyers() : Algorithm("hex.diffing.algorithm.myers.name", "hex.diffing.algorithm.myers.description") {}

        [[nodiscard]] std::vector<DiffTree> analyze(prv::Provider *providerA, prv::Provider *providerB) const override {
            DiffTree differencesA, differencesB;

            EdlibAlignConfig edlibConfig;
            edlibConfig.k = -1;
            edlibConfig.additionalEqualities = nullptr;
            edlibConfig.additionalEqualitiesLength = 0;
            edlibConfig.mode = EdlibAlignMode::EDLIB_MODE_NW;
            edlibConfig.task = EdlibAlignTask::EDLIB_TASK_PATH;

            const auto providerAStart = providerA->getBaseAddress();
            const auto providerBStart = providerB->getBaseAddress();
            const auto providerAEnd = providerAStart + providerA->getActualSize();
            const auto providerBEnd = providerBStart + providerB->getActualSize();

            const auto windowStart = std::max(providerAStart, providerBStart);
            const auto windowEnd   = std::min(providerAEnd, providerBEnd);

            auto &task = TaskManager::getCurrentTask();

            if (providerAStart > providerBStart) {
                differencesA.insert({ providerBStart, providerAStart }, DifferenceType::Deletion);
                differencesB.insert({ providerBStart, providerAStart }, DifferenceType::Deletion);
            } else if (providerAStart < providerBStart) {
                differencesA.insert({ providerAStart, providerBStart }, DifferenceType::Insertion);
                differencesB.insert({ providerAStart, providerBStart }, DifferenceType::Insertion);
            }

            for (u64 address = windowStart; address < windowEnd; address += m_windowSize) {
                if (task.wasInterrupted())
                    break;

                auto currWindowSizeA = std::min<u64>(m_windowSize, providerA->getActualSize() - address);
                auto currWindowSizeB = std::min<u64>(m_windowSize, providerB->getActualSize() - address);
                std::vector<u8> dataA(currWindowSizeA, 0x00), dataB(currWindowSizeB, 0x00);

                providerA->read(address, dataA.data(), dataA.size());
                providerB->read(address, dataB.data(), dataB.size());

                const auto commonSize = std::min(dataA.size(), dataB.size());
                EdlibAlignResult result = edlibAlign(
                    reinterpret_cast<const char*>(dataA.data()), commonSize,
                    reinterpret_cast<const char*>(dataB.data()), commonSize,
                    edlibConfig
                );

                auto currentOperation = DifferenceType(0xFF);
                Region regionA = {}, regionB = {};
                u64 currentAddressA = address, currentAddressB = address;

                const auto insertDifference = [&] {
                    switch (currentOperation) {
                        using enum DifferenceType;

                        case Match:
                            break;
                        case Mismatch:
                            differencesA.insert({ regionA.getStartAddress(), regionA.getEndAddress() }, Mismatch);
                            differencesB.insert({ regionB.getStartAddress(), regionB.getEndAddress() }, Mismatch);
                            break;
                        case Insertion:
                            differencesA.insert({ regionA.getStartAddress(), regionA.getEndAddress() }, Insertion);
                            differencesB.insert({ regionB.getStartAddress(), regionB.getEndAddress() }, Insertion);
                            currentAddressB -= regionA.size;
                            break;
                        case Deletion:
                            differencesA.insert({ regionA.getStartAddress(), regionA.getEndAddress() }, Deletion);
                            differencesB.insert({ regionB.getStartAddress(), regionB.getEndAddress() }, Deletion);
                            currentAddressA -= regionB.size;
                            break;
                    }
                };

                for (const u8 alignmentType : std::span(result.alignment, result.alignmentLength)) {
                    ON_SCOPE_EXIT {
                        currentAddressA++;
                        currentAddressB++;
                    };

                    if (currentOperation == DifferenceType(alignmentType)) {
                        regionA.size++;
                        regionB.size++;

                        continue;
                    } else if (currentOperation != DifferenceType(0xFF)) {
                        insertDifference();

                        currentOperation = DifferenceType(0xFF);
                    }

                    currentOperation = DifferenceType(alignmentType);
                    regionA.address = currentAddressA;
                    regionB.address = currentAddressB;
                    regionA.size = 1;
                    regionB.size = 1;
                }

                insertDifference();

                task.update(address);
            }

            if (providerAEnd > providerBEnd) {
                differencesA.insert({ providerBEnd, providerAEnd }, DifferenceType::Insertion);
                differencesB.insert({ providerBEnd, providerAEnd }, DifferenceType::Insertion);
            } else if (providerAEnd < providerBEnd) {
                differencesA.insert({ providerAEnd, providerBEnd }, DifferenceType::Deletion);
                differencesB.insert({ providerAEnd, providerBEnd }, DifferenceType::Deletion);
            }

            return { differencesA, differencesB };
        }

        void drawSettings() override {
            static u64 min = 32_kiB, max = 128_kiB;
            ImGui::SliderScalar("hex.diffing.algorithm.myers.settings.window_size"_lang, ImGuiDataType_U64, &m_windowSize, &min, &max, "0x%X");
        }

    private:
        u64 m_windowSize = 64_kiB;
    };

    void registerDiffingAlgorithms() {
        ContentRegistry::Diffing::addAlgorithm<AlgorithmSimple>();
        ContentRegistry::Diffing::addAlgorithm<AlgorithmMyers>();
    }

}
