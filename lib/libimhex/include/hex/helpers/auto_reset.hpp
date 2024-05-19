#pragma once

#include <hex/api/event_manager.hpp>
#include <hex/api/imhex_api.hpp>

namespace hex {

    namespace impl {

        class AutoResetBase {
        public:
            virtual ~AutoResetBase() = default;
            virtual void reset() = 0;
        };

    }

    template<typename T>
    class AutoReset : public impl::AutoResetBase {
    public:
        using Type = T;

        AutoReset() {
            ImHexApi::System::impl::addAutoResetObject(this);
        }

        T* operator->() {
            return &m_value;
        }

        const T* operator->() const {
            return &m_value;
        }

        T& operator*() {
            return m_value;
        }

        const T& operator*() const {
            return m_value;
        }

        operator T&() {
            return m_value;
        }

        operator const T&() const {
            return m_value;
        }

        T& operator=(const T &value) {
            m_value = value;
            m_valid = true;
            return m_value;
        }

        T& operator=(T &&value) noexcept {
            m_value = std::move(value);
            m_valid = true;
            return m_value;
        }

        bool isValid() const {
            return m_valid;
        }

    private:
        friend void ImHexApi::System::impl::cleanup();

        void reset() override {
            if constexpr (requires { m_value.reset(); }) {
                m_value.reset();
            } else if constexpr (requires { m_value.clear(); }) {
                m_value.clear();
            } else if constexpr (requires(T t) { t = nullptr; }) {
                m_value = nullptr;
            } else {
                m_value = { };
            }

            m_valid = false;
        }

    private:
        bool m_valid = true;
        T m_value;
    };

}