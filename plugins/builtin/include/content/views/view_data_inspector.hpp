#pragma once

#include <hex/ui/view.hpp>
#include <ui/visualizer_drawer.hpp>

#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/content_registry/views.hpp>
#include <hex/api/content_registry/data_inspector.hpp>
#include <hex/api/task_manager.hpp>

#include <bit>
#include <string>

namespace hex::plugin::builtin {

    class ViewDataInspector : public View::Window {
    public:
        explicit ViewDataInspector();
        ~ViewDataInspector() override;

        void drawContent() override;

        View* getMenuItemInheritView() const override {
            return ContentRegistry::Views::getViewByName("hex.builtin.view.hex_editor.name"_unlocalized);
        }

        void drawHelpText() override;

    private:
        struct InspectorCacheEntry {
            UnlocalizedString unlocalizedName;
            ContentRegistry::DataInspector::impl::DisplayFunction displayFunction;
            std::optional<ContentRegistry::DataInspector::impl::EditingFunction> editingFunction;
            bool editing;
            u64 requiredSize;
            u64 maxSize;

            /**
             * @brief The number of bytes to select when this row is clicked
             *
             * Unset when a click should leave the current selection alone, for
             * a variable size row with no way to know how much a value used.
             */
            std::optional<u64> clickSelectSize;

            std::string filterValue;

            /**
             * @brief Shown in the name column instead of translating unlocalizedName
             *
             * Set for a row whose name is not known until it is built, such as
             * the document encoding row naming the encoding in effect.
             */
            std::optional<std::string> displayName;

            /**
             * @brief Whether this row's display error is already logged
             *
             * Stops a failing row from logging on every frame.
             */
            bool displayErrorLogged = false;
        };

    private:
        void invalidateData();
        void updateInspectorRows();
        void updateInspectorRowsTask();
        void addDocumentEncodingRow();

        void executeInspectors();
        void executeInspector(const std::string& code, const std::fs::path& path, const std::map<std::string, pl::core::Token::Literal>& inVariables);

        void inspectorReadFunction(u64 offset, u8 *buffer, size_t size);

        void preprocessBytes(std::span<u8> data);

        // draw functions
        void drawEndianSetting();
        void drawRadixSetting();
        void drawInvertSetting();
        void drawReverseSetting();
        void drawInspectorRows();
        void drawInspectorRow(InspectorCacheEntry& entry);

        ContentRegistry::DataInspector::impl::DisplayFunction createPatternErrorDisplayFunction(const std::fs::path &path);

    private:
        bool m_shouldInvalidate = true;

        std::endian m_endian = std::endian::native;
        ContentRegistry::DataInspector::NumberDisplayStyle m_numberDisplayStyle = ContentRegistry::DataInspector::NumberDisplayStyle::Decimal;
        bool m_invert = false;
        bool m_reverse = false;

        ui::VisualizerDrawer m_visualizerDrawer;
        ImHexApi::HexEditor::ProviderRegion m_selectedRegion = {};
        size_t m_validBytes = 0;
        std::atomic<bool> m_dataValid = false;

        pl::PatternLanguage m_runtime;
        std::vector<InspectorCacheEntry> m_cachedData, m_workData;
        std::optional<UnlocalizedString> m_selectedEntryName;

        TaskHolder m_updateTask;

        std::string m_editingValue;

        bool m_tableEditingModeEnabled = false;
        std::set<std::string> m_hiddenValues;
    };

}
