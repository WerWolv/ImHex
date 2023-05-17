#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>
#include <hex/helpers/crypto.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    class HashMD5 : public ContentRegistry::Hashes::Hash {
    public:
        HashMD5() : Hash("hex.builtin.hash.md5") {}

        Function create(std::string name) override {
            return Hash::create(name, [](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                auto array = crypt::md5(provider, region.address, region.size);

                return { array.begin(), array.end() };
            });
        }

        [[nodiscard]] nlohmann::json store() const override { return { }; }
        void load(const nlohmann::json &) override {}
    };

    class HashSHA1 : public ContentRegistry::Hashes::Hash {
    public:
        HashSHA1() : Hash("hex.builtin.hash.sha1") {}

        Function create(std::string name) override {
            return Hash::create(name, [](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                auto array = crypt::sha1(provider, region.address, region.size);

                return { array.begin(), array.end() };
            });
        }

        [[nodiscard]] nlohmann::json store() const override { return { }; }
        void load(const nlohmann::json &) override {}
    };

    class HashSHA224 : public ContentRegistry::Hashes::Hash {
    public:
        HashSHA224() : Hash("hex.builtin.hash.sha224") {}

        Function create(std::string name) override {
            return Hash::create(name, [](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                auto array = crypt::sha224(provider, region.address, region.size);

                return { array.begin(), array.end() };
            });
        }

        [[nodiscard]] nlohmann::json store() const override { return { }; }
        void load(const nlohmann::json &) override {}
    };

    class HashSHA256 : public ContentRegistry::Hashes::Hash {
    public:
        HashSHA256() : Hash("hex.builtin.hash.sha256") {}

        Function create(std::string name) override {
            return Hash::create(name, [](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                auto array = crypt::sha256(provider, region.address, region.size);

                return { array.begin(), array.end() };
            });
        }

        [[nodiscard]] nlohmann::json store() const override { return { }; }
        void load(const nlohmann::json &) override {}
    };

    class HashSHA384 : public ContentRegistry::Hashes::Hash {
    public:
        HashSHA384() : Hash("hex.builtin.hash.sha384") {}

        Function create(std::string name) override {
            return Hash::create(name, [](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                auto array = crypt::sha384(provider, region.address, region.size);

                return { array.begin(), array.end() };
            });
        }

        [[nodiscard]] nlohmann::json store() const override { return { }; }
        void load(const nlohmann::json &) override {}
    };

    class HashSHA512 : public ContentRegistry::Hashes::Hash {
    public:
        HashSHA512() : Hash("hex.builtin.hash.sha512") {}

        Function create(std::string name) override {
            return Hash::create(name, [](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                auto array = crypt::sha512(provider, region.address, region.size);

                return { array.begin(), array.end() };
            });
        }

        [[nodiscard]] nlohmann::json store() const override { return { }; }
        void load(const nlohmann::json &) override {}
    };

    template<typename T>
    class HashCRC : public ContentRegistry::Hashes::Hash {
    public:
        using CRCFunction = T(*)(prv::Provider*&, u64, size_t, u32, u32, u32, bool, bool);
        HashCRC(const std::string &name, const CRCFunction &crcFunction, u32 polynomial, u32 initialValue, u32 xorOut, bool reflectIn = false, bool reflectOut = false)
            : Hash(name), m_crcFunction(crcFunction), m_polynomial(polynomial), m_initialValue(initialValue), m_xorOut(xorOut), m_reflectIn(reflectIn), m_reflectOut(reflectOut) {}

        void draw() override {
            ImGui::InputHexadecimal("hex.builtin.hash.crc.poly"_lang, &this->m_polynomial);
            ImGui::InputHexadecimal("hex.builtin.hash.crc.iv"_lang, &this->m_initialValue);
            ImGui::InputHexadecimal("hex.builtin.hash.crc.xor_out"_lang, &this->m_xorOut);

            ImGui::NewLine();

            ImGui::Checkbox("hex.builtin.hash.crc.refl_in"_lang, &this->m_reflectIn);
            ImGui::Checkbox("hex.builtin.hash.crc.refl_out"_lang, &this->m_reflectOut);
        }

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                auto result = hash.m_crcFunction(provider, region.address, region.size, hash.m_polynomial, hash.m_initialValue, hash.m_xorOut, hash.m_reflectIn, hash.m_reflectOut);

                std::vector<u8> bytes(sizeof(result), 0x00);
                std::memcpy(bytes.data(), &result, bytes.size());

                return bytes;
            });
        }

        [[nodiscard]] nlohmann::json store() const override {
            nlohmann::json result;

            result["polynomial"] = this->m_polynomial;
            result["initialValue"] = this->m_initialValue;
            result["xorOut"] = this->m_xorOut;
            result["reflectIn"] = this->m_reflectIn;
            result["reflectOut"] = this->m_reflectOut;

            return result;
        }

        void load(const nlohmann::json &json) override {
            try {
                this->m_polynomial      = json.at("polynomial");
                this->m_initialValue    = json.at("initialValue");
                this->m_xorOut          = json.at("xorOut");
                this->m_reflectIn       = json.at("reflectIn");
                this->m_reflectOut      = json.at("reflectOut");
            } catch (std::exception&) { }
        }

    private:
        CRCFunction m_crcFunction;

        u32 m_polynomial;
        u32 m_initialValue;
        u32 m_xorOut;
        bool m_reflectIn = false, m_reflectOut = false;
    };

    void registerHashes() {
        ContentRegistry::Hashes::add<HashMD5>();

        ContentRegistry::Hashes::add<HashSHA1>();
        ContentRegistry::Hashes::add<HashSHA224>();
        ContentRegistry::Hashes::add<HashSHA256>();
        ContentRegistry::Hashes::add<HashSHA384>();
        ContentRegistry::Hashes::add<HashSHA512>();

        ContentRegistry::Hashes::add<HashCRC<u8>>("hex.builtin.hash.crc8",  crypt::crc8,  0x07,        0x0000,      0x0000);
        ContentRegistry::Hashes::add<HashCRC<u16>>("hex.builtin.hash.crc16", crypt::crc16, 0x8005,      0x0000,      0x0000);
        ContentRegistry::Hashes::add<HashCRC<u32>>("hex.builtin.hash.crc32", crypt::crc32, 0x04C1'1DB7, 0xFFFF'FFFF, 0xFFFF'FFFF);
        ContentRegistry::Hashes::add<HashCRC<u32>>("hex.builtin.hash.crc32mpeg",  crypt::crc32, 0x04C1'1DB7, 0xFFFF'FFFF, 0x0000'0000, false, false);
        ContentRegistry::Hashes::add<HashCRC<u32>>("hex.builtin.hash.crc32posix", crypt::crc32, 0x04C1'1DB7, 0x0000'0000, 0xFFFF'FFFF, false, false);
        ContentRegistry::Hashes::add<HashCRC<u32>>("hex.builtin.hash.crc32c",     crypt::crc32, 0x1EDC'6F41, 0xFFFF'FFFF, 0xFFFF'FFFF, true,  true);
    }

}