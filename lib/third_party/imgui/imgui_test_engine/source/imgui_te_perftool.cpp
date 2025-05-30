// dear imgui test engine
// (performance tool)
// Browse and visualize samples recorded by ctx->PerfCapture() calls.
// User access via 'Test Engine UI -> Tools -> Perf Tool'

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

/*

Index of this file:
// [SECTION] Header mess
// [SECTION] ImGuiPerflogEntry
// [SECTION] Types & everything else
// [SECTION] USER INTERFACE
// [SECTION] SETTINGS
// [SECTION] TESTS

*/

// Terminology:
// * Entry: information about execution of a single perf test. This corresponds to one line in CSV file.
// * Batch: a group of entries that were created together during a single execution. A new batch is created each time
//   one or more perf tests are executed. All entries in a single batch will have a matching ImGuiPerflogEntry::Timestamp.
// * Build: A group of batches that have matching BuildType, OS, Cpu, Compiler, GitBranchName.
// * Baseline: A batch that we are comparing against. Baselines are identified by batch timestamp and build id.

//-------------------------------------------------------------------------
// [SECTION] Header mess
//-------------------------------------------------------------------------

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_te_perftool.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_utils.h"
#include "thirdparty/Str/Str.h"
#include <time.h>               // time(), localtime()
#if IMGUI_TEST_ENGINE_ENABLE_IMPLOT
#include "implot.h"
#include "implot_internal.h"
#endif

// For tests
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "imgui_te_internal.h"  // ImGuiTestEngine_GetPerfTool()
#include "imgui_capture_tool.h"

//-------------------------------------------------------------------------
// [SECTION] ImGuiPerflogEntry
//-------------------------------------------------------------------------

void ImGuiPerfToolEntry::Set(const ImGuiPerfToolEntry& other)
{
    Timestamp = other.Timestamp;
    Category = other.Category;
    TestName = other.TestName;
    DtDeltaMs = other.DtDeltaMs;
    DtDeltaMsMin = other.DtDeltaMsMin;
    DtDeltaMsMax = other.DtDeltaMsMax;
    NumSamples = other.NumSamples;
    PerfStressAmount = other.PerfStressAmount;
    GitBranchName = other.GitBranchName;
    BuildType = other.BuildType;
    Cpu = other.Cpu;
    OS = other.OS;
    Compiler = other.Compiler;
    Date = other.Date;
    //DateMax = ...
    VsBaseline = other.VsBaseline;
    LabelIndex = other.LabelIndex;
}

//-------------------------------------------------------------------------
// [SECTION] Types & everything else
//-------------------------------------------------------------------------

typedef ImGuiID(*HashEntryFn)(ImGuiPerfToolEntry* entry);
typedef void(*FormatEntryLabelFn)(ImGuiPerfTool* perftool, Str* result, ImGuiPerfToolEntry* entry);

struct ImGuiPerfToolColumnInfo
{
    const char*     Title;
    int             Offset;
    ImGuiDataType   Type;
    bool            ShowAlways;
    ImGuiTableFlags Flags;

    template<typename T>
    T GetValue(const ImGuiPerfToolEntry* entry) const { return *(T*)((const char*)entry + Offset); }
};

// Update _ShowEntriesTable() and SaveHtmlReport() when adding new entries.
static const ImGuiPerfToolColumnInfo PerfToolColumnInfo[] =
{
    { /* 00 */ "Date",        offsetof(ImGuiPerfToolEntry, Timestamp),        ImGuiDataType_U64,    true,  ImGuiTableColumnFlags_DefaultHide },
    { /* 01 */ "Test Name",   offsetof(ImGuiPerfToolEntry, TestName),         ImGuiDataType_COUNT,  true,  0 },
    { /* 02 */ "Branch",      offsetof(ImGuiPerfToolEntry, GitBranchName),    ImGuiDataType_COUNT,  true,  0 },
    { /* 03 */ "Compiler",    offsetof(ImGuiPerfToolEntry, Compiler),         ImGuiDataType_COUNT,  true,  0 },
    { /* 04 */ "OS",          offsetof(ImGuiPerfToolEntry, OS),               ImGuiDataType_COUNT,  true,  0 },
    { /* 05 */ "CPU",         offsetof(ImGuiPerfToolEntry, Cpu),              ImGuiDataType_COUNT,  true,  0 },
    { /* 06 */ "Build",       offsetof(ImGuiPerfToolEntry, BuildType),        ImGuiDataType_COUNT,  true,  0 },
    { /* 07 */ "Stress",      offsetof(ImGuiPerfToolEntry, PerfStressAmount), ImGuiDataType_S32,    true,  0 },
    { /* 08 */ "Avg ms",      offsetof(ImGuiPerfToolEntry, DtDeltaMs),        ImGuiDataType_Double, true,  0 },
    { /* 09 */ "Min ms",      offsetof(ImGuiPerfToolEntry, DtDeltaMsMin),     ImGuiDataType_Double, false, 0 },
    { /* 00 */ "Max ms",      offsetof(ImGuiPerfToolEntry, DtDeltaMsMax),     ImGuiDataType_Double, false, 0 },
    { /* 11 */ "Samples",     offsetof(ImGuiPerfToolEntry, NumSamples),       ImGuiDataType_S32,    false, 0 },
    { /* 12 */ "VS Baseline", offsetof(ImGuiPerfToolEntry, VsBaseline),       ImGuiDataType_Float,  true,  0 },
};

static const char* PerfToolReportDefaultOutputPath = "./output/capture_perf_report.html";

// This is declared as a standalone function in order to run without a PerfTool instance
void ImGuiTestEngine_PerfToolAppendToCSV(ImGuiPerfTool* perf_log, ImGuiPerfToolEntry* entry, const char* filename)
{
    if (filename == nullptr)
        filename = IMGUI_PERFLOG_DEFAULT_FILENAME;

    if (!ImFileCreateDirectoryChain(filename, ImPathFindFilename(filename)))
    {
        fprintf(stderr, "Unable to create missing directory '%*s', perftool entry was not saved.\n", (int)(ImPathFindFilename(filename) - filename), filename);
        return;
    }

    // Appends to .csv
    FILE* f = fopen(filename, "a+b");
    if (f == nullptr)
    {
        fprintf(stderr, "Unable to open '%s', perftool entry was not saved.\n", filename);
        return;
    }
    fprintf(f, "%llu,%s,%s,%.3f,x%d,%s,%s,%s,%s,%s,%s\n", entry->Timestamp, entry->Category, entry->TestName,
            entry->DtDeltaMs, entry->PerfStressAmount, entry->GitBranchName, entry->BuildType, entry->Cpu, entry->OS,
            entry->Compiler, entry->Date);
    fflush(f);
    fclose(f);

    // Register to runtime perf tool if any
    if (perf_log != nullptr)
        perf_log->AddEntry(entry);
}

// Tri-state button. Copied and modified ButtonEx().
static bool Button3(const char* label, int* value)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
    float dot_radius2 = g.FontSize;
    ImVec2 btn_size(dot_radius2 * 2, dot_radius2);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(ImVec2(), btn_size.x + label_size.x + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(ImRect(pos, pos + style.FramePadding + btn_size), id, &hovered, &held, 0);

    // Render
    const ImU32 col = ImGui::GetColorU32(ImGuiCol_FrameBg);
#if IMGUI_VERSION_NUM >= 19136
    ImGui::RenderNavCursor(bb, id);
#else
    ImGui::RenderNavHighlight(bb, id);
#endif
    ImGui::RenderFrame(bb.Min + style.FramePadding, bb.Min + style.FramePadding + btn_size, col, true, /*style.FrameRounding*/ 5.0f);

    ImColor btn_col;
    if (held)
        btn_col = style.Colors[ImGuiCol_SliderGrabActive];
    else if (hovered)
        btn_col = style.Colors[ImGuiCol_ButtonHovered];
    else
        btn_col = style.Colors[ImGuiCol_SliderGrab];
    ImVec2 center = bb.Min + ImVec2(dot_radius2 + (dot_radius2 * (float)*value), dot_radius2) * 0.5f + style.FramePadding;
    window->DrawList->AddCircleFilled(center, dot_radius2 * 0.5f, btn_col);

    ImRect text_bb;
    text_bb.Min = bb.Min + style.FramePadding + ImVec2(btn_size.x + style.ItemInnerSpacing.x, 0);
    text_bb.Max = text_bb.Min + label_size;
    ImGui::RenderTextClipped(text_bb.Min, text_bb.Max, label, nullptr, &label_size, style.ButtonTextAlign, &bb);

    *value = (*value + pressed) % 3;
    return pressed;
}

static ImGuiID GetBuildID(const ImGuiPerfToolEntry* entry)
{
    IM_ASSERT(entry != nullptr);
    ImGuiID build_id = ImHashStr(entry->BuildType);
    build_id = ImHashStr(entry->OS, 0, build_id);
    build_id = ImHashStr(entry->Cpu, 0, build_id);
    build_id = ImHashStr(entry->Compiler, 0, build_id);
    build_id = ImHashStr(entry->GitBranchName, 0, build_id);
    return build_id;
}

static ImGuiID GetBuildID(const ImGuiPerfToolBatch* batch)
{
    IM_ASSERT(batch != nullptr);
    IM_ASSERT(!batch->Entries.empty());
    return GetBuildID(&batch->Entries.Data[0]);
}

// Batch ID depends on display type. It is either a build ID (when combinding by build type) or batch timestamp otherwise.
static ImGuiID GetBatchID(const ImGuiPerfTool* perftool, const ImGuiPerfToolEntry* entry)
{
    IM_ASSERT(perftool != nullptr);
    IM_ASSERT(entry != nullptr);
    if (perftool->_DisplayType == ImGuiPerfToolDisplayType_CombineByBuildInfo)
        return GetBuildID(entry);
    else
        return (ImU32)entry->Timestamp;
}

static int PerfToolComparerStr(const void* a, const void* b)
{
    return strcmp(*(const char**)b, *(const char**)a);
}

static int IMGUI_CDECL PerfToolComparerByEntryInfo(const void* lhs, const void* rhs)
{
    const ImGuiPerfToolEntry* a = (const ImGuiPerfToolEntry*)lhs;
    const ImGuiPerfToolEntry* b = (const ImGuiPerfToolEntry*)rhs;

    // While build ID does include git branch it wont ensure branches are grouped together, therefore we do branch
    // sorting manually.
    int result = strcmp(a->GitBranchName, b->GitBranchName);

    // Now that we have groups of branches - sort individual builds within those groups.
    if (result == 0)
        result = ImClamp<int>((int)((ImS64)GetBuildID(a) - (ImS64)GetBuildID(b)), -1, +1);

    // Group individual runs together within build groups.
    if (result == 0)
        result = (int)ImClamp<ImS64>((ImS64)b->Timestamp - (ImS64)a->Timestamp, -1, +1);

    // And finally sort individual runs by perf name so we can have a predictable order (used to optimize in _Rebuild()).
    if (result == 0)
        result = (int)strcmp(a->TestName, b->TestName);

    return result;
}

