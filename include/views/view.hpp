#pragma once

#include <hex.hpp>

#include "imgui.h"

#include "event.hpp"

namespace hex {

    class View {
    public:
        View() { }
        virtual ~View() { }

        virtual void createView() = 0;
        virtual void createMenu() { }
        virtual bool handleShortcut(int key, int mods) { return false; }

    protected:
        void subscribeEvent(Events eventType, std::function<void(void*)> callback) {
            View::s_eventManager.subscribe(eventType, this, callback);
        }

        void unsubscribeEvent(Events eventType) {
            View::s_eventManager.unsubscribe(eventType, this);
        }

        void postEvent(Events eventType, void *userData = nullptr) {
            View::s_eventManager.post(eventType, userData);
        }

    private:
        static inline EventManager s_eventManager;
    };

}