#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>
#include <hex/api/task.hpp>

#include <array>
#include <string>
#include <vector>

#include "ui/hex_editor.hpp"

#include <IntervalTree.h>

namespace hex::plugin::builtin {

    class ViewDiff : public View {
    public:
        ViewDiff();
        ~ViewDiff() override;

        void drawContent() override;

    private:
        struct Column {
            ui::HexEditor hexEditor;
            int provider = -1;
            i32 scrollLock = 0;
        };

        enum class DifferenceType : u8 {
            Added,
            Removed,
            Modified
        };

        struct Diff {
            Region region;
            DifferenceType type;
        };

        bool drawDiffColumn(Column &column, float height) const;
        void drawProviderSelector(Column &column);
        std::string getProviderName(Column &column) const;

    private:
        std::array<Column, 2> m_columns;

        std::vector<Diff> m_diffs;
        TaskHolder m_diffTask;
        std::atomic<bool> m_analyzed = false;
    };

}