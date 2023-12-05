#include <hex/helpers/utils.hpp>

#include <content/pl_visualizers/visualizer_helpers.hpp>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <ui/hex_editor.hpp>
#include <content/providers/memory_file_provider.hpp>

namespace hex::plugin::builtin {

    void drawHexVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static ui::HexEditor editor;
        static std::unique_ptr<MemoryFileProvider> dataProvider;

        if (shouldReset) {
            auto pattern = arguments[0].toPattern();
            std::vector<u8> data;

            dataProvider = std::make_unique<MemoryFileProvider>();
            try {
                data = pattern->getBytes();
            } catch (const std::exception &) {
                dataProvider->resize(0);
                throw;
            }

            dataProvider->resize(data.size());
            dataProvider->writeRaw(0x00, data.data(), data.size());
            dataProvider->setReadOnly(true);

            editor.setProvider(dataProvider.get());
        }

        if (ImGui::BeginChild("##editor", scaled(ImVec2(600, 400)), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            editor.draw();

            ImGui::EndChild();
        }
    }

}