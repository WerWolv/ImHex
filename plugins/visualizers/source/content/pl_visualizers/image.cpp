#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <imgui.h>
#include <hex/helpers/auto_reset.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::visualizers {
    std::vector<u32> getIndices(pl::ptrn::Pattern *colorTablePattern, u64 width, u64 height);
    ImGuiExt::Texture getTexture(pl::ptrn::Pattern *colorTablePattern, std::vector<u32>& indices, u64 width, u64 height);


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

    ImGuiExt::Texture getTexture(pl::ptrn::Pattern *colorTablePattern, std::vector<u32>& indices, u64 width, u64 height) {
        std::vector<u32> colorTable = patternToArray<u32>(colorTablePattern);
        auto colorCount = colorTable.size();
        auto indexCount = indices.size();
        std::vector<u32> image(indexCount);

        for (u32 i = 0; i < indexCount; i++) {
            auto index = indices[i];

            if (index >= colorCount)
                index = 0;
            image[i] = colorTable[index];
        }
        void *tmp = image.data();
        ImU8 *data = static_cast<ImU8 *>(tmp);
        ImGuiExt::Texture texture = ImGuiExt::Texture::fromBitmap(data, indexCount*4, width, height, ImGuiExt::Texture::Filter::Nearest);
        return texture;
    }

    std::vector<u32> getIndices(pl::ptrn::Pattern *pattern, u64 width, u64 height) {
        auto indexCount = width * height / pattern->getSize();
        std::vector<u32> indices;
        auto *iterable = dynamic_cast<pl::ptrn::IIterable *>(pattern);

        if (iterable == nullptr || iterable->getEntryCount() <= 0)
            return indices;
        auto content = iterable->getEntry(0);
        auto byteCount = content->getSize();

        if (byteCount >= indexCount && indexCount != 0) {
            auto bytePerIndex = byteCount / indexCount;

            if (bytePerIndex == 1) {
                auto temp = patternToArray<u8>(pattern);
                indices = std::vector<u32>(temp.begin(), temp.end());
            } else if (bytePerIndex == 2) {
                auto temp = patternToArray<u16>(pattern);
                indices = std::vector<u32>(temp.begin(), temp.end());
            } else // 32 bits indices make no sense.
                return indices;
        } else if (byteCount != 0) {
            auto indicesPerByte = indexCount / byteCount;
            auto temp = patternToArray<u8>(pattern);

            if (indicesPerByte == 2) {
                for (u32 i = 0; i < temp.size(); i++) {
                    indices.push_back(temp[i] & 0xF);
                    indices.push_back((temp[i] >> 4) & 0xF);
                }
            } else  // 2 bits indices are too little
                return indices;
        }
        return indices;
    }
}
