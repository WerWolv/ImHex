#pragma once

#include <hex/api/task_manager.hpp>

#include <hex/ui/view.hpp>
#include <ui/widgets.hpp>

#include <vector>
#include <hex/api/content_registry/disassemblers.hpp>

namespace hex::plugin::disasm {

    class ViewDisassembler : public View::Window {
    public:
        explicit ViewDisassembler();
        ~ViewDisassembler() override;

        void drawContent() override;
        void drawHelpText() override;

    private:
        struct FlowEdge {
            size_t source;
            size_t target;
            size_t lane;
            size_t sourceSlot;
            size_t sourceSlotCount;
            size_t targetSlot;
            size_t targetSlotCount;
        };

        TaskHolder m_disassemblerTask;

        PerProvider<u64> m_imageLoadAddress;
        PerProvider<u64> m_imageBaseAddress;
        PerProvider<ui::RegionType> m_range;
        PerProvider<Region> m_regionToDisassemble;

        PerProvider<std::unique_ptr<ContentRegistry::Disassemblers::Architecture>> m_currArchitecture;

        PerProvider<std::vector<ContentRegistry::Disassemblers::Instruction>> m_disassembly;
        PerProvider<std::vector<FlowEdge>> m_flowEdges;
        PerProvider<std::vector<size_t>> m_returnPrefix;
        PerProvider<std::optional<size_t>> m_selectedInstruction;
        PerProvider<Region> m_selectedRegion;
        PerProvider<bool> m_scrollToSelectedInstruction;
        PerProvider<bool> m_settingsCollapsed;

        void disassemble();
        void exportToFile();
        void updateSelection(prv::Provider *provider, const Region &region);
        static std::vector<FlowEdge> findFlowEdges(const std::vector<ContentRegistry::Disassemblers::Instruction> &disassembly);
    };

}
