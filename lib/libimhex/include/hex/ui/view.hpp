#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/api/imhex_api.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/event_manager.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/providers/provider.hpp>
#include <hex/providers/provider_data.hpp>
#include <hex/helpers/utils.hpp>


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
        virtual void draw() = 0;

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

        [[nodiscard]] virtual bool shouldStoreWindowState() const { return true; }

        [[nodiscard]] const char *getIcon() const { return m_icon; }

        [[nodiscard]] bool &getWindowOpenState();
        [[nodiscard]] const bool &getWindowOpenState() const;

        [[nodiscard]] const UnlocalizedString &getUnlocalizedName() const;
        [[nodiscard]] std::string getName() const;

        [[nodiscard]] bool didWindowJustOpen();
        void setWindowJustOpened(bool state);

        void trackViewOpenState();

        static void discardNavigationRequests();

        [[nodiscard]] static std::string toWindowName(const UnlocalizedString &unlocalizedName);

    public:
        class Window;
        class Special;
        class Floating;
        class Modal;

    private:
        UnlocalizedString m_unlocalizedViewName;
        bool m_windowOpen = false, m_prevWindowOpen = false;
        std::map<Shortcut, ShortcutManager::ShortcutEntry> m_shortcuts;
        bool m_windowJustOpened = false;
        const char *m_icon;

        friend class ShortcutManager;
    };


    /**
     * @brief A view that draws a regular window. This should be the default for most views
     */
    class View::Window : public View {
    public:
        explicit Window(UnlocalizedString unlocalizedName, const char *icon) : View(std::move(unlocalizedName), icon) {}

        void draw() final {
            if (this->shouldDraw()) {
                ImGui::SetNextWindowSizeConstraints(this->getMinSize(), this->getMaxSize());
                if (ImGui::Begin(View::toWindowName(this->getUnlocalizedName()).c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse | this->getWindowFlags())) {
                    this->drawContent();
                }
                ImGui::End();
            }
        }
    };

    /**
     * @brief A view that doesn't handle any window creation and just draws its content.
     * This should be used if you intend to draw your own special window
     */
    class View::Special : public View {
    public:
        explicit Special(UnlocalizedString unlocalizedName) : View(std::move(unlocalizedName), "") {}

        void draw() final {
            if (this->shouldDraw()) {
                ImGui::SetNextWindowSizeConstraints(this->getMinSize(), this->getMaxSize());
                this->drawContent();
            }
        }
    };

    /**
     * @brief A view that draws a floating window. These are the same as regular windows but cannot be docked
     */
    class View::Floating : public View::Window {
    public:
        explicit Floating(UnlocalizedString unlocalizedName) : Window(std::move(unlocalizedName), "") {}

        [[nodiscard]] ImGuiWindowFlags getWindowFlags() const override { return ImGuiWindowFlags_NoDocking; }
        [[nodiscard]] bool shouldStoreWindowState() const override { return false; }
    };

    /**
     * @brief A view that draws a modal window. The window will always be drawn on top and will block input to other windows
     */
    class View::Modal : public View {
    public:
        explicit Modal(UnlocalizedString unlocalizedName) : View(std::move(unlocalizedName), "") {}

        void draw() final {
            if (this->shouldDraw()) {
                if (this->getWindowOpenState())
                    ImGui::OpenPopup(View::toWindowName(this->getUnlocalizedName()).c_str());

                ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
                ImGui::SetNextWindowSizeConstraints(this->getMinSize(), this->getMaxSize());
                if (ImGui::BeginPopupModal(View::toWindowName(this->getUnlocalizedName()).c_str(), this->hasCloseButton() ? &this->getWindowOpenState() : nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | this->getWindowFlags())) {
                    this->drawContent();

                    ImGui::EndPopup();
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    this->getWindowOpenState() = false;
            }
        }

        [[nodiscard]] virtual bool hasCloseButton() const { return true; }
        [[nodiscard]] bool shouldStoreWindowState() const override { return false; }
    };

}