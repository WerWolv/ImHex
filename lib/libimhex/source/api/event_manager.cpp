#include <hex/api/event_manager.hpp>

namespace hex {

    std::multimap<void *, EventManager::EventList::iterator>& EventManager::getTokenStore() {
        static std::multimap<void *, EventManager::EventList::iterator> tokenStore;

        return tokenStore;
    }

    EventManager::EventList& EventManager::getEvents() {
        static EventManager::EventList events;

        return events;
    }

    std::recursive_mutex& EventManager::getEventMutex() {
        static std::recursive_mutex mutex;

        return mutex;
    }


    bool EventManager::isAlreadyRegistered(void *token, impl::EventId id) {
        if (getTokenStore().contains(token)) {
            auto&& [begin, end] = getTokenStore().equal_range(token);

            return std::any_of(begin, end, [&](auto &item) {
                return item.second->first == id;
            });
        }

        return false;
    }

    void EventManager::unsubscribe(void *token, impl::EventId id) {
        auto &tokenStore = getTokenStore();
        auto iter = std::find_if(tokenStore.begin(), tokenStore.end(), [&](auto &item) {
            return item.first == token && item.second->first == id;
        });

        if (iter != tokenStore.end()) {
            getEvents().erase(iter->second);
            tokenStore.erase(iter);
        }
    }



}