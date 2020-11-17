#pragma once

#include <hex.hpp>

#include "imgui.h"

#include "event.hpp"

#include <functional>
#include <vector>


namespace hex {

    class View {
    public:
        View() { }
        virtual ~View() { }

        virtual void createView() = 0;
        virtual void createMenu() { }
        virtual bool handleShortcut(int key, int mods) { return false; }

        static std::vector<std::function<void()>>& getDeferedCalls() {
            return View::s_deferedCalls;
        }

        static void postEvent(Events eventType, const void *userData = nullptr) {
            View::s_eventManager.post(eventType, userData);
        }

    protected:
        void subscribeEvent(Events eventType, std::function<void(const void*)> callback) {
            View::s_eventManager.subscribe(eventType, this, callback);
        }

        void unsubscribeEvent(Events eventType) {
            View::s_eventManager.unsubscribe(eventType, this);
        }

        void doLater(std::function<void()> &&function) {
            View::s_deferedCalls.push_back(function);
        }

    private:
        static inline EventManager s_eventManager;
        static inline std::vector<std::function<void()>> s_deferedCalls;
    };

}