#pragma once

#include "views/view.hpp"

#include <cstdio>

namespace hex {

    class ViewHashes : public View {
    public:
        ViewHashes(FILE* &file);
        virtual ~ViewHashes();

        virtual void createView() override;
        virtual void createMenu() override;

    private:
        FILE* &m_file;
        bool m_windowOpen = true;

        int m_currHashFunction = 0;
        int m_hashStart = 0, m_hashEnd = 0;
        static constexpr const char* HashFunctionNames[] = { "CRC16", "CRC32", "MD5", "SHA-1", "SHA-256" };
    };

}