static ImGuiPerfTool* PerfToolInstance = nullptr;
static int IMGUI_CDECL CompareWithSortSpecs(const void* lhs, const void* rhs)
{
    IM_ASSERT(PerfToolInstance != nullptr);
    ImGuiPerfTool* tool = PerfToolInstance;
    const ImGuiTableSortSpecs* sort_specs = PerfToolInstance->_InfoTableSortSpecs;
    int batch_index_a, entry_index_a, mono_index_a, batch_index_b, entry_index_b, mono_index_b;
    tool->_UnpackSortedKey(*(ImU64*)lhs, &batch_index_a, &entry_index_a, &mono_index_a);
    tool->_UnpackSortedKey(*(ImU64*)rhs, &batch_index_b, &entry_index_b, &mono_index_b);
    for (int i = 0; i < sort_specs->SpecsCount; i++)
    {
        const ImGuiTableColumnSortSpecs* specs = &sort_specs->Specs[i];
        const ImGuiPerfToolColumnInfo& col_info = PerfToolColumnInfo[specs->ColumnIndex];
        const ImGuiPerfToolBatch* batch_a = &tool->_Batches[batch_index_a];
        const ImGuiPerfToolBatch* batch_b = &tool->_Batches[batch_index_b];
        ImGuiPerfToolEntry* a = &batch_a->Entries.Data[entry_index_a];
        ImGuiPerfToolEntry* b = &batch_b->Entries.Data[entry_index_b];
        if (specs->SortDirection == ImGuiSortDirection_Ascending)
            ImSwap(a, b);

        int result = 0;
        switch (col_info.Type)
        {
        case ImGuiDataType_S32:
            result = col_info.GetValue<int>(a) - col_info.GetValue<int>(b);
            break;
        case ImGuiDataType_U64:
            result = (int)(col_info.GetValue<ImU64>(a) - col_info.GetValue<ImU64>(b));
            break;
        case ImGuiDataType_Float:
            result = (int)((col_info.GetValue<float>(a) - col_info.GetValue<float>(b)) * 1000.0f);
            break;
        case ImGuiDataType_Double:
            result = (int)((col_info.GetValue<double>(a) - col_info.GetValue<double>(b)) * 1000.0);
            break;
        case ImGuiDataType_COUNT:
            result = strcmp(col_info.GetValue<const char*>(a), col_info.GetValue<const char*>(b));
            break;
        default:
            IM_ASSERT(false);
        }
        if (result != 0)
            return result;
    }
    return mono_index_a - mono_index_b;
}

// Dates are in format "YYYY-MM-DD"
static bool IsDateValid(const char* date)
{
    if (date[4] != '-' || date[7] != '-')
        return false;
    for (int i = 0; i < 10; i++)
    {
        if (i == 4 || i == 7)
            continue;
        if (date[i] < '0' || date[i] > '9')
            return false;
    }
    return true;
}

static float FormatVsBaseline(ImGuiPerfToolEntry* entry, ImGuiPerfToolEntry* baseline_entry, Str& out_label)
{
    if (baseline_entry == nullptr)
    {
        out_label.appendf("--");
        return FLT_MAX;
    }

    if (entry == baseline_entry)
    {
        out_label.append("baseline");
        return FLT_MAX;
    }

    double percent_vs_first = 100.0 / baseline_entry->DtDeltaMs * entry->DtDeltaMs;
    double dt_change = -(100.0 - percent_vs_first);
    if (dt_change == INFINITY)
        out_label.appendf("--");
    else if (ImAbs(dt_change) > 0.001f)
        out_label.appendf("%+.2lf%% (%s)", dt_change, dt_change < 0.0f ? "faster" : "slower");
    else
        out_label.appendf("==");
    return (float)dt_change;
}

#if IMGUI_TEST_ENGINE_ENABLE_IMPLOT
static void PerfToolFormatBuildInfo(ImGuiPerfTool* perftool, Str* result, ImGuiPerfToolBatch* batch)
{
    IM_ASSERT(perftool != nullptr);
    IM_ASSERT(result != nullptr);
    IM_ASSERT(batch != nullptr);
    IM_ASSERT(batch->Entries.Size > 0);
    ImGuiPerfToolEntry* entry = &batch->Entries.Data[0];
    Str64f legend_format("x%%-%dd %%-%ds %%-%ds %%-%ds %%-%ds %%-%ds %%s%%s%%s%%s(%%-%dd sample%%s)%%s",
        perftool->_AlignStress, perftool->_AlignType, perftool->_AlignCpu, perftool->_AlignOs, perftool->_AlignCompiler,
        perftool->_AlignBranch, perftool->_AlignSamples);
    result->appendf(legend_format.c_str(), entry->PerfStressAmount, entry->BuildType, entry->Cpu, entry->OS,
        entry->Compiler, entry->GitBranchName, entry->Date,
#if 0
        // Show min-max dates.
        perftool->_CombineByBuildInfo ? " - " : "",
        entry->DateMax ? entry->DateMax : "",
#else
        "", "",
#endif
        *entry->Date ? " " : "",
        batch->NumSamples,
        batch->NumSamples > 1 ? "s" : "",                               // Singular/plural form of "sample(s)"
        batch->NumSamples > 1 || perftool->_AlignSamples == 1 ? "" : " " // Space after legend entry to separate * marking baseline
    );
}
#endif

static int PerfToolCountBuilds(ImGuiPerfTool* perftool, bool only_visible)
{
    int num_builds = 0;
    ImU64 build_id = 0;
    for (ImGuiPerfToolEntry& entry : perftool->_SrcData)
    {
        if (build_id != GetBuildID(&entry))
        {
            if (!only_visible || perftool->_IsVisibleBuild(&entry))
                num_builds++;
            build_id = GetBuildID(&entry);
        }
    }
    return num_builds;
}

static bool InputDate(const char* label, char* date, int date_len, bool valid)
{
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("YYYY-MM-DD").x + ImGui::GetStyle().FramePadding.x * 2.0f);
    const bool date_valid = date[0] == 0 || (IsDateValid(date) && valid);
    if (!date_valid)
    {
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 0, 0, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    }
    bool date_changed = ImGui::InputTextWithHint(label, "YYYY-MM-DD", date, date_len);
    if (!date_valid)
    {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
    return date_changed;
}

static void FormatDate(ImU64 microseconds, char* buf, size_t buf_size)
{
    time_t timestamp = (time_t)(microseconds / 1000000);
    tm* time = localtime(&timestamp);
    ImFormatString(buf, buf_size, "%04d-%02d-%02d", time->tm_year + 1900, time->tm_mon + 1, time->tm_mday);
}

static void FormatDateAndTime(ImU64 microseconds, char* buf, size_t buf_size)
{
    time_t timestamp = (time_t)(microseconds / 1000000);
    tm* time = localtime(&timestamp);
    ImFormatString(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d", time->tm_year + 1900, time->tm_mon + 1, time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
}

static void RenderFilterInput(ImGuiPerfTool* perf, const char* hint, float width = -FLT_MIN)
{
    if (ImGui::IsWindowAppearing())
        strcpy(perf->_Filter, "");
    ImGui::SetNextItemWidth(width);
    ImGui::InputTextWithHint("##filter", hint, perf->_Filter, IM_ARRAYSIZE(perf->_Filter));
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
}

static bool RenderMultiSelectFilter(ImGuiPerfTool* perf, const char* filter_hint, ImVector<const char*>* labels)
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStorage& visibility = perf->_Visibility;
    bool modified = false;
    RenderFilterInput(perf, filter_hint, -(ImGui::CalcTextSize("(?)").x + g.Style.ItemSpacing.x));
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Hold CTRL to invert other items.\nHold SHIFT to close popup instantly.");

    // Keep popup open for multiple actions if SHIFT is pressed.
#if IMGUI_VERSION_NUM >= 19094
    if (!io.KeyShift)
        ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
#else
    if (!io.KeyShift)
        ImGui::PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);
#endif

    if (ImGui::MenuItem("Show All"))
    {
        for (const char* label : *labels)
            if (strstr(label, perf->_Filter) != nullptr)
                visibility.SetBool(ImHashStr(label), true);
        modified = true;
    }

    if (ImGui::MenuItem("Hide All"))
    {
        for (const char* label : *labels)
            if (strstr(label, perf->_Filter) != nullptr)
                visibility.SetBool(ImHashStr(label), false);
        modified = true;
    }

    // Render perf labels in reversed order. Labels are sorted, but stored in reversed order to render them on the plot
    // from top down (ImPlot renders stuff from bottom up).
    int filtered_entries = 0;
    for (int i = labels->Size - 1; i >= 0; i--)
    {
        const char* label = (*labels)[i];
        if (strstr(label, perf->_Filter) == nullptr)   // Filter out entries not matching a filter query
            continue;

        if (filtered_entries == 0)
            ImGui::Separator();

        ImGuiID build_id = ImHashStr(label);
        bool visible = visibility.GetBool(build_id, true);
        if (ImGui::MenuItem(label, nullptr, &visible))
        {
            modified = true;
            if (io.KeyCtrl)
            {
                for (const char* label2 : *labels)
                {
                    ImGuiID build_id2 = ImHashStr(label2);
                    visibility.SetBool(build_id2, !visibility.GetBool(build_id2, true));
                }
            }
            else
            {
                visibility.SetBool(build_id, !visibility.GetBool(build_id, true));
            }
        }
        filtered_entries++;
    }

    if (!io.KeyShift)
        ImGui::PopItemFlag();

    return modified;
}

// Based on ImPlot::SetupFinish().
#if IMGUI_TEST_ENGINE_ENABLE_IMPLOT
static ImRect ImPlotGetYTickRect(int t, int y = 0)
{
    ImPlotContext& gp = *GImPlot;
    ImPlotPlot& plot = *gp.CurrentPlot;
    ImPlotAxis& ax = plot.YAxis(y);
    const ImPlotTicker& tkc = ax.Ticker;
    const bool opp = ax.IsOpposite();
    ImRect result(1.0f, 1.0f, -1.0f, -1.0f);
    if (ax.HasTickLabels())
    {
        const ImPlotTick& tk = tkc.Ticks[t];
        const float datum = ax.Datum1 + (opp ? gp.Style.LabelPadding.x : (-gp.Style.LabelPadding.x - tk.LabelSize.x));
        if (tk.ShowLabel && tk.PixelPos >= plot.PlotRect.Min.y - 1 && tk.PixelPos <= plot.PlotRect.Max.y + 1)
        {
            ImVec2 start(datum, tk.PixelPos - 0.5f * tk.LabelSize.y);
            result.Min = start;
            result.Max = start + tk.LabelSize;
        }
    }
    return result;
}
#endif // #if IMGUI_TEST_ENGINE_ENABLE_IMPLOT

