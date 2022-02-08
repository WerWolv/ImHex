#include "content/views/view_hashes.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/crypto.hpp>

#include <vector>


namespace hex::plugin::builtin {

    ViewHashes::ViewHashes() : View("hex.builtin.view.hashes.name") {
        EventManager::subscribe<EventDataChanged>(this, [this]() {
            this->m_shouldInvalidate = true;
        });

        EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
            if (this->m_shouldMatchSelection) {
                if (region.address == size_t(-1)) {
                    this->m_hashRegion[0] = this->m_hashRegion[1] = 0;
                } else {
                    this->m_hashRegion[0] = region.address;
                    this->m_hashRegion[1] = region.size;
                }
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
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.hashes.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav)) {


                auto provider = ImHexApi::Provider::get();
                if (ImHexApi::Provider::isValid() && provider->isAvailable()) {

                    ImGui::TextUnformatted("hex.builtin.common.region"_lang);
                    ImGui::Separator();

                    ImGui::InputScalarN("##nolabel", ImGuiDataType_U64, this->m_hashRegion, 2, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
                    if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                    ImGui::Checkbox("hex.builtin.common.match_selection"_lang, &this->m_shouldMatchSelection);
                    if (ImGui::IsItemEdited()) {
                        // Force execution of Region Selection Event
                        ImHexApi::HexEditor::setSelection(0, 0);
                        this->m_shouldInvalidate = true;
                    }

                    ImGui::NewLine();
                    ImGui::TextUnformatted("hex.builtin.view.hashes.settings"_lang);
                    ImGui::Separator();

                    if (ImGui::BeginCombo("hex.builtin.view.hashes.function"_lang, hashFunctionNames[this->m_currHashFunction].second, 0)) {
                        for (int i = 0; i < hashFunctionNames.size(); i++) {
                            bool is_selected = (this->m_currHashFunction == i);
                            if (ImGui::Selectable(hashFunctionNames[i].second, is_selected))
                                this->m_currHashFunction = i;
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();    // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
                        }
                        ImGui::EndCombo();
                        this->m_shouldInvalidate = true;
                    }

                    size_t dataSize = provider->getSize();
                    if (this->m_hashRegion[1] >= provider->getBaseAddress() + dataSize)
                        this->m_hashRegion[1] = provider->getBaseAddress() + dataSize;


                    switch (hashFunctionNames[this->m_currHashFunction].first) {
                        case HashFunctions::Crc8:
                            {
                                static int polynomial = 0x07, init = 0x0000, xorout = 0x0000;
                                static bool reflectIn = false, reflectOut = false;

                                ImGui::InputInt("hex.builtin.view.hashes.iv"_lang, &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::InputInt("hex.builtin.view.hashes.xorout"_lang, &xorout, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::Checkbox("hex.builtin.common.reflectIn"_lang, &reflectIn);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::Checkbox("hex.builtin.common.reflectOut"_lang, &reflectOut);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::InputInt("hex.builtin.view.hashes.poly"_lang, &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::NewLine();

                                static u8 result = 0;

                                if (this->m_shouldInvalidate)
                                    result = crypt::crc8(provider, this->m_hashRegion[0], this->m_hashRegion[1], polynomial, init, xorout, reflectIn, reflectOut);

                                char buffer[sizeof(result) * 2 + 1];
                                snprintf(buffer, sizeof(buffer), "%02X", result);

                                ImGui::TextUnformatted("hex.builtin.view.hashes.result"_lang);
                                ImGui::Separator();
                                ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                            }
                            break;
                        case HashFunctions::Crc16:
                            {
                                static int polynomial = 0x8005, init = 0x0000, xorout = 0x0000;
                                static bool reflectIn = false, reflectOut = false;

                                ImGui::InputInt("hex.builtin.view.hashes.iv"_lang, &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::InputInt("hex.builtin.view.hashes.xorout"_lang, &xorout, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::Checkbox("hex.builtin.common.reflectIn"_lang, &reflectIn);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::Checkbox("hex.builtin.common.reflectOut"_lang, &reflectOut);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::InputInt("hex.builtin.view.hashes.poly"_lang, &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::NewLine();

                                static u16 result = 0;

                                if (this->m_shouldInvalidate)
                                    result = crypt::crc16(provider, this->m_hashRegion[0], this->m_hashRegion[1], polynomial, init, xorout, reflectIn, reflectOut);

                                char buffer[sizeof(result) * 2 + 1];
                                snprintf(buffer, sizeof(buffer), "%04X", result);

                                ImGui::TextUnformatted("hex.builtin.view.hashes.result"_lang);
                                ImGui::Separator();
                                ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                            }
                            break;
                        case HashFunctions::Crc32:
                            {
                                static int polynomial = 0x04C11DB7, init = 0xFFFFFFFF, xorout = 0xFFFFFFFF;
                                static bool reflectIn = true, reflectOut = true;


                                ImGui::InputInt("hex.builtin.view.hashes.iv"_lang, &init, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::InputInt("hex.builtin.view.hashes.xorout"_lang, &xorout, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::Checkbox("hex.builtin.common.reflectIn"_lang, &reflectIn);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::Checkbox("hex.builtin.common.reflectOut"_lang, &reflectOut);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::InputInt("hex.builtin.view.hashes.poly"_lang, &polynomial, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                                if (ImGui::IsItemEdited()) this->m_shouldInvalidate = true;

                                ImGui::NewLine();

                                static u32 result = 0;

                                if (this->m_shouldInvalidate)
                                    result = crypt::crc32(provider, this->m_hashRegion[0], this->m_hashRegion[1], polynomial, init, xorout, reflectIn, reflectOut);

                                char buffer[sizeof(result) * 2 + 1];
                                snprintf(buffer, sizeof(buffer), "%08X", result);

                                ImGui::TextUnformatted("hex.builtin.view.hashes.result"_lang);
                                ImGui::Separator();
                                ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                            }
                            break;
                        case HashFunctions::Md5:
                            {
                                static std::array<u8, 16> result = { 0 };

                                if (this->m_shouldInvalidate)
                                    result = crypt::md5(provider, this->m_hashRegion[0], this->m_hashRegion[1]);

                                char buffer[sizeof(result) * 2 + 1];
                                formatBigHexInt(result, buffer, sizeof(buffer));

                                ImGui::NewLine();
                                ImGui::TextUnformatted("hex.builtin.view.hashes.result"_lang);
                                ImGui::Separator();
                                ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                            }
                            break;
                        case HashFunctions::Sha1:
                            {
                                static std::array<u8, 20> result = { 0 };

                                if (this->m_shouldInvalidate)
                                    result = crypt::sha1(provider, this->m_hashRegion[0], this->m_hashRegion[1]);

                                char buffer[sizeof(result) * 2 + 1];
                                formatBigHexInt(result, buffer, sizeof(buffer));

                                ImGui::NewLine();
                                ImGui::TextUnformatted("hex.builtin.view.hashes.result"_lang);
                                ImGui::Separator();
                                ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                            }
                            break;
                        case HashFunctions::Sha224:
                            {
                                static std::array<u8, 28> result = { 0 };

                                if (this->m_shouldInvalidate)
                                    result = crypt::sha224(provider, this->m_hashRegion[0], this->m_hashRegion[1]);

                                char buffer[sizeof(result) * 2 + 1];
                                formatBigHexInt(result, buffer, sizeof(buffer));

                                ImGui::NewLine();
                                ImGui::TextUnformatted("hex.builtin.view.hashes.result"_lang);
                                ImGui::Separator();
                                ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                            }
                            break;
                        case HashFunctions::Sha256:
                            {
                                static std::array<u8, 32> result;

                                if (this->m_shouldInvalidate)
                                    result = crypt::sha256(provider, this->m_hashRegion[0], this->m_hashRegion[1]);

                                char buffer[sizeof(result) * 2 + 1];
                                formatBigHexInt(result, buffer, sizeof(buffer));

                                ImGui::NewLine();
                                ImGui::TextUnformatted("hex.builtin.view.hashes.result"_lang);
                                ImGui::Separator();
                                ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                            }
                            break;
                        case HashFunctions::Sha384:
                            {
                                static std::array<u8, 48> result;

                                if (this->m_shouldInvalidate)
                                    result = crypt::sha384(provider, this->m_hashRegion[0], this->m_hashRegion[1]);

                                char buffer[sizeof(result) * 2 + 1];
                                formatBigHexInt(result, buffer, sizeof(buffer));

                                ImGui::NewLine();
                                ImGui::TextUnformatted("hex.builtin.view.hashes.result"_lang);
                                ImGui::Separator();
                                ImGui::InputText("##nolabel", buffer, ImGuiInputTextFlags_ReadOnly);
                            }
                            break;
                        case HashFunctions::Sha512:
                            {
                                static std::array<u8, 64> result;

                                if (this->m_shouldInvalidate)
                                    result = crypt::sha512(provider, this->m_hashRegion[0], this->m_hashRegion[1]);

                                char buffer[sizeof(result) * 2 + 1];
                                formatBigHexInt(result, buffer, sizeof(buffer));

                                ImGui::NewLine();
                                ImGui::TextUnformatted("hex.builtin.view.hashes.result"_lang);
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

}
