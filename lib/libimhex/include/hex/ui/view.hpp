#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/api/shortcut_manager.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/providers/provider_data.hpp>

#include <hex/helpers/scaling.hpp>

#include <map>
#include <string>

namespace hex {

    class View {
        explicit View(UnlocalizedString unlocalizedName, const char *icon);
    public:
        virtual ~View() = default;

        /**
         * @brief Draws the view
         * @note Do not override this method. Override drawContent() instead
         */
        virtual void draw(ImGuiWindowFlags extraFlags = ImGuiWindowFlags_None) = 0;

        /**
         * @brief Draws the content of the view
         */
        virtual void drawContent() = 0;

        /**
         * @brief Draws content that should always be visible, even if the view is not open
         */
        virtual void drawAlwaysVisibleContent() { }

        /**
         * @brief Whether or not the view window should be drawn
         * @return True if the view window should be drawn, false otherwise
         */
        [[nodiscard]] virtual bool shouldDraw() const;

        /**
         * @brief Whether or not the entire view should be processed
         * If this returns false, the view will not be drawn and no shortcuts will be handled. This includes things
         * drawn in the drawAlwaysVisibleContent() function.
         * @return True if the view should be processed, false otherwise
         */
        [[nodiscard]] virtual bool shouldProcess() const;

        /**
         * @brief Whether or not the view should have an entry in the view menu
         * @return True if the view should have an entry in the view menu, false otherwise
         */
        [[nodiscard]] virtual bool hasViewMenuItemEntry() const;

        /**
         * @brief Gets the minimum size of the view window
         * @return The minimum size of the view window
         */
        [[nodiscard]] virtual ImVec2 getMinSize() const;

        /**
         * @brief Gets the maximum size of the view window
         * @return The maximum size of the view window
         */
        [[nodiscard]] virtual ImVec2 getMaxSize() const;

        /**
         * @brief Gets additional window flags for the view window
         * @return Additional window flags for the view window
         */
        [[nodiscard]] virtual ImGuiWindowFlags getWindowFlags() const;

        /**
         * @brief Returns a view whose menu items should be additionally visible when this view is focused
         * @return
         */
        [[nodiscard]] virtual View* getMenuItemInheritView() const { return nullptr; }


        [[nodiscard]] const char *getIcon() const { return m_icon; }
        [[nodiscard]] const UnlocalizedString& getUnlocalizedName() const;
        [[nodiscard]] std::string getName() const;

        [[nodiscard]] virtual bool shouldDefaultFocus() const { return false; }
        [[nodiscard]] virtual bool shouldStoreWindowState() const { return true; }

        [[nodiscard]] bool &getWindowOpenState();
        [[nodiscard]] const bool &getWindowOpenState() const;

        [[nodiscard]] bool isFocused() const { return m_focused; }

        [[nodiscard]] static std::string toWindowName(const UnlocalizedString &unlocalizedName);
        [[nodiscard]] static const View* getLastFocusedView();
        static void discardNavigationRequests();

        void bringToFront();

        [[nodiscard]] bool didWindowJustOpen();
        void setWindowJustOpened(bool state);

        [[nodiscard]] bool didWindowJustClose();
        void setWindowJustClosed(bool state);

        void trackViewState();
        void setFocused(bool focused);

    protected:
        /**
         * @brief Called when this view is opened (i.e. made visible).
         */
        virtual void onOpen() {}

        /**
         * @brief Called when this view is closed (i.e. made invisible).
         */
        virtual void onClose() {}

    public:
        class Window;
        class Special;
        class Floating;
        class Scrolling;
        class Modal;
        class FullScreen;

    private:
        UnlocalizedString m_unlocalizedViewName;
        bool m_windowOpen = false, m_prevWindowOpen = false;
        std::map<Shortcut, ShortcutManager::ShortcutEntry> m_shortcuts;
        bool m_windowJustOpened = false, m_windowJustClosed = false;
        const char *m_icon;
        bool m_focused = false;

        friend class ShortcutManager;
    };


    /**
     * @brief A view that draws a regular window. This should be the default for most views
     */
    class View::Window : public View {
    public:
        explicit Window(UnlocalizedString unlocalizedName, const char *icon) : View(std::move(unlocalizedName), icon) {}

        /**
         * @brief Draws help text for the view
         */
        virtual void drawHelpText() = 0;

        void draw(ImGuiWindowFlags extraFlags = ImGuiWindowFlags_None) override;

        virtual bool allowScroll() const {
            return false;
        }
    };

    /**
     * @brief A view that doesn't handle any window creation and just draws its content.
     * This should be used if you intend to draw your own special window
     */
    class View::Special : public View {
    public:
        explicit Special(UnlocalizedString unlocalizedName) : View(std::move(unlocalizedName), "") {}

        void draw(ImGuiWindowFlags extraFlags = ImGuiWindowFlags_None) final;
    };

    /**
     * @brief A view that draws a floating window. These are the same as regular windows but cannot be docked
     */
    class View::Floating : public View::Window {
    public:
        explicit Floating(UnlocalizedString unlocalizedName, const char *icon) : Window(std::move(unlocalizedName), icon) {}

        void draw(ImGuiWindowFlags extraFlags = ImGuiWindowFlags_None) final;

        [[nodiscard]] bool shouldStoreWindowState() const override { return false; }
    };

    /**
     * @brief A view that draws all its content at once without any scrolling being done by the window itself
     */
    class View::Scrolling : public View::Window {
    public:
        explicit Scrolling(UnlocalizedString unlocalizedName, const char *icon) : Window(std::move(unlocalizedName), icon) {}

        void draw(ImGuiWindowFlags extraFlags = ImGuiWindowFlags_None) final;

        bool allowScroll() const final {
            return true;
        }
    };

    /**
     * @brief A view that draws a modal window. The window will always be drawn on top and will block input to other windows
     */
    class View::Modal : public View {
    public:
        explicit Modal(UnlocalizedString unlocalizedName, const char *icon) : View(std::move(unlocalizedName), icon) {}

        void draw(ImGuiWindowFlags extraFlags = ImGuiWindowFlags_None) final;

        [[nodiscard]] virtual bool hasCloseButton() const { return true; }
        [[nodiscard]] bool shouldStoreWindowState() const override { return false; }
    };

    class View::FullScreen : public View {
    public:
        explicit FullScreen() : View("FullScreen", "") {}

        void draw(ImGuiWindowFlags extraFlags = ImGuiWindowFlags_None) final;
    };

}
