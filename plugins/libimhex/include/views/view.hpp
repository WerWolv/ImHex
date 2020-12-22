#pragma once

#include <hex.hpp>

#include "imgui.h"

#include "helpers/event.hpp"

#include <functional>
#include <string>
#include <vector>


namespace hex {

    class View {
    public:
        View(std::string viewName);
        virtual ~View() = default;

        virtual void drawContent() = 0;
        virtual void drawMenu();
        virtual bool handleShortcut(int key, int mods);

        static std::vector<std::function<void()>>& getDeferedCalls();

        static void postEvent(Events eventType, const void *userData = nullptr);

        static void drawCommonInterfaces();

        static void showErrorPopup(std::string_view errorMessage);

        static void setWindowPosition(s32 x, s32 y);

        static const ImVec2& getWindowPosition();

        static void setWindowSize(s32 width, s32 height);

        static const ImVec2& getWindowSize();

        virtual bool hasViewMenuItemEntry();
        virtual ImVec2 getMinSize();
        virtual ImVec2 getMaxSize();

        bool& getWindowOpenState();

        const std::string getName() const;

    protected:
        void subscribeEvent(Events eventType, std::function<void(const void*)> callback);

        void unsubscribeEvent(Events eventType);

        void doLater(std::function<void()> &&function);

    protected:
        void confirmButtons(const char *textLeft, const char *textRight, std::function<void()> leftButtonFn, std::function<void()> rightButtonFn);
    private:
        std::string m_viewName;
        bool m_windowOpen = false;

        static inline EventManager s_eventManager;
        static inline std::vector<std::function<void()>> s_deferedCalls;

        static inline std::string s_errorMessage;

        static inline ImVec2 s_windowPos;
        static inline ImVec2 s_windowSize;
    };

}