#include <script_api.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>
#include <hex/api/localization.hpp>

#include <hex/ui/popup.hpp>

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
        ImGui::TextFormattedWrapped("{}", this->m_message.c_str());
        ImGui::NewLine();
        ImGui::Separator();

        auto width = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX(width / 9);
        if (ImGui::Button("hex.builtin.common.yes"_lang, ImVec2(width / 3, 0))) {
            s_yesNoQuestionBoxResult = true;
            this->close();
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(width / 9 * 5);
        if (ImGui::Button("hex.builtin.common.no"_lang, ImVec2(width / 3, 0))) {
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
        ImGui::TextFormattedWrapped("{}", this->m_message.c_str());
        ImGui::NewLine();

        ImGui::SetItemDefaultFocus();
        ImGui::SetNextItemWidth(-1);
        bool submitted = ImGui::InputText("##input", this->m_input, ImGuiInputTextFlags_EnterReturnsTrue);
        if (this->m_input.size() > this->m_maxSize)
            this->m_input.resize(this->m_maxSize);

        ImGui::NewLine();
        ImGui::Separator();

        auto width = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX(width / 9);
        ImGui::BeginDisabled(this->m_input.empty());
        if (ImGui::Button("hex.builtin.common.okay"_lang, ImVec2(width / 3, 0)) || submitted) {
            s_inputTextBoxResult = this->m_input;
            this->close();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetCursorPosX(width / 9 * 5);
        if (ImGui::Button("hex.builtin.common.cancel"_lang, ImVec2(width / 3, 0))) {
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
    hex::EventManager::post<hex::RequestOpenInfoPopup>(message);
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