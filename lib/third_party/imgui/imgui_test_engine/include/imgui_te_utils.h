// dear imgui test engine
// (helpers/utilities. do NOT use this as a general purpose library)

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

#pragma once

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------

#include <math.h>   // fabsf
#include <stdint.h> // uint64_t
#include <stdio.h>  // FILE*
#include "imgui.h"  // ImGuiID, ImGuiKey
class Str;          // Str<> from thirdparty/Str/Str.h

//-----------------------------------------------------------------------------
// Hashing Helpers
//-----------------------------------------------------------------------------

ImGuiID     ImHashDecoratedPath(const char* str, const char* str_end = nullptr, ImGuiID seed = 0);
const char* ImFindNextDecoratedPartInPath(const char* str, const char* str_end = nullptr);

//-----------------------------------------------------------------------------
// File/Directory Helpers
//-----------------------------------------------------------------------------

bool        ImFileExist(const char* filename);
bool        ImFileDelete(const char* filename);
bool        ImFileCreateDirectoryChain(const char* path, const char* path_end = nullptr);
bool        ImFileFindInParents(const char* sub_path, int max_parent_count, Str* output);
bool        ImFileLoadSourceBlurb(const char* filename, int line_no_start, int line_no_end, ImGuiTextBuffer* out_buf);

//-----------------------------------------------------------------------------
// Path Helpers
//-----------------------------------------------------------------------------

// Those are strictly string manipulation functions
const char* ImPathFindFilename(const char* path, const char* path_end = nullptr);      // Return value always between path and path_end
const char* ImPathFindExtension(const char* path, const char* path_end = nullptr);     // Return value always between path and path_end
void        ImPathFixSeparatorsForCurrentOS(char* buf);

//-----------------------------------------------------------------------------
// String Helpers
//-----------------------------------------------------------------------------

void        ImStrReplace(Str* s, const char* find, const char* repl);
const char* ImStrchrRangeWithEscaping(const char* str, const char* str_end, char find_c);
void        ImStrXmlEscape(Str* s);
int         ImStrBase64Encode(const unsigned char* src, char* dst, int length);

//-----------------------------------------------------------------------------
// Parsing Helpers
//-----------------------------------------------------------------------------

void        ImParseExtractArgcArgvFromCommandLine(int* out_argc, char const*** out_argv, const char* cmd_line);
bool        ImParseFindIniSection(const char* ini_config, const char* header, ImVector<char>* result);

//-----------------------------------------------------------------------------
// Time Helpers
//-----------------------------------------------------------------------------

uint64_t    ImTimeGetInMicroseconds();
void        ImTimestampToISO8601(uint64_t timestamp, Str* out_date);

//-----------------------------------------------------------------------------
// Threading Helpers
//-----------------------------------------------------------------------------

void        ImThreadSleepInMilliseconds(int ms);
void        ImThreadSetCurrentThreadDescription(const char* description);

//-----------------------------------------------------------------------------
// Build Info helpers
//-----------------------------------------------------------------------------

// All the pointers are expect to be literals/persistent
struct ImBuildInfo
{
    const char*     Type = "";
    const char*     Cpu = "";
    const char*     OS = "";
    const char*     Compiler = "";
    char            Date[32];       // "YYYY-MM-DD"
    const char*     Time = "";
};

const ImBuildInfo*  ImBuildGetCompilationInfo();
bool                ImBuildFindGitBranchName(const char* git_repo_path, Str* branch_name);

//-----------------------------------------------------------------------------
// Operating System Helpers
//-----------------------------------------------------------------------------

enum ImOsConsoleStream
{
    ImOsConsoleStream_StandardOutput,
    ImOsConsoleStream_StandardError,
};

enum ImOsConsoleTextColor
{
    ImOsConsoleTextColor_Black,
    ImOsConsoleTextColor_White,
    ImOsConsoleTextColor_BrightWhite,
    ImOsConsoleTextColor_BrightRed,
    ImOsConsoleTextColor_BrightGreen,
    ImOsConsoleTextColor_BrightBlue,
    ImOsConsoleTextColor_BrightYellow,
};

