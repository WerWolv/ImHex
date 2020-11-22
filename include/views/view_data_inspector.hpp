#pragma once

#include "views/view.hpp"

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

    union PreviewData {
        u8 unsigned8;
        s8 signed8;
        u16 unsigned16;
        s16 signed16;
        u32 unsigned32;
        s32 signed32;
        u64 unsigned64;
        s64 signed64;
        char8_t ansiChar;
        char16_t wideChar;
        u8 utf8Char[4];
        float float32;
        double float64;
        #if defined(_WIN64)
            __time32_t time32;
            __time64_t time64;
        #else
            time_t time;
        #endif
        GUID guid;
    };

    class ViewDataInspector : public View {
    public:
        explicit ViewDataInspector(prv::Provider* &dataProvider);
        ~ViewDataInspector() override;

        void createView() override;
        void createMenu() override;

    private:
        prv::Provider* &m_dataProvider;
        bool m_windowOpen = true;
        bool m_shouldInvalidate = true;

        std::endian m_endianess = std::endian::native;

        PreviewData m_previewData = { 0 };
        size_t m_validBytes = 0;
        std::vector<std::pair<std::string, std::string>> m_cachedData;
    };

}