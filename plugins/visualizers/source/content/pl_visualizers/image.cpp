#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::visualizers {
    std::vector<u32> getIndices(pl::ptrn::Pattern *colorTablePattern, u128 width, u128 height);
    ImGuiExt::Texture getTexture(pl::ptrn::Pattern *colorTablePattern, std::vector<u32>& indices, u128 width, u128 height);


    void drawImageVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static ImGuiExt::Texture texture;
        static float scale = 1.0F;

        if (shouldReset) {
            auto pattern  = arguments[0].toPattern();

            auto data = pattern->getBytes();
            texture = ImGuiExt::Texture::fromImage(data.data(), data.size(), ImGuiExt::Texture::Filter::Nearest);
            scale = 200_scaled / texture.getSize().x;
        }

        if (texture.isValid())
            ImGui::Image(texture, texture.getSize() * scale);

        if (ImGui::IsWindowHovered()) {
            auto scrollDelta = ImGui::GetIO().MouseWheel;
            if (scrollDelta != 0.0F) {
                scale += scrollDelta * 0.1F;
                scale = std::clamp(scale, 0.1F, 10.0F);
            }
        }
    }

    void drawBitmapVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static ImGuiExt::Texture texture;
        static float scale = 1.0F;

        if (shouldReset) {
            auto pattern  = arguments[0].toPattern();
            auto width  = arguments[1].toUnsigned();
            auto height = arguments[2].toUnsigned();
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
                texture = ImGuiExt::Texture::fromBitmap(data.data(), data.size(), width, height,ImGuiExt::Texture::Filter::Nearest);
            }
        }

        if (texture.isValid())
            ImGui::Image(texture, texture.getSize() * scale);

        if (ImGui::IsWindowHovered()) {
            auto scrollDelta = ImGui::GetIO().MouseWheel;

            if (scrollDelta != 0.0F) {
                scale += scrollDelta * 0.1F;
                scale = std::clamp(scale, 0.1F, 10.0F);
            }
        }
    }

    template <typename T>  ImGuiExt::Texture unmapColors(pl::ptrn::Pattern *colorTablePattern, std::vector<u32>& indices, u128 width, u128 height) {
        std::vector<T> colorTable = patternToArray<T>(colorTablePattern);
        auto colorCount = colorTable.size();
        auto indexCount = indices.size();
        std::vector<T> image(indexCount);

        for (u32 i = 0; i < indexCount; i++) {
            auto index = indices[i];

            if (index >= colorCount)
                index = 0;
            image[i] = colorTable[index];
        }
        void *tmp = image.data();
        ImU8 *data = static_cast<ImU8 *>(tmp);
        ImGuiExt::Texture texture = ImGuiExt::Texture::fromBitmap(data, indexCount*sizeof(T), width, height, ImGuiExt::Texture::Filter::Nearest);
        return texture;
    }

    std::vector<u32> getIndices(pl::ptrn::Pattern *pattern, u128 width, u128 height) {
        auto indexCount = 2 * width * height / pattern->getSize();
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
            } else if (bytePerIndex == 4) {
                auto temp = patternToArray<u32>(pattern);
                indices = std::vector<u32>(temp.begin(), temp.end());
            }
        } else if (indexCount != 0) {
            auto indicesPerByte = indexCount / byteCount;
            auto temp = patternToArray<u8>(pattern);

            if (indicesPerByte == 2) {
                for (u32 i = 0; i < temp.size(); i++) {
                    indices.push_back(temp[i] & 0xF);
                    indices.push_back((temp[i] >> 4) & 0xF);
                }
            } else if (indicesPerByte == 4) {
                for (u32 i = 0; i < temp.size(); i++) {
                    indices.push_back(temp[i] & 0x3);
                    indices.push_back((temp[i] >> 2) & 0x3);
                    indices.push_back((temp[i] >> 4) & 0x3);
                    indices.push_back((temp[i] >> 6) & 0x3);
                }
            }
        }
        return indices;
    }

    ImGuiExt::Texture getTexture(pl::ptrn::Pattern *colorTablePattern, std::vector<u32>& indices, u128 width, u128 height) {
        ImGuiExt::Texture texture;
        auto iterable = dynamic_cast<pl::ptrn::IIterable *>(colorTablePattern);

        if (iterable == nullptr || iterable->getEntryCount() <= 0)
            return texture;
        auto content = iterable->getEntry(0);
        auto colorTypeSize = content->getSize()*2;

        if (colorTypeSize == 1) {
            texture = unmapColors<u8>(colorTablePattern, indices, width, height);
        } else if (colorTypeSize == 2) {
            texture = unmapColors<u16>(colorTablePattern, indices, width, height);
        } else if (colorTypeSize == 4) {
            texture = unmapColors<u32>(colorTablePattern, indices, width, height);
        }
        return texture;
    }
}
