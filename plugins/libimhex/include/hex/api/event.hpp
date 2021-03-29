#pragma once

#include <hex.hpp>

#include <list>
#include <map>
#include <string_view>
#include <functional>

#include <hex/api/imhex_api.hpp>

#define EVENT_DEF(event_name, ...)                                  \
    struct event_name final : public hex::Event<__VA_ARGS__> {      \
        constexpr static auto id = [] { return hex::EventId(); }(); \
        event_name(Callback func) noexcept : Event(func) { }        \
    };

class GLFWwindow;

namespace hex {

    class EventId {
    public:
        constexpr EventId(const char *func = __builtin_FUNCTION(), u32 line = __builtin_LINE()) {
            this->m_hash = line ^ 123456789;
            for (auto c : std::string_view(func)) {
                this->m_hash  = (this->m_hash >> 5) | (this->m_hash << 27);
                this->m_hash ^= c;
            }
        }

        constexpr bool operator ==(const EventId &rhs) const = default;

    private:
        u32 m_hash;
    };

    struct EventBase {
        EventBase() noexcept = default;
    };

    template<typename ... Params>
    struct Event : public EventBase {
        using Callback = std::function<void(Params...)>;

        explicit Event(Callback func) noexcept : m_func(func) {}

        void operator()(Params... params) const noexcept {
            this->m_func(params...);
        }

    private:
        Callback m_func;
    };

    class EventManager {
    public:
        using EventList = std::list<std::pair<EventId, EventBase*>>;

        template<typename E>
        [[nodiscard]] static EventList::iterator subscribe(typename E::Callback function) {
            return s_events.insert(s_events.end(), std::make_pair(E::id, new E(function)));
        }

        template<typename E>
        static void subscribe(void *token, typename E::Callback function) {
            s_tokenStore.insert(std::make_pair(token, subscribe<E>(function)));
        }

        static void unsubscribe(EventList::iterator iter) noexcept {
            s_events.remove(*iter);
        }

        template<typename E>
        static void unsubscribe(void *token) noexcept {
            auto iter = std::find_if(s_tokenStore.begin(), s_tokenStore.end(), [&](auto &item){
                return item.first == token && item.second->first == E::id;
            });

            if (iter != s_tokenStore.end()) {
                s_events.remove(*iter->second);
            }
        }

        template<typename E>
        static void post(auto ... args) noexcept {
            for (const auto &[id, event] : s_events) {
                if (id == E::id)
                    (*reinterpret_cast<E *>(event))(args...);
            }
        }
        
    private:
        static std::map<void*, EventList::iterator> s_tokenStore;
        static EventList s_events;
    };

    /* Default Events */
    EVENT_DEF(EventFileLoaded, std::string);
    EVENT_DEF(EventFileUnloaded);
    EVENT_DEF(EventDataChanged);
    EVENT_DEF(EventPatternChanged);
    EVENT_DEF(EventFileDropped, std::string);
    EVENT_DEF(EventWindowClosing, GLFWwindow*);
    EVENT_DEF(EventRegionSelected, Region);
    EVENT_DEF(EventProjectFileStore);
    EVENT_DEF(EventProjectFileLoad);
    EVENT_DEF(EventSettingsChanged);

    EVENT_DEF(RequestOpenWindow, std::string);
    EVENT_DEF(RequestSelectionChange, Region);
    EVENT_DEF(RequestAddBookmark, ImHexApi::Bookmarks::Entry);
    EVENT_DEF(RequestAppendPatternLanguageCode, std::string);
    EVENT_DEF(RequestChangeWindowTitle, std::string);
    EVENT_DEF(RequestCloseImHex);

}