#pragma once

#include <hex/api/event_manager.hpp>

namespace hex {

    template<typename T>
    class AutoReset {
    public:
        using Type = T;

        AutoReset() {
            EventImHexClosing::subscribe(this, [this] {
                this->reset();
            });
        }

        ~AutoReset() {
            EventImHexClosing::unsubscribe(this);
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

        void reset() {
            m_value = T();
        }

    private:
        T m_value;
    };

}