ImGuiPerfTool::ImGuiPerfTool()
{
    _CsvParser = IM_NEW(ImGuiCsvParser)();
    Clear();
}

ImGuiPerfTool::~ImGuiPerfTool()
{
    _SrcData.clear_destruct();
    _Batches.clear_destruct();
    IM_DELETE(_CsvParser);
}

void ImGuiPerfTool::AddEntry(ImGuiPerfToolEntry* entry)
{
    if (strcmp(_FilterDateFrom, entry->Date) > 0)
        ImStrncpy(_FilterDateFrom, entry->Date, IM_ARRAYSIZE(_FilterDateFrom));
    if (strcmp(_FilterDateTo, entry->Date) < 0)
        ImStrncpy(_FilterDateTo, entry->Date, IM_ARRAYSIZE(_FilterDateTo));

    _SrcData.push_back(*entry);
    _Batches.clear_destruct();
}

void ImGuiPerfTool::_Rebuild()
{
    if (_SrcData.empty())
        return;

    ImGuiStorage& temp_set = _TempSet;
    _Labels.resize(0);
    _LabelsVisible.resize(0);
    _InfoTableSort.resize(0);
    _Batches.clear_destruct();
    _InfoTableSortDirty = true;

    // Gather all visible labels. Legend batches will store data in this order.
    temp_set.Data.resize(0);    // name_id:IsLabelSeen
    for (ImGuiPerfToolEntry& entry : _SrcData)
    {
        ImGuiID name_id = ImHashStr(entry.TestName);
        if (!temp_set.GetBool(name_id))
        {
            temp_set.SetBool(name_id, true);
            _Labels.push_back(entry.TestName);
            if (_IsVisibleTest(entry.TestName))
                _LabelsVisible.push_front(entry.TestName);
        }
    }
    int num_visible_labels = _LabelsVisible.Size;

    // Labels are sorted in reverse order so they appear to be oredered from top down.
    ImQsort(_Labels.Data, _Labels.Size, sizeof(const char*), &PerfToolComparerStr);
    ImQsort(_LabelsVisible.Data, num_visible_labels, sizeof(const char*), &PerfToolComparerStr);

    // _SrcData vector stores sorted raw entries of imgui_perflog.csv. Sorting is very important,
    // algorithm depends on data being correctly sorted. Sorting _SrcData is OK, because it is only
    // ever appended to and never written out to disk. Entries are sorted by multiple criteria,
    // in specified order:
    // 1. By branch name
    // 2. By build ID
    // 3. By run timestamp
    // 4. By test name
    // This results in a neatly partitioned dataset where similar data is grouped together and where perf test order
    // is consistent in all batches. Sorting by build ID _before_ timestamp is also important as we will be aggregating
    // entries by build ID instead of timestamp, when appropriate display mode is enabled.
    ImQsort(_SrcData.Data, _SrcData.Size, sizeof(ImGuiPerfToolEntry), &PerfToolComparerByEntryInfo);

    // Sort groups of entries into batches.
    const bool combine_by_build_info = _DisplayType == ImGuiPerfToolDisplayType_CombineByBuildInfo;
    _LabelBarCounts.Data.resize(0);

    // Process all batches. `entry` is always a first batch element (guaranteed by _SrcData being sorted by timestamp).
    // At the end of this loop we fast-forward until next batch (first entry having different batch id (which is a
    // timestamp or build info)).
    for (ImGuiPerfToolEntry* entry = _SrcData.begin(); entry < _SrcData.end();)
    {
        // Filtered out entries can be safely ignored. Note that entry++ does not follow logic of fast-forwarding to the
        // next batch, as found at the end of this loop. This is OK, because all entries belonging to a same batch will
        // also have same date.
        if ((_FilterDateFrom[0] && strcmp(entry->Date, _FilterDateFrom) < 0) || (_FilterDateTo[0] && strcmp(entry->Date, _FilterDateTo) > 0))
        {
            entry++;
            continue;
        }

        _Batches.push_back(ImGuiPerfToolBatch());
        ImGuiPerfToolBatch& batch = _Batches.back();
        batch.BatchID = GetBatchID(this, entry);
        batch.Entries.resize(num_visible_labels);

        // Fill in defaults. Done once before data aggregation loop, because same entry may be touched multiple times in
        // the following loop when entries are being combined by build info.
        for (int i = 0; i < num_visible_labels; i++)
        {
            ImGuiPerfToolEntry* e = &batch.Entries.Data[i];
            *e = *entry;
            e->DtDeltaMs = 0;
            e->NumSamples = 0;
            e->LabelIndex = i;
            e->TestName = _LabelsVisible.Data[i];
        }

        // Find perf test runs for this particular batch and accumulate them.
        for (int i = 0; i < num_visible_labels; i++)
        {
            // This inner loop walks all entries that belong to current batch. Due to sorting we are sure that batch
            // always starts with `entry`, and all entries that belong to a batch (whether we combine by build info or not)
            // will be grouped in _SrcData.
            ImGuiPerfToolEntry* aggregate = &batch.Entries.Data[i];
            for (ImGuiPerfToolEntry* e = entry; e < _SrcData.end() && GetBatchID(this, e) == batch.BatchID; e++)
            {
                if (strcmp(e->TestName, aggregate->TestName) != 0)
                    continue;
                aggregate->DtDeltaMs += e->DtDeltaMs;
                aggregate->NumSamples++;
                aggregate->DtDeltaMsMin = ImMin(aggregate->DtDeltaMsMin, e->DtDeltaMs);
                aggregate->DtDeltaMsMax = ImMax(aggregate->DtDeltaMsMax, e->DtDeltaMs);
            }
        }

        // In case data is combined by build info, DtDeltaMs will be a sum of all combined entries. Average it out.
        if (combine_by_build_info)
            for (int i = 0; i < num_visible_labels; i++)
            {
                ImGuiPerfToolEntry* aggregate = &batch.Entries.Data[i];
                if (aggregate->NumSamples > 0)
                    aggregate->DtDeltaMs /= aggregate->NumSamples;
            }

        // Advance to the next batch.
        batch.NumSamples = 1;
        if (combine_by_build_info)
        {
            ImU64 last_timestamp = entry->Timestamp;
            for (ImGuiID build_id = GetBuildID(entry); entry < _SrcData.end() && build_id == GetBuildID(entry);)
            {
                // Also count how many unique batches participate in this aggregated batch.
                if (entry->Timestamp != last_timestamp)
                {
                    batch.NumSamples++;
                    last_timestamp = entry->Timestamp;
                }
                entry++;
            }
        }
        else
        {
            for (ImU64 timestamp = entry->Timestamp; entry < _SrcData.end() && timestamp == entry->Timestamp;)
                entry++;
        }
    }

    // Create man entries for every batch.
    // Pushed after sorting so they are always at the start of the chart.
    const char* mean_labels[] = { "harmonic mean", "arithmetic mean", "geometric mean" };
    int num_visible_mean_labels = 0;
    for (const char* label : mean_labels)
    {
        _Labels.push_back(label);
        if (_IsVisibleTest(label))
        {
           _LabelsVisible.push_back(label);
           num_visible_mean_labels++;
        }
    }
    for (ImGuiPerfToolBatch& batch : _Batches)
    {
        double delta_sum = 0.0;
        double delta_prd = 1.0;
        double delta_rec = 0.0;
        for (int i = 0; i < batch.Entries.Size; i++)
        {
            ImGuiPerfToolEntry* entry = &batch.Entries.Data[i];
            delta_sum += entry->DtDeltaMs;
            delta_prd *= entry->DtDeltaMs;
            delta_rec += 1 / entry->DtDeltaMs;
        }

        int visible_label_i = 0;
        for (int i = 0; i < IM_ARRAYSIZE(mean_labels); i++)
        {
            if (!_IsVisibleTest(mean_labels[i]))
                continue;

            batch.Entries.push_back(ImGuiPerfToolEntry());
            ImGuiPerfToolEntry* mean_entry = &batch.Entries.back();
            *mean_entry = batch.Entries.Data[0];
            mean_entry->LabelIndex = _LabelsVisible.Size - num_visible_mean_labels + visible_label_i;
            mean_entry->TestName = _LabelsVisible.Data[mean_entry->LabelIndex];
            mean_entry->GitBranchName = "";
            mean_entry->BuildType = "";
            mean_entry->Compiler = "";
            mean_entry->OS = "";
            mean_entry->Cpu = "";
            mean_entry->Date = "";
            visible_label_i++;
            if (i == 0)
                mean_entry->DtDeltaMs = num_visible_labels / delta_rec;
            else if (i == 1)
                mean_entry->DtDeltaMs = delta_sum / num_visible_labels;
            else if (i == 2)
                mean_entry->DtDeltaMs = pow(delta_prd, 1.0 / num_visible_labels);
            else
                IM_ASSERT(0);
        }
        IM_ASSERT(batch.Entries.Size == _LabelsVisible.Size);
    }

    // Find number of bars (batches) each label will render.
    for (ImGuiPerfToolBatch& batch : _Batches)
    {
        if (!_IsVisibleBuild(&batch))
            continue;

        for (ImGuiPerfToolEntry& entry : batch.Entries)
        {
            ImGuiID label_id = ImHashStr(entry.TestName);
            int num_bars = _LabelBarCounts.GetInt(label_id) + 1;
            _LabelBarCounts.SetInt(label_id, num_bars);
        }
    }

    // Index branches, used for per-branch colors.
    temp_set.Data.resize(0);    // ImHashStr(branch_name):linear_index
    int branch_index_last = 0;
    _BaselineBatchIndex = -1;
    for (ImGuiPerfToolBatch& batch : _Batches)
    {
        if (batch.Entries.empty())
            continue;
        ImGuiPerfToolEntry* entry = &batch.Entries.Data[0];
        ImGuiID branch_hash = ImHashStr(entry->GitBranchName);
        batch.BranchIndex = temp_set.GetInt(branch_hash, -1);
        if (batch.BranchIndex < 0)
        {
            batch.BranchIndex = branch_index_last++;
            temp_set.SetInt(branch_hash, batch.BranchIndex);
        }

        if (_BaselineBatchIndex < 0)
            if ((combine_by_build_info && GetBuildID(entry) == _BaselineBuildId) || _BaselineTimestamp == entry->Timestamp)
                _BaselineBatchIndex = _Batches.index_from_ptr(&batch);
    }

    // When per-branch colors are enabled we aggregate sample counts and set them to all batches with identical build info.
    temp_set.Data.resize(0);    // build_id:TotalSamples
    if (_DisplayType == ImGuiPerfToolDisplayType_PerBranchColors)
    {
        // Aggregate totals to temp_set.
        for (ImGuiPerfToolBatch& batch : _Batches)
        {
            ImGuiID build_id = GetBuildID(&batch);
            temp_set.SetInt(build_id, temp_set.GetInt(build_id, 0) + batch.NumSamples);
        }

        // Fill in batch sample counts.
        for (ImGuiPerfToolBatch& batch : _Batches)
        {
            ImGuiID build_id = GetBuildID(&batch);
            batch.NumSamples = temp_set.GetInt(build_id, 1);
        }
    }

    _NumVisibleBuilds = PerfToolCountBuilds(this, true);
    _NumUniqueBuilds = PerfToolCountBuilds(this, false);

    _CalculateLegendAlignment();
    temp_set.Data.resize(0);
}

