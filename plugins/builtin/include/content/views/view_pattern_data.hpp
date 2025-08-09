#pragma once

#include <hex/ui/view.hpp>

#include <ui/pattern_drawer.hpp>

namespace hex::plugin::builtin {

    class ViewPatternData : public View::Window {
    public:
        ViewPatternData();
        ~ViewPatternData() override;

        void drawContent() override;

        View* getMenuItemInheritView() const override {
            return ContentRegistry::Views::getViewByName("hex.builtin.view.pattern_editor.name");
        }

    private:
        bool m_rowColoring = false;
        u32 m_maxFilterItems = 128;
        ui::PatternDrawer::TreeStyle m_treeStyle = ui::PatternDrawer::TreeStyle::Default;

        PerProvider<std::map<u64, std::unique_ptr<ui::PatternDrawer>>> m_patternDrawer;
        Region m_hoveredPatternRegion = Region::Invalid();
        ui::PatternValueEditor m_patternValueEditor;
    };

}