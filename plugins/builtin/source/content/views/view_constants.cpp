#include "content/views/view_constants.hpp"

#include <hex/helpers/logger.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/default_paths.hpp>

#include <wolv/utils/string.hpp>
#include <wolv/io/file.hpp>

#include <filesystem>
#include <nlohmann/json.hpp>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    ViewConstants::ViewConstants() : View::Window("hex.builtin.view.constants.name", ICON_VS_SYMBOL_CONSTANT) {
        this->reloadConstants();
    }

    void ViewConstants::reloadConstants() {
        m_constants.clear();
        m_filterIndices.clear();

        for (const auto &path : paths::Constants.read()) {
            if (!wolv::io::fs::exists(path)) continue;

            std::error_code error;
            for (auto &file : std::fs::directory_iterator(path, error)) {
                if (!file.is_regular_file()) continue;

                if (file.path().extension() != ".json") continue;
                if (file.path().filename().u8string().starts_with('_')) continue;

                try {
                    auto fileData = wolv::io::File(file.path(), wolv::io::File::Mode::Read).readString();
                    auto content = nlohmann::json::parse(fileData);

                    for (auto value : content.at("values")) {
                        Constant constant;
                        constant.category = content.at("name").get<std::string>();
                        constant.name     = value.at("name").get<std::string>();
                        if (value.contains("desc"))
                            constant.description = value.at("desc").get<std::string>();
                        constant.value = value.at("value").get<std::string>();

                        auto type = value.at("type");
                        if (type == "int10")
                            constant.type = ConstantType::Int10;
                        else if (type == "int16be")
                            constant.type = ConstantType::Int16BigEndian;
                        else if (type == "int16le")
                            constant.type = ConstantType::Int16LittleEndian;
                        else
                            throw std::runtime_error("Invalid type");

                        m_filterIndices.push_back(m_constants.size());
                        m_constants.push_back(constant);
                    }
                } catch (...) {
                    log::error("Failed to parse constants file {}", wolv::util::toUTF8String(file.path()));
                }
            }
        }
    }

    void ViewConstants::drawContent() {
        ImGui::PushItemWidth(-1);

        if (ImGuiExt::InputTextIcon("##search", ICON_VS_FILTER, m_filter)) {
            m_filterIndices.clear();

            // Filter the constants according to the entered value
            for (u64 i = 0; i < m_constants.size(); i++) {
                auto &constant = m_constants[i];
                if (hex::containsIgnoreCase(constant.name, m_filter) ||
                    hex::containsIgnoreCase(constant.category, m_filter) ||
                    hex::containsIgnoreCase(constant.description, m_filter) ||
                    hex::containsIgnoreCase(constant.value, m_filter))
                    m_filterIndices.push_back(i);
            }
        }

        ImGui::PopItemWidth();

        if (ImGui::BeginTable("##strings", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.builtin.view.constants.row.category"_lang, 0, -1, ImGui::GetID("category"));
            ImGui::TableSetupColumn("hex.builtin.view.constants.row.name"_lang, 0, -1, ImGui::GetID("name"));
            ImGui::TableSetupColumn("hex.builtin.view.constants.row.desc"_lang, 0, -1, ImGui::GetID("desc"));
            ImGui::TableSetupColumn("hex.builtin.view.constants.row.value"_lang, 0, -1, ImGui::GetID("value"));

            auto sortSpecs = ImGui::TableGetSortSpecs();

            // Handle table sorting
            if (sortSpecs->SpecsDirty) {
                std::sort(m_constants.begin(), m_constants.end(), [&sortSpecs](const Constant &left, const Constant &right) -> bool {
                    if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("category")) {
                        if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                            return left.category > right.category;
                        else
                            return left.category < right.category;
                    } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("name")) {
                        if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                            return left.name > right.name;
                        else
                            return left.name < right.name;
                    } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("desc")) {
                        if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                            return left.description > right.description;
                        else
                            return left.description < right.description;
                    } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("value")) {
                        if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                            return left.value > right.value;
                        else
                            return left.value < right.value;
                    }

                    return false;
                });

                sortSpecs->SpecsDirty = false;
            }

            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(m_filterIndices.size());

            // Draw the constants table
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    auto &constant = m_constants[m_filterIndices[i]];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(constant.category.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(constant.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(constant.description.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(constant.value.c_str());
                }
            }
            clipper.End();

            ImGui::EndTable();
        }
    }

}