#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <imgui_imhex_extensions.h>

#include <fontawesome_font.h>
#include <nfd.hpp>

#include <hex/api/event.hpp>
#include <hex/providers/provider.hpp>

#include <functional>
#include <string>
#include <vector>


namespace hex {

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

        enum class DialogMode {
            Open,
            Save,
            Folder
        };

        static void openFileBrowser(std::string_view title, DialogMode mode, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::string)> &callback);
        static void doLater(std::function<void()> &&function);
        static std::vector<std::function<void()>>& getDeferedCalls();

        static void drawCommonInterfaces();

        static void showErrorPopup(std::string_view errorMessage);
        static void showFatalPopup(std::string_view errorMessage);

        virtual bool hasViewMenuItemEntry();
        virtual ImVec2 getMinSize();
        virtual ImVec2 getMaxSize();

        bool& getWindowOpenState();

        std::string_view getUnlocalizedName() const;

    protected:
        void discardNavigationRequests();

        void confirmButtons(const char *textLeft, const char *textRight, const std::function<void()> &leftButtonFn, const std::function<void()> &rightButtonFn);

        static inline std::string toWindowName(std::string_view unlocalizedName) {
            return LangEntry(unlocalizedName) + "##" + std::string(unlocalizedName);
        }

    private:
        std::string m_unlocalizedViewName;
        bool m_windowOpen = false;
    };

}