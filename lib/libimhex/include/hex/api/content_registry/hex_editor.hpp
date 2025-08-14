#pragma once

#include <hex.hpp>

#include <imgui.h>

#include <hex/api/localization_manager.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <span>

EXPORT_MODULE namespace hex {

    /* Hex Editor Registry. Allows adding new functionality to the hex editor */
    namespace ContentRegistry::HexEditor {

        class DataVisualizer {
        public:
            DataVisualizer(UnlocalizedString unlocalizedName, u16 bytesPerCell, u16 maxCharsPerCell)
                : m_unlocalizedName(std::move(unlocalizedName)),
                  m_bytesPerCell(bytesPerCell),
                  m_maxCharsPerCell(maxCharsPerCell) { }

            virtual ~DataVisualizer() = default;

            virtual void draw(u64 address, const u8 *data, size_t size, bool upperCase) = 0;
            virtual bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) = 0;

            [[nodiscard]] u16 getBytesPerCell() const { return m_bytesPerCell; }
            [[nodiscard]] u16 getMaxCharsPerCell() const { return m_maxCharsPerCell; }

            [[nodiscard]] const UnlocalizedString& getUnlocalizedName() const { return m_unlocalizedName; }

            [[nodiscard]] static int DefaultTextInputFlags();

        protected:
            bool drawDefaultScalarEditingTextBox(u64 address, const char *format, ImGuiDataType dataType, u8 *data, ImGuiInputTextFlags flags) const;
            bool drawDefaultTextEditingTextBox(u64 address, std::string &data, ImGuiInputTextFlags flags) const;

        private:
            UnlocalizedString m_unlocalizedName;
            u16 m_bytesPerCell;
            u16 m_maxCharsPerCell;
        };

        struct MiniMapVisualizer {
            using Callback = std::function<void(u64, std::span<const u8>, std::vector<ImColor>&)>;

            UnlocalizedString unlocalizedName;
            Callback callback;
        };

        namespace impl {

            void addDataVisualizer(std::shared_ptr<DataVisualizer> &&visualizer);

            const std::vector<std::shared_ptr<DataVisualizer>>& getVisualizers();
            const std::vector<std::shared_ptr<MiniMapVisualizer>>& getMiniMapVisualizers();

        }

        /**
         * @brief Adds a new cell data visualizer
         * @tparam T The data visualizer type that extends hex::DataVisualizer
         * @param args The arguments to pass to the constructor of the data visualizer
         */
        template<std::derived_from<DataVisualizer> T, typename... Args>
        void addDataVisualizer(Args &&...args) {
            return impl::addDataVisualizer(std::make_shared<T>(std::forward<Args>(args)...));
        }

        /**
         * @brief Gets a data visualizer by its unlocalized name
         * @param unlocalizedName Unlocalized name of the data visualizer
         * @return The data visualizer, or nullptr if it doesn't exist
         */
        std::shared_ptr<DataVisualizer> getVisualizerByName(const UnlocalizedString &unlocalizedName);

        /**
         * @brief Adds a new minimap visualizer
         * @param unlocalizedName Unlocalized name of the minimap visualizer
         * @param callback The callback that will be called to get the color of a line
         */
        void addMiniMapVisualizer(UnlocalizedString unlocalizedName, MiniMapVisualizer::Callback callback);

    }

}