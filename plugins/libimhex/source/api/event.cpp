#include <hex/api/event.hpp>

namespace hex {

    std::map<void*, EventManager::EventList::iterator> EventManager::s_tokenStore;
    EventManager::EventList EventManager::s_events;

}