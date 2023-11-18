#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/fontawesome_font.h>
#include <fonts/codicons_font.h>

#include <hex/api/imhex_api.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/providers/provider.hpp>
#include <hex/providers/provider_data.hpp>
#include <hex/helpers/utils.hpp>

#include <hex/api/localization.hpp>

#include <functional>
#include <string>

namespace hex {

    class View {
    public:
        explicit View(std::string unlocalizedName);
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

        [[nodiscard]] bool didWindowJustOpen();
        void setWindowJustOpened(bool state);

        static void confirmButtons(const std::string &textLeft, const std::string &textRight, const std::function<void()> &leftButtonFn, const std::function<void()> &rightButtonFn);
        static void discardNavigationRequests();

        static std::string toWindowName(const std::string &unlocalizedName) {
            return LangEntry(unlocalizedName) + "###" + unlocalizedName;
        }

        static ImFontAtlas *getFontAtlas();
        static void setFontAtlas(ImFontAtlas *atlas);

        static ImFontConfig getFontConfig();
        static void setFontConfig(ImFontConfig config);

    private:
        std::string m_unlocalizedViewName;
        bool m_windowOpen = false;
        std::map<Shortcut, ShortcutManager::ShortcutEntry> m_shortcuts;
        bool m_windowJustOpened = false;

        friend class ShortcutManager;
    };

}