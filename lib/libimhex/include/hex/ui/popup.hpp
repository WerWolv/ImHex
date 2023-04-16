#pragma once

#include <hex.hpp>

#include <memory>
#include <string>
#include <vector>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/helpers/utils.hpp>

#include <hex/api/task.hpp>

namespace hex {

    namespace impl {

        class PopupBase {
        public:
            explicit PopupBase(std::string unlocalizedName, bool closeButton, bool modal)
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

            [[nodiscard]] static std::vector<std::unique_ptr<PopupBase>> &getOpenPopups() {
                return s_openPopups;
            }

            [[nodiscard]] const std::string &getUnlocalizedName() const {
                return this->m_unlocalizedName;
            }

            [[nodiscard]] bool hasCloseButton() const {
                return this->m_closeButton;
            }

            [[nodiscard]] bool isModal() const {
                return this->m_modal;
            }

            void close() {
                this->m_close = true;
            }

            [[nodiscard]] bool shouldClose() const {
                return this->m_close;
            }

        protected:
            static std::vector<std::unique_ptr<PopupBase>> s_openPopups;

        private:

            std::string m_unlocalizedName;
            bool m_closeButton, m_modal;
            std::atomic<bool> m_close = false;
        };

    }


    template<typename T>
    class Popup : public impl::PopupBase {
    protected:
        explicit Popup(std::string unlocalizedName, bool closeButton = true, bool modal = true) : PopupBase(std::move(unlocalizedName), closeButton, modal) { }

    public:
        template<typename ...Args>
        static void open(Args && ... args) {
            static std::mutex mutex;
            std::lock_guard lock(mutex);

            auto popup = std::make_unique<T>(std::forward<Args>(args)...);

            s_openPopups.emplace_back(std::move(popup));
        }

    };

}