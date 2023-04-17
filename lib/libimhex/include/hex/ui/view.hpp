#pragma once

#include <hex.hpp>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/fontawesome_font.h>
#include <fonts/codicons_font.h>

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>
#include <hex/providers/provider.hpp>
#include <hex/providers/provider_data.hpp>
#include <hex/helpers/utils.hpp>

#include <hex/api/localization.hpp>

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
        [[nodiscard]] virtual bool isAvailable() const;
        [[nodiscard]] virtual bool shouldProcess() const { return this->isAvailable() && this->getWindowOpenState(); }

        [[nodiscard]] virtual bool hasViewMenuItemEntry() const;
        [[nodiscard]] virtual ImVec2 getMinSize() const;
        [[nodiscard]] virtual ImVec2 getMaxSize() const;

        [[nodiscard]] bool &getWindowOpenState();
        [[nodiscard]] const bool &getWindowOpenState() const;

        [[nodiscard]] const std::string &getUnlocalizedName() const;
        [[nodiscard]] std::string getName() const;

        static void confirmButtons(const std::string &textLeft, const std::string &textRight, const std::function<void()> &leftButtonFn, const std::function<void()> &rightButtonFn);
        static void discardNavigationRequests();

        static inline std::string toWindowName(const std::string &unlocalizedName) {
            return LangEntry(unlocalizedName) + "###" + unlocalizedName;
        }

        static ImFontAtlas *getFontAtlas() { return View::s_fontAtlas; }
        static void setFontAtlas(ImFontAtlas *atlas) { View::s_fontAtlas = atlas; }

        static ImFontConfig getFontConfig() { return View::s_fontConfig; }
        static void setFontConfig(ImFontConfig config) { View::s_fontConfig = config; }

    private:
        std::string m_unlocalizedViewName;
        bool m_windowOpen = false;
        std::map<Shortcut, std::function<void()>> m_shortcuts;

        static ImFontAtlas *s_fontAtlas;
        static ImFontConfig s_fontConfig;

        friend class ShortcutManager;
    };

}