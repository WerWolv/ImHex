// dear imgui test engine
// (performance tool)
// Browse and visualize samples recorded by ctx->PerfCapture() calls.
// User access via 'Test Engine UI -> Tools -> Perf Tool'

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

#pragma once

#include "imgui.h"

// Forward Declaration
struct ImGuiPerfToolColumnInfo;
struct ImGuiTestEngine;
struct ImGuiCsvParser;

// Configuration
#define IMGUI_PERFLOG_DEFAULT_FILENAME  "output/imgui_perflog.csv"

// [Internal] Perf log entry. Changes to this struct should be reflected in ImGuiTestContext::PerfCapture() and ImGuiTestEngine_Start().
// This struct assumes strings stored here will be available until next ImGuiPerfTool::Clear() call. Fortunately we do not have to actively
// manage lifetime of these strings. New entries are created only in two cases:
// 1. ImGuiTestEngine_PerfToolAppendToCSV() call after perf test has run. This call receives ImGuiPerfToolEntry with const strings stored indefinitely by application.
// 2. As a consequence of ImGuiPerfTool::LoadCSV() call, we persist the ImGuiCSVParser instance, which keeps parsed CSV text, from which strings are referenced.
// As a result our solution also doesn't make many allocations.
struct IMGUI_API ImGuiPerfToolEntry
{
    ImU64                       Timestamp = 0;                  // Title of a particular batch of perftool entries.
    const char*                 Category = nullptr;             // Name of category perf test is in.
    const char*                 TestName = nullptr;             // Name of perf test.
    double                      DtDeltaMs = 0.0;                // Result of perf test.
    double                      DtDeltaMsMin = +FLT_MAX;        // May be used by perftool.
    double                      DtDeltaMsMax = -FLT_MAX;        // May be used by perftool.
    int                         NumSamples = 1;                 // Number aggregated samples.
    int                         PerfStressAmount = 0;           //
    const char*                 GitBranchName = nullptr;        // Build information.
    const char*                 BuildType = nullptr;            //
    const char*                 Cpu = nullptr;                  //
    const char*                 OS = nullptr;                   //
    const char*                 Compiler = nullptr;             //
    const char*                 Date = nullptr;                 // Date of this entry or min date of combined entries.
    //const char*               DateMax = nullptr;              // Max date of combined entries, or nullptr.
    double                      VsBaseline = 0.0;               // Percent difference vs baseline.
    int                         LabelIndex = 0;                 // Index of TestName in ImGuiPerfTool::_LabelsVisible.

    ImGuiPerfToolEntry()        { }
    ImGuiPerfToolEntry(const ImGuiPerfToolEntry& rhs)           { Set(rhs); }
    ImGuiPerfToolEntry& operator=(const ImGuiPerfToolEntry& rhs){ Set(rhs); return *this; }
    void Set(const ImGuiPerfToolEntry& rhs);
};

// [Internal] Perf log batch.
struct ImGuiPerfToolBatch
{
    ImU64                       BatchID = 0;                    // Timestamp of the batch, or unique ID of the build in combined mode.
    int                         NumSamples = 0;                 // A number of unique batches aggregated.
    int                         BranchIndex = 0;                // For per-branch color mapping.
    ImVector<ImGuiPerfToolEntry> Entries;                       // Aggregated perf test entries. Order follows ImGuiPerfTool::_LabelsVisible order.
    ~ImGuiPerfToolBatch()       { Entries.clear_destruct(); }   // FIXME: Misleading: nothing to destruct in that struct?
};

enum ImGuiPerfToolDisplayType : int
{
    ImGuiPerfToolDisplayType_Simple,                            // Each run will be displayed individually.
    ImGuiPerfToolDisplayType_PerBranchColors,                   // Use one bar color per branch.
    ImGuiPerfToolDisplayType_CombineByBuildInfo,                // Entries with same build information will be averaged.
};

