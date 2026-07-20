#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/utils.hpp>
#include <functional>

namespace hex::plugin::builtin {

    class PopupPasteFromFileBehaviour final : public ViewHexEditor::Popup {
        public:
            explicit PopupPasteFromFileBehaviour(const ImHexApi::HexEditor::ProviderRegion &selection, 
                                                 const std::function<bool(const ImHexApi::HexEditor::ProviderRegion &src, 
                                                                          const ImHexApi::HexEditor::ProviderRegion &dst, 
                                                                          const u8 mode)> &pasteCallback);
            [[nodiscard]] UnlocalizedString getTitle() const override;
            [[nodiscard]] bool canBePinned() const override;
            void draw(ViewHexEditor *editor) override;

            enum class PasteModeType { 
                ModeNotSelected = 0, 
                ModePasteOverSelection, 
                ModePasteEverything, 
                ModeReplaceSelection, 
                ModeInsertEverything,
                ModeCount
            };

            enum class PasteHintType {
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

            enum class ObjIdType {
                SrcId = 0,
                DstId = 1
            };

            struct ProviderInfo {
                ObjIdType providerObjectId;
                i64 providerIndex = -1;
                bool providerValidity = false;
                ImHexApi::HexEditor::ProviderRegion providerSelection = {0, 0, nullptr};
                bool providerSelectionToggle = false;
            };

            void drawFileSelectionBox(void);
            void drawActionButtonsBox(ViewHexEditor *editor);
            void drawRegionSelectionBox(ViewHexEditor *editor);
            void drawPasteModeBox(void);
            void drawPasteInfoBox(void);
            void drawPasteWarningBox(void);
            void drawPasteDecisionBox(ViewHexEditor *editor);

            void drawFileProvider(ProviderInfo &object);
            void drawActionButtons(ProviderInfo &object, ViewHexEditor *editor);
            void drawRegionProvider(ProviderInfo &object, ViewHexEditor *editor);
            void drawFilePathToolTip(const prv::Provider *provider) const;
            void drawPasteModeButton(const char *buttonLabel, PasteModeType buttonMode, PasteHintType buttonHint, bool buttonDisable, float buttonWidth);
            void drawIntegerInputField(const char *inputLabel, void *inputValue, float inputWidth);

            void setSelection(u64 start, u64 end);
            std::string elapsedTimeFormatted(void) const;

        private:
            const float m_tableWidth = 600_scaled; // Set the same width for all tables which decides the popup width 
            bool m_modeRecommend = true;
            bool m_inputBaseHex = true;
            PasteModeType m_pasteMode = PasteModeType::ModeNotSelected;
            PasteHintType m_pasteHint = PasteHintType::HintDefaultDescription;
            std::chrono::nanoseconds m_elapsedTime{0};
            std::array<ProviderInfo, 2> m_fileProvider;
            u64 m_setSelectionStart = 0;
            u64 m_setSelectionEnd = 0;
            bool m_setSelectionTrigger = false;
            std::function<bool(const ImHexApi::HexEditor::ProviderRegion &src, 
                               const ImHexApi::HexEditor::ProviderRegion &dst, 
                               const u8 mode)> m_pasteCallback;
    };
}