#pragma once

#include <hex.hpp>

#include <functional>
#include <hex/api/localization_manager.hpp>

EXPORT_MODULE namespace hex {

    /* Background Service Registry. Allows adding new background services */
    namespace ContentRegistry::BackgroundServices {

        namespace impl {
            using Callback = std::function<void()>;

            void stopServices();
        }

        void registerService(const UnlocalizedString &unlocalizedName, const impl::Callback &callback);
    }

}