bool        ImOsCreateProcess(const char* cmd_line);
FILE*       ImOsPOpen(const char* cmd_line, const char* mode);
void        ImOsPClose(FILE* fp);
void        ImOsOpenInShell(const char* path);
bool        ImOsIsDebuggerPresent();
void        ImOsOutputDebugString(const char* message);
void        ImOsConsoleSetTextColor(ImOsConsoleStream stream, ImOsConsoleTextColor color);

//-----------------------------------------------------------------------------
// Miscellaneous functions
//-----------------------------------------------------------------------------

// Tables functions
struct ImGuiTable;
ImGuiID     TableGetHeaderID(ImGuiTable* table, const char* column, int instance_no = 0);
ImGuiID     TableGetHeaderID(ImGuiTable* table, int column_n, int instance_no = 0);
void        TableDiscardInstanceAndSettings(ImGuiID table_id);

// DrawData functions
void        DrawDataVerifyMatchingBufferCount(ImDrawData* draw_data);

//-----------------------------------------------------------------------------
// Helper: maintain/calculate moving average
//-----------------------------------------------------------------------------

template<typename TYPE>
struct ImMovingAverage
{
    // Internal Fields
    ImVector<TYPE>  Samples;
    TYPE            Accum;
    int             Idx;
    int             FillAmount;

    // Functions
    ImMovingAverage()               { Accum = (TYPE)0; Idx = FillAmount = 0; }
    void    Init(int count)         { Samples.resize(count); memset(Samples.Data, 0, (size_t)Samples.Size * sizeof(TYPE)); Accum = (TYPE)0; Idx = FillAmount = 0; }
    void    AddSample(TYPE v)       { Accum += v - Samples[Idx]; Samples[Idx] = v; if (++Idx == Samples.Size) Idx = 0; if (FillAmount < Samples.Size) FillAmount++;  }
    TYPE    GetAverage() const      { return Accum / (TYPE)FillAmount; }
    int     GetSampleCount() const  { return Samples.Size; }
    bool    IsFull() const          { return FillAmount == Samples.Size; }
};

//-----------------------------------------------------------------------------
// Helper: Simple/dumb CSV parser
//-----------------------------------------------------------------------------

struct ImGuiCsvParser
{
    // Public fields
    int             Columns = 0;                    // Number of columns in CSV file.
    int             Rows = 0;                       // Number of rows in CSV file.

    // Internal fields
    char*           _Data = nullptr;                   // CSV file data.
    ImVector<char*> _Index;                         // CSV table: _Index[row * _Columns + col].

    // Functions
    ImGuiCsvParser(int columns = -1)                { Columns = columns; }
    ~ImGuiCsvParser()                               { Clear(); }
    bool            Load(const char* file_name);    // Open and parse a CSV file.
    void            Clear();                        // Free allocated buffers.
    const char*     GetCell(int row, int col)       { IM_ASSERT(0 <= row && row < Rows && 0 <= col && col < Columns); return _Index[row * Columns + col]; }
};

//-----------------------------------------------------------------------------
// Misc Dear ImGui extensions
//-----------------------------------------------------------------------------

#if IMGUI_VERSION_NUM < 18924
struct ImGuiTabBar;
struct ImGuiTabItem;
#endif

namespace ImGui
{

IMGUI_API void      ItemErrorFrame(ImU32 col);

#if IMGUI_VERSION_NUM < 18927
ImGuiID             TableGetInstanceID(ImGuiTable* table, int instance_no = 0);
#endif

// Str support for InputText()
IMGUI_API bool      InputText(const char* label, Str* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
IMGUI_API bool      InputTextWithHint(const char* label, const char* hint, Str* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
IMGUI_API bool      InputTextMultiline(const char* label, Str* str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);

// Splitter
IMGUI_API bool      Splitter(const char* id, float* value_1, float* value_2, int axis, int anchor = 0, float min_size_0 = -1.0f, float min_size_1 = -1.0f);

// Misc
IMGUI_API ImFont*   FindFontByPrefix(const char* name);

// Legacy version support
#if IMGUI_VERSION_NUM < 18924
IMGUI_API const char* TabBarGetTabName(ImGuiTabBar* tab_bar, ImGuiTabItem* tab);
#endif

}
