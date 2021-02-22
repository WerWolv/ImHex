#pragma once

#include <hex/views/view.hpp>

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
        explicit ViewStrings();
        ~ViewStrings() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        bool m_searching = false;

        std::vector<FoundString> m_foundStrings;
        int m_minimumLength = 5;
        std::vector<char> m_filter;

        std::string m_selectedString;
        std::string m_demangledName;

        void searchStrings();
        void createStringContextMenu(const FoundString &foundString);
    };

}