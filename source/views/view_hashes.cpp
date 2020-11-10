#include "views/view_hashes.hpp"

#include "utils.hpp"

#include <vector>

namespace hex {

    ViewHashes::ViewHashes(FILE* &file) : View(), m_file(file) {

    }

    ViewHashes::~ViewHashes() {

    }


    void ViewHashes::createView() {
        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("Hashing", &this->m_windowOpen)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            ImGui::NewLine();

            ImGui::Combo("Hash Function", &this->m_currHashFunction, HashFunctionNames, sizeof(HashFunctionNames) / sizeof(const char*));

            ImGui::NewLine();
            ImGui::Separator();
            ImGui::NewLine();

            ImGui::InputInt("Begin", &this->m_hashStart, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::InputInt("End", &this->m_hashEnd, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);

            ImGui::NewLine();
            ImGui::Separator();
            ImGui::NewLine();

            switch (this->m_currHashFunction) {
                case 0: // CRC16
                {
                    if (this->m_file == nullptr)
                        break;

                    std::vector<u8> buffer(this->m_hashEnd - this->m_hashStart + 1, 0x00);
                    fseek(this->m_file, this->m_hashStart, SEEK_SET);
                    fread(buffer.data(), 1, buffer.size(), this->m_file);

                    static int polynomial = 0, init = 0;

                    ImGui::InputInt("Initial Value", &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                    ImGui::InputInt("Polynomial", &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();
                    ImGui::LabelText("Result", "%X", crc16(buffer.data(), buffer.size(), polynomial, init));
                }
                break;
                case 1: // CRC32
                {
                    if (this->m_file == nullptr)
                        break;

                    std::vector<u8> buffer(this->m_hashEnd - this->m_hashStart + 1, 0x00);
                    fseek(this->m_file, this->m_hashStart, SEEK_SET);
                    fread(buffer.data(), 1, buffer.size(), this->m_file);

                    static int polynomial = 0, init = 0;

                    ImGui::InputInt("Initial Value", &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                    ImGui::InputInt("Polynomial", &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();
                    ImGui::LabelText("Result", "%X", crc32(buffer.data(), buffer.size(), polynomial, init));
                }
                break;
                case 2: // MD5
                {
                    if (this->m_file == nullptr)
                        break;

                    std::vector<u8> buffer(this->m_hashEnd - this->m_hashStart + 1, 0x00);
                    fseek(this->m_file, this->m_hashStart, SEEK_SET);
                    fread(buffer.data(), 1, buffer.size(), this->m_file);

                    auto result = md5(buffer.data(), buffer.size());

                    ImGui::LabelText("Result", "%08X%08X%08X%08X", __builtin_bswap32(result[0]), __builtin_bswap32(result[1]), __builtin_bswap32(result[2]), __builtin_bswap32(result[3]));
                }
                    break;
            }

            ImGui::EndChild();
            ImGui::End();
        }
    }

    void ViewHashes::createMenu() {
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Hash View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

}