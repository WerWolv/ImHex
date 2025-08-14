#pragma once

#include <hex.hpp>

#include <functional>
#include <string>
#include <map>
#include <optional>
#include <set>
#include <vector>

EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace prv { class Provider; }
    #endif

    /* Functions to query information from the Hex Editor and interact with it */
    namespace ImHexApi::HexEditor {

        using TooltipFunction = std::function<void(u64, const u8*, size_t)>;

        class Highlighting {
        public:
            Highlighting() = default;
            Highlighting(Region region, color_t color);

            [[nodiscard]] const Region& getRegion() const { return m_region; }
            [[nodiscard]] const color_t& getColor() const { return m_color; }

        private:
            Region m_region = {};
            color_t m_color = 0x00;
        };

        class Tooltip {
        public:
            Tooltip() = default;
            Tooltip(Region region, std::string value, color_t color);

            [[nodiscard]] const Region& getRegion() const { return m_region; }
            [[nodiscard]] const color_t& getColor() const { return m_color; }
            [[nodiscard]] const std::string& getValue() const { return m_value; }

        private:
            Region m_region = {};
            std::string m_value;
            color_t m_color = 0x00;
        };

        struct ProviderRegion : Region {
            prv::Provider *provider;

            [[nodiscard]] prv::Provider *getProvider() const { return this->provider; }

            [[nodiscard]] Region getRegion() const { return { this->address, this->size }; }
        };

        namespace impl {

            using HighlightingFunction = std::function<std::optional<color_t>(u64, const u8*, size_t, bool)>;
            using HoveringFunction = std::function<std::set<Region>(const prv::Provider *, u64, size_t)>;

            const std::map<u32, Highlighting>& getBackgroundHighlights();
            const std::map<u32, HighlightingFunction>& getBackgroundHighlightingFunctions();
            const std::map<u32, Highlighting>& getForegroundHighlights();
            const std::map<u32, HighlightingFunction>& getForegroundHighlightingFunctions();
            const std::map<u32, HoveringFunction>& getHoveringFunctions();
            const std::map<u32, Tooltip>& getTooltips();
            const std::map<u32, TooltipFunction>& getTooltipFunctions();

            void setCurrentSelection(const std::optional<ProviderRegion> &region);
            void setHoveredRegion(const prv::Provider *provider, const Region &region);
        }

        /**
         * @brief Adds a background color highlighting to the Hex Editor
         * @param region The region to highlight
         * @param color The color to use for the highlighting
         * @return Unique ID used to remove the highlighting again later
         */
        u32 addBackgroundHighlight(const Region &region, color_t color);

        /**
         * @brief Removes a background color highlighting from the Hex Editor
         * @param id The ID of the highlighting to remove
         */
        void removeBackgroundHighlight(u32 id);


        /**
         * @brief Adds a foreground color highlighting to the Hex Editor
         * @param region The region to highlight
         * @param color The color to use for the highlighting
         * @return Unique ID used to remove the highlighting again later
         */
        u32 addForegroundHighlight(const Region &region, color_t color);

        /**
         * @brief Removes a foreground color highlighting from the Hex Editor
         * @param id The ID of the highlighting to remove
         */
        void removeForegroundHighlight(u32 id);

        /**
         * @brief Adds a hover tooltip to the Hex Editor
         * @param region The region to add the tooltip to
         * @param value Text to display in the tooltip
         * @param color The color of the tooltip
         * @return Unique ID used to remove the tooltip again later
         */
        u32 addTooltip(Region region, std::string value, color_t color);

        /**
         * @brief Removes a hover tooltip from the Hex Editor
         * @param id The ID of the tooltip to remove
         */
        void removeTooltip(u32 id);


        /**
         * @brief Adds a background color highlighting to the Hex Editor using a callback function
         * @param function Function that draws the highlighting based on the hovered region
         * @return Unique ID used to remove the highlighting again later
         */
        u32 addTooltipProvider(TooltipFunction function);

        /**
         * @brief Removes a background color highlighting from the Hex Editor
         * @param id The ID of the highlighting to remove
         */
        void removeTooltipProvider(u32 id);


        /**
         * @brief Adds a background color highlighting to the Hex Editor using a callback function
         * @param function Function that draws the highlighting based on the hovered region
         * @return Unique ID used to remove the highlighting again later
         */
        u32 addBackgroundHighlightingProvider(const impl::HighlightingFunction &function);

        /**
         * @brief Removes a background color highlighting from the Hex Editor
         * @param id The ID of the highlighting to remove
         */
        void removeBackgroundHighlightingProvider(u32 id);


        /**
         * @brief Adds a foreground color highlighting to the Hex Editor using a callback function
         * @param function Function that draws the highlighting based on the hovered region
         * @return Unique ID used to remove the highlighting again later
         */
        u32 addForegroundHighlightingProvider(const impl::HighlightingFunction &function);

        /**
         * @brief Removes a foreground color highlighting from the Hex Editor
         * @param id The ID of the highlighting to remove
         */
        void removeForegroundHighlightingProvider(u32 id);

        /**
         * @brief Adds a hovering provider to the Hex Editor using a callback function
         * @param function Function that draws the highlighting based on the hovered region
         * @return Unique ID used to remove the highlighting again later
         */
        u32 addHoverHighlightProvider(const impl::HoveringFunction &function);

        /**
         * @brief Removes a hovering color highlighting from the Hex Editor
         * @param id The ID of the highlighting to remove
         */
        void removeHoverHighlightProvider(u32 id);

        /**
         * @brief Checks if there's a valid selection in the Hex Editor right now
         */
        bool isSelectionValid();

        /**
         * @brief Clears the current selection in the Hex Editor
         */
        void clearSelection();

        /**
         * @brief Gets the current selection in the Hex Editor
         * @return The current selection
         */
        std::optional<ProviderRegion> getSelection();

        /**
         * @brief Sets the current selection in the Hex Editor
         * @param region The region to select
         * @param provider The provider to select the region in
         */
        void setSelection(const Region &region, prv::Provider *provider = nullptr);

        /**
         * @brief Sets the current selection in the Hex Editor
         * @param region The region to select
         */
        void setSelection(const ProviderRegion &region);

        /**
         * @brief Sets the current selection in the Hex Editor
         * @param address The address to select
         * @param size The size of the selection
         * @param provider The provider to select the region in
         */
        void setSelection(u64 address, size_t size, prv::Provider *provider = nullptr);

        /**
         * @brief Adds a virtual file to the list in the Hex Editor
         * @param path The path of the file
         * @param data The data of the file
         * @param region The location of the file in the Hex Editor if available
         */
        void addVirtualFile(const std::string &path, std::vector<u8> data, Region region = Region::Invalid());

        /**
         * @brief Gets the currently hovered cell region in the Hex Editor
         * @return
         */
        const std::optional<Region>& getHoveredRegion(const prv::Provider *provider);

    }

}