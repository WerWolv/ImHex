#pragma once

#include <hex.hpp>

#include <memory>
#include <string>
#include <vector>

#include <imgui.h>

namespace hex {

    class PopupBase {
    public:
        explicit PopupBase(std::string unlocalizedName, bool closeButton = true)
            : m_unlocalizedName(std::move(unlocalizedName)), m_closeButton(closeButton) { }

        virtual ~PopupBase() = default;

        virtual void drawContent() = 0;
        [[nodiscard]] virtual ImGuiPopupFlags getFlags() const { return ImGuiPopupFlags_None; }


        [[nodiscard]] static std::vector<std::unique_ptr<PopupBase>> &getOpenPopups() {
            return s_openPopups;
        }

        [[nodiscard]] const std::string &getUnlocalizedName() const {
            return this->m_unlocalizedName;
        }

        [[nodiscard]] bool hasCloseButton() const {
            return this->m_closeButton;
        }

    protected:
        static void close() {
            ImGui::CloseCurrentPopup();
            s_openPopups.pop_back();
        }

        static std::vector<std::unique_ptr<PopupBase>> s_openPopups;
    private:

        std::string m_unlocalizedName;
        bool m_closeButton;
    };

    template<typename T>
    class Popup : public PopupBase {
    protected:
        explicit Popup(std::string unlocalizedName, bool closeButton = true) : PopupBase(std::move(unlocalizedName), closeButton) { }

    public:
        virtual ~Popup() = default;

        template<typename ...Args>
        static void open(Args && ... args) {
            auto popup = std::make_unique<T>(std::forward<Args>(args)...);

            s_openPopups.emplace_back(std::move(popup));
        }

    };

}