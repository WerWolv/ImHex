#include <hex/ui/view.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <hex/api/imhex_api/provider.hpp>
#include <hex/providers/provider.hpp>

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
            this->setWindowJustOpened(true);
        else if (!m_windowOpen && m_prevWindowOpen)
            this->setWindowJustClosed(true);
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


}