#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>
#include <wolv/math_eval/math_evaluator.hpp>
#include <optional>
#include <string>

namespace hex::plugin::builtin {

    class PopupGoto : public ViewHexEditor::Popup {
    public:
        void draw(ViewHexEditor *editor) override;
        [[nodiscard]] UnlocalizedString getTitle() const override;
        bool canBePinned() const override;

    private:
        enum class Mode : u8 {
            Absolute,
            Relative,
            Begin,
            End
        };

        Mode m_mode = Mode::Absolute;
        std::optional<u64> m_newAddress;
        bool m_requestFocus = true;
        std::string m_input = "0x";
        wolv::math_eval::MathEvaluator<i128> m_evaluator;
    };
}