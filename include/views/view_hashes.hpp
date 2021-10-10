#pragma once

#include <hex/views/view.hpp>

#include <array>
#include <utility>
#include <cstdio>

namespace hex {

    namespace prv { class Provider; }

    class ViewHashes : public View {
    public:
        explicit ViewHashes();
        ~ViewHashes() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        enum class HashFunctions { Crc8, Crc16, Crc32, Md5, Sha1, Sha224, Sha256, Sha384, Sha512 };

        bool m_shouldInvalidate = true;
        int m_currHashFunction = 0;
        u64 m_hashRegion[2] = { 0 };
        bool m_shouldMatchSelection = false;

        static constexpr std::array hashFunctionNames {
            std::pair{HashFunctions::Crc8,   "CRC8"},
            std::pair{HashFunctions::Crc16,  "CRC16"},
            std::pair{HashFunctions::Crc32,  "CRC32"},
            std::pair{HashFunctions::Md5,    "MD5"},
            std::pair{HashFunctions::Sha1,   "SHA-1"},
            std::pair{HashFunctions::Sha224, "SHA-224"},
            std::pair{HashFunctions::Sha256, "SHA-256"},
            std::pair{HashFunctions::Sha384, "SHA-384"},
            std::pair{HashFunctions::Sha512, "SHA-512"},
        };
    };

}
