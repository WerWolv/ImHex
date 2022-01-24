#include "content/views/view_constants.hpp"

#include <hex/helpers/paths.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/utils.hpp>

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    ViewConstants::ViewConstants() : View("hex.builtin.view.constants.name") {
        this->reloadConstants();

        this->m_filter.reserve(0xFFFF);
        std::memset(this->m_filter.data(), 0x00, this->m_filter.capacity());
    }

    ViewConstants::~ViewConstants() {
    }

    void ViewConstants::reloadConstants() {
        this->m_constants.clear();
        this->m_filterIndices.clear();

        for (auto &path : hex::getPath(ImHexPath::Constants)) {
            if (!fs::exists(path)) continue;

            for (auto &file : fs::directory_iterator(path)) {
                if (!file.is_regular_file()) continue;

                try {
                    nlohmann::json content;
                    std::ifstream(file.path()) >> content;

                    for (auto value : content["values"]) {
                        Constant constant;
                        constant.category = content["name"];
                        constant.name = value["name"];
                        if (value.contains("desc"))
                            constant.description = value["desc"];
                        constant.value = value["value"];

                        auto type = value["type"];
                        if (type == "int10")
                            constant.type = ConstantType::Int10;
                        else if (type == "int16be")
                            constant.type = ConstantType::Int16BigEndian;
                        else if (type == "int16le")
                            constant.type = ConstantType::Int16LittleEndian;
                        else
                            throw std::runtime_error("Invalid type");

                        this->m_filterIndices.push_back(this->m_constants.size());
                        this->m_constants.push_back(constant);
                    }
                } catch (...) {
                    log::error("Failed to parse constants file {}", file.path().string());
                    continue;
                }
            }
        }
    }

    void ViewConstants::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.constants.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            ImGui::InputText(
                "##search", this->m_filter.data(), this->m_filter.capacity(), ImGuiInputTextFlags_CallbackEdit, [](ImGuiInputTextCallbackData *data) {
                    auto &view = *static_cast<ViewConstants *>(data->UserData);
                    view.m_filter.resize(data->BufTextLen);

                    view.m_filterIndices.clear();
                    for (u64 i = 0; i < view.m_constants.size(); i++) {
                        auto &constant = view.m_constants[i];
                        if (hex::containsIgnoreCase(constant.name, data->Buf) ||
                            hex::containsIgnoreCase(constant.category, data->Buf) ||
                            hex::containsIgnoreCase(constant.description, data->Buf) ||
                            hex::containsIgnoreCase(constant.value, data->Buf))
                            view.m_filterIndices.push_back(i);
                    }

                    return 0;
                },
                this);

            if (ImGui::BeginTable("##strings", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.view.constants.row.category"_lang, 0, -1, ImGui::GetID("category"));
                ImGui::TableSetupColumn("hex.builtin.view.constants.row.name"_lang, 0, -1, ImGui::GetID("name"));
                ImGui::TableSetupColumn("hex.builtin.view.constants.row.desc"_lang, 0, -1, ImGui::GetID("desc"));
                ImGui::TableSetupColumn("hex.builtin.view.constants.row.value"_lang, 0, -1, ImGui::GetID("value"));

                auto sortSpecs = ImGui::TableGetSortSpecs();

                if (sortSpecs->SpecsDirty) {
                    std::sort(this->m_constants.begin(), this->m_constants.end(), [&sortSpecs](Constant &left, Constant &right) -> bool {
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
                clipper.Begin(this->m_filterIndices.size());

                while (clipper.Step()) {
                    for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        auto &constant = this->m_constants[this->m_filterIndices[i]];
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
        ImGui::End();
    }

}