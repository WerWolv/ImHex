#pragma once

#include <hex.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <hex/helpers/utils.hpp>

#include <list>
#include <memory>
#include <mutex>
#include "hex/api/localization_manager.hpp"

namespace hex {

    namespace impl {

        class BannerBase {
        public:
            BannerBase(ImColor color) : m_color(color) {}
            virtual ~BannerBase() = default;

            virtual void draw() { drawContent(); }
            virtual void drawContent() = 0;

            [[nodiscard]] static std::list<std::unique_ptr<BannerBase>> &getOpenBanners();

            [[nodiscard]] const ImColor& getColor() const {
                return m_color;
            }

            void close() { m_shouldClose = true; }
            [[nodiscard]] bool shouldClose() const { return m_shouldClose; }

        protected:
            static std::mutex& getMutex();

            bool m_shouldClose = false;
            ImColor m_color;
        };

    }

    template<typename T>
    class Banner : public impl::BannerBase {
    public:
        using impl::BannerBase::BannerBase;

        template<typename ...Args>
        static void open(Args && ... args) {
            std::lock_guard lock(getMutex());

            auto toast = std::make_unique<T>(std::forward<Args>(args)...);
            getOpenBanners().emplace_back(std::move(toast));
        }
    };

}