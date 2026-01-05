#pragma once

#include <hex.hpp>
#include <hex/api/event_manager.hpp>

/* Provider events definitions */
namespace hex {

    namespace prv {
        class Provider;
    }

    /**
     * @brief Called when the provider is created.
     * This event is responsible for (optionally) initializing the provider and calling EventProviderOpened
     * (although the event can also be called manually without problem)
     */
    EVENT_DEF(EventProviderCreated, std::shared_ptr<prv::Provider>);

    /**
     * @brief Called as a continuation of EventProviderCreated
     * this event is normally called immediately after EventProviderCreated successfully initialized the provider.
     * If no initialization (Provider::skipLoadInterface() has been set), this event should be called manually
     * If skipLoadInterface failed, this event is not called
     *
     * @note this is not related to Provider::open()
     */
    EVENT_DEF(EventProviderOpened,  prv::Provider *);

    /**
     * @brief Signals a change in provider (in-place)
     *
     * Note: if the provider was deleted, the new ("current") provider will be `nullptr`
     *
     * @param oldProvider the old provider
     * @param currentProvider the current provider
     */
    EVENT_DEF(EventProviderChanged, prv::Provider *, prv::Provider *);

    /**
     * @brief Signals that a provider was saved
     *
     * @param provider the saved provider
     */
    EVENT_DEF(EventProviderSaved,   prv::Provider *);

    /**
     * @brief Signals a provider is closing
     *
     * FIXME: as for now, this behaves as a request more than an event. Also, the boolean is always set to true,
     *  and serves no purpose. This should be moved into the Provider requests section and declared accordingly.
     *
     * @param provider the closing provider
     * @param shouldClose whether the provider should close
     */
    EVENT_DEF(EventProviderClosing, prv::Provider *, bool *);

    /**
     * @brief Signals that a provider was closed
     *
     * As this is a closure information broadcast, the provider should generally not be accessed, as it could
     * result in problems.
     *
     * @param provider the now-closed provider
     */
    EVENT_DEF(EventProviderClosed,  prv::Provider *);

    /**
     * @brief Signals that a provider is being deleted
     *
     * Provider's data should not be accessed.
     *
     * @param provider the provider
     */
    EVENT_DEF(EventProviderDeleted, prv::Provider *);

}

/* Provider data events definitions */
namespace hex {

    /**
     * @brief Signals the dirtying of a provider
     *
     * Any data modification that occurs in a provider dirties it, until its state is either saved or restored.
     * This event signals that fact to subscribers so additional code can be executed for certain cases.
     */
    EVENT_DEF(EventProviderDirtied, prv::Provider *);

    /**
     * @brief Signals an insertion of new data into a provider
     *
     * @param provider the provider
     * @param offset the start of the insertion
     * @param size the new data's size
     */
    EVENT_DEF(EventProviderDataInserted, prv::Provider *, u64, u64);

    /**
     * @brief Signals a modification in the provider's data
     *
     * @param provider the provider
     * @param offset the data modification's offset (start address)
     * @param size the buffer's size
     * @param buffer the modified data written at this address
     */
    EVENT_DEF(EventProviderDataModified, prv::Provider *, u64, u64, const u8*);

    /**
     * @brief Signals a removal of some of the provider's data
     *
     * @param provider the provider
     * @param offset the deletion offset (start address)
     * @param size the deleted data's size
     */
    EVENT_DEF(EventProviderDataRemoved, prv::Provider *, u64, u64);

}
