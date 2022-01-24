#include <hex/api/event.hpp>

namespace hex {

    EventManager::EventList EventManager::s_events;
    std::map<void *, EventManager::EventList::iterator> EventManager::s_tokenStore;

}