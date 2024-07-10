#include <hex/helpers/utils.hpp>

#include <hex/providers/memory_provider.hpp>

#include <imgui.h>

#include <pl/pattern_language.hpp>
#include <pl/patterns/pattern.hpp>

#include <hex/ui/imgui_imhex_extensions.h>
#include <ui/hex_editor.hpp>

namespace hex::plugin::builtin {

    void drawHexVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static ui::HexEditor editor;
        static prv::MemoryProvider dataProvider;

        if (shouldReset) {
            auto pattern = arguments[0].toPattern();
            std::vector<u8> data;

            try {
                data = pattern->getBytes();
            } catch (const std::exception &) {
                dataProvider.resize(0);
                throw;
            }

            dataProvider.resize(data.size());
            dataProvider.writeRaw(0x00, data.data(), data.size());

            editor.setProvider(&dataProvider);
        }

        if (ImGui::BeginChild("##editor", scaled(ImVec2(600, 400)), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            editor.draw();

            ImGui::EndChild();
        }
    }

}