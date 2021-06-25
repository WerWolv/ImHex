#include "views/view_constants.hpp"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace hex {

    ViewConstants::ViewConstants() : View("hex.view.constants.name") {
        this->reloadConstants();
        this->m_search.resize(0xFFF, 0x00);
    }

    ViewConstants::~ViewConstants() {

    }

    void ViewConstants::reloadConstants() {
        this->m_constants.clear();

        for (auto &path : hex::getPath(ImHexPath::Constants)) {
            if (!std::filesystem::exists(path)) continue;

            for (auto &file : std::filesystem::directory_iterator(path)) {
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

                        this->m_constants.push_back(constant);
                    }
                } catch (...) {
                    log::info("Error");
                    continue;
                }


            }
        }
    }

    void ViewConstants::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.view.constants.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            if (ImGui::InputText("##search", this->m_search.data(), this->m_search.size())) {
                this->m_filteredConstants.clear();
                for (auto &constant : this->m_constants) {
                    if (constant.value.starts_with(this->m_search.c_str()) || constant.name.starts_with(this->m_search.c_str()) || constant.description.starts_with(this->m_search.c_str()))
                        this->m_filteredConstants.push_back(&constant);
                }
            }

            if (ImGui::BeginTable("##strings", 4,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                                  ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.view.constants.category"_lang, 0, -1, ImGui::GetID("category"));
                ImGui::TableSetupColumn("hex.view.constants.name"_lang, 0, -1, ImGui::GetID("name"));
                ImGui::TableSetupColumn("hex.view.constants.desc"_lang, 0, -1, ImGui::GetID("desc"));
                ImGui::TableSetupColumn("hex.view.constants.value"_lang, 0, -1, ImGui::GetID("value"));

                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(this->m_filteredConstants.size());

                while (clipper.Step()) {
                    for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", this->m_filteredConstants[i]->category.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", this->m_filteredConstants[i]->name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", this->m_filteredConstants[i]->description.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", this->m_filteredConstants[i]->value.c_str());
                    }
                }
                clipper.End();

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void ViewConstants::drawMenu() {

    }

}