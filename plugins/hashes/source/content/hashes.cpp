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

            for (u64 address = region.getStartAddress(); address <= region.getEndAddress(); address += 1_MiB) {
                u64 readSize = std::min<u64>(1_MiB, (region.getEndAddress() - address) + 1);

                auto data = reader.read(address, readSize);
                hashFunction->TransformBytes({ data.begin(), data.end() }, 0, data.size());
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

    class HashCRC : public ContentRegistry::Hashes::Hash {
    public:
        HashCRC() : Hash("Cyclic Redundancy Check (CRC)") {
            m_crcs.push_back(HashFactory::Checksum::CreateCRC(3, 0, 0, false, false, 0, 0, { "hex.hashes.hash.common.standard.custom" }));

            for (CRCStandard standard = CRC3_GSM; standard < CRC64_XZ; standard = CRCStandard(int(standard) + 1)) {
                m_crcs.push_back(HashFactory::Checksum::CreateCRC(standard));
            }
        }

        void draw() override {
            if (ImGui::BeginCombo("hex.hashes.hash.common.standard"_lang, Lang(m_crcs[m_selectedCrc]->GetName()))) {
                for (size_t i = 0; i < m_crcs.size(); i++) {
                    const bool selected = m_selectedCrc == i;
                    if (ImGui::Selectable(Lang(m_crcs[i]->GetName()), selected))
                        m_selectedCrc = i;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            if (m_selectedCrc != 0) {
                const auto crc = dynamic_cast<const IICRC*>(m_crcs[m_selectedCrc].get());
                m_width = crc->GetWidth();
                m_polynomial = crc->GetPolynomial();
                m_initialValue = crc->GetInit();
                m_xorOut = crc->GetXOROut();
                m_reflectIn = crc->GetReflectIn();
                m_reflectOut = crc->GetReflectOut();
            }

            ImGui::BeginDisabled(m_selectedCrc != 0);
            ImGuiExt::InputHexadecimal("hex.hashes.hash.common.size"_lang, &m_width);
            ImGuiExt::InputHexadecimal("hex.hashes.hash.common.poly"_lang, &m_polynomial);
            ImGuiExt::InputHexadecimal("hex.hashes.hash.common.iv"_lang, &m_initialValue);
            ImGuiExt::InputHexadecimal("hex.hashes.hash.common.xor_out"_lang, &m_xorOut);

            ImGui::NewLine();

            ImGui::Checkbox("hex.hashes.hash.common.refl_in"_lang, &m_reflectIn);
            ImGui::Checkbox("hex.hashes.hash.common.refl_out"_lang, &m_reflectOut);
            ImGui::EndDisabled();
        }

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                auto crc = HashFactory::Checksum::CreateCRC(hash.m_width, hash.m_polynomial, hash.m_initialValue, hash.m_reflectIn, hash.m_reflectOut, hash.m_xorOut, 0, { "CRC" });

                crc->Initialize();

                auto bytes = hashProviderRegionWithHashLib(region, provider, crc);

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
        std::vector<IHash> m_crcs;
        size_t m_selectedCrc = 0;

        u32 m_width = 3;
        u64 m_polynomial = 0;
        u64 m_initialValue = 0;
        u64 m_xorOut = 0;
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

    class HashSum : public ContentRegistry::Hashes::Hash {
    public:
        HashSum() : Hash("hex.hashes.hash.sum") {}

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                std::array<u8, 8> result = { 0x00 };

                auto reader = prv::ProviderReader(provider);
                reader.seek(region.getStartAddress());
                reader.setEndAddress(region.getEndAddress());

                u64 sum = hash.m_initialValue;

                u8 progress = 0;
                for (u8 byte : reader) {
                    sum += (byte << (8 * progress));
                    progress += 1;

                    progress = progress % hash.m_inputSize;
                }

                u64 foldedSum = sum;
                if (hash.m_foldOutput) {
                    while (foldedSum >= (1LLU << (hash.m_outputSize * 8))) {
                        u64 partialSum = 0;
                        for (size_t i = 0; i < sizeof(u64); i += hash.m_inputSize) {
                            u64 value = 0;
                            std::memcpy(&value, reinterpret_cast<const u8*>(&foldedSum) + i, hash.m_inputSize);
                            partialSum += value;
                        }
                        foldedSum = partialSum;
                    }
                }

                std::memcpy(result.data(), &foldedSum, hash.m_outputSize);

                return { result.begin(), result.begin() + hash.m_outputSize };
            });
        }

        void draw() override {
            ImGuiExt::InputHexadecimal("hex.hashes.hash.common.iv"_lang, &m_initialValue);
            ImGui::SliderInt("hex.hashes.hash.common.input_size"_lang, &m_inputSize, 1, 8, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SliderInt("hex.hashes.hash.common.output_size"_lang, &m_outputSize, 1, 8, "%d", ImGuiSliderFlags_AlwaysClamp);
            ImGui::Checkbox("hex.hashes.hash.sum.fold"_lang, &m_foldOutput);
        }

        [[nodiscard]] nlohmann::json store() const override {
            nlohmann::json result;

            result["iv"] = m_initialValue;
            result["size"] = m_outputSize;

            return result;
        }

        void load(const nlohmann::json &data) override {
            try {
                m_initialValue = data.at("iv").get<int>();
                m_outputSize = data.at("size").get<int>();
            } catch (std::exception&) { }
        }

    private:
        u64 m_initialValue = 0x00;
        int m_inputSize = 1;
        int m_outputSize = 1;
        bool m_foldOutput = false;
    };

    class HashSnefru : public ContentRegistry::Hashes::Hash {
    public:
        using FactoryFunction = IHash(*)(Int32 a_security_level, const HashSize &a_hash_size);

        explicit HashSnefru(FactoryFunction function) : Hash("Snefru"), m_factoryFunction(function) {}
        void draw() override {
            ImGui::SliderInt("hex.hashes.hash.common.security_level"_lang, &m_securityLevel, 1, 1024);
            ImGui::Combo("hex.hashes.hash.common.size"_lang, &m_hashSize, "128 Bits\0" "256 Bits\0");

        }

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                u32 hashSize = 16;
                switch (hash.m_hashSize) {
                    case 0: hashSize = 16; break;
                    case 1: hashSize = 32; break;
                }

                IHash hashFunction = hash.m_factoryFunction(hash.m_securityLevel, HashSize(hashSize));

                hashFunction->Initialize();

                return hashProviderRegionWithHashLib(region, provider, hashFunction);
            });

        }

        [[nodiscard]] nlohmann::json store() const override {
            nlohmann::json result;

            result["securityLevel"] = m_securityLevel;
            result["size"] = m_hashSize;

            return result;
        }

        void load(const nlohmann::json &data) override {
            try {
                m_securityLevel = data.at("securityLevel").get<int>();
                m_hashSize = data.at("size").get<int>();
            } catch (std::exception&) { }
        }

    private:
        FactoryFunction m_factoryFunction;
        int m_securityLevel = 8;
        int m_hashSize = 0;
    };

    class HashHaval : public ContentRegistry::Hashes::Hash {
    public:
        using FactoryFunction = IHash(*)(const HashRounds& a_rounds, const HashSize& a_hash_size);

        explicit HashHaval(FactoryFunction function) : Hash("Haval"), m_factoryFunction(function) {}
        void draw() override {
            ImGui::Combo("hex.hashes.hash.common.rounds"_lang, &m_hashRounds, "3 Rounds\0" "4 Rounds\0" "5 Rounds\0");
            ImGui::Combo("hex.hashes.hash.common.size"_lang, &m_hashSize, "128 Bits\0" "160 Bits\0" "192 Bits\0" "224 Bits\0" "256 Bits\0");

        }

        Function create(std::string name) override {
            return Hash::create(name, [hash = *this](const Region& region, prv::Provider *provider) -> std::vector<u8> {
                u32 hashSize = 16;
                switch (hash.m_hashSize) {
                    case 0: hashSize = 16; break;
                    case 1: hashSize = 32; break;
                }

                u32 hashRounds = 3;
                switch (hash.m_hashRounds) {
                    case 0: hashRounds = 3; break;
                    case 1: hashRounds = 4; break;
                    case 2: hashRounds = 5; break;
                }

                IHash hashFunction = hash.m_factoryFunction(HashRounds(hashRounds), HashSize(hashSize));

                hashFunction->Initialize();

                return hashProviderRegionWithHashLib(region, provider, hashFunction);
            });

        }

        [[nodiscard]] nlohmann::json store() const override {
            nlohmann::json result;

            result["rounds"] = m_hashRounds;
            result["size"] = m_hashSize;

            return result;
        }

        void load(const nlohmann::json &data) override {
            try {
                m_hashRounds = data.at("rounds").get<int>();
                m_hashSize = data.at("size").get<int>();
            } catch (std::exception&) { }
        }

    private:
        FactoryFunction m_factoryFunction;
        int m_hashRounds = 0;
        int m_hashSize = 0;
    };

    void registerHashes() {
        ContentRegistry::Hashes::add<HashSum>();

        ContentRegistry::Hashes::add<HashCRC>();
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Checksum::CreateAdler32);

        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateMD2);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateMD4);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateMD5);

        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA0);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA1);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA2_224);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA2_256);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA2_384);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA2_512);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA2_512_224);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA2_512_256);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA3_224);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA3_256);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA3_384);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateSHA3_512);

        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateKeccak_224);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateKeccak_256);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateKeccak_288);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateKeccak_384);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateKeccak_512);


        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateGrindahl256);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateGrindahl512);

        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreatePanama);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateWhirlPool);

        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateRadioGatun32);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateRadioGatun64);

        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateGost);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateGOST3411_2012_256);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateGOST3411_2012_512);

        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateHAS160);

        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateRIPEMD);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateRIPEMD128);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateRIPEMD160);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateRIPEMD256);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Crypto::CreateRIPEMD320);

        ContentRegistry::Hashes::add<HashSnefru>(HashFactory::Crypto::CreateSnefru);
        ContentRegistry::Hashes::add<HashHaval>(HashFactory::Crypto::CreateHaval);

        ContentRegistry::Hashes::add<HashTiger>("Tiger", HashFactory::Crypto::CreateTiger);
        ContentRegistry::Hashes::add<HashTiger>("Tiger2", HashFactory::Crypto::CreateTiger2);

        ContentRegistry::Hashes::add<HashBlake2<Blake2BConfig, IBlake2BConfig, IBlake2BTreeConfig>>("Blake2b", HashFactory::Crypto::CreateBlake2B);
        ContentRegistry::Hashes::add<HashBlake2<Blake2SConfig, IBlake2SConfig, IBlake2STreeConfig>>("Blake2s", HashFactory::Crypto::CreateBlake2S);

        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateAP);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateBKDR);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateBernstein);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateBernstein1);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateDEK);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateDJB);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateELF);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateFNV1a_32);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateFNV32);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateJS);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateOneAtTime);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreatePJW);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateRotating);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateRS);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateSDBM);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateShiftAndXor);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash32::CreateSuperFast);

        ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash32::CreateMurmur2_32);
        ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash32::CreateMurmurHash3_x86_32);
        ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash32::CreateXXHash32);

        ContentRegistry::Hashes::add<HashInitialValue>(HashFactory::Hash32::CreateJenkins3);

        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash64::CreateFNV64);
        ContentRegistry::Hashes::add<HashBasic>(HashFactory::Hash64::CreateFNV1a_64);

        ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash64::CreateMurmur2_64);
        ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash64::CreateSipHash64_2_4);
        ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash64::CreateXXHash64);

        ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash128::CreateSipHash128_2_4);
        ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash128::CreateMurmurHash3_x86_128);
        ContentRegistry::Hashes::add<HashWithKey>(HashFactory::Hash128::CreateMurmurHash3_x64_128);
    }

}
