#pragma once

#include <hex/api/event_manager.hpp>

/* Provider requests definitions */
namespace hex {

    /**
     * @brief Creates a provider from its unlocalized name, and add it to the provider list
    */
    EVENT_DEF(RequestCreateProvider, std::string, bool, bool, hex::prv::Provider **);

    /**
     * @brief Move the data from all PerProvider instances from one provider to another
     *
     * The 'from' provider should not have any per provider data after this, and should be immediately deleted
     *
     * FIXME: rename with the "Request" prefix to apply standard naming convention.
    */
    EVENT_DEF(MovePerProviderData, prv::Provider *, prv::Provider *);

}
