#pragma once

#include <hex/api/content_registry/tools.hpp>
#include <hex/ui/view.hpp>

#include <vector>

namespace hex::plugin::builtin {

    class ViewTools : public View::Scrolling {
    public:
        ViewTools();
        ~ViewTools() override = default;

        void drawContent() override;
        void drawAlwaysVisibleContent() override;
        void drawHelpText() override;

    private:
        std::vector<ContentRegistry::Tools::impl::Entry>::const_iterator m_dragStartIterator;

        std::map<ImGuiWindow*, float> m_windowHeights;
        std::map<UnlocalizedString, bool> m_detachedTools;
        std::map<UnlocalizedString, bool> m_collapsedTools;
    };

}