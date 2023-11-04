#include <hex/api/event.hpp>

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


}