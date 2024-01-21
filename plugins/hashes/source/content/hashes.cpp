#include <hex/api/content_registry.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

#include <nlohmann/json.hpp>

#include <wolv/literals.hpp>

#include <HashFactory.h>

namespace hex::plugin::hashes {

    namespace {

        using namespace wolv::literals;

        std::vector<u8> hashProviderRegionWithHashLib(const Region& region, prv::Provider *provider, auto &hashFunction) {
            auto reader = prv::ProviderReader(provider);
            reader.seek(region.getStartAddress());
            reader.setEndAddress(region.getEndAddress());

            for (u64 address = region.getStartAddress(); address < region.getEndAddress(); address += 1_MiB) {
                u64 readSize = std::min<u64>(1_MiB, (region.getEndAddress() - address) + 1);

                auto data = reader.read(address, readSize);
                hashFunction->TransformBytes({ data.begin(), data.end() }, address - region.getStartAddress(), data.size());
            }

            auto result = hashFunction->TransformFinal();

            auto bytes = result->GetBytes();
            return { bytes.begin(), bytes.end() };
        }

    }

    class HashMD5 : public ContentRegistry::Hashes::Hash {
    public:
        HashMD5() : Hash("hex.hashes.hash.md5") {}

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
        HashSHA1() : Hash("hex.hashes.hash.sha1") {}

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
        HashSHA224() : Hash("hex.hashes.hash.sha224") {}

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
        HashSHA256() : Hash("hex.hashes.hash.sha256") {}

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
        HashSHA384() : Hash("hex.hashes.hash.sha384") {}

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
        HashSHA512() : Hash("hex.hashes.hash.sha512") {}

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
            ImGuiExt::InputHexadecimal("hex.hashes.hash.common.poly"_lang, &m_polynomial);
            ImGuiExt::InputHexadecimal("hex.hashes.hash.common.iv"_lang, &m_initialValue);
            ImGuiExt::InputHexadecimal("hex.hashes.hash.common.xor_out"_lang, &m_xorOut);

            ImGui::NewLine();

            ImGui::Checkbox("hex.hashes.hash.common.refl_in"_lang, &m_reflectIn);
            ImGui::Checkbox("hex.hashes.hash.common.refl_out"_lang, &m_reflectOut);
        }

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                auto result = hash.m_crcFunction(provider, region.address, region.size, hash.m_polynomial, hash.m_initialValue, hash.m_xorOut, hash.m_reflectIn, hash.m_reflectOut);

                std::vector<u8> bytes(sizeof(result), 0x00);
                std::memcpy(bytes.data(), &result, bytes.size());

                if constexpr (std::endian::native == std::endian::little)
                    std::reverse(bytes.begin(), bytes.end());