void ImGuiPerfTool::Clear()
{
    _Labels.clear();
    _LabelsVisible.clear();
    _Batches.clear_destruct();
    _Visibility.Clear();
    _SrcData.clear_destruct();
    _CsvParser->Clear();

    ImStrncpy(_FilterDateFrom, "9999-99-99", IM_ARRAYSIZE(_FilterDateFrom));
    ImStrncpy(_FilterDateTo, "0000-00-00", IM_ARRAYSIZE(_FilterDateFrom));
}

bool ImGuiPerfTool::LoadCSV(const char* filename)
{
    if (filename == nullptr)
        filename = IMGUI_PERFLOG_DEFAULT_FILENAME;

    Clear();

    ImGuiCsvParser* parser = _CsvParser;
    parser->Columns = 11;
    if (!parser->Load(filename))
        return false;

    // Read perf test entries from CSV
    for (int row = 0; row < parser->Rows; row++)
    {
        ImGuiPerfToolEntry entry;
        int col = 0;
        sscanf(parser->GetCell(row, col++), "%llu", &entry.Timestamp);
        entry.Category = parser->GetCell(row, col++);
        entry.TestName = parser->GetCell(row, col++);
        sscanf(parser->GetCell(row, col++), "%lf", &entry.DtDeltaMs);
        sscanf(parser->GetCell(row, col++), "x%d", &entry.PerfStressAmount);
        entry.GitBranchName = parser->GetCell(row, col++);
        entry.BuildType = parser->GetCell(row, col++);
        entry.Cpu = parser->GetCell(row, col++);
        entry.OS = parser->GetCell(row, col++);
        entry.Compiler = parser->GetCell(row, col++);
        entry.Date = parser->GetCell(row, col++);
        AddEntry(&entry);
    }

    return true;
}

void ImGuiPerfTool::ViewOnly(const char** perf_names)
{
    // Data would not be built if we tried to view perftool of a particular test without first opening perftool via button. We need data to be built to hide perf tests.
    if (_Batches.empty())
        _Rebuild();

    // Hide other perf tests.
    for (const char* label : _Labels)
    {
        bool visible = false;
        for (const char** p_name = perf_names; !visible && *p_name; p_name++)
            visible |= strcmp(label, *p_name) == 0;
        _Visibility.SetBool(ImHashStr(label), visible);
    }
}

void ImGuiPerfTool::ViewOnly(const char* perf_name)
{
    const char* names[] = { perf_name, nullptr };
    ViewOnly(names);
}

ImGuiPerfToolEntry* ImGuiPerfTool::GetEntryByBatchIdx(int idx, const char* perf_name)
{
    if (idx < 0)
        return nullptr;
    IM_ASSERT(idx < _Batches.Size);
    ImGuiPerfToolBatch& batch = _Batches.Data[idx];
    for (int i = 0; i < batch.Entries.Size; i++)
        if (ImGuiPerfToolEntry* entry = &batch.Entries.Data[i])
            if (strcmp(entry->TestName, perf_name) == 0)
                return entry;
    return nullptr;
}

bool ImGuiPerfTool::_IsVisibleBuild(ImGuiPerfToolBatch* batch)
{
    IM_ASSERT(batch != nullptr);
    if (batch->Entries.empty())
        return false;   // All entries are hidden.
    return _IsVisibleBuild(&batch->Entries.Data[0]);
}

bool ImGuiPerfTool::_IsVisibleBuild(ImGuiPerfToolEntry* entry)
{
    return _Visibility.GetBool(ImHashStr(entry->GitBranchName), true) &&
        _Visibility.GetBool(ImHashStr(entry->Compiler), true) &&
        _Visibility.GetBool(ImHashStr(entry->Cpu), true) &&
        _Visibility.GetBool(ImHashStr(entry->OS), true) &&
        _Visibility.GetBool(ImHashStr(entry->BuildType), true);
}

bool ImGuiPerfTool::_IsVisibleTest(const char* test_name)
{
    return _Visibility.GetBool(ImHashStr(test_name), true);
}

void ImGuiPerfTool::_CalculateLegendAlignment()
{
    // Estimate paddings for legend format so it looks nice and aligned
    // FIXME: Rely on font being monospace. May need to recalculate every frame on a per-need basis based on font?
    _AlignStress = _AlignType = _AlignCpu = _AlignOs = _AlignCompiler = _AlignBranch = _AlignSamples = 0;
    for (ImGuiPerfToolBatch& batch : _Batches)
    {
        if (batch.Entries.empty())
            continue;
        ImGuiPerfToolEntry* entry = &batch.Entries.Data[0];
        if (!_IsVisibleBuild(entry))
            continue;
        _AlignStress = ImMax(_AlignStress, (int)ceil(log10(entry->PerfStressAmount)));
        _AlignType = ImMax(_AlignType, (int)strlen(entry->BuildType));
        _AlignCpu = ImMax(_AlignCpu, (int)strlen(entry->Cpu));
        _AlignOs = ImMax(_AlignOs, (int)strlen(entry->OS));
        _AlignCompiler = ImMax(_AlignCompiler, (int)strlen(entry->Compiler));
        _AlignBranch = ImMax(_AlignBranch, (int)strlen(entry->GitBranchName));
        _AlignSamples = ImMax(_AlignSamples, (int)Str16f("%d", entry->NumSamples).length());
    }
}

bool ImGuiPerfTool::SaveHtmlReport(const char* file_name, const char* image_file)
{
    if (!ImFileCreateDirectoryChain(file_name, ImPathFindFilename(file_name)))
        return false;

    FILE* fp = fopen(file_name, "w+");
    if (fp == nullptr)
        return false;

    fprintf(fp, "<!doctype html>\n"
                "<html>\n"
                "<head>\n"
                "  <meta charset=\"utf-8\"/>\n"
                "  <title>Dear ImGui perf report</title>\n"
                "</head>\n"
                "<body>\n"
                "  <pre id=\"content\">\n");

    // Embed performance chart.
    fprintf(fp, "## Dear ImGui perf report\n\n");

    if (image_file != nullptr)
    {
        FILE* fp_img = fopen(image_file, "rb");
        if (fp_img != nullptr)
        {
            ImVector<char> image_buffer;
            ImVector<char> base64_buffer;
            fseek(fp_img, 0, SEEK_END);
            image_buffer.resize((int)ftell(fp_img));
            base64_buffer.resize(((image_buffer.Size / 3) + 1) * 4 + 1);
            rewind(fp_img);
            fread(image_buffer.Data, 1, image_buffer.Size, fp_img);
            fclose(fp_img);
            int len = ImStrBase64Encode((unsigned char*)image_buffer.Data, base64_buffer.Data, image_buffer.Size);
            base64_buffer.Data[len] = 0;
            fprintf(fp, "![](data:image/png;base64,%s)\n\n", base64_buffer.Data);
        }
    }

    // Print info table.
    const bool combine_by_build_info = _DisplayType == ImGuiPerfToolDisplayType_CombineByBuildInfo;
    for (const auto& column_info : PerfToolColumnInfo)
        if (column_info.ShowAlways || combine_by_build_info)
            fprintf(fp, "| %s ", column_info.Title);
    fprintf(fp, "|\n");
    for (const auto& column_info : PerfToolColumnInfo)
        if (column_info.ShowAlways || combine_by_build_info)
            fprintf(fp, "| -- ");
    fprintf(fp, "|\n");

    for (int row_index = _InfoTableSort.Size - 1; row_index >= 0; row_index--)
    {
        int batch_index_sorted, entry_index_sorted;
        _UnpackSortedKey(_InfoTableSort[row_index], &batch_index_sorted, &entry_index_sorted);
        ImGuiPerfToolBatch* batch = &_Batches[batch_index_sorted];
        ImGuiPerfToolEntry* entry = &batch->Entries[entry_index_sorted];
        const char* test_name = entry->TestName;
        if (!_IsVisibleBuild(entry) || entry->NumSamples == 0)
            continue;

        ImGuiPerfToolEntry* baseline_entry = GetEntryByBatchIdx(_BaselineBatchIndex, test_name);
        for (int i = 0; i < IM_ARRAYSIZE(PerfToolColumnInfo); i++)
        {
            Str30f label("");
            const ImGuiPerfToolColumnInfo& column_info = PerfToolColumnInfo[i];
            if (column_info.ShowAlways || combine_by_build_info)
            {
                switch (i)
                {
                case 0:
                {
                    char date[64];
                    FormatDateAndTime(entry->Timestamp, date, IM_ARRAYSIZE(date));
                    fprintf(fp, "| %s ", date);
                    break;
                }
                case 1:  fprintf(fp, "| %s ", entry->TestName);             break;
                case 2:  fprintf(fp, "| %s ", entry->GitBranchName);        break;
                case 3:  fprintf(fp, "| %s ", entry->Compiler);             break;
                case 4:  fprintf(fp, "| %s ", entry->OS);                   break;
                case 5:  fprintf(fp, "| %s ", entry->Cpu);                  break;
                case 6:  fprintf(fp, "| %s ", entry->BuildType);            break;
                case 7:  fprintf(fp, "| x%d ", entry->PerfStressAmount);    break;
                case 8:  fprintf(fp, "| %.2f ", entry->DtDeltaMs);          break;
                case 9:  fprintf(fp, "| %.2f ", entry->DtDeltaMsMin);       break;
                case 10: fprintf(fp, "| %.2f ", entry->DtDeltaMsMax);       break;
                case 11: fprintf(fp, "| %d ", entry->NumSamples);           break;
                case 12: FormatVsBaseline(entry, baseline_entry, label); fprintf(fp, "| %s ", label.c_str()); break;
                default: IM_ASSERT(0); break;
                }
            }
        }
        fprintf(fp, "|\n");
    }

    fprintf(fp, "</pre>\n"
                "  <script src=\"https://cdn.jsdelivr.net/npm/marked@4.0.0/marked.min.js\"></script>\n"
                "  <script>\n"
                "    var content = document.getElementById('content');\n"
                "    content.innerHTML = marked.parse(content.innerText);\n"
                "  </script>\n"
                "</body>\n"
                "</html>\n");

    fclose(fp);
    return true;
}

