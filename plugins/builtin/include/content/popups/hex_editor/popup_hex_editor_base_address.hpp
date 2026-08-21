#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>

namespace hex::plugin::builtin {

    class PopupBaseAddress : public ViewHexEditor::Popup {
    public:
        explicit PopupBaseAddress(const ImHexApi::HexEditor::ProviderRegion &selection);
        void draw(ViewHexEditor *editor) override;
        [[nodiscard]] UnlocalizedString getTitle() const override;
        [[nodiscard]] bool isValid() const;

        void setBaseAddress() const;
        bool isNewAddressValid() const;

    private:
        ImHexApi::HexEditor::ProviderRegion m_selection;
        u64 m_baseAddress;
        u64 m_byteAddress;
        u64 m_byteOffset;
        bool m_setBaseAddressFlag;
        bool m_setByteAddressFlag;
        bool m_setByteAddressEnable;
    };
}