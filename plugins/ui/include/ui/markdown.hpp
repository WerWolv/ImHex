#pragma once

#include <hex.hpp>
#include <string>
#include <future>
#include <md4c.h>
#include <wolv/container/lazy.hpp>

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
        std::map<u32, std::future<wolv::container::Lazy<ImGuiExt::Texture>>> m_futureImages;
        std::map<u32, ImGuiExt::Texture> m_images;
        std::function<std::vector<u8>(const std::string &)> m_romfsFileReader;
    };

}