#pragma once

#include <hex.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/ui/view.hpp>

#include <array>
#include <string>

namespace hex::plugin::builtin {

    class ViewTools : public View {
    public:
        ViewTools();
        ~ViewTools() override = default;

        void drawContent() override;

    private:
        std::vector<ContentRegistry::Tools::impl::Entry>::iterator m_dragStartIterator;
    };

}