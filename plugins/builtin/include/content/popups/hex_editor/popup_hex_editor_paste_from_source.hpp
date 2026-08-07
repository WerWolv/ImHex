#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/utils.hpp>
#include <functional>

namespace hex::plugin::builtin {

    class PopupPasteFromSource final : public ViewHexEditor::Popup {
        public:
            explicit PopupPasteFromSource(const ImHexApi::HexEditor::ProviderRegion &selection);
            [[nodiscard]] UnlocalizedString getTitle() const override;
            [[nodiscard]] bool canBePinned() const override;
            void draw(ViewHexEditor *editor) override;

            enum class PasteModeType : u8 { 
                ModeNotSelected = 0, 
                ModePasteOverSelection, 
                ModePasteEverything, 
                ModeReplaceSelection, 
                ModeInsertEverything,
                ModeCount
            };

            enum class PasteHintType : u8 {
                HintDefaultDescription = 0,
                HintPasteOverSelection,
                HintPasteEverything,
                HintReplaceSelection,
                HintInsertEverything,
                HintReadOnlyDestination,
                HintSelectMode,
                HintPasteSuccess,
                HintPasteFailure
            };

            enum class ObjectIdType : u8 {
                SourceId = 0,
                DestId = 1
            };

            struct ProviderInfo {
                ObjectIdType providerObjectId;
                i64 providerIndex = -1;
                bool providerValidity = false;
                ImHexApi::HexEditor::ProviderRegion providerSelection = {0, 0, nullptr};
                bool providerSelectionToggle = false;
            };

            void drawProviderSelectionBox(void);
            void drawActionButtonsBox(ViewHexEditor *editor);
            void drawRegionSelectionBox(ViewHexEditor *editor);
            void drawPasteModeBox(void);
            void drawPasteInfoBox(void);
            void drawPasteDecisionBox(ViewHexEditor *editor);

            void drawProviderSelector(ProviderInfo &object);
            void drawActionButtons(ProviderInfo &object, ViewHexEditor *editor);
            void drawRegionInput(ProviderInfo &object, ViewHexEditor *editor);
            void drawPasteModeButton(const char *buttonLabel, PasteModeType buttonMode, PasteHintType buttonHint, bool buttonDisable, float buttonWidth);
            void drawIntegerInputField(const char *inputLabel, void *inputValue, float inputWidth);
            void drawPasteConfirmationPopup(ViewHexEditor *editor);

            void setSelection(u64 start, u64 end);
            bool executePasteOperation(void) const;
            std::string elapsedTimeFormatted(void) const;

        private:
            float m_getTableWidth(void) { return (600_scaled); } // Set the same width for all tables which decides the popup width 
            struct { ProviderInfo source; ProviderInfo dest; } m_providers;
            PasteModeType m_pasteMode = PasteModeType::ModeNotSelected;
            PasteHintType m_pasteHint = PasteHintType::HintDefaultDescription;
            bool m_inputBaseHex = true;
            bool m_modeRecommend = true;
            bool m_setSelectionTrigger = false;
            u64 m_setSelectionStart = 0;
            u64 m_setSelectionEnd = 0;
            std::chrono::nanoseconds m_elapsedTime{0};
    };
}