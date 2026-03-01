#include <hex/ui/view.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/tutorial_manager.hpp>

#include <imgui_internal.h>

#include <imgui.h>

#include <string>

namespace hex {

    static AutoReset<View*> s_lastFocusedView = nullptr;

    View::View(UnlocalizedString unlocalizedName, const char *icon) : m_unlocalizedViewName(std::move(unlocalizedName)), m_icon(icon) { }

    bool View::shouldDraw() const {
        return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isAvailable();
    }

    bool View::shouldProcess() const {
        return this->shouldDraw() && this->getWindowOpenState();
    }

    bool View::hasViewMenuItemEntry() const {
        return true;
    }

    ImVec2 View::getMinSize() const {
        return scaled({ 300, 400 });
    }

    ImVec2 View::getMaxSize() const {
        return { FLT_MAX, FLT_MAX };
    }

    ImGuiWindowFlags View::getWindowFlags() const {
        return ImGuiWindowFlags_None;
    }



    bool &View::getWindowOpenState() {
        return m_windowOpen;
    }

    const bool &View::getWindowOpenState() const {
        return m_windowOpen;
    }

    const UnlocalizedString &View::getUnlocalizedName() const {
        return m_unlocalizedViewName;
    }

    std::string View::getName() const {
        return View::toWindowName(m_unlocalizedViewName);
    }

    bool View::didWindowJustOpen() {
        return std::exchange(m_windowJustOpened, false);
    }

    void View::setWindowJustOpened(const bool state) {
        m_windowJustOpened = state;
    }

    bool View::didWindowJustClose() {
        return std::exchange(m_windowJustClosed, false);
    }

    void View::setWindowJustClosed(const bool state) {
        m_windowJustClosed = state;
    }

    void View::trackViewState() {
        if (m_windowOpen && !m_prevWindowOpen)
        {
            this->setWindowJustOpened(true);
            this->onOpen();
        } else if (!m_windowOpen && m_prevWindowOpen) {
            this->setWindowJustClosed(true);
            this->onClose();
        }
        m_prevWindowOpen = m_windowOpen;
    }


    void View::discardNavigationRequests() {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    }

    void View::bringToFront() {
        getWindowOpenState() = true;
        TaskManager::doLater([this]{ ImGui::SetWindowFocus(toWindowName(getUnlocalizedName()).c_str()); });
    }


    std::string View::toWindowName(const UnlocalizedString &unlocalizedName) {
        return fmt::format("{}###{}", Lang(unlocalizedName), unlocalizedName.get());
    }

    void View::setFocused(bool focused) {
        m_focused = focused;
        if (focused)
            s_lastFocusedView = this;
    }


    const View* View::getLastFocusedView() {
        if (!ImHexApi::Provider::isValid())
            return nullptr;

        return s_lastFocusedView;
    }