void ImGuiPerfTool::_SetBaseline(int batch_index)
{
    IM_ASSERT(batch_index < _Batches.Size);
    _BaselineBatchIndex = batch_index;
    if (batch_index >= 0)
    {
        _BaselineTimestamp = _Batches.Data[batch_index].Entries.Data[0].Timestamp;
        _BaselineBuildId = GetBuildID(&_Batches.Data[batch_index]);
    }
}

//-------------------------------------------------------------------------
// [SECTION] USER INTERFACE
//-------------------------------------------------------------------------

void ImGuiPerfTool::ShowPerfToolWindow(ImGuiTestEngine* engine, bool* p_open)
{
    if (!ImGui::Begin("Dear ImGui Perf Tool", p_open))
    {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowAppearing() && Empty())
        LoadCSV();

    // -----------------------------------------------------------------------------------------------------------------
    // Render utility buttons
    // -----------------------------------------------------------------------------------------------------------------

    // Date filter
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Date Range:");
    ImGui::SameLine();

    bool dirty = _Batches.empty();
    bool date_changed = InputDate("##date-from", _FilterDateFrom, IM_ARRAYSIZE(_FilterDateFrom),
                                  (strcmp(_FilterDateFrom, _FilterDateTo) <= 0 || !*_FilterDateTo));
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup("InputDate From Menu");
    ImGui::SameLine(0, 0.0f);
    ImGui::TextUnformatted("..");
    ImGui::SameLine(0, 0.0f);
    date_changed |= InputDate("##date-to", _FilterDateTo, IM_ARRAYSIZE(_FilterDateTo),
                              (strcmp(_FilterDateFrom, _FilterDateTo) <= 0 || !*_FilterDateFrom));
    if (date_changed)
    {
        dirty = (!_FilterDateFrom[0] || IsDateValid(_FilterDateFrom)) && (!_FilterDateTo[0] || IsDateValid(_FilterDateTo));
        if (_FilterDateFrom[0] && _FilterDateTo[0])
            dirty &= strcmp(_FilterDateFrom, _FilterDateTo) <= 0;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup("InputDate To Menu");
    ImGui::SameLine();

    for (int i = 0; i < 2; i++)
    {
        if (ImGui::BeginPopup(i == 0 ? "InputDate From Menu" : "InputDate To Menu"))
        {
            char* date = i == 0 ? _FilterDateFrom : _FilterDateTo;
            int date_size = i == 0 ? IM_ARRAYSIZE(_FilterDateFrom) : IM_ARRAYSIZE(_FilterDateTo);
            if (i == 0 && ImGui::MenuItem("Set Min"))
            {
                for (ImGuiPerfToolEntry& entry : _SrcData)
                    if (strcmp(date, entry.Date) > 0)
                    {
                        ImStrncpy(date, entry.Date, date_size);
                        dirty = true;
                    }
            }
            if (ImGui::MenuItem("Set Max"))
            {
                for (ImGuiPerfToolEntry& entry : _SrcData)
                    if (strcmp(date, entry.Date) < 0)
                    {
                        ImStrncpy(date, entry.Date, date_size);
                        dirty = true;
                    }
            }
            if (ImGui::MenuItem("Set Today"))
            {
                time_t now = time(nullptr);
                FormatDate((ImU64)now * 1000000, date, date_size);
                dirty = true;
            }
            ImGui::EndPopup();
        }
    }

    if (ImGui::Button(Str64f("Filter builds (%d/%d)###Filter builds", _NumVisibleBuilds, _NumUniqueBuilds).c_str()))
        ImGui::OpenPopup("Filter builds");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Hide or show individual builds.");
    ImGui::SameLine();
    if (ImGui::Button(Str64f("Filter tests (%d/%d)###Filter tests", _LabelsVisible.Size, _Labels.Size).c_str()))
        ImGui::OpenPopup("Filter perfs");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Hide or show individual tests.");
    ImGui::SameLine();

    dirty |= Button3("Combine", (int*)&_DisplayType);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::RadioButton("Display each run separately", _DisplayType == ImGuiPerfToolDisplayType_Simple);
        ImGui::RadioButton("Use one color per branch. Disables baseline comparisons!", _DisplayType == ImGuiPerfToolDisplayType_PerBranchColors);
        ImGui::RadioButton("Combine multiple runs with same build info into one averaged build entry.", _DisplayType == ImGuiPerfToolDisplayType_CombineByBuildInfo);
        ImGui::EndTooltip();
    }

    ImGui::SameLine();
    if (_ReportGenerating && ImGuiTestEngine_IsTestQueueEmpty(engine))
    {
        _ReportGenerating = false;
        ImOsOpenInShell(PerfToolReportDefaultOutputPath);
    }
    if (_Batches.empty())
        ImGui::BeginDisabled();
    if (ImGui::Button("Html Export"))
    {
        // In order to capture a screenshot Report is saved by executing a "capture_perf_report" test.
        _ReportGenerating = true;
        ImGuiTestEngine_QueueTests(engine, ImGuiTestGroup_Tests, "capture_perf_report");
    }
    if (_Batches.empty())
        ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Generate a report and open it in the browser.");

    // Align help button to the right.
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(0.0f, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("(?)").x));
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::BulletText("To change baseline build, double-click desired build in the legend.");
        ImGui::BulletText("Extra information is displayed when hovering bars of a particular perf test and holding SHIFT.");
        ImGui::BulletText("Double-click plot to fit plot into available area.");
        ImGui::EndTooltip();
    }

    if (ImGui::BeginPopup("Filter builds"))
    {
        ImGuiStorage& temp_set = _TempSet;
        temp_set.Data.resize(0);    // ImHashStr(BuildProperty):seen

        static const char* columns[] = { "Branch", "Build", "CPU", "OS", "Compiler" };
        bool show_all = ImGui::Button("Show All");
        ImGui::SameLine();
        bool hide_all = ImGui::Button("Hide All");
        if (ImGui::BeginTable("Builds", IM_ARRAYSIZE(columns), ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit))
        {
            for (int i = 0; i < IM_ARRAYSIZE(columns); i++)
                ImGui::TableSetupColumn(columns[i]);
            ImGui::TableHeadersRow();

            // Find columns with nothing checked.
            bool checked_any[] = { false, false, false, false, false };
            for (ImGuiPerfToolEntry& entry : _SrcData)
            {
                const char* properties[] = { entry.GitBranchName, entry.BuildType, entry.Cpu, entry.OS, entry.Compiler };
                for (int i = 0; i < IM_ARRAYSIZE(properties); i++)
                {
                    ImGuiID hash = ImHashStr(properties[i]);
                    checked_any[i] |= _Visibility.GetBool(hash, true);
                }
            }

            int property_offsets[] =
            {
                offsetof(ImGuiPerfToolEntry, GitBranchName),
                offsetof(ImGuiPerfToolEntry, BuildType),
                offsetof(ImGuiPerfToolEntry, Cpu),
                offsetof(ImGuiPerfToolEntry, OS),
                offsetof(ImGuiPerfToolEntry, Compiler),
            };

            ImGui::TableNextRow();
            for (int i = 0; i < IM_ARRAYSIZE(property_offsets); i++)
            {
                ImGui::TableSetColumnIndex(i);
                for (ImGuiPerfToolEntry& entry : _SrcData)
                {
                    const char* property = *(const char**)((const char*)&entry + property_offsets[i]);
                    ImGuiID hash = ImHashStr(property);
                    if (temp_set.GetBool(hash))
                        continue;
                    temp_set.SetBool(hash, true);

                    bool visible = _Visibility.GetBool(hash, true) || show_all;
                    if (hide_all)
                        visible = false;
                    bool modified = ImGui::Checkbox(property, &visible) || show_all || hide_all;
                    _Visibility.SetBool(hash, visible);
                    if (modified)
                    {
                        _CalculateLegendAlignment();
                        _NumVisibleBuilds = PerfToolCountBuilds(this, true);
                        dirty = true;
                    }
                    if (!checked_any[i])
                    {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImColor(1.0f, 0.0f, 0.0f, 0.2f));
                        if (ImGui::TableGetColumnFlags() & ImGuiTableColumnFlags_IsHovered)
                            ImGui::SetTooltip("Check at least one item in each column to see any data.");
                    }
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("Filter perfs"))
    {
        dirty |= RenderMultiSelectFilter(this, "Filter by perf test", &_Labels);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (dirty)
        _Rebuild();

    // Rendering a plot of empty dataset is not possible.
    if (_Batches.empty() || _LabelsVisible.Size == 0 || _NumVisibleBuilds == 0)
    {
        ImGui::TextUnformatted("No data is available. Run some perf tests or adjust filter settings.");
    }
    else
    {
#if IMGUI_TEST_ENGINE_ENABLE_IMPLOT
        // Splitter between two following child windows is rendered first.
        ImGuiStyle& style = ImGui::GetStyle();
        float plot_height = 0.0f;
        float& table_height = _InfoTableHeight;
        ImGui::Splitter("splitter", &plot_height, &table_height, ImGuiAxis_Y, +1);

        // Double-click to move splitter to bottom
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            table_height = 0;
            plot_height = ImGui::GetContentRegionAvail().y - style.ItemSpacing.y;
            ImGui::ClearActiveID();
        }

        // Render entries plot
        if (ImGui::BeginChild(ImGui::GetID("plot"), ImVec2(0, plot_height)))
            _ShowEntriesPlot();
        ImGui::EndChild();

        // Render entries tables
        if (table_height > 0.0f)
        {
            if (ImGui::BeginChild(ImGui::GetID("info-table"), ImVec2(0, table_height)))
                _ShowEntriesTable();
            ImGui::EndChild();
        }
#else
        _ShowEntriesTable();
#endif
    }
    ImGui::End();
}

#if IMGUI_TEST_ENGINE_ENABLE_IMPLOT
static double GetLabelVerticalOffset(double occupy_h, int max_visible_builds, int now_visible_builds)
{
    const double h = occupy_h / (float)max_visible_builds;
    double offset = -h * ((max_visible_builds - 1) * 0.5);
    return (double)now_visible_builds * h + offset;
}
#endif

void ImGuiPerfTool::_ShowEntriesPlot()
{
#if IMGUI_TEST_ENGINE_ENABLE_IMPLOT
    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    Str256 label;
    Str256 display_label;

    ImPlot::PushStyleColor(ImPlotCol_AxisBgHovered, IM_COL32(0, 0, 0, 0));
    ImPlot::PushStyleColor(ImPlotCol_AxisBgActive, IM_COL32(0, 0, 0, 0));
    if (!ImPlot::BeginPlot("PerfTool", ImVec2(-1, -1), ImPlotFlags_NoTitle))
        return;

    ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
    if (_LabelsVisible.Size > 1)
    {
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, _LabelsVisible.Size, _LabelsVisible.Size, _LabelsVisible.Data);
    }
    else if (_LabelsVisible.Size == 1)
    {
        const char* labels[] = { _LabelsVisible[0], "" };
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, _LabelsVisible.Size, 2, labels);
    }
    ImPlot::SetupLegend(ImPlotLocation_NorthEast);

    // Amount of vertical space bars of one label will occupy. 1.0 would leave no space between bars of adjacent labels.
    const float occupy_h = 0.8f;

    // Plot bars
    bool legend_hovered = false;
    ImGuiStorage& temp_set = _TempSet;
    temp_set.Data.resize(0);    // ImHashStr(TestName):now_visible_builds_i
    int current_baseline_batch_index = _BaselineBatchIndex; // Cache this value before loop, so toggling it does not create flicker.
    for (int batch_index = 0; batch_index < _Batches.Size; batch_index++)
    {
        ImGuiPerfToolBatch& batch = _Batches[batch_index];
        if (!_IsVisibleBuild(&batch.Entries.Data[0]))
            continue;

        // Plot bars.
        label.clear();
        display_label.clear();
        PerfToolFormatBuildInfo(this, &label, &batch);
        display_label.append(label.c_str());
        ImGuiID batch_label_id;
        bool baseline_match = false;
        if (_DisplayType == ImGuiPerfToolDisplayType_PerBranchColors)
        {
            // No "vs baseline" comparison for per-branch colors, because runs are combined in the legend, but not in the info table.
            batch_label_id = GetBuildID(&batch);
        }
        else
        {
            batch_label_id = ImHashData(&batch.BatchID, sizeof(batch.BatchID));
            baseline_match = current_baseline_batch_index == batch_index;
        }
        display_label.appendf("%s###%08X", baseline_match ? " *" : "", batch_label_id);

        // Plot all bars one by one, so batches with varying number of bars would not contain empty holes.
        for (ImGuiPerfToolEntry& entry : batch.Entries)
        {
            if (entry.NumSamples == 0)
                continue;   // Dummy entry, perf did not run for this test in this batch.
            ImGuiID label_id = ImHashStr(entry.TestName);
            const int max_visible_builds = _LabelBarCounts.GetInt(label_id);
            const int now_visible_builds = temp_set.GetInt(label_id);
            temp_set.SetInt(label_id, now_visible_builds + 1);
            double y_pos = (double)entry.LabelIndex + GetLabelVerticalOffset(occupy_h, max_visible_builds, now_visible_builds);
            ImPlot::SetNextFillStyle(ImPlot::GetColormapColor(_DisplayType == ImGuiPerfToolDisplayType_PerBranchColors ? batch.BranchIndex : batch_index));
            ImPlot::PlotBars<double>(display_label.c_str(), &entry.DtDeltaMs, &y_pos, 1, occupy_h / (double)max_visible_builds, ImPlotBarsFlags_Horizontal);
        }
        legend_hovered |= ImPlot::IsLegendEntryHovered(display_label.c_str());

        // Set baseline.
        if (ImPlot::IsLegendEntryHovered(display_label.c_str()))
        {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                _SetBaseline(batch_index);
        }
    }

    // Plot highlights.
    ImPlotContext& gp = *GImPlot;
    ImPlotPlot& plot = *gp.CurrentPlot;
    _PlotHoverTest = -1;
    _PlotHoverBatch = -1;
    _PlotHoverTestLabel = false;
    bool can_highlight = !legend_hovered && (ImPlot::IsPlotHovered() || ImPlot::IsAxisHovered(ImAxis_Y1));
    ImDrawList* plot_draw_list = ImPlot::GetPlotDrawList();

    // Highlight bars when hovering a label.
    int hovered_label_index = -1;
    for (int i = 0; i < _LabelsVisible.Size && can_highlight; i++)
    {
        ImRect label_rect_loose = ImPlotGetYTickRect(i);                // Rect around test label
        ImRect label_rect_tight;                                        // Rect around test label, covering bar height and label area width
        label_rect_tight.Min.y = ImPlot::PlotToPixels(0, (float)i + 0.5f).y;
        label_rect_tight.Max.y = ImPlot::PlotToPixels(0, (float)i - 0.5f).y;
        label_rect_tight.Min.x = plot.CanvasRect.Min.x;
        label_rect_tight.Max.x = plot.PlotRect.Min.x;

        ImRect rect_bars;                                               // Rect around bars only
        rect_bars.Min.x = plot.PlotRect.Min.x;
        rect_bars.Max.x = plot.PlotRect.Max.x;
        rect_bars.Min.y = ImPlot::PlotToPixels(0, (float)i + 0.5f).y;
        rect_bars.Max.y = ImPlot::PlotToPixels(0, (float)i - 0.5f).y;

        // Render underline signaling it is clickable. Clicks are handled when rendering info table.
        if (label_rect_loose.Contains(io.MousePos))
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            plot_draw_list->AddLine(ImFloor(label_rect_loose.GetBL()), ImFloor(label_rect_loose.GetBR()),
                ImColor(style.Colors[ImGuiCol_Text]));
        }

        // Highlight bars belonging to hovered label.
        if (label_rect_tight.Contains(io.MousePos))
        {
            plot_draw_list->AddRectFilled(rect_bars.Min, rect_bars.Max, ImColor(style.Colors[ImGuiCol_TextSelectedBg]));
            _PlotHoverTestLabel = true;
            _PlotHoverTest = i;
        }

        if (rect_bars.Contains(io.MousePos))
            hovered_label_index = i;
    }

    // Highlight individual bars when hovering them on the plot or info table.
    temp_set.Data.resize(0);    // ImHashStr(hovered_label):now_visible_builds_i
    if (hovered_label_index < 0)
        hovered_label_index = _TableHoveredTest;
    if (hovered_label_index >= 0)
    {
        const char* hovered_label = _LabelsVisible.Data[hovered_label_index];
        ImGuiID label_id = ImHashStr(hovered_label);
        for (ImGuiPerfToolBatch& batch : _Batches)
        {
            int batch_index = _Batches.index_from_ptr(&batch);
            if (!_IsVisibleBuild(&batch))
                continue;

            ImGuiPerfToolEntry* entry = &batch.Entries.Data[hovered_label_index];
            if (entry->NumSamples == 0)
                continue;   // Dummy entry, perf did not run for this test in this batch.

            int max_visible_builds = _LabelBarCounts.GetInt(label_id);
            const int now_visible_builds = temp_set.GetInt(label_id);
            temp_set.SetInt(label_id, now_visible_builds + 1);
            float h = occupy_h / (float)max_visible_builds;
            float y_pos = (float)entry->LabelIndex;
            y_pos += (float)GetLabelVerticalOffset(occupy_h, max_visible_builds, now_visible_builds);
            ImRect rect_bar;                                                    // Rect around hovered bar only
            rect_bar.Min.x = plot.PlotRect.Min.x;
            rect_bar.Max.x = plot.PlotRect.Max.x;
            rect_bar.Min.y = ImPlot::PlotToPixels(0, y_pos - h * 0.5f + h).y;   // ImPlot y_pos is for bar center, therefore we adjust positions by half-height to get a bounding box.
            rect_bar.Max.y = ImPlot::PlotToPixels(0, y_pos - h * 0.5f).y;

            // Mouse is hovering label or bars of a perf test - highlight them in info table.
            if (_PlotHoverTest < 0 && rect_bar.Min.y <= io.MousePos.y && io.MousePos.y < rect_bar.Max.y && io.MousePos.x > plot.PlotRect.Min.x)
            {
                // _LabelsVisible is inverted to make perf test order match info table order. Revert it back.
                _PlotHoverTest = hovered_label_index;
                _PlotHoverBatch = batch_index;
                plot_draw_list->AddRectFilled(rect_bar.Min, rect_bar.Max, ImColor(style.Colors[ImGuiCol_TextSelectedBg]));
            }

            // Mouse is hovering a row in info table - highlight relevant bars on the plot.
            if (_TableHoveredBatch == batch_index && _TableHoveredTest == hovered_label_index)
                plot_draw_list->AddRectFilled(rect_bar.Min, rect_bar.Max, ImColor(style.Colors[ImGuiCol_TextSelectedBg]));
        }
    }

    if (io.KeyShift && _PlotHoverTest >= 0)
    {
        // Info tooltip with delta times of each batch for a hovered test.
        const char* test_name = _LabelsVisible.Data[_PlotHoverTest];
        ImGui::BeginTooltip();
        float w = ImGui::CalcTextSize(test_name).x;
        float total_w = ImGui::GetContentRegionAvail().x;
        if (total_w > w)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (total_w - w) * 0.5f);
        ImGui::TextUnformatted(test_name);

        for (int i = 0; i < _Batches.Size; i++)
        {
            if (ImGuiPerfToolEntry* hovered_entry = GetEntryByBatchIdx(i, test_name))
                ImGui::Text("%s %.3fms", label.c_str(), hovered_entry->DtDeltaMs);
            else
                ImGui::Text("%s --", label.c_str());
        }
        ImGui::EndTooltip();
    }

    ImPlot::EndPlot();
    ImPlot::PopStyleColor(2);
