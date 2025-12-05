#include "content/popups/hex_editor/popup_hex_editor_goto.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>

#include <content/differing_byte_searcher.hpp>

#include <toasts/toast_notification.hpp>

#include <fonts/vscode_icons.hpp>

#include <pl/patterns/pattern.hpp>

namespace hex::plugin::builtin {

    void PopupGoto::draw(ViewHexEditor *editor) {
        bool updateAddress = false;
        if (ImGui::BeginTabBar("goto_tabs")) {
            if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.absolute"_lang)) {
                m_mode = Mode::Absolute;
                updateAddress = true;
                ImGui::EndTabItem();
            }

            ImGui::BeginDisabled(!editor->isSelectionValid());
            if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.relative"_lang)) {
                m_mode = Mode::Relative;
                updateAddress = true;
                ImGui::EndTabItem();
            }
            ImGui::EndDisabled();

            if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.begin"_lang)) {
                m_mode = Mode::Begin;
                updateAddress = true;
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.end"_lang)) {
                m_mode = Mode::End;
                updateAddress = true;
                ImGui::EndTabItem();
            }

            if (m_requestFocus){
                ImGui::SetKeyboardFocusHere();
                m_requestFocus = false;
            }

            if (ImGuiExt::InputTextIcon("##input", ICON_VS_SYMBOL_OPERATOR, m_input)) {
                updateAddress = true;
            }

            if (updateAddress) {
                if (auto result = m_evaluator.evaluate(m_input); result.has_value()) {
                    const auto inputResult = u64(result.value());
                    auto provider = ImHexApi::Provider::get();

                    switch (m_mode) {
                        case Mode::Absolute: {
                            m_newAddress = inputResult;
                        }
                            break;
                        case Mode::Relative: {
                            const auto selection = editor->getSelection();
                            m_newAddress = selection.getStartAddress() + inputResult;
                        }
                            break;
                        case Mode::Begin: {
                            m_newAddress = provider->getBaseAddress() + provider->getCurrentPageAddress() + inputResult;
                        }
                            break;
                        case Mode::End: {
                            m_newAddress = provider->getActualSize() - inputResult;
                        }
                            break;
                    }
                } else {
                    m_newAddress.reset();
                }
            }

            bool isOffsetValid = m_newAddress <= ImHexApi::Provider::get()->getActualSize();

            bool executeGoto = false;

            if (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                executeGoto = true;
            }

            ImGui::BeginDisabled(!m_newAddress.has_value() || !isOffsetValid);
            {
                const auto label = fmt::format("{} {}", "hex.builtin.view.hex_editor.menu.file.goto"_lang, m_newAddress.has_value() ? fmt::format("0x{:08X}", *m_newAddress) : "???");
                const auto buttonWidth = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2;
                if (ImGuiExt::DimmedButton(label.c_str(), ImVec2(buttonWidth, 0))) {
                    executeGoto = true;
                }
            }
            ImGui::EndDisabled();

            if (executeGoto && m_newAddress.has_value()) {
                editor->setSelection(*m_newAddress, *m_newAddress);
                editor->jumpToSelection();

                if (!this->isPinned())
                    editor->closePopup();
            }

            ImGui::EndTabBar();
        }
    }

    UnlocalizedString PopupGoto::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.file.goto";
    }

    bool PopupGoto::canBePinned() const {
        return true;
    }

}