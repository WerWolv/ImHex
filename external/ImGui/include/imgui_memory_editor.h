// Mini memory editor for Dear ImGui (to embed in your game/tools)
// Get latest version at http://www.github.com/ocornut/imgui_club
//
// Right-click anywhere to access the Options menu!
// You can adjust the keyboard repeat delay/rate in ImGuiIO.
// The code assume a mono-space font for simplicity!
// If you don't use the default font, use ImGui::PushFont()/PopFont() to switch to a mono-space font before caling this.
//
// Usage:
//   // Create a window and draw memory editor inside it:
//   static MemoryEditor mem_edit_1;
//   static char data[0x10000];
//   size_t data_size = 0x10000;
//   mem_edit_1.DrawWindow("Memory Editor", data, data_size);
//
// Usage:
//   // If you already have a window, use DrawContents() instead:
//   static MemoryEditor mem_edit_2;
//   ImGui::Begin("MyWindow")
//   mem_edit_2.DrawContents(this, sizeof(*this), (size_t)this);
//   ImGui::End();
//
// Changelog:
// - v0.10: initial version
// - v0.23 (2017/08/17): added to github. fixed right-arrow triggering a byte write.
// - v0.24 (2018/06/02): changed DragInt("Rows" to use a %d data format (which is desirable since imgui 1.61).
// - v0.25 (2018/07/11): fixed wording: all occurrences of "Rows" renamed to "Columns".
// - v0.26 (2018/08/02): fixed clicking on hex region
// - v0.30 (2018/08/02): added data preview for common data types
// - v0.31 (2018/10/10): added OptUpperCaseHex option to select lower/upper casing display [@samhocevar]
// - v0.32 (2018/10/10): changed signatures to use void* instead of unsigned char*
// - v0.33 (2018/10/10): added OptShowOptions option to hide all the interactive option setting.
// - v0.34 (2019/05/07): binary preview now applies endianness setting [@nicolasnoble]
// - v0.35 (2020/01/29): using ImGuiDataType available since Dear ImGui 1.69.
// - v0.36 (2020/05/05): minor tweaks, minor refactor.
// - v0.40 (2020/10/04): fix misuse of ImGuiListClipper API, broke with Dear ImGui 1.79. made cursor position appears on left-side of edit box. option popup appears on mouse release. fix MSVC warnings where _CRT_SECURE_NO_WARNINGS wasn't working in recent versions.
// - v0.41 (2020/10/05): fix when using with keyboard/gamepad navigation enabled.
// - v0.42 (2020/10/14): fix for . character in ASCII view always being greyed out.
//
// Todo/Bugs:
// - This is generally old code, it should work but please don't use this as reference!
// - Arrows are being sent to the InputText() about to disappear which for LeftArrow makes the text cursor appear at position 1 for one frame.
// - Using InputText() is awkward and maybe overkill here, consider implementing something custom.

#pragma once

#include <stdio.h>      // sprintf, scanf
#include <stdint.h>     // uint8_t, etc.
#include <hex/helpers/utils.hpp>

#include <hex/api/event.hpp>

#include <string>

#ifdef _MSC_VER
#define _PRISizeT   "I"
#define ImSnprintf  _snprintf
#else
#define _PRISizeT   "z"
#define ImSnprintf  snprintf
#endif

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4996) // warning C4996: 'sprintf': This function or variable may be unsafe.
#endif

ImU32 ImAlphaBlendColors(ImU32 col_a, ImU32 col_b);

struct MemoryEditor
{
    enum DataFormat
    {
        DataFormat_Bin = 0,
        DataFormat_Dec = 1,
        DataFormat_Hex = 2,
        DataFormat_COUNT
    };

    struct DecodeData {
        std::string data;
        size_t advance;
        ImColor color;
    };

    // Settings
    bool            ReadOnly;                                   // = false  // disable any editing.
    int             Cols;                                       // = 16     // number of columns to display.
    bool            OptShowOptions;                             // = true   // display options button/context menu. when disabled, options will be locked unless you provide your own UI for them.
    bool            OptShowHexII;                               // = false  // display values in HexII representation instead of regular hexadecimal: hide null/zero bytes, ascii values as ".X".
    bool            OptShowAscii;                               // = true   // display ASCII representation on the right side.
    bool            OptShowAdvancedDecoding;                    // = true   // display advanced decoding data on the right side.
    bool            OptGreyOutZeroes;                           // = true   // display null/zero bytes using the TextDisabled color.
    bool            OptUpperCaseHex;                            // = true   // display hexadecimal values as "FF" instead of "ff".
    int             OptMidColsCount;                            // = 8      // set to 0 to disable extra spacing between every mid-cols.
    int             OptAddrDigitsCount;                         // = 0      // number of addr digits to display (default calculated based on maximum displayed addr).
    ImU32           HighlightColor;                             //          // background color of highlighted bytes.
    ImU8            (*ReadFn)(const ImU8* data, size_t off);    // = 0      // optional handler to read bytes.
    void            (*WriteFn)(ImU8* data, size_t off, ImU8 d); // = 0      // optional handler to write bytes.
    bool            (*HighlightFn)(const ImU8* data, size_t off, bool next);//= 0      // optional handler to return Highlight property (to support non-contiguous highlighting).
    void            (*HoverFn)(const ImU8 *data, size_t off);
    DecodeData      (*DecodeFn)(const ImU8 *data, size_t off);