#else
    ImGui::TextUnformatted("Not enabled because ImPlot is not available (IMGUI_TEST_ENGINE_ENABLE_IMPLOT=0).");
#endif
}

void ImGuiPerfTool::_ShowEntriesTable()
{
    ImGuiTableFlags table_flags = ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_SortMulti | ImGuiTableFlags_SortTristate | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("PerfInfo", IM_ARRAYSIZE(PerfToolColumnInfo), table_flags))
        return;

    ImGuiStyle& style = ImGui::GetStyle();
    int num_visible_labels = _LabelsVisible.Size;

    // Test name column is not sorted because we do sorting only within perf runs of a particular tests,
    // so as far as sorting function is concerned all items in first column are identical.
    for (int i = 0; i < IM_ARRAYSIZE(PerfToolColumnInfo); i++)
    {
        const ImGuiPerfToolColumnInfo& info = PerfToolColumnInfo[i];
        ImGuiTableColumnFlags column_flags = info.Flags;
        if (i == 0 && _DisplayType != ImGuiPerfToolDisplayType_Simple)
            column_flags |= ImGuiTableColumnFlags_Disabled; // Date only visible in non-combining mode.
        if (!info.ShowAlways && _DisplayType != ImGuiPerfToolDisplayType_CombineByBuildInfo)
            column_flags |= ImGuiTableColumnFlags_Disabled;
        ImGui::TableSetupColumn(info.Title, column_flags);
    }
    ImGui::TableSetupScrollFreeze(0, 1);

    if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
        if (sorts_specs->SpecsDirty || _InfoTableSortDirty)
        {
            // Fill sort table with unsorted indices.
            sorts_specs->SpecsDirty = _InfoTableSortDirty = false;

            // Reinitialize sorting table to unsorted state.
            _InfoTableSort.resize(num_visible_labels * _Batches.Size);
            for (int entry_index = 0, i = 0; entry_index < num_visible_labels; entry_index++)
                for (int batch_index = 0; batch_index < _Batches.Size; batch_index++, i++)
                    _InfoTableSort.Data[i] = (((ImU64)batch_index * num_visible_labels + entry_index) << 24) | i;

            // Sort batches of each label.
            if (sorts_specs->SpecsCount > 0)
            {
                _InfoTableSortSpecs = sorts_specs;
                PerfToolInstance = this;
                ImQsort(_InfoTableSort.Data, (size_t)_InfoTableSort.Size, sizeof(_InfoTableSort.Data[0]), CompareWithSortSpecs);
                _InfoTableSortSpecs = nullptr;
                PerfToolInstance = nullptr;
            }
        }

    ImGui::TableHeadersRow();

    // ImPlot renders bars from bottom to the top. We want bars to render from top to the bottom, therefore we loop
    // labels and batches in reverse order.
    _TableHoveredTest = -1;
    _TableHoveredBatch = -1;
    const bool scroll_into_view = _PlotHoverTestLabel && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    const float header_row_height = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 0).GetHeight();
    ImRect scroll_into_view_rect(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (int row_index = _InfoTableSort.Size - 1; row_index >= 0; row_index--)
    {
        int batch_index_sorted, entry_index_sorted;
        _UnpackSortedKey(_InfoTableSort[row_index], &batch_index_sorted, &entry_index_sorted);
        ImGuiPerfToolBatch* batch = &_Batches[batch_index_sorted];
        ImGuiPerfToolEntry* entry = &batch->Entries[entry_index_sorted];
        const char* test_name = entry->TestName;

        if (!_IsVisibleBuild(entry) || !_IsVisibleTest(entry->TestName) || entry->NumSamples == 0)
            continue;

        ImGui::PushID(entry);
        ImGui::TableNextRow();
        if (row_index & 1)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt, 0.5f));
        else
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableRowBg, 0.5f));

        if (_PlotHoverTest == entry_index_sorted)
        {
            // Highlight a row that corresponds to hovered bar, or all rows that correspond to hovered perf test label.
            if (_PlotHoverBatch == batch_index_sorted || _PlotHoverTestLabel)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImColor(style.Colors[ImGuiCol_TextSelectedBg]));
        }

        ImGuiPerfToolEntry* baseline_entry = GetEntryByBatchIdx(_BaselineBatchIndex, test_name);

        // Date
        if (ImGui::TableNextColumn())
        {
            char date[64];
            FormatDateAndTime(entry->Timestamp, date, IM_ARRAYSIZE(date));
            ImGui::TextUnformatted(date);
        }

        // Build info
        if (ImGui::TableNextColumn())
        {
            // ImGuiSelectableFlags_Disabled + changing ImGuiCol_TextDisabled color prevents selectable from overriding table highlight behavior.
            ImGui::PushStyleColor(ImGuiCol_Header, style.Colors[ImGuiCol_Text]);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.Colors[ImGuiCol_TextSelectedBg]);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.Colors[ImGuiCol_TextSelectedBg]);
            ImGui::Selectable(entry->TestName, false, ImGuiSelectableFlags_SpanAllColumns);
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered())
            {
                _TableHoveredTest = entry_index_sorted;
                _TableHoveredBatch = batch_index_sorted;
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (entry == baseline_entry)
                    ImGui::BeginDisabled();
                if (ImGui::MenuItem("Set as baseline"))
                    _SetBaseline(batch_index_sorted);
                if (entry == baseline_entry)
                    ImGui::EndDisabled();
                ImGui::EndPopup();
            }
        }
        if (ImGui::TableNextColumn())
            ImGui::TextUnformatted(entry->GitBranchName);
        if (ImGui::TableNextColumn())
            ImGui::TextUnformatted(entry->Compiler);
        if (ImGui::TableNextColumn())
            ImGui::TextUnformatted(entry->OS);
        if (ImGui::TableNextColumn())
            ImGui::TextUnformatted(entry->Cpu);
        if (ImGui::TableNextColumn())
            ImGui::TextUnformatted(entry->BuildType);
        if (ImGui::TableNextColumn())
            ImGui::Text("x%d", entry->PerfStressAmount);

        // Avg ms
        if (ImGui::TableNextColumn())
            ImGui::Text("%.3lf", entry->DtDeltaMs);

        // Min ms
        if (ImGui::TableNextColumn())
            ImGui::Text("%.3lf", entry->DtDeltaMsMin);

        // Max ms
        if (ImGui::TableNextColumn())
            ImGui::Text("%.3lf", entry->DtDeltaMsMax);

        // Num samples
        if (ImGui::TableNextColumn())
            ImGui::Text("%d", entry->NumSamples);

        // VS Baseline
        if (ImGui::TableNextColumn())
        {
            float dt_change = (float)entry->VsBaseline;
            if (_DisplayType == ImGuiPerfToolDisplayType_PerBranchColors)
            {
                ImGui::TextUnformatted("--");
            }
            else
            {
                Str30 label;
                dt_change = FormatVsBaseline(entry, baseline_entry, label);
                ImGui::TextUnformatted(label.c_str());
                if (dt_change != entry->VsBaseline)
                {
                    entry->VsBaseline = dt_change;
                    _InfoTableSortDirty = true;             // Force re-sorting.
                }
            }
        }

        if (_PlotHoverTest == entry_index_sorted && scroll_into_view)
        {
            ImGuiTable* table = ImGui::GetCurrentTable();
            scroll_into_view_rect.Add(ImGui::TableGetCellBgRect(table, 0));
        }

        ImGui::PopID();
    }

    if (scroll_into_view)
    {
        scroll_into_view_rect.Min.y -= header_row_height;   // FIXME-TABLE: Compensate for frozen header row covering a first content row scrolled into view.
        ImGui::ScrollToRect(ImGui::GetCurrentWindow(), scroll_into_view_rect, ImGuiScrollFlags_NoScrollParent);
    }

    ImGui::EndTable();
}

