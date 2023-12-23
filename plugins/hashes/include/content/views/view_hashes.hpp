#pragma once

#include <hex/api/content_registry.hpp>

#include <hex/ui/view.hpp>

namespace hex::plugin::hashes {

    class ViewHashes : public View::Window {
    public:
        explicit ViewHashes();
        ~ViewHashes() override;

        void drawContent() override;

    private:
        bool importHashes(prv::Provider *provider, const nlohmann::json &json);
        bool exportHashes(prv::Provider *provider, nlohmann::json &json);

    private:
        ContentRegistry::Hashes::Hash *m_selectedHash = nullptr;
        std::string m_newHashName;

        PerProvider<std::vector<ContentRegistry::Hashes::Hash::Function>> m_hashFunctions;

    };

}
