#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <string>
#include <future>
#include <map>

#include <md4c.h>

#include <wolv/container/lazy.hpp>
#include <romfs/romfs.hpp>

namespace hex::ui {

    class Markdown {
    public:
        Markdown() = default;
        Markdown(std::string text);
        Markdown(const Markdown &) = delete;
        Markdown(Markdown &&other) = default;

        Markdown &operator=(const Markdown &) = delete;
        Markdown &operator=(Markdown &&other) = default;

        void draw();
        void reset();

        void setRomfsTextureLookupFunction(std::function<wolv::container::Lazy<ImGuiExt::Texture>(const std::string &)> romfsFileReader) {
            m_romfsFileReader = std::move(romfsFileReader);
        }

    private:
        bool inTable() const;
        std::string getElementId();

    private:
        std::string m_text;
        bool m_initialized = false;
        MD_RENDERER m_mdRenderer = {};
        bool m_firstLine = true;
        u32 m_elementId = 1;
        std::string m_currentLink;
        bool m_drawingImageAltText = false;
        u32 m_listIndent = 0;
        std::vector<u8> m_tableVisibleStack;

        std::map<u32, std::future<wolv::container::Lazy<ImGuiExt::Texture>>> m_futureImages;
        std::map<u32, ImGuiExt::Texture> m_images;
        std::function<wolv::container::Lazy<ImGuiExt::Texture>(const std::string &)> m_romfsFileReader;

        std::vector<ImVec2> m_quoteStarts;
        std::vector<u8> m_quoteNeedsChildEnd;
        bool m_quoteStart = false;
    };

}