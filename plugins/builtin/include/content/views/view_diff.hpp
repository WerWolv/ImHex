#pragma once

#include <hex.hpp>

#include <hex/ui/view.hpp>
#include <hex/api/task_manager.hpp>

#include <array>
#include <vector>

#include "ui/hex_editor.hpp"

namespace hex::plugin::builtin {

    class ViewDiff : public View::Window {
    public:
        ViewDiff();
        ~ViewDiff() override;

        void drawContent() override;
        ImGuiWindowFlags getWindowFlags() const override { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; }

    public:
        enum class DifferenceType : u8 {
            Match       = 0,
            Insertion   = 1,
            Deletion    = 2,
            Mismatch    = 3
        };

        using DiffTree = wolv::container::IntervalTree<DifferenceType>;
        struct Column {
            ui::HexEditor hexEditor;
            DiffTree diffTree;

            int provider = -1;
            i32 scrollLock = 0;
        };

        class Algorithm {
        public:
            Algorithm() = default;
            virtual ~Algorithm() = default;

            virtual const char* getName() const = 0;

            virtual std::vector<DiffTree> analyze(Task &task, prv::Provider *providerA, prv::Provider *providerB) = 0;
        };

    private:
        std::function<std::optional<color_t>(u64, const u8*, size_t)> createCompareFunction(size_t otherIndex) const;
        void analyze(prv::Provider *providerA, prv::Provider *providerB);

    private:
        std::array<Column, 2> m_columns;

        TaskHolder m_diffTask;
        std::atomic<bool> m_analyzed = false;
        std::unique_ptr<Algorithm> m_algorithm;
    };

}