    // [Internal State]
    bool            ContentsWidthChanged;
    size_t          DataPreviewAddr;
    size_t          DataPreviewAddrOld;
    size_t          DataPreviewAddrEnd;
    size_t          DataPreviewAddrEndOld;
    size_t          DataEditingAddr;
    bool            DataEditingTakeFocus;
    char            DataInputBuf[32];
    char            AddrInputBuf[32];
    size_t          GotoAddr;
    size_t          HighlightMin, HighlightMax;
    int             PreviewEndianess;
    ImGuiDataType   PreviewDataType;

    MemoryEditor()
    {
        // Settings
        ReadOnly = false;
        Cols = 16;
        OptShowOptions = true;
        OptShowHexII = false;
        OptShowAscii = true;
        OptShowAdvancedDecoding = true;
        OptGreyOutZeroes = true;
        OptUpperCaseHex = true;
        OptMidColsCount = 8;
        OptAddrDigitsCount = 0;
        HighlightColor = IM_COL32(255, 255, 255, 50);
        ReadFn = NULL;
        WriteFn = NULL;
        HighlightFn = NULL;
        HoverFn = NULL;
        DecodeFn = NULL;

        // State/Internals
        ContentsWidthChanged = false;
        DataPreviewAddr = DataEditingAddr = DataPreviewAddrEnd = (size_t)-1;
        DataPreviewAddrOld = DataPreviewAddrEndOld = (size_t)-1;
        DataEditingTakeFocus = false;
        memset(DataInputBuf, 0, sizeof(DataInputBuf));
        memset(AddrInputBuf, 0, sizeof(AddrInputBuf));
        GotoAddr = (size_t)-1;
        HighlightMin = HighlightMax = (size_t)-1;
        PreviewEndianess = 0;
        PreviewDataType = ImGuiDataType_S32;
    }

    void GotoAddrAndHighlight(size_t addr_min, size_t addr_max)
    {
        GotoAddr = addr_min;
        HighlightMin = addr_min;
        HighlightMax = addr_max;
    }

    void GotoAddrAndSelect(size_t addr_min, size_t addr_max)
    {
        GotoAddr = addr_min;
        DataPreviewAddr = addr_min;
        DataPreviewAddrEnd = addr_max;
        DataPreviewAddrOld = addr_min;
        DataPreviewAddrEndOld = addr_max;
    }

    struct Sizes
    {
        int     AddrDigitsCount;
        float   LineHeight;
        float   GlyphWidth;
        float   HexCellWidth;
        float   SpacingBetweenMidCols;
        float   PosHexStart;
        float   PosHexEnd;
        float   PosAsciiStart;
        float   PosAsciiEnd;
        float   PosDecodingStart;
        float   PosDecodingEnd;
        float   WindowWidth;

        Sizes() { memset(this, 0, sizeof(*this)); }
    };

    void CalcSizes(Sizes& s, size_t mem_size, size_t base_display_addr)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        s.AddrDigitsCount = OptAddrDigitsCount;
        if (s.AddrDigitsCount == 0)
            for (size_t n = base_display_addr + mem_size - 1; n > 0; n >>= 4)
                s.AddrDigitsCount++;
        s.LineHeight = ImGui::GetTextLineHeight();
        s.GlyphWidth = ImGui::CalcTextSize("F").x + 1;                  // We assume the font is mono-space
        s.HexCellWidth = (float)(int)(s.GlyphWidth * 2.5f);             // "FF " we include trailing space in the width to easily catch clicks everywhere
        s.SpacingBetweenMidCols = (float)(int)(s.HexCellWidth * 0.25f); // Every OptMidColsCount columns we add a bit of extra spacing
        s.PosHexStart = (s.AddrDigitsCount + 2) * s.GlyphWidth;
        s.PosHexEnd = s.PosHexStart + (s.HexCellWidth * Cols);
        s.PosAsciiStart = s.PosAsciiEnd = s.PosHexEnd;

