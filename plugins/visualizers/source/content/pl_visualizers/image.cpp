#include <content/visualizer_helpers.hpp>

#include <hex/helpers/scaling.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::visualizers {
    std::vector<u32> getIndices(pl::ptrn::Pattern *colorTablePattern, u64 width, u64 height);
    ImGuiExt::Texture getTexture(pl::ptrn::Pattern *colorTablePattern, const std::vector<u32> &indices, u64 width, u64 height);


    void drawImageVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static AutoReset<ImGuiExt::Texture> texture;
        static float scale = 1.0F;

        if (shouldReset) {
            auto pattern  = arguments[0].toPattern();

            auto data = pattern->getBytes();
            texture = ImGuiExt::Texture::fromImage(data.data(), data.size(), ImGuiExt::Texture::Filter::Nearest);
            scale = 200_scaled / texture->getSize().x;
        }

        if (texture.isValid())
            ImGui::Image(*texture, texture->getSize() * scale);

        if (ImGui::IsWindowHovered()) {
            auto scrollDelta = ImGui::GetIO().MouseWheel;
            if (scrollDelta != 0.0F) {
                scale += scrollDelta * 0.1F;
                scale = std::clamp(scale, 0.1F, 10.0F);
            }
        }
    }

    void drawBitmapVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static AutoReset<ImGuiExt::Texture> texture;
        static float scale = 1.0F;

        if (shouldReset) {
            auto pattern  = arguments[0].toPattern();
            auto width    = u64(arguments[1].toUnsigned());
            auto height   = u64(arguments[2].toUnsigned());
            bool hasColorTable = false;

            if (arguments.size() == 4) {
                auto colorTablePattern = arguments[3].toPattern();

                if (colorTablePattern->getSize() > 0) {
                    auto indices = getIndices(pattern.get(), width, height);
                    texture = getTexture(colorTablePattern.get(), indices, width, height);
                    hasColorTable = true;
                }
            }

            if (!hasColorTable) {
                auto data = pattern->getBytes();
                texture = ImGuiExt::Texture::fromBitmap(data.data(), data.size(), width, height, ImGuiExt::Texture::Filter::Nearest);
            }
        }

        if (texture.isValid())
            ImGui::Image(*texture, texture->getSize() * scale);

        if (ImGui::IsWindowHovered()) {
            auto scrollDelta = ImGui::GetIO().MouseWheel;

            if (scrollDelta != 0.0F) {
                scale += scrollDelta * 0.1F;
                scale = std::clamp(scale, 0.1F, 10.0F);
            }
        }
    }

    ImGuiExt::Texture getTexture(pl::ptrn::Pattern *colorTablePattern, const std::vector<u32> &indices, u64 width, u64 height) {
        std::vector<u32> colorTable = patternToArray<u32>(colorTablePattern);
        auto colorCount = colorTable.size();
        auto indexCount = indices.size();
        std::vector<u32> image(indexCount);
        u32 *imageData = image.data();

        for (auto index : indices) {

            if (index >= colorCount)
                index = 0;
            *imageData++ = colorTable[index];
        }

        void *tmp = image.data();
        auto *data = static_cast<const ImU8 *>(tmp);
        ImGuiExt::Texture texture = ImGuiExt::Texture::fromBitmap(data, indexCount * 4, width, height, ImGuiExt::Texture::Filter::Nearest);
        return texture;
    }

    std::vector<u32> getIndices(pl::ptrn::Pattern *pattern, u64 width, u64 height) {

        std::vector<u32> indices;

        auto indexCount = (width * height);
        auto byteCount =  pattern->getSize();
        if (byteCount == 0 || indexCount == 0)
            return indices;

        if (byteCount >= indexCount) {
            auto bytesPerIndex = byteCount / indexCount;

            if (bytesPerIndex == 1) {
                auto bytes = patternToArray<u8>(pattern);
                indices = std::vector<u32>(bytes.begin(), bytes.end());
            } else if (bytesPerIndex == 2) {
                auto shorts = patternToArray<u16>(pattern);
                indices = std::vector<u32>(shorts.begin(), shorts.end());
            }
        } else {
            auto indicesPerByte = indexCount / byteCount;
            auto bytes = patternToArray<u8>(pattern);

            if (indicesPerByte == 2) {
                for (u8 byte : bytes) {
                    indices.push_back(byte & 0xF);
                    indices.push_back((byte >> 4) & 0xF);
                }
            }
        }
        return indices;
    }
}
