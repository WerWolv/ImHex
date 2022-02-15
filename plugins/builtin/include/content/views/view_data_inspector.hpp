#pragma once

#include <hex/ui/view.hpp>

#include <hex/api/content_registry.hpp>

#include <bit>
#include <cstdio>
#include <string>

namespace hex::plugin::builtin {

    namespace prv {
        class Provider;
    }

    class ViewDataInspector : public View {
    public:
        explicit ViewDataInspector();
        ~ViewDataInspector() override;

        void drawContent() override;

    private:
        struct InspectorCacheEntry {
            std::string unlocalizedName;
            ContentRegistry::DataInspector::impl::DisplayFunction displayFunction;
            std::optional<ContentRegistry::DataInspector::impl::EditingFunction> editingFunction;
            bool editing;
        };

        bool m_shouldInvalidate = true;

        std::endian m_endian                                                    = std::endian::native;
        ContentRegistry::DataInspector::NumberDisplayStyle m_numberDisplayStyle = ContentRegistry::DataInspector::NumberDisplayStyle::Decimal;

        u64 m_startAddress  = 0;
        size_t m_validBytes = 0;
        std::vector<InspectorCacheEntry> m_cachedData;

        std::string m_editingValue;
    };

}