#include <script_api.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/ui/popup.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/ui/view.hpp>

#include <popups/popup_notification.hpp>
#include <toasts/toast_notification.hpp>

using namespace hex;

#define VERSION V1

static std::optional<std::string> s_inputTextBoxResult;
static std::optional<bool> s_yesNoQuestionBoxResult;

class PopupYesNo : public Popup<PopupYesNo> {
public:
    PopupYesNo(std::string title, std::string message)
            : hex::Popup<PopupYesNo>(std::move(title), false),
              m_message(std::move(message)) { }

    void drawContent() override {
        ImGuiExt::TextFormattedWrapped("{}", m_message.c_str());
        ImGui::NewLine();
        ImGui::Separator();

        auto width = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX(width / 9);
        if (ImGui::Button("hex.ui.common.yes"_lang, ImVec2(width / 3, 0))) {
            s_yesNoQuestionBoxResult = true;
            this->close();
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(width / 9 * 5);
        if (ImGui::Button("hex.ui.common.no"_lang, ImVec2(width / 3, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            s_yesNoQuestionBoxResult = false;
            this->close();
        }

        ImGui::SetWindowPos((ImHexApi::System::getMainWindowSize() - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
    }

    [[nodiscard]] ImGuiWindowFlags getFlags() const override {
        return ImGuiWindowFlags_AlwaysAutoResize;
    }

    [[nodiscard]] ImVec2 getMinSize() const override {
        return scaled({ 400, 100 });
    }

    [[nodiscard]] ImVec2 getMaxSize() const override {
        return scaled({ 600, 300 });
    }

private:
    std::string m_message;
};

class PopupInputText : public Popup<PopupInputText> {
public:
    PopupInputText(std::string title, std::string message, size_t maxSize)
            : hex::Popup<PopupInputText>(std::move(title), false),
              m_message(std::move(message)), m_maxSize(maxSize) { }

    void drawContent() override {
        ImGuiExt::TextFormattedWrapped("{}", m_message.c_str());
        ImGui::NewLine();

        ImGui::SetItemDefaultFocus();
        ImGui::SetNextItemWidth(-1);
        bool submitted = ImGui::InputText("##input", m_input, ImGuiInputTextFlags_EnterReturnsTrue);
        if (m_input.size() > m_maxSize)
            m_input.resize(m_maxSize);

        ImGui::NewLine();
        ImGui::Separator();

        auto width = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX(width / 9);
        ImGui::BeginDisabled(m_input.empty());
        if (ImGui::Button("hex.ui.common.okay"_lang, ImVec2(width / 3, 0)) || submitted) {
            s_inputTextBoxResult = m_input;
            this->close();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetCursorPosX(width / 9 * 5);
        if (ImGui::Button("hex.ui.common.cancel"_lang, ImVec2(width / 3, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            s_inputTextBoxResult = "";
            this->close();
        }

        ImGui::SetWindowPos((ImHexApi::System::getMainWindowSize() - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
    }

    [[nodiscard]] ImGuiWindowFlags getFlags() const override {
        return ImGuiWindowFlags_AlwaysAutoResize;
    }

    [[nodiscard]] ImVec2 getMinSize() const override {
        return scaled({ 400, 100 });
    }

    [[nodiscard]] ImVec2 getMaxSize() const override {
        return scaled({ 600, 300 });
    }

private:
    std::string m_message;
    std::string m_input;
    size_t m_maxSize;
};

SCRIPT_API(void showMessageBox, const char *message) {
    ui::PopupInfo::open(message);
}

SCRIPT_API(void showInputTextBox, const char *title, const char *message, char *buffer, u32 bufferSize) {
    PopupInputText::open(std::string(title), std::string(message), bufferSize - 1);

    while (!s_inputTextBoxResult.has_value()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto &value = s_inputTextBoxResult.value();
    std::memcpy(buffer, value.c_str(), std::min<size_t>(value.size() + 1, bufferSize));
    s_inputTextBoxResult.reset();
}

SCRIPT_API(void showYesNoQuestionBox, const char *title, const char *message, bool *result) {
    PopupYesNo::open(std::string(title), std::string(message));

    while (!s_yesNoQuestionBoxResult.has_value()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    *result = s_yesNoQuestionBoxResult.value();
    s_yesNoQuestionBoxResult.reset();
}

SCRIPT_API(void showToast, const char *message, u32 type) {
    switch (type) {
        case 0:
            ui::ToastInfo::open(message);
            break;
        case 1:
            ui::ToastWarning::open(message);
            break;
        case 2:
            ui::ToastError::open(message);
            break;
        default:
            break;
    }
}

SCRIPT_API(void* getImGuiContext) {
    return ImGui::GetCurrentContext();
}

class ScriptView : public View::Window {
public:
    using DrawFunction = void(*)();
    ScriptView(const char *icon, const char *name, DrawFunction function) : View::Window(UnlocalizedString(name), icon), m_drawFunction(function) { }
    void drawContent() override {
        m_drawFunction();
    }

private:
    DrawFunction m_drawFunction;
};

SCRIPT_API(void registerView, const char *icon, const char *name, void *drawFunction) {
    ContentRegistry::Views::add<ScriptView>(icon, name, ScriptView::DrawFunction(drawFunction));
}

SCRIPT_API(void addMenuItem, const char *icon, const char *menuName, const char *itemName, void *function) {
    using MenuFunction = void(*)();
    ContentRegistry::Interface::addMenuItem({ menuName, itemName }, icon, 9999, Shortcut::None, reinterpret_cast<MenuFunction>(function));
}