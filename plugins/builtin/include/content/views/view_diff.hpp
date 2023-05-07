#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>
#include <hex/api/task.hpp>

#include <array>
#include <string>
#include <vector>

#include "ui/hex_editor.hpp"

#include <IITree.h>

namespace hex::plugin::builtin {

    class ViewDiff : public View {
    public:
        ViewDiff();
        ~ViewDiff() override;

        void drawContent() override;

    public:
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

    private:
        std::array<Column, 2> m_columns;

        std::vector<Diff> m_diffs;
        TaskHolder m_diffTask;
        std::atomic<bool> m_analyzed = false;
    };

}