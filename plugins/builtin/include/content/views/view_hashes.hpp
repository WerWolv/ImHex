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
        static bool importHashes(prv::Provider *provider, const nlohmann::json &json);
        static bool exportHashes(prv::Provider *provider, nlohmann::json &json);

    private:
        ContentRegistry::Hashes::Hash *m_selectedHash = nullptr;
        std::string m_newHashName;
    };

}
