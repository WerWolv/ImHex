#pragma once

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>
#include <hex/providers/provider.hpp>

#include <concepts>
#include <map>
#include <utility>

namespace hex {

    template<typename T>
    class PerProvider {
    public:
        PerProvider() { this->onCreate(); }
        PerProvider(const PerProvider&) = delete;
        PerProvider(PerProvider&&) = delete;
        PerProvider& operator=(const PerProvider&) = delete;
        PerProvider& operator=(PerProvider&&) = delete;

        PerProvider(T data) : m_data({ { ImHexApi::Provider::get(), std::move(data) } }) { this->onCreate(); }

        ~PerProvider() = default;

        T* operator->() {
            return &this->get();
        }

        const T* operator->() const {
            return &this->get();
        }

        T& get(prv::Provider *provider = ImHexApi::Provider::get()) {
            return this->m_data[provider];
        }

        const T& get(prv::Provider *provider = ImHexApi::Provider::get()) const {
            return this->m_data[provider];
        }

        T& operator*() {
            return this->get();
        }

        const T& operator*() const {
            return this->get();
        }

        PerProvider& operator=(T data) {
            this->m_data = std::move(data);
            return *this;
        }

        operator T&() {
            return this->get();
        }

    private:
        void onCreate() {
            (void)EventManager::subscribe<EventProviderOpened>([this](prv::Provider *provider) {
                this->m_data.emplace(provider, T());
            });

            (void)EventManager::subscribe<EventProviderClosed>([this](prv::Provider *provider){
                this->m_data.erase(provider);
            });

            EventManager::subscribe<EventImHexClosing>([this] {
                this->m_data.clear();
            });
        }

    private:
        std::map<prv::Provider *, T> m_data;
    };

}