//
struct IMGUI_API ImGuiPerfTool
{
    ImVector<ImGuiPerfToolEntry>_SrcData;                       // Raw entries from CSV file (with string pointer into CSV data).
    ImVector<const char*>       _Labels;
    ImVector<const char*>       _LabelsVisible;                 // ImPlot requires a pointer of all labels beforehand. Always contains a dummy "" entry at the end!
    ImVector<ImGuiPerfToolBatch> _Batches;
    ImGuiStorage                _LabelBarCounts;                // Number bars each label will render.
    int                         _NumVisibleBuilds = 0;          // Cached number of visible builds.
    int                         _NumUniqueBuilds = 0;           // Cached number of unique builds.
    ImGuiPerfToolDisplayType    _DisplayType = ImGuiPerfToolDisplayType_CombineByBuildInfo;
    int                         _BaselineBatchIndex = 0;        // Index of baseline build.
    ImU64                       _BaselineTimestamp = 0;
    ImU64                       _BaselineBuildId = 0;
    char                        _Filter[128];                   // Context menu filtering substring.
    char                        _FilterDateFrom[11] = {};
    char                        _FilterDateTo[11] = {};
    float                       _InfoTableHeight = 180.0f;
    int                         _AlignStress = 0;               // Alignment values for build info components, so they look aligned in the legend.
    int                         _AlignType = 0;
    int                         _AlignOs = 0;
    int                         _AlignCpu = 0;
    int                         _AlignCompiler = 0;
    int                         _AlignBranch = 0;
    int                         _AlignSamples = 0;
    bool                        _InfoTableSortDirty = false;
    ImVector<ImU64>             _InfoTableSort;                 // _InfoTableSort[_LabelsVisible.Size * _Batches.Size]. Contains sorted batch indices for each label.
    const ImGuiTableSortSpecs*  _InfoTableSortSpecs = nullptr;  // Current table sort specs.
    ImGuiStorage                _TempSet;                       // Used as a set
    int                         _TableHoveredTest = -1;         // Index within _VisibleLabelPointers array.
    int                         _TableHoveredBatch = -1;
    int                         _PlotHoverTest = -1;
    int                         _PlotHoverBatch = -1;
    bool                        _PlotHoverTestLabel = false;
    bool                        _ReportGenerating = false;
    ImGuiStorage                _Visibility;
    ImGuiCsvParser*             _CsvParser = nullptr;           // We keep this around and point to its fields

    ImGuiPerfTool();
    ~ImGuiPerfTool();

    void        Clear();
    bool        LoadCSV(const char* filename = nullptr);
    void        AddEntry(ImGuiPerfToolEntry* entry);

    void        ShowPerfToolWindow(ImGuiTestEngine* engine, bool* p_open);
    void        ViewOnly(const char* perf_name);
    void        ViewOnly(const char** perf_names);
    ImGuiPerfToolEntry* GetEntryByBatchIdx(int idx, const char* perf_name = nullptr);
    bool        SaveHtmlReport(const char* file_name, const char* image_file = nullptr);
    inline bool Empty()         { return _SrcData.empty(); }

    void        _Rebuild();
    bool        _IsVisibleBuild(ImGuiPerfToolBatch* batch);
    bool        _IsVisibleBuild(ImGuiPerfToolEntry* batch);
    bool        _IsVisibleTest(const char* test_name);
    void        _CalculateLegendAlignment();
    void        _ShowEntriesPlot();
    void        _ShowEntriesTable();
    void        _SetBaseline(int batch_index);
    void        _AddSettingsHandler();
    void        _UnpackSortedKey(ImU64 key, int* batch_index, int* entry_index, int* monotonic_index = nullptr);
};

IMGUI_API void    ImGuiTestEngine_PerfToolAppendToCSV(ImGuiPerfTool* perf_log, ImGuiPerfToolEntry* entry, const char* filename = nullptr);
