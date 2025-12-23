#pragma once

#include <hex/api/imhex_api/system.hpp>
#include <hex/helpers/logger.hpp>

namespace hex {

    namespace impl {

        class AutoResetBase {
        public:
            virtual ~AutoResetBase() = default;

        private:
            friend void ImHexApi::System::impl::cleanup();
            virtual void reset() = 0;
        };

    }

    template<typename T>
    class AutoReset : public impl::AutoResetBase {
    public:
        using Type = T;

        AutoReset() noexcept {
            try {
                ImHexApi::System::impl::addAutoResetObject(this);
            } catch (std::exception &e) {
                log::error("Failed to register AutoReset object: {}", e.what());
            }
        }

        explicit(false) AutoReset(const T &value) : AutoReset() {
            m_value = value;
            m_valid = true;
        }

        explicit(false) AutoReset(T &&value) noexcept : AutoReset() {
            m_value = std::move(value);
            m_valid = true;
        }

        ~AutoReset() override {
            ImHexApi::System::impl::removeAutoResetObject(this);
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

        AutoReset& operator=(const T &value) {
            m_value = value;
            m_valid = true;
            return *this;
        }

        AutoReset& operator=(T &&value) noexcept {
            m_value = std::move(value);
            m_valid = true;
            return *this;
        }

        [[nodiscard]] bool isValid() const {
            return m_valid;
        }

    private:
        void reset() override {
            if constexpr (requires { m_value.reset(); }) {
                m_value.reset();
            } else if constexpr (requires { m_value.clear(); }) {
                m_value.clear();
            } else if constexpr (std::is_pointer_v<T>) {
                m_value = nullptr; // cppcheck-suppress nullPointer
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