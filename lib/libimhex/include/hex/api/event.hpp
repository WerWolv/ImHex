#pragma once

#include <hex.hpp>

#include <list>
#include <map>
#include <string_view>
#include <functional>

#include <hex/api/imhex_api.hpp>
#include <hex/helpers/fs.hpp>

#define EVENT_DEF(event_name, ...)                                               \
    struct event_name final : public hex::Event<__VA_ARGS__> {                   \
        constexpr static auto id = [] { return hex::EventId(); }();              \
        explicit event_name(Callback func) noexcept : Event(std::move(func)) { } \
    }

struct GLFWwindow;

namespace hex {

    class EventId {
    public:
        explicit constexpr EventId(const char *func = __builtin_FUNCTION(), u32 line = __builtin_LINE()) {
            this->m_hash = line ^ 987654321;
            for (auto c : std::string_view(func)) {
                this->m_hash = (this->m_hash >> 5) | (this->m_hash << 27);
                this->m_hash ^= c;
            }
        }

        constexpr bool operator==(const EventId &rhs) const = default;

    private:
        u32 m_hash;
    };

    struct EventBase {
        EventBase() noexcept = default;
    };

    template<typename... Params>
    struct Event : public EventBase {
        using Callback = std::function<void(Params...)>;

        explicit Event(Callback func) noexcept : m_func(std::move(func)) { }

        void operator()(Params... params) const noexcept {
            this->m_func(params...);
        }

    private:
        Callback m_func;
    };

    class EventManager {
    public:
        using EventList = std::list<std::pair<EventId, EventBase *>>;

        template<typename E>
        static EventList::iterator subscribe(typename E::Callback function) {
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
            auto iter = std::find_if(s_tokenStore.begin(), s_tokenStore.end(), [&](auto &item) {
                return item.first == token && item.second->first == E::id;
            });

            if (iter != s_tokenStore.end()) {
                s_events.remove(*iter->second);
                s_tokenStore.erase(iter);
            }

        }

        template<typename E>
        static void post(auto &&...args) noexcept {
            for (const auto &[id, event] : s_events) {
                if (id == E::id)
                    (*static_cast<E *const>(event))(std::forward<decltype(args)>(args)...);
            }
        }

        static void clear() noexcept {
            s_events.clear();
            s_tokenStore.clear();
        }

    private:
        static std::map<void *, EventList::iterator> s_tokenStore;
        static EventList s_events;
    };

    /* Default Events */
    EVENT_DEF(EventFileLoaded, std::fs::path);
    EVENT_DEF(EventDataChanged);
    EVENT_DEF(EventHighlightingChanged);
    EVENT_DEF(EventWindowClosing, GLFWwindow *);
    EVENT_DEF(EventRegionSelected, ImHexApi::HexEditor::ProviderRegion);
    EVENT_DEF(EventSettingsChanged);
    EVENT_DEF(EventAbnormalTermination, int);
    EVENT_DEF(EventOSThemeChanged);
    EVENT_DEF(EventProviderCreated, prv::Provider *);
    EVENT_DEF(EventProviderChanged, prv::Provider *, prv::Provider *);
    EVENT_DEF(EventProviderOpened,  prv::Provider *);
    EVENT_DEF(EventProviderClosing, prv::Provider *, bool *);
    EVENT_DEF(EventProviderClosed,  prv::Provider *);
    EVENT_DEF(EventProviderDeleted, prv::Provider *);
    EVENT_DEF(EventFrameBegin);
    EVENT_DEF(EventFrameEnd);
    EVENT_DEF(EventWindowInitialized);

    EVENT_DEF(RequestOpenWindow, std::string);
    EVENT_DEF(RequestSelectionChange, Region);
    EVENT_DEF(RequestAddBookmark, Region, std::string, std::string, color_t);
    EVENT_DEF(RequestSetPatternLanguageCode, std::string);
    EVENT_DEF(RequestChangeWindowTitle);
    EVENT_DEF(RequestCloseImHex, bool);
    EVENT_DEF(RequestRestartImHex);
    EVENT_DEF(RequestOpenFile, std::fs::path);
    EVENT_DEF(RequestChangeTheme, std::string);
    EVENT_DEF(RequestOpenPopup, std::string);
    EVENT_DEF(RequestCreateProvider, std::string, bool, hex::prv::Provider **);

    EVENT_DEF(RequestShowInfoPopup, std::string);
    EVENT_DEF(RequestShowErrorPopup, std::string);
    EVENT_DEF(RequestShowFatalErrorPopup, std::string);
    EVENT_DEF(RequestShowYesNoQuestionPopup, std::string, std::function<void()>, std::function<void()>);
    EVENT_DEF(RequestShowFileChooserPopup, std::vector<std::fs::path>, std::vector<nfdfilteritem_t>, std::function<void(std::fs::path)>);

}