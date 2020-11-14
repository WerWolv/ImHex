#include "views/view_tools.hpp"

#include <cxxabi.h>
#include <cstring>

namespace hex {

    ViewTools::ViewTools() {
        this->m_mangledBuffer = new char[0xFF'FFFF];
        this->m_demangledName = static_cast<char*>(malloc(8));

        std::memset(this->m_mangledBuffer, 0xFF'FFFF, 0x00);
        strcpy(this->m_demangledName, "< ??? >");
    }

    ViewTools::~ViewTools() {
        delete[] this->m_mangledBuffer;
        free(this->m_demangledName);
    }

    void ViewTools::createView() {
        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("Tools", &this->m_windowOpen)) {

            ImGui::Text("Itanium demangler");
            ImGui::Separator();
            ImGui::NewLine();

            if (ImGui::InputText("Mangled name", this->m_mangledBuffer, 0xFF'FFFF)) {
                size_t length = 0;
                int status = 0;

                if (this->m_demangledName != nullptr)
                    free(this->m_demangledName);

                this->m_demangledName = abi::__cxa_demangle(this->m_mangledBuffer, nullptr, &length, &status);

                if (status != 0) {
                    this->m_demangledName = static_cast<char*>(malloc(8));
                    strcpy(this->m_demangledName, "< ??? >");
                }
            }

            ImGui::InputText("Demangled name", this->m_demangledName, strlen(this->m_demangledName), ImGuiInputTextFlags_ReadOnly);



        }
        ImGui::End();
    }

    void ViewTools::createMenu() {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Tools View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

}