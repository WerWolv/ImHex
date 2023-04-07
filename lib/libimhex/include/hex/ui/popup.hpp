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
                return { FLT_MAX, FLT_MAX };
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

            static void close() {
                if (s_openPopups.empty())
                    return;

                TaskManager::doLater([]{
                    std::lock_guard lock(s_mutex);

                    ImGui::CloseCurrentPopup();
                    s_openPopups.pop_back();
                });
            }

            static std::mutex& getMutex() {
                return s_mutex;
            }

        protected:
            static std::vector<std::unique_ptr<PopupBase>> s_openPopups;
            static std::mutex s_mutex;

        private:

            std::string m_unlocalizedName;
            bool m_closeButton, m_modal;
        };

    }


    template<typename T>
    class Popup : public impl::PopupBase {
    protected:
        explicit Popup(std::string unlocalizedName, bool closeButton = true, bool modal = true) : PopupBase(std::move(unlocalizedName), closeButton, modal) { }

    public:
        template<typename ...Args>
        static void open(Args && ... args) {
            std::lock_guard lock(s_mutex);

            auto popup = std::make_unique<T>(std::forward<Args>(args)...);

            s_openPopups.emplace_back(std::move(popup));
        }

    };

}