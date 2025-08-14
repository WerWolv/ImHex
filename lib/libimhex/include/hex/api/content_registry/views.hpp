#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/ui/view.hpp>

#include <map>
#include <memory>
#include <functional>

EXPORT_MODULE namespace hex {

    /* View Registry. Allows adding of new windows */
    namespace ContentRegistry::Views {

        namespace impl {

            void add(std::unique_ptr<View> &&view);
            void setFullScreenView(std::unique_ptr<View> &&view);

            const std::map<UnlocalizedString, std::unique_ptr<View>>& getEntries();
            const std::unique_ptr<View>& getFullScreenView();

        }

        /**
         * @brief Adds a new view to ImHex
         * @tparam T The custom view class that extends View
         * @tparam Args Arguments types
         * @param args Arguments passed to the constructor of the view
         */
        template<std::derived_from<View> T, typename... Args>
        void add(Args &&...args) {
            return impl::add(std::make_unique<T>(std::forward<Args>(args)...));
        }

        /**
         * @brief Sets a view as a full-screen view. This will cause the view to take up the entire ImHex window
         * @tparam T The custom view class that extends View
         * @tparam Args Arguments types
         * @param args Arguments passed to the constructor of the view
         */
        template<std::derived_from<View> T, typename... Args>
        void setFullScreenView(Args &&...args) {
            return impl::setFullScreenView(std::make_unique<T>(std::forward<Args>(args)...));
        }

        /**
         * @brief Gets a view by its unlocalized name
         * @param unlocalizedName The unlocalized name of the view
         * @return The view if it exists, nullptr otherwise
         */
        View* getViewByName(const UnlocalizedString &unlocalizedName);

        /**
         * @brief Gets the currently focused view
         * @return The view that is focused right now. nullptr if none is focused
         */
        View* getFocusedView();
    }

}