#pragma once

#include "views/view.hpp"

#include <helpers/content_registry.hpp>

#include <bit>
#include <cstdio>
#include <ctime>
#include <string>

namespace hex {

    namespace prv { class Provider; }

    struct GUID {
        u32 data1;
        u16 data2;
        u16 data3;
        u8  data4[8];
    };

    class ViewDataInspector : public View {
    public:
        explicit ViewDataInspector();
        ~ViewDataInspector() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        bool m_shouldInvalidate = true;

        std::endian m_endian = std::endian::native;
        ContentRegistry::DataInspector::NumberDisplayStyle m_numberDisplayStyle =ContentRegistry::DataInspector::NumberDisplayStyle::Decimal;

        u64 m_startAddress = 0;
        size_t m_validBytes = 0;
        std::vector<std::pair<std::string, ContentRegistry::DataInspector::DisplayFunction>> m_cachedData;
    };

}