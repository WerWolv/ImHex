#pragma once

#include <hex.hpp>

#include <list>
#include <map>
#include <string_view>
#include <functional>

#include <hex/api/imhex_api.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>

#include <wolv/types/type_name.hpp>

#define EVENT_DEF_IMPL(event_name, should_log, ...)                                     \
    struct event_name final : public hex::impl::Event<__VA_ARGS__> {                    \
        constexpr static auto Id = [] { return hex::impl::EventId(); }();               \
        constexpr static auto ShouldLog = (should_log);                                 \
        explicit event_name(Callback func) noexcept : Event(std::move(func)) { }        \
    }

#define EVENT_DEF(event_name, ...)          EVENT_DEF_IMPL(event_name, true, __VA_ARGS__)
#define EVENT_DEF_NO_LOG(event_name, ...)   EVENT_DEF_IMPL(event_name, false, __VA_ARGS__)

struct GLFWwindow;

namespace hex {

    namespace impl {

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

    }


    /**
     * @brief The EventManager allows subscribing to and posting events to different parts of the program.
     * To create a new event, use the EVENT_DEF macro. This will create a new event type with the given name and parameters
     */
    class EventManager {
    public:
        using EventList = std::list<std::pair<impl::EventId, impl::EventBase *>>;

        /**
         * @brief Subscribes to an event
         * @tparam E Event
         * @param function Function to call when the event is posted
         * @return Token to unsubscribe from the event
         */
        template<typename E>
        static EventList::iterator subscribe(typename E::Callback function) {
            return s_events.insert(s_events.end(), std::make_pair(E::Id, new E(function)));
        }

        /**
         * @brief Subscribes to an event
         * @tparam E Event
         * @param token Unique token to register the event to. Later required to unsubscribe again
         * @param function Function to call when the event is posted
         */
        template<typename E>
        static void subscribe(void *token, typename E::Callback function) {
            s_tokenStore.insert(std::make_pair(token, subscribe<E>(function)));
        }

        /**
         * @brief Unsubscribes from an event
         * @param token Token returned by subscribe
         */
        static void unsubscribe(const EventList::iterator &token) noexcept {
            s_events.erase(token);
        }

        /**
         * @brief Unsubscribes from an event
         * @tparam E Event
         * @param token Token passed to subscribe
         */
        template<typename E>
        static void unsubscribe(void *token) noexcept {
            auto iter = std::find_if(s_tokenStore.begin(), s_tokenStore.end(), [&](auto &item) {
                return item.first == token && item.second->first == E::Id;
            });

            if (iter != s_tokenStore.end()) {
                s_events.remove(*iter->second);
                s_tokenStore.erase(iter);
            }

        }

        /**
         * @brief Posts an event to all subscribers of it
         * @tparam E Event
         * @param args Arguments to pass to the event
         */
        template<typename E>
        static void post(auto &&...args) noexcept {
            for (const auto &[id, event] : s_events) {
                if (id == E::Id) {
                    (*static_cast<E *const>(event))(std::forward<decltype(args)>(args)...);
                }
            }

            #if defined (DEBUG)
                if (E::ShouldLog)
                    log::debug("Event posted: '{}'", wolv::type::getTypeName<E>());
            #endif
        }

        /**
         * @brief Unsubscribe all subscribers from all events
         */
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

    /**
     * @brief Called when the provider is created.
     * This event is responsible for (optionally) initializing the provider and calling EventProviderOpened
     * (although the event can also be called manually without problem)
     */
    EVENT_DEF(EventProviderCreated, prv::Provider *);
    EVENT_DEF(EventProviderChanged, prv::Provider *, prv::Provider *);

    /**
     * @brief Called as a continuation of EventProviderCreated
     * this event is normally called immediately after EventProviderCreated successfully initialized the provider.
     * If no initialization (Provider::skipLoadInterface() has been set), this event should be called manually
     * If skipLoadInterface failed, this event is not called
     * 
     * @note this is not related to Provider::open()
     */
    EVENT_DEF(EventProviderOpened,  prv::Provider *);
    EVENT_DEF(EventProviderClosing, prv::Provider *, bool *);
    EVENT_DEF(EventProviderClosed,  prv::Provider *);
    EVENT_DEF(EventProviderDeleted, prv::Provider *);
    EVENT_DEF(EventProviderSaved,   prv::Provider *);
    EVENT_DEF(EventWindowInitialized);
    EVENT_DEF(EventBookmarkCreated, ImHexApi::Bookmarks::Entry&);
    EVENT_DEF(EventPatchCreated, u64, u8, u8);
    EVENT_DEF(EventPatternExecuted, const std::string&);
    EVENT_DEF(EventPatternEditorChanged, const std::string&);
    EVENT_DEF(EventStoreContentDownloaded, const std::fs::path&);
    EVENT_DEF(EventStoreContentRemoved, const std::fs::path&);
    EVENT_DEF(EventImHexClosing);

    EVENT_DEF_NO_LOG(EventFrameBegin);
    EVENT_DEF_NO_LOG(EventFrameEnd);
    EVENT_DEF_NO_LOG(EventSetTaskBarIconState, u32, u32, u32);

    EVENT_DEF(RequestOpenWindow, std::string);
    EVENT_DEF(RequestSelectionChange, Region);
    EVENT_DEF(RequestAddBookmark, Region, std::string, std::string, color_t);
    EVENT_DEF(RequestSetPatternLanguageCode, std::string);
    EVENT_DEF(RequestLoadPatternLanguageFile, std::fs::path);
    EVENT_DEF(RequestSavePatternLanguageFile, std::fs::path);
    EVENT_DEF(RequestUpdateWindowTitle);
    EVENT_DEF(RequestCloseImHex, bool);
    EVENT_DEF(RequestRestartImHex);
    EVENT_DEF(RequestOpenFile, std::fs::path);
    EVENT_DEF(RequestChangeTheme, std::string);
    EVENT_DEF(RequestOpenPopup, std::string);

    /**
     * @brief Creates a provider from it's unlocalized name, and add it to the provider list
    */
    EVENT_DEF(RequestCreateProvider, std::string, bool, hex::prv::Provider **);
    EVENT_DEF(RequestInitThemeHandlers);

    EVENT_DEF(RequestOpenInfoPopup, const std::string);
    EVENT_DEF(RequestOpenErrorPopup, const std::string);
    EVENT_DEF(RequestOpenFatalPopup, const std::string);
}