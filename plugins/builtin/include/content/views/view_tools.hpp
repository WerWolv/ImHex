#pragma once

#include <hex/api/content_registry.hpp>
#include <hex/ui/view.hpp>

#include <vector>

namespace hex::plugin::builtin {

    class ViewTools : public View::Window {
    public:
        ViewTools();
        ~ViewTools() override = default;

        void drawContent() override;
        void drawAlwaysVisibleContent() override;

    private:
        std::vector<const ContentRegistry::Tools::impl::Entry>::iterator m_dragStartIterator;

        std::map<ImGuiWindow*, float> m_windowHeights;
        std::map<UnlocalizedString, bool> m_detachedTools;
    };

}