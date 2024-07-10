#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <hex/ui/imgui_imhex_extensions.h>

#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_bitfield.hpp>

namespace hex::plugin::visualizers {

    void drawTableVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static std::vector<std::string> tableContent;
        static u128 width = 0, height = 0;

        if (shouldReset) {
            tableContent.clear();
            width = height = 0;

            auto pattern  = arguments[0].toPattern();
            if (dynamic_cast<pl::ptrn::PatternArrayStatic*>(pattern.get()) == nullptr &&
                dynamic_cast<pl::ptrn::PatternArrayDynamic*>(pattern.get()) == nullptr &&
                dynamic_cast<pl::ptrn::PatternBitfieldArray*>(pattern.get()) == nullptr)
            {
                throw std::logic_error("Table visualizer requires an array pattern as the first argument.");
            }

            width = arguments[1].toUnsigned();
            height = arguments[2].toUnsigned();

            auto iterable = dynamic_cast<pl::ptrn::IIterable*>(pattern.get());
            iterable->forEachEntry(0, iterable->getEntryCount(), [&](u64, pl::ptrn::Pattern *entry) {
                tableContent.push_back(entry->toString());
            });
        }

        if (width >= IMGUI_TABLE_MAX_COLUMNS)
            throw std::logic_error(hex::format("Table visualizer cannot have more than {} columns.", IMGUI_TABLE_MAX_COLUMNS));

        if (ImGui::BeginTable("##visualizer_table", width, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            for (u128 i = 0; i < height; i += 1) {
                ImGui::TableNextRow();
                for (u128 j = 0; j < width; j += 1) {
                    ImGui::TableSetColumnIndex(j);
                    if (i * width + j < tableContent.size())
                        ImGui::TextUnformatted(tableContent[(i * width) + j].c_str());
                    else
                        ImGui::TextUnformatted("??");
                }
            }
            ImGui::EndTable();
        }
    }

}