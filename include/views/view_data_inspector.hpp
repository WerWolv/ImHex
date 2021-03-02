#pragma once

#include <hex/views/view.hpp>

#include <hex/api/content_registry.hpp>

#include <bit>
#include <cstdio>
#include <string>

namespace hex {

    namespace prv { class Provider; }

    class ViewDataInspector : public View {
    public:
        explicit ViewDataInspector();
        ~ViewDataInspector() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        struct InspectorCacheEntry {
            std::string unlocalizedName;
            ContentRegistry::DataInspector::DisplayFunction displayFunction;
        };

        bool m_shouldInvalidate = true;

        std::endian m_endian = std::endian::native;
        ContentRegistry::DataInspector::NumberDisplayStyle m_numberDisplayStyle =ContentRegistry::DataInspector::NumberDisplayStyle::Decimal;

        u64 m_startAddress = 0;
        size_t m_validBytes = 0;
        std::vector<InspectorCacheEntry> m_cachedData;
    };

}