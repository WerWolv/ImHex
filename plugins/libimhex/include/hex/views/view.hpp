#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <imgui_imhex_extensions.h>

#include <fontawesome_font.h>
#include <codicons_font.h>

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>
#include <hex/providers/provider.hpp>

#include <hex/helpers/lang.hpp>

#include <functional>
#include <string>
#include <vector>


namespace hex {

    using namespace hex::lang_literals;

    class View {
    public:
        explicit View(std::string unlocalizedViewName);
        virtual ~View() = default;

        virtual void drawContent() = 0;
        virtual void drawAlwaysVisible() { }
        virtual void drawMenu();
        virtual bool handleShortcut(bool keys[512], bool ctrl, bool shift, bool alt);
        virtual bool isAvailable();
        virtual bool shouldProcess() { return this->isAvailable() && this->getWindowOpenState(); }

        static void doLater(std::function<void()> &&function);
        static std::vector<std::function<void()>>& getDeferedCalls();

        static void drawCommonInterfaces();

        static void showErrorPopup(const std::string &errorMessage);
        static void showFatalPopup(const std::string &errorMessage);

        virtual bool hasViewMenuItemEntry();
        virtual ImVec2 getMinSize();
        virtual ImVec2 getMaxSize();

        bool& getWindowOpenState();

        const std::string& getUnlocalizedName() const;

    protected:
        void discardNavigationRequests();

        void confirmButtons(const std::string &textLeft, const std::string &textRight, const std::function<void()> &leftButtonFn, const std::function<void()> &rightButtonFn);

        static inline std::string toWindowName(const std::string &unlocalizedName) {
            return LangEntry(unlocalizedName) + "###" + unlocalizedName;
        }

    private:
        std::string m_unlocalizedViewName;
        bool m_windowOpen = false;
    };

}