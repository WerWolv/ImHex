#pragma once

#include <hex.hpp>
#include <hex/api/event_manager.hpp>

namespace hex {
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

    EVENT_DEF(EventProviderDataModified, prv::Provider *, u64, u64, const u8*);
    EVENT_DEF(EventProviderDataInserted, prv::Provider *, u64, u64);
    EVENT_DEF(EventProviderDataRemoved, prv::Provider *, u64, u64);
    EVENT_DEF(EventProviderDirtied, prv::Provider *);
}
