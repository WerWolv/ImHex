#include <hex/helpers/clipboard.hpp>

#if __has_include(<clip.h>)
    #define CLIP_LIBRARY
#endif

#if defined(CLIP_LIBRARY)
    #include <clip.h>
#else
    #include <imgui.h>
    #include <fmt/color.h>
    #include <hex/helpers/utils.hpp>
#endif

namespace hex::clipboard {

#if defined(CLIP_LIBRARY)

    static clip::format s_binaryFormat;
    static clip::format s_textFormat;

    void init() {
        s_binaryFormat = clip::register_format("net.werwolv.imhex.binary");
        s_textFormat   = clip::text_format();
    }

    void setBinaryData(std::span<const u8> data) {
        clip::lock l;
        l.set_data(s_binaryFormat, reinterpret_cast<const char*>(data.data()), data.size());
    }

    std::vector<u8> getBinaryData() {
        clip::lock l;

        const auto size = l.get_data_length(s_binaryFormat);
        std::vector<u8> data(size);
        l.get_data(s_binaryFormat, reinterpret_cast<char*>(data.data()), size);

        return data;
    }

    void setTextData(const std::string &string) {
        clip::lock l;
        l.set_data(s_textFormat, string.data(), string.size());
    }

    std::string getTextData() {
        clip::lock l;

        const auto size = l.get_data_length(s_binaryFormat);
        std::string data(size, 0x00);
        l.get_data(s_textFormat, data.data(), size);

        return data;
    }

#else

    void init() {}

    void setBinaryData(std::span<const u8> data) {
        constexpr static auto Format = "{0:02X} ";
        std::string result;
        result.reserve(fmt::format(Format, 0x00).size() * data.size_bytes());

        for (const auto &byte : data)
            result += fmt::format(Format, byte);
        result.pop_back();

        ImGui::SetClipboardText(result.c_str());
    }

    std::vector<u8> getBinaryData() {
        auto clipboard = ImGui::GetClipboardText();
        if (clipboard == nullptr)
            return {};

        return parseHexString(clipboard);
    }

    void setTextData(const std::string &string) {
        ImGui::SetClipboardText(string.c_str());
    }

    std::string getTextData() {
        return ImGui::GetClipboardText();
    }

#endif

}