                return bytes;
            });
        }

        [[nodiscard]] nlohmann::json store() const override {
            nlohmann::json result;

            result["polynomial"] = m_polynomial;
            result["initialValue"] = m_initialValue;
            result["xorOut"] = m_xorOut;
            result["reflectIn"] = m_reflectIn;
            result["reflectOut"] = m_reflectOut;

            return result;
        }

        void load(const nlohmann::json &json) override {
            try {
                m_polynomial      = json.at("polynomial");
                m_initialValue    = json.at("initialValue");
                m_xorOut          = json.at("xorOut");
                m_reflectIn       = json.at("reflectIn");
                m_reflectOut      = json.at("reflectOut");
            } catch (std::exception&) { }
        }

    private:
        CRCFunction m_crcFunction;

        u32 m_polynomial;
        u32 m_initialValue;
        u32 m_xorOut;
        bool m_reflectIn = false, m_reflectOut = false;
    };

    class HashBasic : public ContentRegistry::Hashes::Hash {
    public:
        using FactoryFunction = IHash(*)();

        explicit HashBasic(FactoryFunction function) : Hash(function()->GetName()), m_factoryFunction(function) {}

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                IHash hashFunction = hash.m_factoryFunction();

                hashFunction->Initialize();

                return hashProviderRegionWithHashLib(region, provider, hashFunction);
            });

        }

        [[nodiscard]] nlohmann::json store() const override { return { }; }
        void load(const nlohmann::json &) override {}

    private:
        FactoryFunction m_factoryFunction;
    };

    class HashWithKey : public ContentRegistry::Hashes::Hash {
    public:
        using FactoryFunction = IHashWithKey(*)();

        explicit HashWithKey(FactoryFunction function) : Hash(function()->GetName()), m_factoryFunction(function) {}

        void draw() override {
            ImGui::InputText("hex.hashes.hash.common.key"_lang, m_key, ImGuiInputTextFlags_CharsHexadecimal);
        }

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this, key = hex::parseByteString(m_key)](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                IHashWithKey hashFunction = hash.m_factoryFunction();

                hashFunction->Initialize();
                hashFunction->SetKey(key);

                return hashProviderRegionWithHashLib(region, provider, hashFunction);
            });

        }

        [[nodiscard]] nlohmann::json store() const override {
            nlohmann::json result;

            result["key"] = m_key;

            return result;
        }

        void load(const nlohmann::json &data) override {
            try {
                m_key = data.at("key").get<std::string>();
            } catch (std::exception&) { }
        }

    private:
        FactoryFunction m_factoryFunction;

        std::string m_key;
    };

    class HashInitialValue : public ContentRegistry::Hashes::Hash {
    public:
        using FactoryFunction = IHash(*)(const Int32);

        explicit HashInitialValue(FactoryFunction function) : Hash(function(0)->GetName()), m_factoryFunction(function) {}

        void draw() override {
            ImGuiExt::InputHexadecimal("hex.hashes.hash.common.iv"_lang, &m_initialValue);
        }

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                IHash hashFunction = hash.m_factoryFunction(Int32(hash.m_initialValue));

                hashFunction->Initialize();

                return hashProviderRegionWithHashLib(region, provider, hashFunction);
            });

        }

        [[nodiscard]] nlohmann::json store() const override {
            nlohmann::json result;

            result["iv"] = m_initialValue;

            return result;
        }

        void load(const nlohmann::json &data) override {
            try {
                m_initialValue = data.at("iv").get<u32>();
            } catch (std::exception&) { }
        }

    private:
        FactoryFunction m_factoryFunction;
        u32 m_initialValue = 0x00;
    };

    class HashTiger : public ContentRegistry::Hashes::Hash {
    public:
        using FactoryFunction = IHash(*)(const Int32, const HashRounds&);

        explicit HashTiger(std::string name, FactoryFunction function) : Hash(std::move(name)), m_factoryFunction(function) {}
        void draw() override {
            ImGui::Combo("hex.hashes.hash.common.size"_lang, &m_hashSize, "128 Bits\0" "160 Bits\0" "192 Bits\0");
            ImGui::Combo("hex.hashes.hash.common.rounds"_lang, &m_hashRounds, "3 Rounds\0" "4 Rounds\0" "5 Rounds\0" "8 Rounds\0");
        }

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                Int32 hashSize = 16;
                switch (hash.m_hashSize) {
                    case 0: hashSize = 16; break;
                    case 1: hashSize = 20; break;
                    case 2: hashSize = 24; break;
                }

                HashRounds hashRounds = HashRounds::Rounds3;
                switch (hash.m_hashRounds) {
                    case 0: hashRounds = HashRounds::Rounds3; break;
                    case 1: hashRounds = HashRounds::Rounds4; break;
                    case 2: hashRounds = HashRounds::Rounds5; break;
                    case 3: hashRounds = HashRounds::Rounds8; break;
                }

                IHash hashFunction = hash.m_factoryFunction(hashSize, hashRounds);

                hashFunction->Initialize();

                return hashProviderRegionWithHashLib(region, provider, hashFunction);
            });

        }

        [[nodiscard]] nlohmann::json store() const override {
            nlohmann::json result;

            result["size"] = m_hashSize;
            result["rounds"] = m_hashRounds;

            return result;
        }

        void load(const nlohmann::json &data) override {
            try {
                m_hashSize = data.at("size").get<int>();
                m_hashRounds = data.at("rounds").get<int>();
            } catch (std::exception&) { }
        }

    private:
        FactoryFunction m_factoryFunction;

        int m_hashSize = 0, m_hashRounds = 0;
    };

    template<typename Config, typename T1, typename T2>
    class HashBlake2 : public ContentRegistry::Hashes::Hash {
    public:
        using FactoryFunction = IHash(*)(T1 a_Config, T2 a_TreeConfig);

        explicit HashBlake2(std::string name, FactoryFunction function) : Hash(std::move(name)), m_factoryFunction(function) {}
        void draw() override {
            ImGui::InputText("hex.hashes.hash.common.salt"_lang, m_salt, ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::InputText("hex.hashes.hash.common.key"_lang, m_key, ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::InputText("hex.hashes.hash.common.personalization"_lang, m_personalization, ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::Combo("hex.hashes.hash.common.size"_lang, &m_hashSize, "128 Bits\0" "160 Bits\0" "192 Bits\0" "224 Bits\0" "256 Bits\0" "288 Bits\0" "384 Bits\0" "512 Bits\0");

        }

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this, key = hex::parseByteString(m_key), salt = hex::parseByteString(m_salt), personalization = hex::parseByteString(m_personalization)](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                u32 hashSize = 16;
                switch (hash.m_hashSize) {
                    case 0: hashSize = 16; break;
                    case 1: hashSize = 20; break;
                    case 2: hashSize = 24; break;
                    case 3: hashSize = 28; break;
                    case 4: hashSize = 32; break;
                    case 5: hashSize = 36; break;
                    case 6: hashSize = 48; break;
                    case 7: hashSize = 64; break;
                }

                auto config = Config::GetDefaultConfig();
                config->SetKey(key);
                config->SetSalt(salt);
                config->SetPersonalization(personalization);
                config->SetHashSize(hashSize);
                IHash hashFunction = hash.m_factoryFunction(config, nullptr);

                hashFunction->Initialize();

                return hashProviderRegionWithHashLib(region, provider, hashFunction);
            });

        }

        [[nodiscard]] nlohmann::json store() const override {
            nlohmann::json result;

            result["salt"] = m_salt;
            result["key"] = m_key;
            result["personalization"] = m_personalization;
            result["size"] = m_hashSize;

            return result;
        }

        void load(const nlohmann::json &data) override {
            try {
                m_hashSize = data.at("size").get<int>();
                m_salt = data.at("salt").get<std::string>();
                m_key = data.at("key").get<std::string>();
                m_personalization = data.at("personalization").get<std::string>();
            } catch (std::exception&) { }
        }

    private:
        FactoryFunction m_factoryFunction;
        std::string m_salt, m_key, m_personalization;
        int m_hashSize = 0;
    };

    void registerHashes() {
        ContentRegistry::Hashes::add<HashMD5>();

        ContentRegistry::Hashes::add<HashSHA1>();
        ContentRegistry::Hashes::add<HashSHA224>();
        ContentRegistry::Hashes::add<HashSHA256>();
        ContentRegistry::Hashes::add<HashSHA384>();
        ContentRegistry::Hashes::add<HashSHA512>();

        ContentRegistry::Hashes::add<HashCRC<u8>>("hex.hashes.hash.crc8",        crypt::crc8,  0x07,        0x0000,      0x0000);
        ContentRegistry::Hashes::add<HashCRC<u16>>("hex.hashes.hash.crc16",      crypt::crc16, 0x8005,      0x0000,      0x0000);
        ContentRegistry::Hashes::add<HashCRC<u32>>("hex.hashes.hash.crc32",      crypt::crc32, 0x04C1'1DB7, 0xFFFF'FFFF, 0xFFFF'FFFF, true, true);
        ContentRegistry::Hashes::add<HashCRC<u32>>("hex.hashes.hash.crc32mpeg",  crypt::crc32, 0x04C1'1DB7, 0xFFFF'FFFF, 0x0000'0000, false, false);
        ContentRegistry::Hashes::add<HashCRC<u32>>("hex.hashes.hash.crc32posix", crypt::crc32, 0x04C1'1DB7, 0x0000'0000, 0xFFFF'FFFF, false, false);
        ContentRegistry::Hashes::add<HashCRC<u32>>("hex.hashes.hash.crc32c",     crypt::crc32, 0x1EDC'6F41, 0xFFFF'FFFF, 0xFFFF'FFFF, true,  true);

        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Checksum::CreateAdler32);

        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateAP);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateBKDR);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateBernstein);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateBernstein1);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateDEK);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateDJB);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateELF);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateFNV1a_32);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateFNV32);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateJS);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateOneAtTime);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreatePJW);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateRotating);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateRS);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateSDBM);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateShiftAndXor);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateSuperFast);

        hex::ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash32::CreateMurmur2_32);
        hex::ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash32::CreateMurmurHash3_x86_32);
        hex::ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash32::CreateXXHash32);

        hex::ContentRegistry::Hashes::add<HashInitialValue>(HashFactory::Hash32::CreateJenkins3);

        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash64::CreateFNV64);
        hex::ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash64::CreateFNV1a_64);

        hex::ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash64::CreateMurmur2_64);
        hex::ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash64::CreateSipHash64_2_4);
        hex::ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash64::CreateXXHash64);

        hex::ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash128::CreateSipHash128_2_4);
        hex::ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash128::CreateMurmurHash3_x86_128);
        hex::ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash128::CreateMurmurHash3_x64_128);

        hex::ContentRegistry::Hashes::add<HashTiger>("hex.hashes.hash.tiger", HashFactory::Crypto::CreateTiger);
        hex::ContentRegistry::Hashes::add<HashTiger>("hex.hashes.hash.tiger2", HashFactory::Crypto::CreateTiger2);

        hex::ContentRegistry::Hashes::add<HashBlake2<Blake2BConfig, IBlake2BConfig, IBlake2BTreeConfig>>("hex.hashes.hash.blake2b", HashFactory::Crypto::CreateBlake2B);
        hex::ContentRegistry::Hashes::add<HashBlake2<Blake2SConfig, IBlake2SConfig, IBlake2STreeConfig>>("hex.hashes.hash.blake2s", HashFactory::Crypto::CreateBlake2S);
    }

}