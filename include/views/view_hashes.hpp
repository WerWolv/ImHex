#pragma once

#include "views/view.hpp"

#include <cstdio>

namespace hex {

    namespace prv { class Provider; }

    class ViewHashes : public View {
    public:
        explicit ViewHashes(prv::Provider* &dataProvider);
        ~ViewHashes() override;

        void createView() override;
        void createMenu() override;

    private:
        prv::Provider* &m_dataProvider;
        bool m_windowOpen = true;

        bool m_shouldInvalidate = true;
        int m_currHashFunction = 0;
        int m_hashStart = 0, m_hashEnd = 0;
        static constexpr const char* HashFunctionNames[] = { "CRC16", "CRC32", "MD4", "MD5", "SHA-1", "SHA-224", "SHA-256", "SHA-384", "SHA-512" };
    };

}