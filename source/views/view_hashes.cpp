#include "views/view_hashes.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>

#include "helpers/crypto.hpp"

#include <vector>


namespace hex {

    ViewHashes::ViewHashes() : View("Hashes") {
        View::subscribeEvent(Events::DataChanged, [this](auto) {
            this->m_shouldInvalidate = true;
        });

        View::subscribeEvent(Events::RegionSelected, [this](auto userData) {
            auto region = std::any_cast<const Region>(userData);

            if (this->m_shouldMatchSelection) {
                this->m_hashRegion[0] = region.address;
                this->m_hashRegion[1] = region.address + region.size - 1;
                this->m_shouldInvalidate = true;
            }
        });
    }

    ViewHashes::~ViewHashes() {
        View::unsubscribeEvent(Events::DataChanged);
        View::unsubscribeEvent(Events::RegionSelected);
    }


    static void formatBigHexInt(auto dataArray, char *buffer, size_t bufferSize) {
        for (int i = 0; i < dataArray.size(); i++)
            snprintf(buffer + 8 * i, bufferSize - 8 * i, "%08X", hex::changeEndianess(dataArray[i], std::endian::big));
    }

    void ViewHashes::drawContent() {
        if (ImGui::Begin("Hashing", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            auto provider = SharedData::currentProvider;
            if (provider != nullptr && provider->isAvailable()) {

                ImGui::TextUnformatted("Region");
                ImGui::Separator();

                ImGui::InputScalarN("##nolabel", ImGuiDataType_U64, this->m_hashRegion, 2, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                ImGui::Checkbox("Match selection", &this->m_shouldMatchSelection);
                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                ImGui::NewLine();
                ImGui::TextUnformatted("Settings");
                ImGui::Separator();

                if (ImGui::Combo("Hash Function", &this->m_currHashFunction, HashFunctionNames,sizeof(HashFunctionNames) / sizeof(const char *)))
                    this->m_shouldInvalidate = true;

                size_t dataSize = provider->getSize();
                if (this->m_hashRegion[1] >= dataSize)
                    this->m_hashRegion[1] = dataSize - 1;


                if (this->m_hashRegion[1] >= this->m_hashRegion[0]) {

                    switch (this->m_currHashFunction) {
                        case 0: // CRC16
                        {
                            static int polynomial = 0, init = 0;

                            ImGui::InputInt("Initial Value", &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                            if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                            ImGui::InputInt("Polynomial", &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                            if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                            ImGui::NewLine();

                            static u16 result = 0;

                            if (this->m_shouldInvalidate)
                                result = crc16(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1, polynomial, init);

                            char buffer[sizeof(result) * 2 + 1];
                            snprintf(buffer, sizeof(buffer), "%04X", result);

                            ImGui::TextUnformatted("Result");
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 1: // CRC32
                        {
                            static int polynomial = 0, init = 0;

                            ImGui::InputInt("Initial Value", &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                            if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                            ImGui::InputInt("Polynomial", &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                            if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                            ImGui::NewLine();

                            static u32 result = 0;

                            if (this->m_shouldInvalidate)
                                result = crc32(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1, polynomial, init);

                            char buffer[sizeof(result) * 2 + 1];
                            snprintf(buffer, sizeof(buffer), "%08X", result);

                            ImGui::TextUnformatted("Result");
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 2: // MD4
                        {
                            static std::array<u32, 4> result;

                            if (this->m_shouldInvalidate)
                                result = md4(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("Result");
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 3: // MD5
                        {
                            static std::array<u32, 4> result = { 0 };

                            if (this->m_shouldInvalidate)
                                result = md5(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("Result");
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 4: // SHA-1
                        {
                            static std::array<u32, 5> result = { 0 };

                            if (this->m_shouldInvalidate)
                                result = sha1(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("Result");
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 5: // SHA-224
                        {
                            static std::array<u32, 7> result = { 0 };

                            if (this->m_shouldInvalidate)
                                result = sha224(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("Result");
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 6: // SHA-256
                        {
                            static std::array<u32, 8> result;

                            if (this->m_shouldInvalidate)
                                result = sha256(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("Result");
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 7: // SHA-384
                        {
                            static std::array<u32, 12> result;

                            if (this->m_shouldInvalidate)
                                result = sha384(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("Result");
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 8: // SHA-512
                        {
                            static std::array<u32, 16> result;

                            if (this->m_shouldInvalidate)
                                result = sha512(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("Result");
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                    }

                }

                this->m_shouldInvalidate = false;
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ViewHashes::drawMenu() {

    }

}