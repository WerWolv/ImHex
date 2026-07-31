#pragma once

#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/requests_provider.hpp>

#include <functional>
#include <map>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace hex {

    template<typename T>
    class PerProvider {
    public:
        PerProvider() { this->onCreate(); }
        PerProvider(const PerProvider&) = delete;
        PerProvider(PerProvider&&) = delete;
        PerProvider& operator=(const PerProvider&) = delete;
        PerProvider& operator=(PerProvider &&) = delete;

        ~PerProvider() { this->onDestroy(); }

        T* operator->() {
            return &this->get();
        }

        const T* operator->() const {
            return &this->get();
        }

        T& get(const prv::Provider *provider = ImHexApi::Provider::get()) {
            if (provider == nullptr) [[unlikely]]
                throw std::invalid_argument("PerProvider::get called with nullptr");

            return m_data[provider];
        }

        const T& get(const prv::Provider *provider = ImHexApi::Provider::get()) const {
            if (provider == nullptr) [[unlikely]]
                throw std::invalid_argument("PerProvider::get called with nullptr");

            return m_data.at(provider);
        }

        void set(const T &data, const prv::Provider *provider = ImHexApi::Provider::get()) {
            if (provider == nullptr) [[unlikely]]
                throw std::invalid_argument("PerProvider::set called with nullptr");

            m_data[provider] = data;
        }

        void set(T &&data, const prv::Provider *provider = ImHexApi::Provider::get()) {
            if (provider == nullptr) [[unlikely]]
                throw std::invalid_argument("PerProvider::set called with nullptr");

            m_data[provider] = std::move(data);
        }

        T& operator*() {
            return this->get();
        }

        const T& operator*() const {
            return this->get();
        }

        PerProvider& operator=(const T &data) {
            this->set(data);
            return *this;
        }

        PerProvider& operator=(T &&data) {
            this->set(std::move(data));
            return *this;
        }

        operator T&() {
            return this->get();
        }

        auto all() {
            return m_data | std::views::values;
        }

        void setOnCreateCallback(std::function<void(prv::Provider *, T&)> callback) {
            m_onCreateCallback = std::move(callback);
        }

        void setOnDestroyCallback(std::function<void(prv::Provider *, T&)> callback) {
            m_onDestroyCallback = std::move(callback);
        }

    private:
        void onCreate() {
            EventProviderOpened::subscribe(this, [this](prv::Provider *provider) {
                auto [it, inserted] = m_data.emplace(provider, T());
                auto &[key, value] = *it;
                if (m_onCreateCallback)
                    m_onCreateCallback(provider, value);
            });

            EventProviderDeleted::subscribe(this, [this](prv::Provider *provider){
                if (auto it = m_data.find(provider); it != m_data.end()) {
                    if (m_onDestroyCallback)
                        m_onDestroyCallback(provider, m_data.at(provider));

                    m_data.erase(it);
                }
            });

            EventImHexClosing::subscribe(this, [this] {
                m_data.clear();
            });

            MovePerProviderData::subscribe(this, [this](prv::Provider *from, prv::Provider *to) {
                auto node = m_data.extract(from);
                if (node.empty())
                    return;

                m_data.erase(to);
                node.key() = to;
                m_data.insert(std::move(node));
            });
        }

        void onDestroy() {
            EventProviderOpened::unsubscribe(this);
            EventProviderDeleted::unsubscribe(this);
            EventImHexClosing::unsubscribe(this);
            MovePerProviderData::unsubscribe(this);
        }

    private:
        std::map<const prv::Provider *, T> m_data;
        std::function<void(prv::Provider *, T&)> m_onCreateCallback, m_onDestroyCallback;
    };

}
