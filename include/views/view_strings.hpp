#pragma once

#include "views/view.hpp"

#include <cstdio>
#include <string>

namespace hex {

    namespace prv { class Provider; }

    struct FoundString {
        std::string string;
        u64 offset;
        size_t size;
    };

    class ViewStrings : public View {
    public:
        explicit ViewStrings(prv::Provider* &dataProvider);
        ~ViewStrings() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        prv::Provider* &m_dataProvider;
        bool m_shouldInvalidate = false;

        std::vector<FoundString> m_foundStrings;
        int m_minimumLength = 5;
        char *m_filter;

        std::string m_selectedString;
        std::string m_demangledName;

        void createStringContextMenu(const FoundString &foundString);
    };

}