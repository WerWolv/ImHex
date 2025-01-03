#pragma once

#include <hex.hpp>

#include <algorithm>
#include <functional>
#include <list>
#include <mutex>
#include <map>
#include <string_view>

#include <hex/api/imhex_api.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/patches.hpp>

#include <wolv/types/type_name.hpp>

#define EVENT_DEF_IMPL(event_name, event_name_string, should_log, ...)                                                                                      \
    struct event_name final : public hex::impl::Event<__VA_ARGS__> {                                                                                        \
        constexpr static auto Id = [] { return hex::impl::EventId(event_name_string); }();                                                                  \
        constexpr static auto ShouldLog = (should_log);                                                                                                     \
        explicit event_name(Callback func) noexcept : Event(std::move(func)) { }                                                                            \
                                                                                                                                                            \
        static EventManager::EventList::iterator subscribe(Event::Callback function) { return EventManager::subscribe<event_name>(std::move(function)); }   \
        static void subscribe(void *token, Event::Callback function) { EventManager::subscribe<event_name>(token, std::move(function)); }                   \
        static void unsubscribe(const EventManager::EventList::iterator &token) noexcept { EventManager::unsubscribe(token); }                              \
        static void unsubscribe(void *token) noexcept { EventManager::unsubscribe<event_name>(token); }                                                     \
        static void post(auto &&...args) { EventManager::post<event_name>(std::forward<decltype(args)>(args)...); }                                         \
    }

#define EVENT_DEF(event_name, ...)          EVENT_DEF_IMPL(event_name, #event_name, true, __VA_ARGS__)
#define EVENT_DEF_NO_LOG(event_name, ...)   EVENT_DEF_IMPL(event_name, #event_name, false, __VA_ARGS__)


/* Forward declarations */
struct GLFWwindow;
namespace hex {
    class Achievement;
    class View;
}

namespace pl::ptrn { class Pattern; }

namespace hex {

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

            explicit Event(Callback func) noexcept : m_func(std::move(func)) { }

            template<typename E>
            void call(Params... params) const {
                try {
                    m_func(params...);
                } catch (const std::exception &e) {
                    log::error("An exception occurred while handling event {}: {}", wolv::type::getTypeName<E>(), e.what());
                    throw;
                }
            }

