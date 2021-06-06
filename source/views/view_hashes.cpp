#include "views/view_hashes.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/crypto.hpp>

#include <vector>


namespace hex {

    ViewHashes::ViewHashes() : View("hex.view.hashes.name") {
        EventManager::subscribe<EventDataChanged>(this, [this]() {
            this->m_shouldInvalidate = true;
        });

        EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
            if (this->m_shouldMatchSelection) {
                this->m_hashRegion[0] = region.address;
                this->m_hashRegion[1] = region.address + region.size;
                this->m_shouldInvalidate = true;
            }
        });
    }

    ViewHashes::~ViewHashes() {
        EventManager::unsubscribe<EventDataChanged>(this);
        EventManager::unsubscribe<EventRegionSelected>(this);
    }


    template<size_t Size>
    static void formatBigHexInt(std::array<u8, Size> dataArray, char *buffer, size_t bufferSize) {
        for (int i = 0; i < dataArray.size(); i++)
            snprintf(buffer + 2 * i, bufferSize - 2 * i, "%02X", dataArray[i]);
    }

    void ViewHashes::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.view.hashes.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            auto provider = SharedData::currentProvider;
            if (provider != nullptr && provider->isAvailable()) {

                ImGui::TextUnformatted("hex.common.region"_lang);
                ImGui::Separator();

                ImGui::InputScalarN("##nolabel", ImGuiDataType_U64, this->m_hashRegion, 2, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                ImGui::Checkbox("hex.common.match_selection"_lang, &this->m_shouldMatchSelection);
                if (ImGui::IsItemEdited()) {
                    // Force execution of Region Selection Event
                    EventManager::post<RequestSelectionChange>(Region{ 0, 0 });
                    this->m_shouldInvalidate = true;
                }

                ImGui::NewLine();
                ImGui::TextUnformatted("hex.view.hashes.settings"_lang);
                ImGui::Separator();

                if (ImGui::Combo("hex.view.hashes.function"_lang, &this->m_currHashFunction, HashFunctionNames,sizeof(HashFunctionNames) / sizeof(const char *)))
                    this->m_shouldInvalidate = true;

                size_t dataSize = provider->getSize();
                if (this->m_hashRegion[1] >= provider->getBaseAddress() + dataSize)
                    this->m_hashRegion[1] = provider->getBaseAddress() + dataSize;


                if (this->m_hashRegion[1] >= this->m_hashRegion[0]) {

                    switch (this->m_currHashFunction) {
                        case 0: // CRC16
                        {
                            static int polynomial = 0x8005, init = 0x0000;

                            ImGui::InputInt("hex.view.hashes.iv"_lang, &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                            if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                            ImGui::InputInt("hex.view.hashes.poly"_lang, &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                            if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                            ImGui::NewLine();

                            static u16 result = 0;

                            if (this->m_shouldInvalidate)
                                result = crypt::crc16(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1, polynomial, init);

                            char buffer[sizeof(result) * 2 + 1];
                            snprintf(buffer, sizeof(buffer), "%04X", result);

                            ImGui::TextUnformatted("hex.view.hashes.result"_lang);
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 1: // CRC32
                        {
                            static int polynomial = 0x04C11DB7, init = 0xFFFFFFFF;

                            ImGui::InputInt("hex.view.hashes.iv"_lang, &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                            if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                            ImGui::InputInt("hex.view.hashes.poly"_lang, &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                            if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                            ImGui::NewLine();

                            static u32 result = 0;

                            if (this->m_shouldInvalidate)
                                result = crypt::crc32(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1, polynomial, init);

                            char buffer[sizeof(result) * 2 + 1];
                            snprintf(buffer, sizeof(buffer), "%08X", result);

                            ImGui::TextUnformatted("hex.view.hashes.result"_lang);
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 2: // MD5
                        {
                            static std::array<u8, 16> result = { 0 };

                            if (this->m_shouldInvalidate)
                                result = crypt::md5(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("hex.view.hashes.result"_lang);
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 3: // SHA-1
                        {
                            static std::array<u8, 20> result = { 0 };

                            if (this->m_shouldInvalidate)
                                result = crypt::sha1(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("hex.view.hashes.result"_lang);
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 4: // SHA-224
                        {
                            static std::array<u8, 28> result = { 0 };

                            if (this->m_shouldInvalidate)
                                result = crypt::sha224(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("hex.view.hashes.result"_lang);
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 5: // SHA-256
                        {
                            static std::array<u8, 32> result;

                            if (this->m_shouldInvalidate)
                                result = crypt::sha256(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("hex.view.hashes.result"_lang);
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 6: // SHA-384
                        {
                            static std::array<u8, 48> result;

                            if (this->m_shouldInvalidate)
                                result = crypt::sha384(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("hex.view.hashes.result"_lang);
                            ImGui::Separator();
                            ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                        }
                            break;
                        case 7: // SHA-512
                        {
                            static std::array<u8, 64> result;

                            if (this->m_shouldInvalidate)
                                result = crypt::sha512(provider, this->m_hashRegion[0], this->m_hashRegion[1] - this->m_hashRegion[0] + 1);

                            char buffer[sizeof(result) * 2 + 1];
                            formatBigHexInt(result, buffer, sizeof(buffer));

                            ImGui::NewLine();
                            ImGui::TextUnformatted("hex.view.hashes.result"_lang);
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