#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::visualizers {

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

            auto data = pattern->getBytes();
            texture = ImGuiExt::Texture::fromBitmap(data.data(), data.size(), width, height, ImGuiExt::Texture::Filter::Nearest);
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

}