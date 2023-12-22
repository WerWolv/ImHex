#pragma once

#include <hex.hpp>
#include <imgui.h>

#include <list>
#include <memory>
#include <mutex>

namespace hex {

    namespace impl {

        class ToastBase {
        public:
            ToastBase(ImColor color) : m_color(color) {}
            virtual ~ToastBase() = default;

            virtual void draw() { drawContent(); }
            virtual void drawContent() = 0;

            [[nodiscard]] static std::list<std::unique_ptr<ToastBase>> &getQueuedToasts();

            [[nodiscard]] const ImColor& getColor() const {
                return m_color;
            }

            void setAppearTime(double appearTime) {
                m_appearTime = appearTime;
            }

            [[nodiscard]] double getAppearTime() const {
                return m_appearTime;
            }

            constexpr static double VisibilityTime = 4.0;

        protected:
            static std::mutex& getMutex();

            double m_appearTime = 0;
            ImColor m_color;
        };

    }

    template<typename T>
    class Toast : public impl::ToastBase {
    public:
        using impl::ToastBase::ToastBase;

        template<typename ...Args>
        static void open(Args && ... args) {
            std::lock_guard lock(getMutex());

            auto toast = std::make_unique<T>(std::forward<Args>(args)...);
            getQueuedToasts().emplace_back(std::move(toast));
        }
    };

}