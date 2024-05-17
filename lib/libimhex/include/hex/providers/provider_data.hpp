#pragma once

#include <hex/api/imhex_api.hpp>
#include <hex/api/event_manager.hpp>

#include <map>
#include <ranges>
#include <utility>

namespace hex {

    namespace prv {
        class Provider;
    }

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
            return m_data[provider];
        }

        const T& get(const prv::Provider *provider = ImHexApi::Provider::get()) const {
            return m_data.at(provider);
        }

        void set(const T &data, const prv::Provider *provider = ImHexApi::Provider::get()) {
            m_data[provider] = data;
        }

        void set(T &&data, const prv::Provider *provider = ImHexApi::Provider::get()) {
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

            // Moves the data of this PerProvider instance from one provider to another
            MovePerProviderData::subscribe(this, [this](prv::Provider *from, prv::Provider *to) {
                // Get the value from the old provider, (removes it from the map)
                auto node = m_data.extract(from);

                // Ensure the value existed
                if (node.empty()) return;

                // Delete the value from the new provider, that we want to replace
                m_data.erase(to);

                // Re-insert it with the key of the new provider
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