    void View::Window::draw(ImGuiWindowFlags extraFlags) {
        if (this->shouldDraw()) {
            const auto title = fmt::format("{} {}", this->getIcon(), View::toWindowName(this->getUnlocalizedName()));

            const ImGuiContext& g = *ImGui::GetCurrentContext();
            bool foundTopFocused = false;
            ImGuiWindow *imguiFocusedWindow = nullptr;
            ImGuiWindow *focusedSubWindow = nullptr;

            if (g.NavWindow != nullptr) {
                imguiFocusedWindow = g.NavWindow;
                foundTopFocused = true;
            }
            for (auto focusedWindow: g.WindowsFocusOrder | std::views::reverse) {
                if (focusedWindow == nullptr || !focusedWindow->WasActive)
                    continue;
                std::string focusedWindowName = focusedWindow->Name;
                if (!foundTopFocused) {
                    imguiFocusedWindow = focusedWindow;
                    foundTopFocused = true;
                }
                if (imguiFocusedWindow == nullptr || !focusedWindowName.contains("###hex.builtin.view."))
                    continue;
                if (auto focusedChild = focusedWindow->NavLastChildNavWindow; focusedChild != nullptr)
                    focusedSubWindow = focusedChild;
                else if (focusedWindow == focusedWindow->RootWindow)
                    focusedSubWindow = focusedWindow;

                break;
            }

            std::string imguiFocusedWindowName = "NULL";
            if (imguiFocusedWindow != nullptr)
                imguiFocusedWindowName.assign(imguiFocusedWindow->Name);

            std::string focusedSubWindowName;
            if (focusedSubWindow != nullptr || m_focusedSubWindow != nullptr) {
                focusedSubWindowName = focusedSubWindow != nullptr ? focusedSubWindow->Name : m_focusedSubWindow->Name;
                if (focusedSubWindow != nullptr && m_focusedSubWindow != nullptr) {
                    std::string_view windowName = m_focusedSubWindow->Name;
                    auto stringsVector = wolv::util::splitString(focusedSubWindowName, "/");
                    if (stringsVector.back().contains("resize") || (focusedSubWindow == focusedSubWindow->RootWindow && windowName.starts_with(focusedSubWindowName)))
                        focusedSubWindowName = windowName;
                    else
                        m_focusedSubWindow = focusedSubWindow;
                } else if (focusedSubWindow != nullptr)
                    m_focusedSubWindow = focusedSubWindow;

                bool windowAlreadyFocused = focusedSubWindowName == imguiFocusedWindowName;
                bool titleFocused = focusedSubWindowName.starts_with(title);

                if (titleFocused && !windowAlreadyFocused) {

                    bool windowMayNeedFocus = focusedSubWindowName.starts_with(imguiFocusedWindowName);
                    std::string activeName = g.ActiveIdWindow ? g.ActiveIdWindow->Name : "NULL";

                    if ((activeName == "NULL" || windowMayNeedFocus) && (imguiFocusedWindowName == "##MainMenuBar" || imguiFocusedWindowName.starts_with("ImHexDockSpace") || imguiFocusedWindowName.contains("###hex.builtin.view."))) {
                        if (m_focusedSubWindow == m_focusedSubWindow->RootWindow)
                            ImGui::FocusWindow(m_focusedSubWindow, ImGuiFocusRequestFlags_RestoreFocusedChild);
                        else
                            ImGui::FocusWindow(m_focusedSubWindow, ImGuiFocusRequestFlags_None);
                    }
                }
            }

            if (!allowScroll())
                extraFlags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

            ImGui::SetNextWindowSizeConstraints(this->getMinSize(), this->getMaxSize());
            if (ImGui::Begin(title.c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse | extraFlags | this->getWindowFlags())) {
                TutorialManager::setLastItemInteractiveHelpPopup([this]{ this->drawHelpText(); });
                this->drawContent();
            }
            ImGui::End();
        }
    }

    void View::Special::draw(ImGuiWindowFlags extraFlags) {
        std::ignore = extraFlags;

        if (this->shouldDraw()) {
            ImGui::SetNextWindowSizeConstraints(this->getMinSize(), this->getMaxSize());
            this->drawContent();
        }
    }

    void View::Floating::draw(ImGuiWindowFlags extraFlags) {
        Window::draw(extraFlags | ImGuiWindowFlags_NoDocking);
    }

    void View::Scrolling::draw(ImGuiWindowFlags extraFlags) {
        Window::draw(extraFlags);
    }

    void View::Modal::draw(ImGuiWindowFlags extraFlags) {
        if (this->shouldDraw()) {
            if (this->getWindowOpenState())
                ImGui::OpenPopup(View::toWindowName(this->getUnlocalizedName()).c_str());

            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
            ImGui::SetNextWindowSizeConstraints(this->getMinSize(), this->getMaxSize());
            const auto title = fmt::format("{} {}", this->getIcon(), View::toWindowName(this->getUnlocalizedName()));
            if (ImGui::BeginPopupModal(title.c_str(), this->hasCloseButton() ? &this->getWindowOpenState() : nullptr, ImGuiWindowFlags_NoCollapse | extraFlags | this->getWindowFlags())) {
                this->drawContent();

                ImGui::EndPopup();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                this->getWindowOpenState() = false;
        }
    }

    void View::FullScreen::draw(ImGuiWindowFlags extraFlags) {
        std::ignore = extraFlags;

        this->drawContent();
        this->drawAlwaysVisibleContent();
    }


}