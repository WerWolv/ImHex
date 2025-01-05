#pragma once

#include <hex/ui/view.hpp>
#include <ui/visualizer_drawer.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/task_manager.hpp>

#include <bit>
#include <string>

namespace hex::plugin::builtin {

    class ViewDataInspector : public View::Window {
    public:
        explicit ViewDataInspector();
        ~ViewDataInspector() override;

        void drawContent() override;

    private:
        struct InspectorCacheEntry {
            UnlocalizedString unlocalizedName;
            ContentRegistry::DataInspector::impl::DisplayFunction displayFunction;
            std::optional<ContentRegistry::DataInspector::impl::EditingFunction> editingFunction;
            bool editing;
            u64 requiredSize;

            std::string filterValue;
        };

    private:
        void invalidateData();
        void updateInspectorRows();
        void updateInspectorRowsTask();

        void executeInspectors();
        void executeInspector(const std::string& code, const std::fs::path& path, const std::map<std::string, pl::core::Token::Literal>& inVariables);

        void inspectorReadFunction(u64 offset, u8 *buffer, size_t size);

        // draw functions
        void drawEndianSetting();
        void drawRadixSetting();
        void drawInvertSetting();
        void drawInspectorRows();
        void drawInspectorRow(InspectorCacheEntry& entry);

        ContentRegistry::DataInspector::impl::DisplayFunction createPatternErrorDisplayFunction();

    private:
        bool m_shouldInvalidate = true;

        std::endian m_endian = std::endian::native;
        ContentRegistry::DataInspector::NumberDisplayStyle m_numberDisplayStyle = ContentRegistry::DataInspector::NumberDisplayStyle::Decimal;
        bool m_invert = false;

        ui::VisualizerDrawer m_visualizerDrawer;
        u64 m_startAddress  = 0;
        size_t m_validBytes = 0;
        prv::Provider *m_selectedProvider = nullptr;
        std::atomic<bool> m_dataValid = false;

        pl::PatternLanguage m_runtime;
        std::vector<InspectorCacheEntry> m_cachedData, m_workData;
        std::optional<UnlocalizedString> m_selectedEntryName;

        TaskHolder m_updateTask;

        std::string m_editingValue = "";

        bool m_tableEditingModeEnabled = false;
        std::set<std::string> m_hiddenValues;
    };

}