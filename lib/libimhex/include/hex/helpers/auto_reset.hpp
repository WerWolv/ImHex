#pragma once

#include <hex/api/event_manager.hpp>
#include <hex/api/imhex_api.hpp>

namespace hex {

    class AutoResetBase {
    public:
        virtual ~AutoResetBase() = default;
        virtual void reset() = 0;
    };

    template<typename T>
    class AutoReset : AutoResetBase {
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
            return m_value;
        }

        T& operator=(T &&value) noexcept {
            m_value = std::move(value);
            return m_value;
        }

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
        }

    private:
        T m_value;
    };

}