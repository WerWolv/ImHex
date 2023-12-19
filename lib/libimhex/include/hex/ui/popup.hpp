#pragma once

#include <hex.hpp>
#include <hex/api/localization_manager.hpp>

#include <memory>
#include <string>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/api/task_manager.hpp>
#include <hex/helpers/utils.hpp>

namespace hex {

    namespace impl {

        class PopupBase {
        public:
            explicit PopupBase(UnlocalizedString unlocalizedName, bool closeButton, bool modal)
                : m_unlocalizedName(std::move(unlocalizedName)), m_closeButton(closeButton), m_modal(modal) { }

            virtual ~PopupBase() = default;

            virtual void drawContent() = 0;
            [[nodiscard]] virtual ImGuiWindowFlags getFlags() const { return ImGuiWindowFlags_None; }

            [[nodiscard]] virtual ImVec2 getMinSize() const {
                return { 0, 0 };
            }

            [[nodiscard]] virtual ImVec2 getMaxSize() const {
                return { 0, 0 };
            }

            [[nodiscard]] static std::vector<std::unique_ptr<PopupBase>> &getOpenPopups();

            [[nodiscard]] const UnlocalizedString &getUnlocalizedName() const {
                return m_unlocalizedName;
            }

            [[nodiscard]] bool hasCloseButton() const {
                return m_closeButton;
            }

            [[nodiscard]] bool isModal() const {
                return m_modal;
            }

            void close() {
                m_close = true;
            }

            [[nodiscard]] bool shouldClose() const {
                return m_close;
            }

        protected:
            static std::mutex& getMutex();
        private:
            UnlocalizedString m_unlocalizedName;
            bool m_closeButton, m_modal;
            std::atomic<bool> m_close = false;
        };

    }


    template<typename T>
    class Popup : public impl::PopupBase {
    protected:
        explicit Popup(UnlocalizedString unlocalizedName, bool closeButton = true, bool modal = true) : PopupBase(std::move(unlocalizedName), closeButton, modal) { }

    public:
        template<typename ...Args>
        static void open(Args && ... args) {
            std::lock_guard lock(getMutex());

            auto popup = std::make_unique<T>(std::forward<Args>(args)...);

            getOpenPopups().emplace_back(std::move(popup));
        }

    };

}