        private:
            Callback m_func;
        };

        template<typename T>
        concept EventType = std::derived_from<T, EventBase>;

    }


    /**
     * @brief The EventManager allows subscribing to and posting events to different parts of the program.
     * To create a new event, use the EVENT_DEF macro. This will create a new event type with the given name and parameters
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
        static EventList::iterator subscribe(typename E::Callback function) {
            std::scoped_lock lock(getEventMutex());

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
        static void subscribe(void *token, typename E::Callback function) {
            std::scoped_lock lock(getEventMutex());

            if (isAlreadyRegistered(token, E::Id)) {
                log::fatal("The token '{}' has already registered the same event ('{}')", token, wolv::type::getTypeName<E>());
                return;
            }

            getTokenStore().insert({ token, subscribe<E>(function) });
        }

        /**
         * @brief Unsubscribes from an event
         * @param token Token returned by subscribe
         */
        static void unsubscribe(const EventList::iterator &token) noexcept {
            std::scoped_lock lock(getEventMutex());

            getEvents().erase(token);
        }

        /**
         * @brief Unsubscribes from an event
         * @tparam E Event
         * @param token Token passed to subscribe
         */
        template<impl::EventType E>
        static void unsubscribe(void *token) noexcept {
            std::scoped_lock lock(getEventMutex());

            unsubscribe(token, E::Id);
        }

        /**
         * @brief Posts an event to all subscribers of it
         * @tparam E Event
         * @param args Arguments to pass to the event
         */
        template<impl::EventType E>
        static void post(auto && ...args) {
            std::scoped_lock lock(getEventMutex());

            auto [begin, end] = getEvents().equal_range(E::Id);
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
            std::scoped_lock lock(getEventMutex());

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

    /* Default Events */
    
    /**
     * @brief Called when Imhex finished startup, and will enter the main window rendering loop
     */
    EVENT_DEF(EventImHexStartupFinished);

    EVENT_DEF(EventFileLoaded, std::fs::path);
    EVENT_DEF(EventDataChanged, prv::Provider *);
    EVENT_DEF(EventHighlightingChanged);
    EVENT_DEF(EventWindowClosing, GLFWwindow *);
    EVENT_DEF(EventRegionSelected, ImHexApi::HexEditor::ProviderRegion);
    EVENT_DEF(EventAbnormalTermination, int);
    EVENT_DEF(EventThemeChanged);
    EVENT_DEF(EventOSThemeChanged);
    EVENT_DEF(EventDPIChanged, float, float);
    EVENT_DEF(EventWindowFocused, bool);
    EVENT_DEF(EventImHexUpdated, SemanticVersion, SemanticVersion);

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
    EVENT_DEF(EventWindowDeinitializing, GLFWwindow *);
    EVENT_DEF(EventBookmarkCreated, ImHexApi::Bookmarks::Entry&);

    /**
     * @brief Called upon creation of an IPS patch.
     * As for now, the event only serves a purpose for the achievement unlock.
     */
    EVENT_DEF(EventPatchCreated, const u8*, u64, const PatchKind);
    EVENT_DEF(EventPatternEvaluating);
    EVENT_DEF(EventPatternExecuted, const std::string&);
    EVENT_DEF(EventPatternEditorChanged, const std::string&);
    EVENT_DEF(EventStoreContentDownloaded, const std::fs::path&);
    EVENT_DEF(EventStoreContentRemoved, const std::fs::path&);
    EVENT_DEF(EventImHexClosing);
    EVENT_DEF(EventAchievementUnlocked, const Achievement&);
    EVENT_DEF(EventSearchBoxClicked, u32);
    EVENT_DEF(EventViewOpened, View*);
    EVENT_DEF(EventFirstLaunch);

    EVENT_DEF(EventFileDragged, bool);
    EVENT_DEF(EventFileDropped, std::fs::path);

    EVENT_DEF(EventProviderDataModified, prv::Provider *, u64, u64, const u8*);
    EVENT_DEF(EventProviderDataInserted, prv::Provider *, u64, u64);
    EVENT_DEF(EventProviderDataRemoved, prv::Provider *, u64, u64);
    EVENT_DEF(EventProviderDirtied, prv::Provider *);

    /**
     * @brief Called when a project has been loaded
     */
    EVENT_DEF(EventProjectOpened);

    EVENT_DEF_NO_LOG(EventFrameBegin);
    EVENT_DEF_NO_LOG(EventFrameEnd);
    EVENT_DEF_NO_LOG(EventSetTaskBarIconState, u32, u32, u32);
    EVENT_DEF_NO_LOG(EventImGuiElementRendered, ImGuiID, const std::array<float, 4>&);

    EVENT_DEF(RequestAddInitTask, std::string, bool, std::function<bool()>);
    EVENT_DEF(RequestAddExitTask, std::string, std::function<bool()>);
    EVENT_DEF(RequestOpenWindow, std::string);
    EVENT_DEF(RequestHexEditorSelectionChange, Region);
    EVENT_DEF(RequestPatternEditorSelectionChange, u32, u32);
    EVENT_DEF(RequestJumpToPattern, const pl::ptrn::Pattern*);
    EVENT_DEF(RequestAddBookmark, Region, std::string, std::string, color_t, u64*);
    EVENT_DEF(RequestRemoveBookmark, u64);
    EVENT_DEF(RequestSetPatternLanguageCode, std::string);
    EVENT_DEF(RequestRunPatternCode);
    EVENT_DEF(RequestLoadPatternLanguageFile, std::fs::path);
    EVENT_DEF(RequestSavePatternLanguageFile, std::fs::path);
    EVENT_DEF(RequestUpdateWindowTitle);
    EVENT_DEF(RequestCloseImHex, bool);
    EVENT_DEF(RequestRestartImHex);
    EVENT_DEF(RequestOpenFile, std::fs::path);
    EVENT_DEF(RequestChangeTheme, std::string);
    EVENT_DEF(RequestOpenPopup, std::string);
    EVENT_DEF(RequestAddVirtualFile, std::fs::path, std::vector<u8>, Region);
    EVENT_DEF(RequestStartMigration);

    /**
     * @brief Creates a provider from it's unlocalized name, and add it to the provider list
    */
    EVENT_DEF(RequestCreateProvider, std::string, bool, bool, hex::prv::Provider **);
    EVENT_DEF(RequestInitThemeHandlers);

    /**
     * @brief Send an event to the main Imhex instance
     */
    EVENT_DEF(SendMessageToMainInstance, const std::string, const std::vector<u8>&);

    /**
     * Move the data from all PerProvider instances from one provider to another.
     * The 'from' provider should not have any per provider data after this, and should be immediately deleted
    */
    EVENT_DEF(MovePerProviderData, prv::Provider *, prv::Provider *);

    /**
     * Called when ImHex managed to catch an error in a general try/catch to prevent/recover from a crash
    */
    EVENT_DEF(EventCrashRecovered, const std::exception &);
}