//-------------------------------------------------------------------------
// [SECTION] SETTINGS
//-------------------------------------------------------------------------

static void PerflogSettingsHandler_ClearAll(ImGuiContext*, ImGuiSettingsHandler* ini_handler)
{
    ImGuiPerfTool* perftool = (ImGuiPerfTool*)ini_handler->UserData;
    perftool->_Visibility.Clear();
}

static void* PerflogSettingsHandler_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char*)
{
    return (void*)1;
}

static void PerflogSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler* ini_handler, void*, const char* line)
{
    ImGuiPerfTool* perftool = (ImGuiPerfTool*)ini_handler->UserData;
    char buf[128];
    int visible = -1, display_type = -1;
    /**/ if (sscanf(line, "DateFrom=%10s", perftool->_FilterDateFrom)) {}
    else if (sscanf(line, "DateTo=%10s", perftool->_FilterDateTo)) {}
    else if (sscanf(line, "DisplayType=%d", &display_type)) { perftool->_DisplayType = (ImGuiPerfToolDisplayType)display_type; }
    else if (sscanf(line, "BaselineBuildId=%llu", &perftool->_BaselineBuildId)) {}
    else if (sscanf(line, "BaselineTimestamp=%llu", &perftool->_BaselineTimestamp)) {}
    else if (sscanf(line, "TestVisibility=%[^,],%d", buf, &visible) == 2) { perftool->_Visibility.SetBool(ImHashStr(buf), !!visible); }
    else if (sscanf(line, "BuildVisibility=%[^,],%d", buf, &visible) == 2) { perftool->_Visibility.SetBool(ImHashStr(buf), !!visible); }
}

static void PerflogSettingsHandler_ApplyAll(ImGuiContext*, ImGuiSettingsHandler* ini_handler)
{
    ImGuiPerfTool* perftool = (ImGuiPerfTool*)ini_handler->UserData;
    perftool->_Batches.clear_destruct();
    perftool->_SetBaseline(-1);
}

static void PerflogSettingsHandler_WriteAll(ImGuiContext*, ImGuiSettingsHandler* ini_handler, ImGuiTextBuffer* buf)
{
    ImGuiPerfTool* perftool = (ImGuiPerfTool*)ini_handler->UserData;
    if (perftool->_Batches.empty())
        return;
    buf->appendf("[%s][Data]\n", ini_handler->TypeName);
    buf->appendf("DateFrom=%s\n", perftool->_FilterDateFrom);
    buf->appendf("DateTo=%s\n", perftool->_FilterDateTo);
    buf->appendf("DisplayType=%d\n", perftool->_DisplayType);
    buf->appendf("BaselineBuildId=%llu\n", perftool->_BaselineBuildId);
    buf->appendf("BaselineTimestamp=%llu\n", perftool->_BaselineTimestamp);
    for (const char* label : perftool->_Labels)
        buf->appendf("TestVisibility=%s,%d\n", label, perftool->_Visibility.GetBool(ImHashStr(label), true));

    ImGuiStorage& temp_set = perftool->_TempSet;
    temp_set.Data.clear();
    for (ImGuiPerfToolEntry& entry : perftool->_SrcData)
    {
        const char* properties[] = { entry.GitBranchName, entry.BuildType, entry.Cpu, entry.OS, entry.Compiler };
        for (int i = 0; i < IM_ARRAYSIZE(properties); i++)
        {
            ImGuiID hash = ImHashStr(properties[i]);
            if (!temp_set.GetBool(hash))
            {
                temp_set.SetBool(hash, true);
                buf->appendf("BuildVisibility=%s,%d\n", properties[i], perftool->_Visibility.GetBool(hash, true));
            }
        }
    }
    buf->append("\n");
}

