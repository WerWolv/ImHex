#include <hex/ui/view.hpp>

#include <imgui.h>

#include <functional>
#include <string>

namespace hex {

    View::View(UnlocalizedString unlocalizedName) : m_unlocalizedViewName(std::move(unlocalizedName)) { }

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
        return this->m_windowOpen;
    }

    const bool &View::getWindowOpenState() const {
        return this->m_windowOpen;
    }

    const UnlocalizedString &View::getUnlocalizedName() const {
        return this->m_unlocalizedViewName;
    }

    std::string View::getName() const {
        return View::toWindowName(this->m_unlocalizedViewName);
    }

    bool View::didWindowJustOpen() {
        return std::exchange(this->m_windowJustOpened, false);
    }

    void View::setWindowJustOpened(bool state) {
        this->m_windowJustOpened = state;
    }

    void View::trackViewOpenState() {
        if (this->m_windowOpen && !this->m_prevWindowOpen)
            this->setWindowJustOpened(true);
        this->m_prevWindowOpen = this->m_windowOpen;
    }


    void View::discardNavigationRequests() {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    }

    std::string View::toWindowName(const UnlocalizedString &unlocalizedName) {
        return Lang(unlocalizedName) + "###" + unlocalizedName.get();
    }

}