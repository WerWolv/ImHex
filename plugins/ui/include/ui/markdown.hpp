#pragma once

#include <hex.hpp>
#include <string>
#include <md4c.h>

namespace hex::ui {

    class Markdown {
    public:
        Markdown() = default;
        Markdown(const std::string &text);

        void draw();

    private:
        bool inTable() const;
        std::string getElementId();
    private:
        std::string m_text;
        MD_RENDERER m_mdRenderer;
        u32 m_elementId = 1;
        std::string m_currentLink;
        bool m_drawingImageAltText = false;
        u32 m_listIndent = 0;
        std::vector<u8> m_tableVisibleStack;
    };

}