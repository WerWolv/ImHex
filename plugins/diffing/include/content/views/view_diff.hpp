#pragma once

#include <hex.hpp>

#include <hex/ui/view.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/content_registry/diffing.hpp>

#include <array>
#include <vector>

#include "ui/hex_editor.hpp"

namespace hex::plugin::diffing {

    class ViewDiff : public View::Window {
    public:
        ViewDiff();
        ~ViewDiff() override;

        void drawContent() override;
        void drawAlwaysVisibleContent() override;
        void drawHelpText() override;

    public:
        struct Column {
            ui::HexEditor hexEditor;
            ContentRegistry::Diffing::DiffTree diffTree;
            std::vector<ContentRegistry::Diffing::DiffTree::Data> differences;

            int provider = -1;
            i32 scrollLock = 0;
        };

    private:
        std::function<std::optional<color_t>(u64, const u8*, size_t)> createCompareFunction(size_t otherIndex) const;
        void analyze(prv::Provider *providerA, prv::Provider *providerB);

        void registerMenuItems();

        void reset();

    private:
        std::array<Column, 2> m_columns;

        TaskHolder m_diffTask;
        std::atomic<bool> m_analyzed = false;
        std::atomic<bool> m_analysisInterrupted = false;
        ContentRegistry::Diffing::Algorithm *m_algorithm = nullptr;

        u64 m_selectedAddress  = 0;
        prv::Provider *m_selectedProvider = nullptr;
    };

}
