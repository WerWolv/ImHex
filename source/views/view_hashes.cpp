#include "views/view_hashes.hpp"

#include "providers/provider.hpp"

#include "utils.hpp"

#include <vector>

namespace hex {

    ViewHashes::ViewHashes(prv::Provider* &dataProvider) : View(), m_dataProvider(dataProvider) {

    }

    ViewHashes::~ViewHashes() {

    }


    void ViewHashes::createView() {
        static bool invalidate = false;

        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("Hashing", &this->m_windowOpen)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
            ImGui::NewLine();

            if (this->m_dataProvider != nullptr && this->m_dataProvider->isAvailable()) {

                ImGui::Combo("Hash Function", &this->m_currHashFunction, HashFunctionNames,
                             sizeof(HashFunctionNames) / sizeof(const char *));

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();

                ImGui::InputInt("Begin", &this->m_hashStart, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);

                ImGui::InputInt("End", &this->m_hashEnd, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);

                size_t dataSize = this->m_dataProvider->getSize();
                if (this->m_hashEnd >= dataSize)
                    this->m_hashEnd = dataSize - 1;

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();

                if (this->m_hashEnd >= this->m_hashStart) {

                    std::vector<u8> buffer;

                    if (invalidate) {
                        buffer = std::vector<u8>(this->m_hashEnd - this->m_hashStart + 1, 0x00);
                        this->m_dataProvider->read(this->m_hashStart, buffer.data(), buffer.size());
                    }

                    switch (this->m_currHashFunction) {
                        case 0: // CRC16
                        {
                            static int polynomial = 0, init = 0;

                            ImGui::InputInt("Initial Value", &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                            ImGui::InputInt("Polynomial", &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);

                            ImGui::NewLine();
                            ImGui::Separator();
                            ImGui::NewLine();

                            static u16 result = 0;

                            if (invalidate)
                                result = crc16(buffer.data(), buffer.size(), polynomial, init);

                            ImGui::LabelText("##nolabel", "%X", result);
                        }
                            break;
                        case 1: // CRC32
                        {
                            static int polynomial = 0, init = 0;

                            ImGui::InputInt("Initial Value", &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                            ImGui::InputInt("Polynomial", &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);

                            ImGui::NewLine();
                            ImGui::Separator();
                            ImGui::NewLine();

                            static u32 result = 0;

                            if (invalidate)
                                result = crc32(buffer.data(), buffer.size(), polynomial, init);

                            ImGui::LabelText("##nolabel", "%X", result);
                        }
                            break;
                        case 2: // MD5
                        {
                            static std::array<u32, 4> result;

                            if (invalidate)
                                result = md5(buffer.data(), buffer.size());

                            ImGui::LabelText("##nolabel", "%08X%08X%08X%08X",
                                             __builtin_bswap32(result[0]),
                                             __builtin_bswap32(result[1]),
                                             __builtin_bswap32(result[2]),
                                             __builtin_bswap32(result[3]));
                        }
                            break;
                    }

                }

                invalidate = false;

                ImGui::SameLine();
                if (ImGui::Button("Hash"))
                    invalidate = true;

            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ViewHashes::createMenu() {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hash View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

}