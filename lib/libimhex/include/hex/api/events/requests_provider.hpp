#pragma once

#include <hex.hpp>
#include <hex/api/event_manager.hpp>


namespace hex {


    /**
     * @brief Creates a provider from it's unlocalized name, and add it to the provider list
    */
    EVENT_DEF(RequestCreateProvider, std::string, bool, bool, hex::prv::Provider **);

    /**
     * Move the data from all PerProvider instances from one provider to another.
     * The 'from' provider should not have any per provider data after this, and should be immediately deleted
    */
    EVENT_DEF(MovePerProviderData, prv::Provider *, prv::Provider *);

}
