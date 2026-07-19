#pragma once

#include <hex/ui/popup.hpp>

#include <hex/providers/provider.hpp>

#include <functional>
#include <vector>

namespace hex::plugin::builtin {

    struct ProviderDirtyState {
        prv::Provider *provider;
        bool dataDirty;
        bool metadataDirty;
    };

    class PopupUnsavedChanges : public Popup<PopupUnsavedChanges> {
    public:
        PopupUnsavedChanges(std::vector<ProviderDirtyState> providers,
                            std::function<void()> saveDataFunction, std::function<void()> saveProjectFunction,
                            std::function<void()> discardFunction, std::function<void()> cancelFunction);

        void drawContent() override;
        [[nodiscard]] ImGuiWindowFlags getFlags() const override;
        [[nodiscard]] ImVec2 getMinSize() const override;
        [[nodiscard]] ImVec2 getMaxSize() const override;

    private:
        std::vector<ProviderDirtyState> m_providers;
        std::function<void()> m_saveDataFunction, m_saveProjectFunction, m_discardFunction, m_cancelFunction;
    };

}