        if (OptShowAscii && OptShowAdvancedDecoding) {
            s.PosAsciiStart = s.PosHexEnd + s.GlyphWidth * 1;
            if (OptMidColsCount > 0)
                s.PosAsciiStart += (float)((Cols + OptMidColsCount - 1) / OptMidColsCount) * s.SpacingBetweenMidCols;
            s.PosAsciiEnd = s.PosAsciiStart + Cols * s.GlyphWidth;

            s.PosDecodingStart = s.PosAsciiEnd + s.GlyphWidth * 1;
            if (OptMidColsCount > 0)
                s.PosDecodingStart += (float)((Cols + OptMidColsCount - 1) / OptMidColsCount) * s.SpacingBetweenMidCols;
            s.PosDecodingEnd = s.PosDecodingStart + Cols * s.GlyphWidth;
        } else if (OptShowAscii) {
            s.PosAsciiStart = s.PosHexEnd + s.GlyphWidth * 1;
            if (OptMidColsCount > 0)
                s.PosAsciiStart += (float)((Cols + OptMidColsCount - 1) / OptMidColsCount) * s.SpacingBetweenMidCols;
            s.PosAsciiEnd = s.PosAsciiStart + Cols * s.GlyphWidth;
        } else if (OptShowAdvancedDecoding) {
            s.PosDecodingStart = s.PosHexEnd + s.GlyphWidth * 1;
            if (OptMidColsCount > 0)
                s.PosDecodingStart += (float)((Cols + OptMidColsCount - 1) / OptMidColsCount) * s.SpacingBetweenMidCols;
            s.PosDecodingEnd = s.PosDecodingStart + Cols * s.GlyphWidth;
        }
        s.WindowWidth = s.PosAsciiEnd + style.ScrollbarSize + style.WindowPadding.x * 2 + s.GlyphWidth;
    }

    // Standalone Memory Editor window
    void DrawWindow(const char* title, bool *p_open, void* mem_data, size_t mem_size, size_t base_display_addr = 0x0000)
    {
        Sizes s;
        CalcSizes(s, mem_size, base_display_addr);

        if (ImGui::Begin(title, p_open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavInputs))
        {
            if (DataPreviewAddr != DataPreviewAddrOld || DataPreviewAddrEnd != DataPreviewAddrEndOld) {
                hex::Region selectionRegion = { std::min(DataPreviewAddr, DataPreviewAddrEnd) + base_display_addr, std::max(DataPreviewAddr, DataPreviewAddrEnd) - std::min(DataPreviewAddr, DataPreviewAddrEnd) };
                hex::EventManager::post<hex::EventRegionSelected>(selectionRegion);
            }

            DataPreviewAddrOld = DataPreviewAddr;
            DataPreviewAddrEndOld = DataPreviewAddrEnd;

            DrawContents(mem_data, mem_size, base_display_addr);
            if (ContentsWidthChanged)
            {
                CalcSizes(s, mem_size, base_display_addr);
                ImGui::SetWindowSize(ImVec2(s.WindowWidth, ImGui::GetWindowSize().y));
            }
        }
        ImGui::End();

    }

    // Memory Editor contents only
    void DrawContents(void* mem_data_void, size_t mem_size, size_t base_display_addr = 0x0000)
    {
        if (Cols < 1)
            Cols = 1;

        ImU8* mem_data = (ImU8*)mem_data_void;
        Sizes s;
        CalcSizes(s, mem_size, base_display_addr);
        ImGuiStyle& style = ImGui::GetStyle();

        // We begin into our scrolling region with the 'ImGuiWindowFlags_NoMove' in order to prevent click from moving the window.
        // This is used as a facility since our main click detection code doesn't assign an ActiveId so the click would normally be caught as a window-move.
        const float height_separator = style.ItemSpacing.y;
        float footer_height = 0;
        if (OptShowOptions)
            footer_height += height_separator + ImGui::GetFrameHeightWithSpacing() * 1;

        ImGui::BeginChild("offset", ImVec2(0, s.LineHeight), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
        ImGui::Text("%*c   ", s.AddrDigitsCount, ' ');
        for (int i = 0; i < Cols; i++) {
            float byte_pos_x = s.PosHexStart + s.HexCellWidth * i;
            if (OptMidColsCount > 0)
                byte_pos_x += (float)(i / OptMidColsCount) * s.SpacingBetweenMidCols;
            ImGui::SameLine(byte_pos_x);
            ImGui::Text("%02llX", i + (base_display_addr % Cols));
        }
        ImGui::EndChild();

        ImGui::BeginChild("##scrolling", ImVec2(0, -footer_height), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));



        // We are not really using the clipper API correctly here, because we rely on visible_start_addr/visible_end_addr for our scrolling function.
        ImGuiListClipper clipper;
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        const int line_total_count = (int)((mem_size + Cols - 1) / Cols);
        clipper.Begin(line_total_count, s.LineHeight);
        clipper.Step();
        const size_t visible_start_addr = clipper.DisplayStart * Cols;
        const size_t visible_end_addr = clipper.DisplayEnd * Cols;
        const size_t visible_count = visible_end_addr - visible_start_addr;

        bool data_next = false;

        if (DataEditingAddr >= mem_size)
            DataEditingAddr = (size_t)-1;
        if (DataPreviewAddr >= mem_size)
            DataPreviewAddr = (size_t)-1;
        if (DataPreviewAddrEnd >= mem_size)
            DataPreviewAddrEnd = (size_t)-1;

        size_t data_editing_addr_backup = DataEditingAddr;
        size_t data_preview_addr_backup = DataPreviewAddr;
        size_t data_editing_addr_next = (size_t)-1;
        size_t data_preview_addr_next = (size_t)-1;

        if (ImGui::IsWindowFocused()) {
            if (DataEditingAddr != (size_t)-1)
            {
                // Move cursor but only apply on next frame so scrolling with be synchronized (because currently we can't change the scrolling while the window is being rendered)
                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) && DataEditingAddr >= (size_t)Cols)          { data_editing_addr_next = DataEditingAddr - Cols; DataEditingTakeFocus = true; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) && DataEditingAddr < mem_size - Cols) { data_editing_addr_next = DataEditingAddr + Cols; DataEditingTakeFocus = true; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) && DataEditingAddr > 0)               { data_editing_addr_next = DataEditingAddr - 1; DataEditingTakeFocus = true; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) && DataEditingAddr < mem_size - 1)   { data_editing_addr_next = DataEditingAddr + 1; DataEditingTakeFocus = true; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp)) && DataEditingAddr > 0)                  { data_editing_addr_next = std::max(s64(0), s64(DataEditingAddr) - s64(visible_count)); DataEditingTakeFocus = true; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown)) && DataEditingAddr < mem_size - 1)     { data_editing_addr_next = std::min(s64(mem_size - 1), s64(DataEditingAddr) + s64(visible_count)); DataEditingTakeFocus = true; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)) && DataEditingAddr > 0)                    { data_editing_addr_next = 0; DataEditingTakeFocus = true; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)) && DataEditingAddr < mem_size - 1)          { data_editing_addr_next = mem_size - 1; DataEditingTakeFocus = true; }
            } else if (DataPreviewAddr != -1) {
                // Move cursor but only apply on next frame so scrolling with be synchronized (because currently we can't change the scrolling while the window is being rendered)
                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) && DataPreviewAddr >= (size_t)Cols)          { DataPreviewAddr = data_preview_addr_next = DataPreviewAddr - Cols; if (!ImGui::GetIO().KeyShift) DataPreviewAddrEnd = DataPreviewAddr; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) && DataPreviewAddr < mem_size - Cols) { DataPreviewAddr = data_preview_addr_next = DataPreviewAddr + Cols; if (!ImGui::GetIO().KeyShift) DataPreviewAddrEnd = DataPreviewAddr; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) && DataPreviewAddr > 0)               { DataPreviewAddr = data_preview_addr_next = DataPreviewAddr - 1; if (!ImGui::GetIO().KeyShift) DataPreviewAddrEnd = DataPreviewAddr; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) && DataPreviewAddr < mem_size - 1)   { DataPreviewAddr = data_preview_addr_next = DataPreviewAddr + 1; if (!ImGui::GetIO().KeyShift) DataPreviewAddrEnd = DataPreviewAddr; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp)) && DataPreviewAddr > 0)                  { DataPreviewAddr = data_preview_addr_next = std::max(s64(0), s64(DataPreviewAddr) - s64(visible_count)); if (!ImGui::GetIO().KeyShift) DataPreviewAddrEnd = DataPreviewAddr; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown)) && DataPreviewAddr < mem_size - 1)     { DataPreviewAddr = data_preview_addr_next = std::min(s64(mem_size - 1), s64(DataPreviewAddr) + s64(visible_count)); if (!ImGui::GetIO().KeyShift) DataPreviewAddrEnd = DataPreviewAddr; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)) && DataPreviewAddr > 0)                    { DataPreviewAddr = data_preview_addr_next = 0; if (!ImGui::GetIO().KeyShift) DataPreviewAddrEnd = DataPreviewAddr; }
                else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)) && DataPreviewAddr < mem_size - 1)          { DataPreviewAddr = data_preview_addr_next = mem_size - 1; if (!ImGui::GetIO().KeyShift) DataPreviewAddrEnd = DataPreviewAddr; }
            }
        }

        if (data_preview_addr_next != (size_t)-1 && (data_preview_addr_next / Cols) != (data_preview_addr_backup / Cols))
        {
            // Track cursor movements
            const int scroll_offset = ((int)(data_preview_addr_next / Cols) - (int)(data_preview_addr_backup / Cols));
            const bool scroll_desired = (scroll_offset < 0 && data_preview_addr_next < visible_start_addr + Cols * 2) || (scroll_offset > 0 && data_preview_addr_next > visible_end_addr - Cols * 2);
            if (scroll_desired)
                ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offset * s.LineHeight);
        }
        if (data_editing_addr_next != (size_t)-1 && (data_editing_addr_next / Cols) != (data_editing_addr_backup / Cols))
        {
            // Track cursor movements
            const int scroll_offset = ((int)(data_editing_addr_next / Cols) - (int)(data_editing_addr_backup / Cols));
            const bool scroll_desired = (scroll_offset < 0 && data_editing_addr_next < visible_start_addr + Cols * 2) || (scroll_offset > 0 && data_editing_addr_next > visible_end_addr - Cols * 2);
            if (scroll_desired)
                ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offset * s.LineHeight);
        }

        // Draw vertical separator
        ImVec2 window_pos = ImGui::GetWindowPos();
        float scrollX = ImGui::GetScrollX();
        
        if (OptShowAscii)
            draw_list->AddLine(ImVec2(window_pos.x + s.PosAsciiStart - s.GlyphWidth - scrollX, window_pos.y), ImVec2(window_pos.x + s.PosAsciiStart - s.GlyphWidth - scrollX, window_pos.y + 9999), ImGui::GetColorU32(ImGuiCol_Border));
        if (OptShowAdvancedDecoding)
            draw_list->AddLine(ImVec2(window_pos.x + s.PosDecodingStart - s.GlyphWidth - scrollX, window_pos.y), ImVec2(window_pos.x + s.PosDecodingStart - s.GlyphWidth - scrollX, window_pos.y + 9999), ImGui::GetColorU32(ImGuiCol_Border));

        const ImU32 color_text = ImGui::GetColorU32(ImGuiCol_Text);
        const ImU32 color_disabled = OptGreyOutZeroes ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : color_text;

        const char* format_address = OptUpperCaseHex ? "%0*" _PRISizeT "X: " : "%0*" _PRISizeT "x: ";
        const char* format_data = OptUpperCaseHex ? "%0*" _PRISizeT "X" : "%0*" _PRISizeT "x";
        const char* format_byte = OptUpperCaseHex ? "%02X" : "%02x";
        const char* format_byte_space = OptUpperCaseHex ? "%02X " : "%02x ";

        bool tooltipShown = false;
        for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++) // display only visible lines
        {
            size_t addr = (size_t)(line_i * Cols);
            ImGui::Text(format_address, s.AddrDigitsCount, base_display_addr + addr);

            // Draw Hexadecimal
            for (int n = 0; n < Cols && addr < mem_size; n++, addr++)
            {
                float byte_pos_x = s.PosHexStart + s.HexCellWidth * n;
                if (OptMidColsCount > 0)
                    byte_pos_x += (float)(n / OptMidColsCount) * s.SpacingBetweenMidCols;
                ImGui::SameLine(byte_pos_x);

                // Draw highlight
                bool is_highlight_from_user_range = (addr >= HighlightMin && addr < HighlightMax);
                bool is_highlight_from_user_func = (HighlightFn && HighlightFn(mem_data, addr, false));
                bool is_highlight_from_preview = (addr >= DataPreviewAddr && addr <= DataPreviewAddrEnd) || (addr >= DataPreviewAddrEnd && addr <= DataPreviewAddr);
                if (is_highlight_from_user_range || is_highlight_from_user_func || is_highlight_from_preview)
                {
                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    float highlight_width = s.GlyphWidth * 2;
                    bool is_next_byte_highlighted = (addr + 1 < mem_size) &&
                                                    ((HighlightMax != (size_t)-1 && addr + 1 < HighlightMax) ||
                                                    (HighlightFn && HighlightFn(mem_data, addr + 1, true)) ||
                                                    ((addr + 1) >= DataPreviewAddr && (addr + 1) <= DataPreviewAddrEnd) || ((addr + 1) >= DataPreviewAddrEnd && (addr + 1) <= DataPreviewAddr));
                    if (is_next_byte_highlighted)
                    {
                        highlight_width = s.HexCellWidth;
                        if (OptMidColsCount > 0 && n > 0 && (n + 1) < Cols && ((n + 1) % OptMidColsCount) == 0)
                            highlight_width += s.SpacingBetweenMidCols;
                    }

                    ImU32 color = HighlightColor;
                    if ((is_highlight_from_user_range + is_highlight_from_user_func + is_highlight_from_preview) > 1)
                        color = (ImAlphaBlendColors(HighlightColor, 0x60C08080) & 0x00FFFFFF) | 0x90000000;

                    draw_list->AddRectFilled(pos, ImVec2(pos.x + highlight_width, pos.y + s.LineHeight), color);
                }

                if (DataEditingAddr == addr)
                {
                    // Display text input on current byte
                    bool data_write = false;
                    ImGui::PushID((void*)addr);
                    if (DataEditingTakeFocus)
                    {
                        ImGui::SetKeyboardFocusHere();
                        ImGui::CaptureKeyboardFromApp(true);
                        sprintf(AddrInputBuf, format_data, s.AddrDigitsCount, base_display_addr + addr);
                        sprintf(DataInputBuf, format_byte, ReadFn ? ReadFn(mem_data, addr) : mem_data[addr]);
                    }
                    ImGui::PushItemWidth(s.GlyphWidth * 2);
                    struct UserData
                    {
                        // FIXME: We should have a way to retrieve the text edit cursor position more easily in the API, this is rather tedious. This is such a ugly mess we may be better off not using InputText() at all here.
                        static int Callback(ImGuiInputTextCallbackData* data)
                        {
                            UserData* user_data = (UserData*)data->UserData;
                            if (!data->HasSelection())
                                user_data->CursorPos = data->CursorPos;
                            if (data->SelectionStart == 0 && data->SelectionEnd == data->BufTextLen)
                            {
                                // When not editing a byte, always rewrite its content (this is a bit tricky, since InputText technically "owns" the master copy of the buffer we edit it in there)
                                data->DeleteChars(0, data->BufTextLen);
                                data->InsertChars(0, user_data->CurrentBufOverwrite);
                                data->SelectionStart = 0;
                                data->SelectionEnd = 2;
                                data->CursorPos = 0;
                            }
                            return 0;
                        }
                        char   CurrentBufOverwrite[3];  // Input
                        int    CursorPos;               // Output
                    };
                    UserData user_data;
                    user_data.CursorPos = -1;
                    sprintf(user_data.CurrentBufOverwrite, format_byte, ReadFn ? ReadFn(mem_data, addr) : mem_data[addr]);
                    ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_AlwaysInsertMode | ImGuiInputTextFlags_CallbackAlways;
                    if (ImGui::InputText("##data", DataInputBuf, 32, flags, UserData::Callback, &user_data))
                        data_write = data_next = true;
                    else if (!DataEditingTakeFocus && !ImGui::IsItemActive())
                        DataEditingAddr = data_editing_addr_next = (size_t)-1;
                    DataEditingTakeFocus = false;
                    ImGui::PopItemWidth();
                    if (user_data.CursorPos >= 2)
                        data_write = data_next = true;
                    if (data_editing_addr_next != (size_t)-1)
                        data_write = data_next = false;
                    unsigned int data_input_value = 0;
                    if (data_write && sscanf(DataInputBuf, "%X", &data_input_value) == 1)
                    {
                        if (WriteFn)
                            WriteFn(mem_data, addr, (ImU8)data_input_value);
                        else
                            mem_data[addr] = (ImU8)data_input_value;
                    }
                    ImGui::PopID();
                }
                else
                {
                    // NB: The trailing space is not visible but ensure there's no gap that the mouse cannot click on.
                    ImU8 b = ReadFn ? ReadFn(mem_data, addr) : mem_data[addr];

                    if (OptShowHexII)
                    {
                        if ((b >= 32 && b < 128))
                            ImGui::Text(".%c ", b);
                        else if (b == 0xFF && OptGreyOutZeroes)
                            ImGui::TextDisabled("## ");
                        else if (b == 0x00)
                            ImGui::Text("   ");
                        else
                            ImGui::Text(format_byte_space, b);
                    }
                    else
                    {
                        if (b == 0 && OptGreyOutZeroes)
                            ImGui::TextDisabled("00 ");
                        else
                            ImGui::Text(format_byte_space, b);
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0) && !ImGui::GetIO().KeyShift)
                    {
                        if (!ReadOnly && ImGui::IsMouseDoubleClicked(0)) {
                            DataEditingTakeFocus = true;
                            data_editing_addr_next = addr;
                        }

                        DataPreviewAddr = addr;
                        DataPreviewAddrEnd = addr;
                    }
                    if (ImGui::IsItemHovered() && ((ImGui::IsMouseClicked(0) && ImGui::GetIO().KeyShift) || ImGui::IsMouseDragging(0))) {
                        DataPreviewAddrEnd = addr;
                    }
                    if (ImGui::IsItemHovered() && !tooltipShown) {
                        if (HoverFn) {
                            HoverFn(mem_data, addr);
                            tooltipShown = true;
                        }
                    }
                }
            }

            if (OptShowAscii)
            {
                // Draw ASCII values
                ImGui::SameLine(s.PosAsciiStart);
                ImVec2 pos = ImGui::GetCursorScreenPos();
                addr = line_i * Cols;

                ImGui::PushID(-1);
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(s.GlyphWidth, s.LineHeight));

                ImGui::PopID();

                for (int n = 0; n < Cols && addr < mem_size; n++, addr++)
                {
                    if (addr == DataEditingAddr)
                    {
                        draw_list->AddRectFilled(pos, ImVec2(pos.x + s.GlyphWidth, pos.y + s.LineHeight), ImGui::GetColorU32(ImGuiCol_FrameBg));
                        draw_list->AddRectFilled(pos, ImVec2(pos.x + s.GlyphWidth, pos.y + s.LineHeight), ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
                    }
                    unsigned char c = ReadFn ? ReadFn(mem_data, addr) : mem_data[addr];
                    char display_c = (c < 32 || c >= 128) ? '.' : c;
                    draw_list->AddText(pos, (display_c == c) ? color_text : color_disabled, &display_c, &display_c + 1);

                    // Draw highlight
                    bool is_highlight_from_user_range = (addr >= HighlightMin && addr < HighlightMax);
                    bool is_highlight_from_user_func = (HighlightFn && HighlightFn(mem_data, addr, false));
                    bool is_highlight_from_preview = (addr >= DataPreviewAddr && addr <= DataPreviewAddrEnd) || (addr >= DataPreviewAddrEnd && addr <= DataPreviewAddr);
                    if (is_highlight_from_user_range || is_highlight_from_user_func || is_highlight_from_preview)
                    {
                        ImU32 color = HighlightColor;
                        if ((is_highlight_from_user_range + is_highlight_from_user_func + is_highlight_from_preview) > 1)
                            color = (ImAlphaBlendColors(HighlightColor, 0x60C08080) & 0x00FFFFFF) | 0x90000000;

                        draw_list->AddRectFilled(pos, ImVec2(pos.x + s.GlyphWidth, pos.y + s.LineHeight), color);
                    }


                    ImGui::PushID(line_i * Cols + n);
                    ImGui::SameLine();
                    ImGui::Dummy(ImVec2(s.GlyphWidth, s.LineHeight));

                    ImGui::PopID();

                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0) && !ImGui::GetIO().KeyShift)
                    {
                        if (!ReadOnly && ImGui::IsMouseDoubleClicked(0)) {
                            DataEditingTakeFocus = true;
                            data_editing_addr_next = addr;
                        }

                        DataPreviewAddr = addr;
                        DataPreviewAddrEnd = addr;

                    }
                    if (ImGui::IsItemHovered() && ((ImGui::IsMouseClicked(0) && ImGui::GetIO().KeyShift) || ImGui::IsMouseDragging(0))) {
                        DataPreviewAddrEnd = addr;
                    }

                    pos.x += s.GlyphWidth;
                }

                ImGui::PushID(-1);
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(s.GlyphWidth, s.LineHeight));

                ImGui::PopID();
            }

            if (OptShowAdvancedDecoding && DecodeFn) {
                // Draw decoded bytes
                ImGui::SameLine(s.PosDecodingStart);
                ImVec2 pos = ImGui::GetCursorScreenPos();
                addr = line_i * Cols;

                ImGui::PushID(-1);
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(s.GlyphWidth, s.LineHeight));

                ImGui::PopID();

                for (int n = 0; n < Cols && addr < mem_size;)
                {
                    auto decodedData = DecodeFn(mem_data, addr);

                    auto displayData = decodedData.data;
                    auto glyphWidth = ImGui::CalcTextSize(displayData.c_str()).x + 1;

                    if (addr == DataEditingAddr)
                    {
                        draw_list->AddRectFilled(pos, ImVec2(pos.x + glyphWidth, pos.y + s.LineHeight), ImGui::GetColorU32(ImGuiCol_FrameBg));
                        draw_list->AddRectFilled(pos, ImVec2(pos.x + glyphWidth, pos.y + s.LineHeight), ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
                    }

                    draw_list->AddText(pos, decodedData.color, displayData.c_str(), displayData.c_str() + displayData.length());

                    // Draw highlight
                    bool is_highlight_from_user_range = (addr >= HighlightMin && addr < HighlightMax);
                    bool is_highlight_from_user_func = (HighlightFn && HighlightFn(mem_data, addr, false));
                    bool is_highlight_from_preview = (addr >= DataPreviewAddr && addr <= DataPreviewAddrEnd) || (addr >= DataPreviewAddrEnd && addr <= DataPreviewAddr);
                    if (is_highlight_from_user_range || is_highlight_from_user_func || is_highlight_from_preview)
                    {
                        ImU32 color = HighlightColor;
                        if ((is_highlight_from_user_range + is_highlight_from_user_func + is_highlight_from_preview) > 1)
                            color = (ImAlphaBlendColors(HighlightColor, 0x60C08080) & 0x00FFFFFF) | 0x90000000;

                        draw_list->AddRectFilled(pos, ImVec2(pos.x + glyphWidth, pos.y + s.LineHeight), color);
                    }


                    ImGui::PushID(line_i * Cols + n);
                    ImGui::SameLine();
                    ImGui::Dummy(ImVec2(glyphWidth, s.LineHeight));

                    ImGui::PopID();

                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0) && !ImGui::GetIO().KeyShift)
                    {
                        if (!ReadOnly && ImGui::IsMouseDoubleClicked(0)) {
                            DataEditingTakeFocus = true;
                            data_editing_addr_next = addr;
                        }

                        DataPreviewAddr = addr;
                        DataPreviewAddrEnd = addr;

                    }
                    if (ImGui::IsItemHovered() && ((ImGui::IsMouseClicked(0) && ImGui::GetIO().KeyShift) || ImGui::IsMouseDragging(0))) {
                        DataPreviewAddrEnd = addr;
                    }

                    pos.x += glyphWidth;

                    if (addr <= 1) {
                        n++;
                        addr++;
                    } else {
                        n += decodedData.advance;
                        addr += decodedData.advance;
                    }
                }
            }
        }
        IM_ASSERT(clipper.Step() == false);
        clipper.End();
        ImGui::PopStyleVar(2);
        ImGui::EndChild();

        if (data_next && DataEditingAddr < mem_size)
        {
            DataEditingAddr = DataPreviewAddr = DataEditingAddr + 1;
            DataEditingTakeFocus = true;
        }
        else if (data_editing_addr_next != (size_t)-1)
        {
            DataEditingAddr = DataPreviewAddr = DataPreviewAddrEnd = data_editing_addr_next;
        }

        if (OptShowOptions)
        {
            ImGui::Separator();
            DrawOptionsLine(s, mem_data, mem_size, base_display_addr);
        }

        // Notify the main window of our ideal child content size (FIXME: we are missing an API to get the contents size from the child)
        ImGui::SetCursorPosX(s.WindowWidth);
    }

    void DrawOptionsLine(const Sizes& s, void* mem_data, size_t mem_size, size_t base_display_addr)
    {
        IM_UNUSED(mem_data);
        ImGuiStyle& style = ImGui::GetStyle();
        const char* format_range = OptUpperCaseHex ? "Range %0*" _PRISizeT "X..%0*" _PRISizeT "X" : "Range %0*" _PRISizeT "x..%0*" _PRISizeT "x";
        const char* format_selection = OptUpperCaseHex ? "Selection %0*" _PRISizeT "X..%0*" _PRISizeT "X (%ld %s)" : "Range %0*" _PRISizeT "x..%0*" _PRISizeT "x (%ld %s)";

        // Options menu
        if (ImGui::Button("Options"))
            ImGui::OpenPopup("options");

        if (ImGui::BeginPopup("options")) {
            ImGui::PushItemWidth(ImGui::CalcTextSize("00 cols").x * 1.1f);
            if (ImGui::DragInt("##cols", &Cols, 0.2f, 4, 32, "%d cols")) { ContentsWidthChanged = true; if (Cols < 1) Cols = 1; }
            ImGui::PopItemWidth();
            ImGui::Checkbox("Show HexII", &OptShowHexII);
            if (ImGui::Checkbox("Show Ascii", &OptShowAscii)) { ContentsWidthChanged = true; }
            if (ImGui::Checkbox("Show Advanced Decoding", &OptShowAdvancedDecoding)) { ContentsWidthChanged = true; }
            ImGui::Checkbox("Grey out zeroes", &OptGreyOutZeroes);
            ImGui::Checkbox("Uppercase Hex", &OptUpperCaseHex);

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        ImGui::Text(format_range, s.AddrDigitsCount, base_display_addr, s.AddrDigitsCount, base_display_addr + mem_size - 1);
        if (DataPreviewAddr != (size_t)-1 && DataPreviewAddrEnd != (size_t)-1) {
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();

            auto selectionStart = std::min(DataPreviewAddr, DataPreviewAddrEnd);
            auto selectionEnd = std::max(DataPreviewAddr, DataPreviewAddrEnd);

            size_t regionSize = (selectionEnd - selectionStart) + 1;
            ImGui::Text(format_selection, s.AddrDigitsCount, base_display_addr + selectionStart, s.AddrDigitsCount, base_display_addr + selectionEnd, regionSize, regionSize == 1 ? "byte" : "bytes");
        }

        if (GotoAddr != (size_t)-1)
        {
            if (GotoAddr < mem_size)
            {
                ImGui::BeginChild("##scrolling");
                ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + (GotoAddr / Cols) * ImGui::GetTextLineHeight());
                ImGui::EndChild();
            }
            GotoAddr = (size_t)-1;
        }
    }

    static bool IsBigEndian()
    {
        uint16_t x = 1;
        char c[2];
        memcpy(c, &x, 2);
        return c[0] != 0;
    }

    static void* EndianessCopyBigEndian(void* _dst, void* _src, size_t s, int is_little_endian)
    {
        if (is_little_endian)
        {
            uint8_t* dst = (uint8_t*)_dst;
            uint8_t* src = (uint8_t*)_src + s - 1;
            for (int i = 0, n = (int)s; i < n; ++i)
                memcpy(dst++, src--, 1);
            return _dst;
        }
        else
        {
            return memcpy(_dst, _src, s);
        }
    }

    static void* EndianessCopyLittleEndian(void* _dst, void* _src, size_t s, int is_little_endian)
    {
        if (is_little_endian)
        {
            return memcpy(_dst, _src, s);
        }
        else
        {
            uint8_t* dst = (uint8_t*)_dst;
            uint8_t* src = (uint8_t*)_src + s - 1;
            for (int i = 0, n = (int)s; i < n; ++i)
                memcpy(dst++, src--, 1);
            return _dst;
        }
    }

    void* EndianessCopy(void* dst, void* src, size_t size) const
    {
        static void* (*fp)(void*, void*, size_t, int) = NULL;
        if (fp == NULL)
            fp = IsBigEndian() ? EndianessCopyBigEndian : EndianessCopyLittleEndian;
        return fp(dst, src, size, PreviewEndianess);
    }
};

#undef _PRISizeT
#undef ImSnprintf

#ifdef _MSC_VER
#pragma warning (pop)
#endif