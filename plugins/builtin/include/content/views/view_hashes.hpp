#pragma once

#include <hex/api/content_registry.hpp>

#include <hex/ui/view.hpp>

#include <array>
#include <utility>
#include <cstdio>

namespace hex::plugin::builtin {

    class ViewHashes : public View {
    public:
        explicit ViewHashes();
        ~ViewHashes() override;

        void drawContent() override;

    private:
        ContentRegistry::Hashes::Hash *m_selectedHash = nullptr;
        std::string m_newHashName;

        std::vector<ContentRegistry::Hashes::Hash::Function> m_hashFunctions;
    };

}
