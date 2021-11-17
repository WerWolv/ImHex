#pragma once

#include <hex/views/view.hpp>

#include <cstdio>
#include <string>

namespace hex {

    namespace prv { class Provider; }

    struct FoundString {
        u64 offset;
        size_t size;
    };

    class ViewStrings : public View {
    public:
        explicit ViewStrings();
        ~ViewStrings() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        bool m_searching = false;
        bool m_regex = false;

        std::vector<FoundString> m_foundStrings;
        std::vector<size_t> m_filterIndices;
        int m_minimumLength = 5;
        std::string m_filter;

        std::string m_selectedString;
        std::string m_demangledName;

        void searchStrings();
        void createStringContextMenu(const FoundString &foundString);
    };

}