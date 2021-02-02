#pragma once

#include <hex/views/view.hpp>

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
        bool m_shouldInvalidate = true;
        int m_currHashFunction = 0;
        u64 m_hashRegion[2] = { 0 };
        bool m_shouldMatchSelection = false;

        static constexpr const char* HashFunctionNames[] = { "CRC16", "CRC32", "MD5", "SHA-1", "SHA-224", "SHA-256", "SHA-384", "SHA-512" };
    };

}