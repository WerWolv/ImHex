#pragma once

#include <hex.hpp>

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

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
        virtual bool isAvailable() const;
        virtual bool shouldProcess() const { return this->isAvailable() && this->getWindowOpenState(); }

        static void doLater(std::function<void()> &&function);
        static std::vector<std::function<void()>> &getDeferedCalls();

        static void drawCommonInterfaces();

        static void showMessagePopup(const std::string &message);
        static void showErrorPopup(const std::string &errorMessage);
        static void showFatalPopup(const std::string &errorMessage);

        static void showFileChooserPopup(const std::vector<fs::path> &paths, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(fs::path)> &callback);

        virtual bool hasViewMenuItemEntry() const;
        virtual ImVec2 getMinSize() const;
        virtual ImVec2 getMaxSize() const;

        bool &getWindowOpenState();
        const bool &getWindowOpenState() const;

        [[nodiscard]] const std::string &getUnlocalizedName() const;
        [[nodiscard]] std::string getName() const;

        static void confirmButtons(const std::string &textLeft, const std::string &textRight, const std::function<void()> &leftButtonFn, const std::function<void()> &rightButtonFn);
        static void discardNavigationRequests();

        static inline std::string toWindowName(const std::string &unlocalizedName) {
            return LangEntry(unlocalizedName) + "###" + unlocalizedName;
        }

    private:
        std::string m_unlocalizedViewName;
        bool m_windowOpen = false;
        std::map<Shortcut, std::function<void()>> m_shortcuts;

        friend class ShortcutManager;
    };

}