void ImGuiPerfTool::_AddSettingsHandler()
{
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "TestEnginePerfTool";
    ini_handler.TypeHash = ImHashStr("TestEnginePerfTool");
    ini_handler.ClearAllFn = PerflogSettingsHandler_ClearAll;
    ini_handler.ReadOpenFn = PerflogSettingsHandler_ReadOpen;
    ini_handler.ReadLineFn = PerflogSettingsHandler_ReadLine;
    ini_handler.ApplyAllFn = PerflogSettingsHandler_ApplyAll;
    ini_handler.WriteAllFn = PerflogSettingsHandler_WriteAll;
    ini_handler.UserData = this;
    ImGui::AddSettingsHandler(&ini_handler);
}

void ImGuiPerfTool::_UnpackSortedKey(ImU64 key, int* batch_index, int* entry_index, int* monotonic_index)
{
    IM_ASSERT(batch_index != nullptr);
    IM_ASSERT(entry_index != nullptr);
    const int num_visible_labels = _LabelsVisible.Size;
    *batch_index = (int)((key >> 24) / num_visible_labels);
    *entry_index = (int)((key >> 24) % num_visible_labels);
    if (monotonic_index)
        *monotonic_index = (int)(key & 0xFFFFFF);
}

//-------------------------------------------------------------------------
// [SECTION] TESTS
//-------------------------------------------------------------------------

static bool SetPerfToolWindowOpen(ImGuiTestContext* ctx, bool is_open)
{
    ctx->MenuClick("//Dear ImGui Test Engine/Tools");
    bool was_open = ctx->ItemIsChecked("//$FOCUSED/Perf Tool");
    ctx->MenuAction(is_open ? ImGuiTestAction_Check : ImGuiTestAction_Uncheck, "//Dear ImGui Test Engine/Tools/Perf Tool");
    return was_open;
}

void RegisterTests_TestEnginePerfTool(ImGuiTestEngine* e)
{
    ImGuiTest* t = nullptr;

    // ## Flex perf tool code.
    t = IM_REGISTER_TEST(e, "testengine", "testengine_cov_perftool");
    t->GuiFunc = [](ImGuiTestContext* ctx)
    {
        IM_UNUSED(ctx);
        ImGui::Begin("Test Func", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
        int loop_count = 1000;
        bool v1 = false, v2 = true;
        for (int n = 0; n < loop_count / 2; n++)
        {
            ImGui::PushID(n);
            ImGui::Checkbox("Hello, world", &v1);
            ImGui::Checkbox("Hello, world", &v2);
            ImGui::PopID();
        }
        ImGui::End();
    };
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ImGuiPerfTool* perftool = ImGuiTestEngine_GetPerfTool(ctx->Engine);
        const char* temp_perf_csv = "output/misc_cov_perf_tool.csv";

        Str16f min_date_bkp = perftool->_FilterDateFrom;
        Str16f max_date_bkp = perftool->_FilterDateTo;

        // Execute few perf tests, serialize them to temporary csv file.
        ctx->PerfIterations = 50; // Make faster
        ctx->PerfCapture("perf", "misc_cov_perf_tool_1", temp_perf_csv);
        ctx->PerfCapture("perf", "misc_cov_perf_tool_2", temp_perf_csv);

        // Load perf data from csv file and open perf tool.
        perftool->Clear();
        perftool->LoadCSV(temp_perf_csv);
        bool perf_was_open = SetPerfToolWindowOpen(ctx, true);
        ctx->Yield();

        ImGuiWindow* window = ctx->GetWindowByRef("Dear ImGui Perf Tool");
        IM_CHECK(window != nullptr);
        ImVec2 pos_bkp = window->Pos;
        ImVec2 size_bkp = window->Size;
        ctx->SetRef(window);
        ctx->WindowMove("", ImVec2(50, 50));
        ctx->WindowResize("", ImVec2(1400, 900));
#if IMGUI_TEST_ENGINE_ENABLE_IMPLOT
        ImGuiWindow* plot_child = ctx->WindowInfo("plot").Window;  // "plot/PerfTool" prior to implot 2023/08/21
        IM_CHECK(plot_child != nullptr);

        // Move legend to right side.
        ctx->MouseMoveToPos(plot_child->Rect().GetCenter());
        ctx->MouseDoubleClick(ImGuiMouseButton_Left);               // Auto-size plots while at it
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->MenuClick("//$FOCUSED/Legend/NE");

        // Click some stuff for more coverage.
        ctx->MouseMoveToPos(plot_child->Rect().GetCenter());
        ctx->KeyPress(ImGuiMod_Shift);
#endif
        ctx->ItemClick("##date-from", ImGuiMouseButton_Right);
        ctx->ItemClick(ctx->GetID("//$FOCUSED/Set Min"));
        ctx->ItemClick("##date-to", ImGuiMouseButton_Right);
        ctx->ItemClick(ctx->GetID("//$FOCUSED/Set Max"));
        ctx->ItemClick("###Filter builds");
        ctx->ItemClick("###Filter tests");
        ctx->ItemClick("Combine", 0, ImGuiTestOpFlags_MoveToEdgeL); // Toggle thrice to leave state unchanged
        ctx->ItemClick("Combine", 0, ImGuiTestOpFlags_MoveToEdgeL);
        ctx->ItemClick("Combine", 0, ImGuiTestOpFlags_MoveToEdgeL);

        // Restore original state.
        perftool->Clear();                                           // Clear test data and load original data
        ImFileDelete(temp_perf_csv);
        perftool->LoadCSV();
        ctx->Yield();
#if IMGUI_TEST_ENGINE_ENABLE_IMPLOT
        ctx->MouseMoveToPos(plot_child->Rect().GetCenter());
        ctx->MouseDoubleClick(ImGuiMouseButton_Left);               // Fit plot to original data
#endif
        ImStrncpy(perftool->_FilterDateFrom, min_date_bkp.c_str(), IM_ARRAYSIZE(perftool->_FilterDateFrom));
        ImStrncpy(perftool->_FilterDateTo, max_date_bkp.c_str(), IM_ARRAYSIZE(perftool->_FilterDateTo));
        ImGui::SetWindowPos(window, pos_bkp);
        ImGui::SetWindowSize(window, size_bkp);
        SetPerfToolWindowOpen(ctx, perf_was_open);                   // Restore window visibility
    };

    // ## Capture perf tool graph.
    t = IM_REGISTER_TEST(e, "capture", "capture_perf_report");
    t->TestFunc = [](ImGuiTestContext* ctx)
    {
        ImGuiPerfTool* perftool = ImGuiTestEngine_GetPerfTool(ctx->Engine);
        const char* perf_report_image = nullptr;
        if (!ImFileExist(IMGUI_PERFLOG_DEFAULT_FILENAME))
        {
            ctx->LogWarning("Perf tool has no data. Perf report generation was aborted.");
            return;
        }

        char min_date_bkp[sizeof(perftool->_FilterDateFrom)], max_date_bkp[sizeof(perftool->_FilterDateTo)];
        ImStrncpy(min_date_bkp, perftool->_FilterDateFrom, IM_ARRAYSIZE(min_date_bkp));
        ImStrncpy(max_date_bkp, perftool->_FilterDateTo, IM_ARRAYSIZE(max_date_bkp));
        bool perf_was_open = SetPerfToolWindowOpen(ctx, true);
        ctx->Yield();

        ImGuiWindow* window = ctx->GetWindowByRef("Dear ImGui Perf Tool");
        IM_CHECK_SILENT(window != nullptr);
        ImVec2 pos_bkp = window->Pos;
        ImVec2 size_bkp = window->Size;
        ctx->SetRef(window);
        ctx->WindowMove("", ImVec2(50, 50));
        ctx->WindowResize("", ImVec2(1400, 900));
#if IMGUI_TEST_ENGINE_ENABLE_IMPLOT
        ctx->ItemDoubleClick("splitter");   // Hide info table

        ImGuiWindow* plot_child = ctx->WindowInfo("plot").Window;  // "plot/PerfTool" prior to implot 2023/08/21
        IM_CHECK(plot_child != nullptr);

        // Move legend to right side.
        ctx->MouseMoveToPos(plot_child->Rect().GetCenter());
        ctx->MouseDoubleClick(ImGuiMouseButton_Left);               // Auto-size plots while at it
        ctx->MouseClick(ImGuiMouseButton_Right);
        ctx->MenuClick("//$FOCUSED/Legend/NE");
#endif
        // Click some stuff for more coverage.
        ctx->ItemClick("##date-from", ImGuiMouseButton_Right);
        ctx->ItemClick(ctx->GetID("//$FOCUSED/Set Min"));
        ctx->ItemClick("##date-to", ImGuiMouseButton_Right);
        ctx->ItemClick(ctx->GetID("//$FOCUSED/Set Max"));
#if IMGUI_TEST_ENGINE_ENABLE_IMPLOT
        // Take a screenshot.
        ImGuiCaptureArgs* args = ctx->CaptureArgs;
        args->InCaptureRect = plot_child->Rect();
        ctx->CaptureAddWindow(window->ID);
        ctx->CaptureScreenshot(ImGuiCaptureFlags_HideMouseCursor);
        ctx->ItemDragWithDelta("splitter", ImVec2(0, -180));        // Show info table
        perf_report_image = args->InOutputFile;
#endif
        ImStrncpy(perftool->_FilterDateFrom, min_date_bkp, IM_ARRAYSIZE(min_date_bkp));
        ImStrncpy(perftool->_FilterDateTo, max_date_bkp, IM_ARRAYSIZE(max_date_bkp));
        ImGui::SetWindowPos(window, pos_bkp);
        ImGui::SetWindowSize(window, size_bkp);
        SetPerfToolWindowOpen(ctx, perf_was_open);                   // Restore window visibility

#if !IMGUI_TEST_ENGINE_IS_GAME_CONSOLE
        const char* perf_report_output = getenv("CAPTURE_PERF_REPORT_OUTPUT");
#else
        const char* perf_report_output = nullptr;
#endif
        if (perf_report_output == nullptr)
            perf_report_output = PerfToolReportDefaultOutputPath;
        perftool->SaveHtmlReport(perf_report_output, perf_report_image);
    };
}

//-------------------------------------------------------------------------
