#pragma once

#include <hex.hpp>

#include <imgui.h>

#include <hex/api/event.hpp>

#include <functional>
#include <string>
#include <vector>


namespace hex {

    class View {
    public:
        explicit View(std::string viewName);
        virtual ~View() = default;

        virtual void drawContent() = 0;
        virtual void drawMenu();
        virtual bool handleShortcut(int key, int mods);

        static void doLater(std::function<void()> &&function);
        static std::vector<std::function<void()>>& getDeferedCalls();

        static std::vector<std::any> postEvent(Events eventType, const std::any &userData = { });

        static void drawCommonInterfaces();

        static void showErrorPopup(std::string_view errorMessage);

        virtual bool hasViewMenuItemEntry();
        virtual ImVec2 getMinSize();
        virtual ImVec2 getMaxSize();

        bool& getWindowOpenState();

        std::string_view getName() const;

    protected:
        void subscribeEvent(Events eventType, const std::function<std::any(const std::any&)> &callback);
        void subscribeEvent(Events eventType, const std::function<void(const std::any&)> &callback);

        void unsubscribeEvent(Events eventType);


    protected:
        void confirmButtons(const char *textLeft, const char *textRight, const std::function<void()> &leftButtonFn, const std::function<void()> &rightButtonFn);

    private:
        std::string m_viewName;
        bool m_windowOpen = false;
    };

}