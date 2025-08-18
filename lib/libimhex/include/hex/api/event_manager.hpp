#pragma once

#include <hex.hpp>

#include <algorithm>
#include <functional>
#include <list>
#include <mutex>
#include <map>
#include <string_view>

#include <hex/helpers/logger.hpp>

#include <wolv/types/type_name.hpp>

#define EVENT_DEF_IMPL(event_name, event_name_string, should_log, ...)                                                          \
    struct event_name final : public hex::impl::Event<__VA_ARGS__> {                                                            \
        constexpr static auto Id = [] { return hex::impl::EventId(event_name_string); }();                                      \
        constexpr static auto ShouldLog = (should_log);                                                                         \
        explicit event_name(Callback func) noexcept : Event(std::move(func)) { }                                                \
                                                                                                                                \
        static EventManager::EventList::iterator subscribe(Event::Callback function) {                                          \
            return EventManager::subscribe<event_name>(std::move(function));                                                    \
        }                                                                                                                       \
        template<typename = void>                                                                                               \
        static EventManager::EventList::iterator subscribe(Event::BaseCallback function)                                        \
        requires (!std::same_as<Event::Callback, Event::BaseCallback>) {                                                        \
            return EventManager::subscribe<event_name>([function = std::move(function)](auto && ...) { function(); });          \
        }                                                                                                                       \
        static void subscribe(void *token, Event::Callback function) {                                                          \
            EventManager::subscribe<event_name>(token, std::move(function));                                                    \
        }                                                                                                                       \
        template<typename = void>                                                                                               \
        static void subscribe(void *token, Event::BaseCallback function)                                                        \
        requires (!std::same_as<Event::Callback, Event::BaseCallback>) {                                                        \
            return EventManager::subscribe<event_name>(token, [function = std::move(function)](auto && ...) { function(); });   \
        }                                                                                                                       \
        static void unsubscribe(const EventManager::EventList::iterator &token) noexcept {                                      \
            EventManager::unsubscribe(token);                                                                                   \
        }                                                                                                                       \
        static void unsubscribe(void *token) noexcept {                                                                         \
            EventManager::unsubscribe<event_name>(token);                                                                       \
        }                                                                                                                       \
        static void post(auto &&...args) {                                                                                      \
            EventManager::post<event_name>(std::forward<decltype(args)>(args)...);                                              \
        }                                                                                                                       \
    }

#define EVENT_DEF(event_name, ...)          EVENT_DEF_IMPL(event_name, #event_name, true, __VA_ARGS__)
#define EVENT_DEF_NO_LOG(event_name, ...)   EVENT_DEF_IMPL(event_name, #event_name, false, __VA_ARGS__)


EXPORT_MODULE namespace hex {

    namespace impl {

        class EventId {
        public:
            explicit constexpr EventId(const char *eventName) {
                m_hash = 0x811C'9DC5;
                for (const char c : std::string_view(eventName)) {
                    m_hash = (m_hash >> 5) | (m_hash << 27);
                    m_hash ^= c;
                }
            }

            constexpr bool operator==(const EventId &other) const {
                return m_hash == other.m_hash;
            }

            constexpr auto operator<=>(const EventId &other) const {
                return m_hash <=> other.m_hash;
            }

        private:
            u32 m_hash;
        };

        struct EventBase {
            EventBase() noexcept = default;
            virtual ~EventBase() = default;
        };

        template<typename... Params>
        struct Event : EventBase {
            using Callback = std::function<void(Params...)>;
            using BaseCallback = std::function<void()>;

            explicit Event(Callback func) noexcept : m_func(std::move(func)) { }

            template<typename E>
            void call(auto&& ... params) const {
                #if defined(DEBUG)
                    m_func(std::forward<decltype(params)>(params)...);
                #else
                    try {
                        m_func(std::forward<decltype(params)>(params)...);
                    } catch (const std::exception &e) {
                        log::error("An exception occurred while handling event {}: {}", wolv::type::getTypeName<E>(), e.what());
                        throw;
                    }
                #endif
            }

        private:
            Callback m_func;
        };

        template<typename T>
        concept EventType = std::derived_from<T, EventBase>;

    }


    /**
     * @brief The EventManager allows subscribing to and posting events to different parts of the program.
     * To create a new event, use the EVENT_DEF macro. This will create a new event type with the given name and parameters.
     * Events should be created in an `events_*.hpp` category file under the `events` folder, and never directly here.
     */
    class EventManager {
    public:
        using EventList = std::multimap<impl::EventId, std::unique_ptr<impl::EventBase>>;

        /**
         * @brief Subscribes to an event
         * @tparam E Event
         * @param function Function to call when the event is posted
         * @return Token to unsubscribe from the event
         */
        template<impl::EventType E>
        static EventList::iterator subscribe(E::Callback function) {
            std::lock_guard lock(getEventMutex());

            auto &events = getEvents();
            return events.insert({ E::Id, std::make_unique<E>(function) });
        }

        /**
         * @brief Subscribes to an event
         * @tparam E Event
         * @param token Unique token to register the event to. Later required to unsubscribe again
         * @param function Function to call when the event is posted
         */
        template<impl::EventType E>
        static void subscribe(void *token, E::Callback function) {
            std::lock_guard lock(getEventMutex());

            if (isAlreadyRegistered(token, E::Id)) {
                log::fatal("The token '{}' has already registered the same event ('{}')", token, wolv::type::getTypeName<E>());
                return;
            }

            getTokenStore().insert({ token, subscribe<E>(std::move(function)) });
        }

        /**
         * @brief Unsubscribes from an event
         * @param token Token returned by subscribe
         */
        static void unsubscribe(const EventList::iterator &token) noexcept {
            std::lock_guard lock(getEventMutex());

            getEvents().erase(token);
        }

        /**
         * @brief Unsubscribes from an event
         * @tparam E Event
         * @param token Token passed to subscribe
         */
        template<impl::EventType E>
        static void unsubscribe(void *token) noexcept {
            std::lock_guard lock(getEventMutex());

            unsubscribe(token, E::Id);
        }

        /**
         * @brief Posts an event to all subscribers of it
         * @tparam E Event
         * @param args Arguments to pass to the event
         */
        template<impl::EventType E>
        static void post(auto && ...args) {
            std::lock_guard lock(getEventMutex());

            const auto &[begin, end] = getEvents().equal_range(E::Id);
            for (auto it = begin; it != end; ++it) {
                const auto &[id, event] = *it;
                (*static_cast<E *const>(event.get())).template call<E>(std::forward<decltype(args)>(args)...);
            }

            #if defined (DEBUG)
                if constexpr (E::ShouldLog)
                    log::debug("Event posted: '{}'", wolv::type::getTypeName<E>());
            #endif
        }

        /**
         * @brief Unsubscribe all subscribers from all events
         */
        static void clear() noexcept {
            std::lock_guard lock(getEventMutex());

            getEvents().clear();
            getTokenStore().clear();
        }

    private:
        static std::multimap<void *, EventList::iterator>& getTokenStore();
        static EventList& getEvents();
        static std::recursive_mutex& getEventMutex();

        static bool isAlreadyRegistered(void *token, impl::EventId id);
        static void unsubscribe(void *token, impl::EventId id);
    };

}