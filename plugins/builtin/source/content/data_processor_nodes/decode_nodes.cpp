#include <algorithm>
#include <hex/api/content_registry.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/data_processor/node.hpp>

#include <mbedtls/cipher.h>
#include <mbedtls/error.h>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    class NodeCryptoAESDecrypt : public dp::Node {
    public:
        NodeCryptoAESDecrypt() : Node("hex.builtin.nodes.crypto.aes.header",
                                     { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.key"),
                                         dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.iv"),
                                         dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.nonce"),
                                         dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"),
                                         dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            ImGui::Combo("hex.builtin.nodes.crypto.aes.mode"_lang, &m_mode, "ECB\0CBC\0CFB128\0CTR\0GCM\0CCM\0OFB\0");
            ImGui::Combo("hex.builtin.nodes.crypto.aes.key_length"_lang, &m_keyLength, "128 Bits\000192 Bits\000256 Bits\000");
            ImGui::PopItemWidth();
        }

        void process() override {
            const auto mode = static_cast<crypt::AESMode>(m_mode);
            const auto keyLength = static_cast<crypt::KeyLength>(m_keyLength);

            const auto &key   = this->getBufferOnInput(0);
            const auto &iv    = this->getBufferOnInput(1);
            const auto &nonce = this->getBufferOnInput(2);
            const auto &input = this->getBufferOnInput(3);

            if (key.empty())
                throwNodeError("Key cannot be empty");

            if (input.empty())
                throwNodeError("Input cannot be empty");

            std::array<u8, 8> ivData = { 0 }, nonceData = { 0 };

            if (mode != crypt::AESMode::ECB) {
                if (iv.empty())
                    throwNodeError("IV cannot be empty");

                if (nonce.empty())
                    throwNodeError("Nonce cannot be empty");

                std::ranges::copy(iv, ivData.begin());
                std::ranges::copy(nonce, nonceData.begin());
            }

            auto output = crypt::aesDecrypt(mode, keyLength, key, nonceData, ivData, input);
            if (!output) {
                switch (output.error()) {
                    case CRYPTO_ERROR_INVALID_KEY_LENGTH:
                        throwNodeError("Invalid key length");
                    case CRYPTO_ERROR_INVALID_MODE:
                        throwNodeError("Invalid mode");
                    default: {
                        std::array<char, 128> errorBuffer = { 0 };
                        mbedtls_strerror(output.error(), errorBuffer.data(), errorBuffer.size());

                        throwNodeError(std::string(errorBuffer.data()));
                    }
                }
            }

            this->setBufferOnOutput(4, output.value());
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["data"]               = nlohmann::json::object();
            j["data"]["mode"]       = m_mode;
            j["data"]["key_length"] = m_keyLength;
        }

        void load(const nlohmann::json &j) override {
            m_mode      = j["data"]["mode"];
            m_keyLength = j["data"]["key_length"];
        }

    private:
        int m_mode      = 0;
        int m_keyLength = 0;
    };

    class NodeDecodingBase64 : public dp::Node {
    public:
        NodeDecodingBase64() : Node("hex.builtin.nodes.decoding.base64.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            auto output = crypt::decode64(input);

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeDecodingHex : public dp::Node {
    public:
        NodeDecodingHex() : Node("hex.builtin.nodes.decoding.hex.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            auto input = this->getBufferOnInput(0);

            std::erase_if(input, [](u8 c) { return std::isspace(c); });

            if (input.size() % 2 != 0)
                throwNodeError("Can't decode odd number of hex characters");

            std::vector<u8> output;
            for (u32 i = 0; i < input.size(); i += 2) {
                char c1 = static_cast<char>(std::tolower(input[i]));
                char c2 = static_cast<char>(std::tolower(input[i + 1]));

                if (!std::isxdigit(c1) || !isxdigit(c2))
                    throwNodeError("Can't decode non-hexadecimal character");

                u8 value;
                if (std::isdigit(c1))
                    value = (c1 - '0') << 4;
                else
                    value = ((c1 - 'a') + 0x0A) << 4;

                if (std::isdigit(c2))
                    value |= c2 - '0';
                else
                    value |= (c2 - 'a') + 0x0A;

                output.push_back(value);
            }

            this->setBufferOnOutput(1, output);
        }
    };

    void registerDecodeDataProcessorNodes() {
        ContentRegistry::DataProcessorNode::add<NodeDecodingBase64>("hex.builtin.nodes.decoding", "hex.builtin.nodes.decoding.base64");
        ContentRegistry::DataProcessorNode::add<NodeDecodingHex>("hex.builtin.nodes.decoding", "hex.builtin.nodes.decoding.hex");
        ContentRegistry::DataProcessorNode::add<NodeCryptoAESDecrypt>("hex.builtin.nodes.crypto", "hex.builtin.nodes.crypto.aes");
    }

}
