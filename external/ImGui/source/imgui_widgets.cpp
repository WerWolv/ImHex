// dear imgui, v1.80 WIP
// (widgets code)

/*

Index of this file:

// [SECTION] Forward Declarations
// [SECTION] Widgets: Text, etc.
// [SECTION] Widgets: Main (Button, Image, Checkbox, RadioButton, ProgressBar, Bullet, etc.)
// [SECTION] Widgets: Low-level Layout helpers (Spacing, Dummy, NewLine, Separator, etc.)
// [SECTION] Widgets: ComboBox
// [SECTION] Data Type and Data Formatting Helpers
// [SECTION] Widgets: DragScalar, DragFloat, DragInt, etc.
// [SECTION] Widgets: SliderScalar, SliderFloat, SliderInt, etc.
// [SECTION] Widgets: InputScalar, InputFloat, InputInt, etc.
// [SECTION] Widgets: InputText, InputTextMultiline
// [SECTION] Widgets: ColorEdit, ColorPicker, ColorButton, etc.
// [SECTION] Widgets: TreeNode, CollapsingHeader, etc.
// [SECTION] Widgets: Selectable
// [SECTION] Widgets: ListBox
// [SECTION] Widgets: PlotLines, PlotHistogram
// [SECTION] Widgets: Value helpers
// [SECTION] Widgets: MenuItem, BeginMenu, EndMenu, etc.
// [SECTION] Widgets: BeginTabBar, EndTabBar, etc.
// [SECTION] Widgets: BeginTabItem, EndTabItem, etc.
// [SECTION] Widgets: Columns, BeginColumns, EndColumns, etc.
// [SECTION] Widgets: BeginTable, EndTable, etc.

*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "imgui.h"
#ifndef IMGUI_DISABLE

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

#include <ctype.h>      // toupper
#if defined(_MSC_VER) && _MSC_VER <= 1500 // MSVC 2008 or earlier
#include <stddef.h>     // intptr_t
#else
#include <stdint.h>     // intptr_t
#endif

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning (disable: 4127)     // condition expression is constant
#pragma warning (disable: 4996)     // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#if defined(_MSC_VER) && _MSC_VER >= 1922 // MSVC 2019 16.2 or later
#pragma warning (disable: 5054)     // operator '|': deprecated between enumerations of different types
#endif
#endif

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#if __has_warning("-Wunknown-warning-option")
#pragma clang diagnostic ignored "-Wunknown-warning-option"         // warning: unknown warning group 'xxx'                      // not all warnings are known by all Clang versions and they tend to be rename-happy.. so ignoring warnings triggers new warnings on some configuration. Great!
#endif
#pragma clang diagnostic ignored "-Wunknown-pragmas"                // warning: unknown warning group 'xxx'
#pragma clang diagnostic ignored "-Wold-style-cast"                 // warning: use of old-style cast                            // yes, they are more terse.
#pragma clang diagnostic ignored "-Wfloat-equal"                    // warning: comparing floating point with == or != is unsafe // storing and comparing against same constants (typically 0.0f) is ok.
#pragma clang diagnostic ignored "-Wformat-nonliteral"              // warning: format string is not a string literal            // passing non-literal to vsnformat(). yes, user passing incorrect format strings can crash the code.
#pragma clang diagnostic ignored "-Wsign-conversion"                // warning: implicit conversion changes signedness
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"  // warning: zero as null pointer constant                    // some standard header variations use #define NULL 0
#pragma clang diagnostic ignored "-Wdouble-promotion"               // warning: implicit conversion from 'float' to 'double' when passing argument to function  // using printf() is a misery with this as C++ va_arg ellipsis changes float to double.
#pragma clang diagnostic ignored "-Wenum-enum-conversion"           // warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_')
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"// warning: bitwise operation between different enumeration types ('XXXFlags_' and 'XXXFlagsPrivate_') is deprecated
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"  // warning: implicit conversion from 'xxx' to 'float' may lose precision
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpragmas"                  // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wformat-nonliteral"        // warning: format not a string literal, format string not checked
#pragma GCC diagnostic ignored "-Wclass-memaccess"          // [__GNUC__ >= 8] warning: 'memset/memcpy' clearing/writing an object of type 'xxxx' with no trivial copy-assignment; use assignment or value-initialization instead
#endif

//-------------------------------------------------------------------------
// Data
//-------------------------------------------------------------------------

// Those MIN/MAX values are not define because we need to point to them
static const signed char    IM_S8_MIN  = -128;
static const signed char    IM_S8_MAX  = 127;
static const unsigned char  IM_U8_MIN  = 0;
static const unsigned char  IM_U8_MAX  = 0xFF;
static const signed short   IM_S16_MIN = -32768;
static const signed short   IM_S16_MAX = 32767;
static const unsigned short IM_U16_MIN = 0;
static const unsigned short IM_U16_MAX = 0xFFFF;
static const ImS32          IM_S32_MIN = INT_MIN;    // (-2147483647 - 1), (0x80000000);
static const ImS32          IM_S32_MAX = INT_MAX;    // (2147483647), (0x7FFFFFFF)
static const ImU32          IM_U32_MIN = 0;
static const ImU32          IM_U32_MAX = UINT_MAX;   // (0xFFFFFFFF)
#ifdef LLONG_MIN
static const ImS64          IM_S64_MIN = LLONG_MIN;  // (-9223372036854775807ll - 1ll);
static const ImS64          IM_S64_MAX = LLONG_MAX;  // (9223372036854775807ll);
#else
static const ImS64          IM_S64_MIN = -9223372036854775807LL - 1;
static const ImS64          IM_S64_MAX = 9223372036854775807LL;
#endif
static const ImU64          IM_U64_MIN = 0;
#ifdef ULLONG_MAX
static const ImU64          IM_U64_MAX = ULLONG_MAX; // (0xFFFFFFFFFFFFFFFFull);
#else
static const ImU64          IM_U64_MAX = (2ULL * 9223372036854775807LL + 1);
#endif

static const float          TOOLTIP_DELAY = 0.50f;   // Time before slow tooltips appears (FIXME: This is temporary until we merge in tooltip timer+priority work)

//-------------------------------------------------------------------------
// [SECTION] Forward Declarations
//-------------------------------------------------------------------------

// For InputTextEx()
static bool             InputTextFilterCharacter(unsigned int* p_char, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);
static int              InputTextCalcTextLenAndLineCount(const char* text_begin, const char** out_text_end);
static ImVec2           InputTextCalcTextSizeW(const ImWchar* text_begin, const ImWchar* text_end, const ImWchar** remaining = NULL, ImVec2* out_offset = NULL, bool stop_on_new_line = false);

//-------------------------------------------------------------------------
// [SECTION] Widgets: Text, etc.
//-------------------------------------------------------------------------
// - TextEx() [Internal]
// - TextUnformatted()
// - Text()
// - TextV()
// - TextColored()
// - TextColoredV()
// - TextDisabled()
// - TextDisabledV()
// - TextWrapped()
// - TextWrappedV()
// - LabelText()
// - LabelTextV()
// - BulletText()
// - BulletTextV()
//-------------------------------------------------------------------------

void ImGui::TextEx(const char* text, const char* text_end, ImGuiTextFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    IM_ASSERT(text != NULL);
    const char* text_begin = text;
    if (text_end == NULL)
        text_end = text + strlen(text); // FIXME-OPT

    const ImVec2 text_pos(window->DC.CursorPos.x, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
    const float wrap_pos_x = window->DC.TextWrapPos;
    const bool wrap_enabled = (wrap_pos_x >= 0.0f);
    if (text_end - text > 2000 && !wrap_enabled)
    {
        // Long text!
        // Perform manual coarse clipping to optimize for long multi-line text
        // - From this point we will only compute the width of lines that are visible. Optimization only available when word-wrapping is disabled.
        // - We also don't vertically center the text within the line full height, which is unlikely to matter because we are likely the biggest and only item on the line.
        // - We use memchr(), pay attention that well optimized versions of those str/mem functions are much faster than a casually written loop.
        const char* line = text;
        const float line_height = GetTextLineHeight();
        ImVec2 text_size(0, 0);

        // Lines to skip (can't skip when logging text)
        ImVec2 pos = text_pos;
        if (!g.LogEnabled)
        {
            int lines_skippable = (int)((window->ClipRect.Min.y - text_pos.y) / line_height);
            if (lines_skippable > 0)
            {
                int lines_skipped = 0;
                while (line < text_end && lines_skipped < lines_skippable)
                {
                    const char* line_end = (const char*)memchr(line, '\n', text_end - line);
                    if (!line_end)
                        line_end = text_end;
                    if ((flags & ImGuiTextFlags_NoWidthForLargeClippedText) == 0)
                        text_size.x = ImMax(text_size.x, CalcTextSize(line, line_end).x);
                    line = line_end + 1;
                    lines_skipped++;
                }
                pos.y += lines_skipped * line_height;
            }
        }

        // Lines to render
        if (line < text_end)
        {
            ImRect line_rect(pos, pos + ImVec2(FLT_MAX, line_height));
            while (line < text_end)
            {
                if (IsClippedEx(line_rect, 0, false))
                    break;

                const char* line_end = (const char*)memchr(line, '\n', text_end - line);
                if (!line_end)
                    line_end = text_end;
                text_size.x = ImMax(text_size.x, CalcTextSize(line, line_end).x);
                RenderText(pos, line, line_end, false);
                line = line_end + 1;
                line_rect.Min.y += line_height;
                line_rect.Max.y += line_height;
                pos.y += line_height;
            }

            // Count remaining lines
            int lines_skipped = 0;
            while (line < text_end)
            {
                const char* line_end = (const char*)memchr(line, '\n', text_end - line);
                if (!line_end)
                    line_end = text_end;
                if ((flags & ImGuiTextFlags_NoWidthForLargeClippedText) == 0)
                    text_size.x = ImMax(text_size.x, CalcTextSize(line, line_end).x);
                line = line_end + 1;
                lines_skipped++;
            }
            pos.y += lines_skipped * line_height;
        }
        text_size.y = (pos - text_pos).y;

        ImRect bb(text_pos, text_pos + text_size);
        ItemSize(text_size, 0.0f);
        ItemAdd(bb, 0);
    }
    else
    {
        const float wrap_width = wrap_enabled ? CalcWrapWidthForPos(window->DC.CursorPos, wrap_pos_x) : 0.0f;
        const ImVec2 text_size = CalcTextSize(text_begin, text_end, false, wrap_width);

        ImRect bb(text_pos, text_pos + text_size);
        ItemSize(text_size, 0.0f);
        if (!ItemAdd(bb, 0))
            return;

        // Render (we don't hide text after ## in this end-user function)
        RenderTextWrapped(bb.Min, text_begin, text_end, wrap_width);
    }
}

void ImGui::TextUnformatted(const char* text, const char* text_end)
{
    TextEx(text, text_end, ImGuiTextFlags_NoWidthForLargeClippedText);
}

void ImGui::Text(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextV(fmt, args);
    va_end(args);
}

void ImGui::TextV(const char* fmt, va_list args)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const char* text_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);
    TextEx(g.TempBuffer, text_end, ImGuiTextFlags_NoWidthForLargeClippedText);
}

void ImGui::TextColored(const ImVec4& col, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextColoredV(col, fmt, args);
    va_end(args);
}

void ImGui::TextColoredV(const ImVec4& col, const char* fmt, va_list args)
{
    PushStyleColor(ImGuiCol_Text, col);
    if (fmt[0] == '%' && fmt[1] == 's' && fmt[2] == 0)
        TextEx(va_arg(args, const char*), NULL, ImGuiTextFlags_NoWidthForLargeClippedText); // Skip formatting
    else
        TextV(fmt, args);
    PopStyleColor();
}

void ImGui::TextDisabled(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextDisabledV(fmt, args);
    va_end(args);
}

void ImGui::TextDisabledV(const char* fmt, va_list args)
{
    ImGuiContext& g = *GImGui;
    PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
    if (fmt[0] == '%' && fmt[1] == 's' && fmt[2] == 0)
        TextEx(va_arg(args, const char*), NULL, ImGuiTextFlags_NoWidthForLargeClippedText); // Skip formatting
    else
        TextV(fmt, args);
    PopStyleColor();
}

void ImGui::TextWrapped(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TextWrappedV(fmt, args);
    va_end(args);
}

void ImGui::TextWrappedV(const char* fmt, va_list args)
{
    ImGuiContext& g = *GImGui;
    bool need_backup = (g.CurrentWindow->DC.TextWrapPos < 0.0f);  // Keep existing wrap position if one is already set
    if (need_backup)
        PushTextWrapPos(0.0f);
    if (fmt[0] == '%' && fmt[1] == 's' && fmt[2] == 0)
        TextEx(va_arg(args, const char*), NULL, ImGuiTextFlags_NoWidthForLargeClippedText); // Skip formatting
    else
        TextV(fmt, args);
    if (need_backup)
        PopTextWrapPos();
}

void ImGui::LabelText(const char* label, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LabelTextV(label, fmt, args);
    va_end(args);
}

// Add a label+text combo aligned to other label+value widgets
void ImGui::LabelTextV(const char* label, const char* fmt, va_list args)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float w = CalcItemWidth();

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImRect value_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2));
    const ImRect total_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w + (label_size.x > 0.0f ? style.ItemInnerSpacing.x : 0.0f), style.FramePadding.y * 2) + label_size);
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, 0))
        return;

    // Render
    const char* value_text_begin = &g.TempBuffer[0];
    const char* value_text_end = value_text_begin + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);
    RenderTextClipped(value_bb.Min, value_bb.Max, value_text_begin, value_text_end, NULL, ImVec2(0.0f, 0.5f));
    if (label_size.x > 0.0f)
        RenderText(ImVec2(value_bb.Max.x + style.ItemInnerSpacing.x, value_bb.Min.y + style.FramePadding.y), label);
}

void ImGui::BulletText(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    BulletTextV(fmt, args);
    va_end(args);
}

// Text with a little bullet aligned to the typical tree node.
void ImGui::BulletTextV(const char* fmt, va_list args)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const char* text_begin = g.TempBuffer;
    const char* text_end = text_begin + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);
    const ImVec2 label_size = CalcTextSize(text_begin, text_end, false);
    const ImVec2 total_size = ImVec2(g.FontSize + (label_size.x > 0.0f ? (label_size.x + style.FramePadding.x * 2) : 0.0f), label_size.y);  // Empty text doesn't add padding
    ImVec2 pos = window->DC.CursorPos;
    pos.y += window->DC.CurrLineTextBaseOffset;
    ItemSize(total_size, 0.0f);
    const ImRect bb(pos, pos + total_size);
    if (!ItemAdd(bb, 0))
        return;

    // Render
    ImU32 text_col = GetColorU32(ImGuiCol_Text);
    RenderBullet(window->DrawList, bb.Min + ImVec2(style.FramePadding.x + g.FontSize * 0.5f, g.FontSize * 0.5f), text_col);
    RenderText(bb.Min + ImVec2(g.FontSize + style.FramePadding.x * 2, 0.0f), text_begin, text_end, false);
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Main
//-------------------------------------------------------------------------
// - ButtonBehavior() [Internal]
// - Button()
// - SmallButton()
// - InvisibleButton()
// - ArrowButton()
// - CloseButton() [Internal]
// - CollapseButton() [Internal]
// - GetWindowScrollbarID() [Internal]
// - GetWindowScrollbarRect() [Internal]
// - Scrollbar() [Internal]
// - ScrollbarEx() [Internal]
// - Image()
// - ImageButton()
// - Checkbox()
// - CheckboxFlagsT() [Internal]
// - CheckboxFlags()
// - RadioButton()
// - ProgressBar()
// - Bullet()
//-------------------------------------------------------------------------

// The ButtonBehavior() function is key to many interactions and used by many/most widgets.
// Because we handle so many cases (keyboard/gamepad navigation, drag and drop) and many specific behavior (via ImGuiButtonFlags_),
// this code is a little complex.
// By far the most common path is interacting with the Mouse using the default ImGuiButtonFlags_PressedOnClickRelease button behavior.
// See the series of events below and the corresponding state reported by dear imgui:
//------------------------------------------------------------------------------------------------------------------------------------------------
// with PressedOnClickRelease:             return-value  IsItemHovered()  IsItemActive()  IsItemActivated()  IsItemDeactivated()  IsItemClicked()
//   Frame N+0 (mouse is outside bb)        -             -                -               -                  -                    -
//   Frame N+1 (mouse moves inside bb)      -             true             -               -                  -                    -
//   Frame N+2 (mouse button is down)       -             true             true            true               -                    true
//   Frame N+3 (mouse button is down)       -             true             true            -                  -                    -
//   Frame N+4 (mouse moves outside bb)     -             -                true            -                  -                    -
//   Frame N+5 (mouse moves inside bb)      -             true             true            -                  -                    -
//   Frame N+6 (mouse button is released)   true          true             -               -                  true                 -
//   Frame N+7 (mouse button is released)   -             true             -               -                  -                    -
//   Frame N+8 (mouse moves outside bb)     -             -                -               -                  -                    -
//------------------------------------------------------------------------------------------------------------------------------------------------
// with PressedOnClick:                    return-value  IsItemHovered()  IsItemActive()  IsItemActivated()  IsItemDeactivated()  IsItemClicked()
//   Frame N+2 (mouse button is down)       true          true             true            true               -                    true
//   Frame N+3 (mouse button is down)       -             true             true            -                  -                    -
//   Frame N+6 (mouse button is released)   -             true             -               -                  true                 -
//   Frame N+7 (mouse button is released)   -             true             -               -                  -                    -
//------------------------------------------------------------------------------------------------------------------------------------------------
// with PressedOnRelease:                  return-value  IsItemHovered()  IsItemActive()  IsItemActivated()  IsItemDeactivated()  IsItemClicked()
//   Frame N+2 (mouse button is down)       -             true             -               -                  -                    true
//   Frame N+3 (mouse button is down)       -             true             -               -                  -                    -
//   Frame N+6 (mouse button is released)   true          true             -               -                  -                    -
//   Frame N+7 (mouse button is released)   -             true             -               -                  -                    -
//------------------------------------------------------------------------------------------------------------------------------------------------
// with PressedOnDoubleClick:              return-value  IsItemHovered()  IsItemActive()  IsItemActivated()  IsItemDeactivated()  IsItemClicked()
//   Frame N+0 (mouse button is down)       -             true             -               -                  -                    true
//   Frame N+1 (mouse button is down)       -             true             -               -                  -                    -
//   Frame N+2 (mouse button is released)   -             true             -               -                  -                    -
//   Frame N+3 (mouse button is released)   -             true             -               -                  -                    -
//   Frame N+4 (mouse button is down)       true          true             true            true               -                    true
//   Frame N+5 (mouse button is down)       -             true             true            -                  -                    -
//   Frame N+6 (mouse button is released)   -             true             -               -                  true                 -
//   Frame N+7 (mouse button is released)   -             true             -               -                  -                    -
//------------------------------------------------------------------------------------------------------------------------------------------------
// Note that some combinations are supported,
// - PressedOnDragDropHold can generally be associated with any flag.
// - PressedOnDoubleClick can be associated by PressedOnClickRelease/PressedOnRelease, in which case the second release event won't be reported.
//------------------------------------------------------------------------------------------------------------------------------------------------
// The behavior of the return-value changes when ImGuiButtonFlags_Repeat is set:
//                                         Repeat+                  Repeat+           Repeat+             Repeat+
//                                         PressedOnClickRelease    PressedOnClick    PressedOnRelease    PressedOnDoubleClick
//-------------------------------------------------------------------------------------------------------------------------------------------------
//   Frame N+0 (mouse button is down)       -                        true              -                   true
//   ...                                    -                        -                 -                   -
//   Frame N + RepeatDelay                  true                     true              -                   true
//   ...                                    -                        -                 -                   -
//   Frame N + RepeatDelay + RepeatRate*N   true                     true              -                   true
//-------------------------------------------------------------------------------------------------------------------------------------------------

bool ImGui::ButtonBehavior(const ImRect& bb, ImGuiID id, bool* out_hovered, bool* out_held, ImGuiButtonFlags flags)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();

    if (flags & ImGuiButtonFlags_Disabled)
    {
        if (out_hovered) *out_hovered = false;
        if (out_held) *out_held = false;
        if (g.ActiveId == id) ClearActiveID();
        return false;
    }

    // Default only reacts to left mouse button
    if ((flags & ImGuiButtonFlags_MouseButtonMask_) == 0)
        flags |= ImGuiButtonFlags_MouseButtonDefault_;

    // Default behavior requires click + release inside bounding box
    if ((flags & ImGuiButtonFlags_PressedOnMask_) == 0)
        flags |= ImGuiButtonFlags_PressedOnDefault_;

    ImGuiWindow* backup_hovered_window = g.HoveredWindow;
    const bool flatten_hovered_children = (flags & ImGuiButtonFlags_FlattenChildren) && g.HoveredRootWindow == window->RootWindow;
    if (flatten_hovered_children)
        g.HoveredWindow = window;

#ifdef IMGUI_ENABLE_TEST_ENGINE
    if (id != 0 && window->DC.LastItemId != id)
        IMGUI_TEST_ENGINE_ITEM_ADD(bb, id);
#endif

    bool pressed = false;
    bool hovered = ItemHoverable(bb, id);

    // Drag source doesn't report as hovered
    if (hovered && g.DragDropActive && g.DragDropPayload.SourceId == id && !(g.DragDropSourceFlags & ImGuiDragDropFlags_SourceNoDisableHover))
        hovered = false;

    // Special mode for Drag and Drop where holding button pressed for a long time while dragging another item triggers the button
    if (g.DragDropActive && (flags & ImGuiButtonFlags_PressedOnDragDropHold) && !(g.DragDropSourceFlags & ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            const float DRAG_DROP_HOLD_TIMER = 0.70f;
            hovered = true;
            SetHoveredID(id);
            if (CalcTypematicRepeatAmount(g.HoveredIdTimer + 0.0001f - g.IO.DeltaTime, g.HoveredIdTimer + 0.0001f, DRAG_DROP_HOLD_TIMER, 0.00f))
            {
                pressed = true;
                g.DragDropHoldJustPressedId = id;
                FocusWindow(window);
            }
        }

    if (flatten_hovered_children)
        g.HoveredWindow = backup_hovered_window;

    // AllowOverlap mode (rarely used) requires previous frame HoveredId to be null or to match. This allows using patterns where a later submitted widget overlaps a previous one.
    if (hovered && (flags & ImGuiButtonFlags_AllowItemOverlap) && (g.HoveredIdPreviousFrame != id && g.HoveredIdPreviousFrame != 0))
        hovered = false;

    // Mouse handling
    if (hovered)
    {
        if (!(flags & ImGuiButtonFlags_NoKeyModifiers) || (!g.IO.KeyCtrl && !g.IO.KeyShift && !g.IO.KeyAlt))
        {
            // Poll buttons
            int mouse_button_clicked = -1;
            int mouse_button_released = -1;
            if ((flags & ImGuiButtonFlags_MouseButtonLeft) && g.IO.MouseClicked[0])         { mouse_button_clicked = 0; }
            else if ((flags & ImGuiButtonFlags_MouseButtonRight) && g.IO.MouseClicked[1])   { mouse_button_clicked = 1; }
            else if ((flags & ImGuiButtonFlags_MouseButtonMiddle) && g.IO.MouseClicked[2])  { mouse_button_clicked = 2; }
            if ((flags & ImGuiButtonFlags_MouseButtonLeft) && g.IO.MouseReleased[0])        { mouse_button_released = 0; }
            else if ((flags & ImGuiButtonFlags_MouseButtonRight) && g.IO.MouseReleased[1])  { mouse_button_released = 1; }
            else if ((flags & ImGuiButtonFlags_MouseButtonMiddle) && g.IO.MouseReleased[2]) { mouse_button_released = 2; }

            if (mouse_button_clicked != -1 && g.ActiveId != id)
            {
                if (flags & (ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnClickReleaseAnywhere))
                {
                    SetActiveID(id, window);
                    g.ActiveIdMouseButton = mouse_button_clicked;
                    if (!(flags & ImGuiButtonFlags_NoNavFocus))
                        SetFocusID(id, window);
                    FocusWindow(window);
                }
                if ((flags & ImGuiButtonFlags_PressedOnClick) || ((flags & ImGuiButtonFlags_PressedOnDoubleClick) && g.IO.MouseDoubleClicked[mouse_button_clicked]))
                {
                    pressed = true;
                    if (flags & ImGuiButtonFlags_NoHoldingActiveId)
                        ClearActiveID();
                    else
                        SetActiveID(id, window); // Hold on ID
                    g.ActiveIdMouseButton = mouse_button_clicked;
                    FocusWindow(window);
                }
            }
            if ((flags & ImGuiButtonFlags_PressedOnRelease) && mouse_button_released != -1)
            {
                // Repeat mode trumps on release behavior
                const bool has_repeated_at_least_once = (flags & ImGuiButtonFlags_Repeat) && g.IO.MouseDownDurationPrev[mouse_button_released] >= g.IO.KeyRepeatDelay;
                if (!has_repeated_at_least_once)
                    pressed = true;
                ClearActiveID();
            }

            // 'Repeat' mode acts when held regardless of _PressedOn flags (see table above).
            // Relies on repeat logic of IsMouseClicked() but we may as well do it ourselves if we end up exposing finer RepeatDelay/RepeatRate settings.
            if (g.ActiveId == id && (flags & ImGuiButtonFlags_Repeat))
                if (g.IO.MouseDownDuration[g.ActiveIdMouseButton] > 0.0f && IsMouseClicked(g.ActiveIdMouseButton, true))
                    pressed = true;
        }

        if (pressed)
            g.NavDisableHighlight = true;
    }

    // Gamepad/Keyboard navigation
    // We report navigated item as hovered but we don't set g.HoveredId to not interfere with mouse.
    if (g.NavId == id && !g.NavDisableHighlight && g.NavDisableMouseHover && (g.ActiveId == 0 || g.ActiveId == id || g.ActiveId == window->MoveId))
        if (!(flags & ImGuiButtonFlags_NoHoveredOnFocus))
            hovered = true;
    if (g.NavActivateDownId == id)
    {
        bool nav_activated_by_code = (g.NavActivateId == id);
        bool nav_activated_by_inputs = IsNavInputTest(ImGuiNavInput_Activate, (flags & ImGuiButtonFlags_Repeat) ? ImGuiInputReadMode_Repeat : ImGuiInputReadMode_Pressed);
        if (nav_activated_by_code || nav_activated_by_inputs)
            pressed = true;
        if (nav_activated_by_code || nav_activated_by_inputs || g.ActiveId == id)
        {
            // Set active id so it can be queried by user via IsItemActive(), equivalent of holding the mouse button.
            g.NavActivateId = id; // This is so SetActiveId assign a Nav source
            SetActiveID(id, window);
            if ((nav_activated_by_code || nav_activated_by_inputs) && !(flags & ImGuiButtonFlags_NoNavFocus))
                SetFocusID(id, window);
        }
    }

    // Process while held
    bool held = false;
    if (g.ActiveId == id)
    {
        if (g.ActiveIdSource == ImGuiInputSource_Mouse)
        {
            if (g.ActiveIdIsJustActivated)
                g.ActiveIdClickOffset = g.IO.MousePos - bb.Min;

            const int mouse_button = g.ActiveIdMouseButton;
            IM_ASSERT(mouse_button >= 0 && mouse_button < ImGuiMouseButton_COUNT);
            if (g.IO.MouseDown[mouse_button])
            {
                held = true;
            }
            else
            {
                bool release_in = hovered && (flags & ImGuiButtonFlags_PressedOnClickRelease) != 0;
                bool release_anywhere = (flags & ImGuiButtonFlags_PressedOnClickReleaseAnywhere) != 0;
                if ((release_in || release_anywhere) && !g.DragDropActive)
                {
                    // Report as pressed when releasing the mouse (this is the most common path)
                    bool is_double_click_release = (flags & ImGuiButtonFlags_PressedOnDoubleClick) && g.IO.MouseDownWasDoubleClick[mouse_button];
                    bool is_repeating_already = (flags & ImGuiButtonFlags_Repeat) && g.IO.MouseDownDurationPrev[mouse_button] >= g.IO.KeyRepeatDelay; // Repeat mode trumps <on release>
                    if (!is_double_click_release && !is_repeating_already)
                        pressed = true;
                }
                ClearActiveID();
            }
            if (!(flags & ImGuiButtonFlags_NoNavFocus))
                g.NavDisableHighlight = true;
        }
        else if (g.ActiveIdSource == ImGuiInputSource_Nav)
        {
            // When activated using Nav, we hold on the ActiveID until activation button is released
            if (g.NavActivateDownId != id)
                ClearActiveID();
        }
        if (pressed)
            g.ActiveIdHasBeenPressedBefore = true;
    }

    if (out_hovered) *out_hovered = hovered;
    if (out_held) *out_held = held;

    return pressed;
}

bool ImGui::ButtonEx(const char* label, const ImVec2& size_arg, ImGuiButtonFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
        pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    if (window->DC.ItemFlags & ImGuiItemFlags_ButtonRepeat)
        flags |= ImGuiButtonFlags_Repeat;
    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    RenderNavHighlight(bb, id);
    RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);
    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);

    // Automatically close popups
    //if (pressed && !(flags & ImGuiButtonFlags_DontClosePopups) && (window->Flags & ImGuiWindowFlags_Popup))
    //    CloseCurrentPopup();

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
    return pressed;
}

bool ImGui::Button(const char* label, const ImVec2& size_arg)
{
    return ButtonEx(label, size_arg, ImGuiButtonFlags_None);
}

// Small buttons fits within text without additional vertical spacing.
bool ImGui::SmallButton(const char* label)
{
    ImGuiContext& g = *GImGui;
    float backup_padding_y = g.Style.FramePadding.y;
    g.Style.FramePadding.y = 0.0f;
    bool pressed = ButtonEx(label, ImVec2(0, 0), ImGuiButtonFlags_AlignTextBaseLine);
    g.Style.FramePadding.y = backup_padding_y;
    return pressed;
}

// Tip: use ImGui::PushID()/PopID() to push indices or pointers in the ID stack.
// Then you can keep 'str_id' empty or the same for all your buttons (instead of creating a string based on a non-string id)
bool ImGui::InvisibleButton(const char* str_id, const ImVec2& size_arg, ImGuiButtonFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    // Cannot use zero-size for InvisibleButton(). Unlike Button() there is not way to fallback using the label size.
    IM_ASSERT(size_arg.x != 0.0f && size_arg.y != 0.0f);

    const ImGuiID id = window->GetID(str_id);
    ImVec2 size = CalcItemSize(size_arg, 0.0f, 0.0f);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ItemSize(size);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    return pressed;
}

bool ImGui::ArrowButtonEx(const char* str_id, ImGuiDir dir, ImVec2 size, ImGuiButtonFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(str_id);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    const float default_size = GetFrameHeight();
    ItemSize(size, (size.y >= default_size) ? g.Style.FramePadding.y : -1.0f);
    if (!ItemAdd(bb, id))
        return false;

    if (window->DC.ItemFlags & ImGuiItemFlags_ButtonRepeat)
        flags |= ImGuiButtonFlags_Repeat;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    const ImU32 text_col = GetColorU32(ImGuiCol_Text);
    RenderNavHighlight(bb, id);
    RenderFrame(bb.Min, bb.Max, bg_col, true, g.Style.FrameRounding);
    RenderArrow(window->DrawList, bb.Min + ImVec2(ImMax(0.0f, (size.x - g.FontSize) * 0.5f), ImMax(0.0f, (size.y - g.FontSize) * 0.5f)), text_col, dir);

    return pressed;
}

bool ImGui::ArrowButton(const char* str_id, ImGuiDir dir)
{
    float sz = GetFrameHeight();
    return ArrowButtonEx(str_id, dir, ImVec2(sz, sz), ImGuiButtonFlags_None);
}

// Button to close a window
bool ImGui::CloseButton(ImGuiID id, const ImVec2& pos)//, float size)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    // We intentionally allow interaction when clipped so that a mechanical Alt,Right,Validate sequence close a window.
    // (this isn't the regular behavior of buttons, but it doesn't affect the user much because navigation tends to keep items visible).
    const ImRect bb(pos, pos + ImVec2(g.FontSize, g.FontSize) + g.Style.FramePadding * 2.0f);
    bool is_clipped = !ItemAdd(bb, id);

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    if (is_clipped)
        return pressed;

    // Render
    ImU32 col = GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);
    ImVec2 center = bb.GetCenter();
    if (hovered)
        window->DrawList->AddCircleFilled(center, ImMax(2.0f, g.FontSize * 0.5f + 1.0f), col, 12);

    float cross_extent = g.FontSize * 0.5f * 0.7071f - 1.0f;
    ImU32 cross_col = GetColorU32(ImGuiCol_Text);
    center -= ImVec2(0.5f, 0.5f);
    window->DrawList->AddLine(center + ImVec2(+cross_extent, +cross_extent), center + ImVec2(-cross_extent, -cross_extent), cross_col, 1.0f);
    window->DrawList->AddLine(center + ImVec2(+cross_extent, -cross_extent), center + ImVec2(-cross_extent, +cross_extent), cross_col, 1.0f);

    return pressed;
}

// The Collapse button also functions as a Dock Menu button.
bool ImGui::CollapseButton(ImGuiID id, const ImVec2& pos, ImGuiDockNode* dock_node)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    ImRect bb(pos, pos + ImVec2(g.FontSize, g.FontSize) + g.Style.FramePadding * 2.0f);
    ItemAdd(bb, id);
    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);

    // Render
    //bool is_dock_menu = (window->DockNodeAsHost && !window->Collapsed);
    ImVec2 off = dock_node ? ImVec2(IM_FLOOR(-g.Style.ItemInnerSpacing.x * 0.5f) + 0.5f, 0.0f) : ImVec2(0.0f, 0.0f);
    ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    ImU32 text_col = GetColorU32(ImGuiCol_Text);
    ImVec2 center = bb.GetCenter();
    if (hovered || held)
        window->DrawList->AddCircleFilled(center + off + ImVec2(0,-0.5f), g.FontSize * 0.5f + 1.0f, bg_col, 12);

    if (dock_node)
        RenderArrowDockMenu(window->DrawList, bb.Min + g.Style.FramePadding, g.FontSize, text_col);
    else
        RenderArrow(window->DrawList, bb.Min + g.Style.FramePadding, text_col, window->Collapsed ? ImGuiDir_Right : ImGuiDir_Down, 1.0f);

    // Switch to moving the window after mouse is moved beyond the initial drag threshold
    if (IsItemActive() && IsMouseDragging(0))
        StartMouseMovingWindowOrNode(window, dock_node, true);

    return pressed;
}

ImGuiID ImGui::GetWindowScrollbarID(ImGuiWindow* window, ImGuiAxis axis)
{
    return window->GetIDNoKeepAlive(axis == ImGuiAxis_X ? "#SCROLLX" : "#SCROLLY");
}

// Return scrollbar rectangle, must only be called for corresponding axis if window->ScrollbarX/Y is set.
ImRect ImGui::GetWindowScrollbarRect(ImGuiWindow* window, ImGuiAxis axis)
{
    const ImRect outer_rect = window->Rect();
    const ImRect inner_rect = window->InnerRect;
    const float border_size = window->WindowBorderSize;
    const float scrollbar_size = window->ScrollbarSizes[axis ^ 1]; // (ScrollbarSizes.x = width of Y scrollbar; ScrollbarSizes.y = height of X scrollbar)
    IM_ASSERT(scrollbar_size > 0.0f);
    if (axis == ImGuiAxis_X)
        return ImRect(inner_rect.Min.x, ImMax(outer_rect.Min.y, outer_rect.Max.y - border_size - scrollbar_size), inner_rect.Max.x, outer_rect.Max.y);
    else
        return ImRect(ImMax(outer_rect.Min.x, outer_rect.Max.x - border_size - scrollbar_size), inner_rect.Min.y, outer_rect.Max.x, inner_rect.Max.y);
}

void ImGui::Scrollbar(ImGuiAxis axis)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    const ImGuiID id = GetWindowScrollbarID(window, axis);
    KeepAliveID(id);

    // Calculate scrollbar bounding box
    ImRect bb = GetWindowScrollbarRect(window, axis);
    ImDrawCornerFlags rounding_corners = 0;
    if (axis == ImGuiAxis_X)
    {
        rounding_corners |= ImDrawCornerFlags_BotLeft;
        if (!window->ScrollbarY)
            rounding_corners |= ImDrawCornerFlags_BotRight;
    }
    else
    {
        if ((window->Flags & ImGuiWindowFlags_NoTitleBar) && !(window->Flags & ImGuiWindowFlags_MenuBar))
            rounding_corners |= ImDrawCornerFlags_TopRight;
        if (!window->ScrollbarX)
            rounding_corners |= ImDrawCornerFlags_BotRight;
    }
    float size_avail = window->InnerRect.Max[axis] - window->InnerRect.Min[axis];
    float size_contents = window->ContentSize[axis] + window->WindowPadding[axis] * 2.0f;
    ScrollbarEx(bb, id, axis, &window->Scroll[axis], size_avail, size_contents, rounding_corners);
}

// Vertical/Horizontal scrollbar
// The entire piece of code below is rather confusing because:
// - We handle absolute seeking (when first clicking outside the grab) and relative manipulation (afterward or when clicking inside the grab)
// - We store values as normalized ratio and in a form that allows the window content to change while we are holding on a scrollbar
// - We handle both horizontal and vertical scrollbars, which makes the terminology not ideal.
// Still, the code should probably be made simpler..
bool ImGui::ScrollbarEx(const ImRect& bb_frame, ImGuiID id, ImGuiAxis axis, float* p_scroll_v, float size_avail_v, float size_contents_v, ImDrawCornerFlags rounding_corners)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    const float bb_frame_width = bb_frame.GetWidth();
    const float bb_frame_height = bb_frame.GetHeight();
    if (bb_frame_width <= 0.0f || bb_frame_height <= 0.0f)
        return false;

    // When we are too small, start hiding and disabling the grab (this reduce visual noise on very small window and facilitate using the window resize grab)
    float alpha = 1.0f;
    if ((axis == ImGuiAxis_Y) && bb_frame_height < g.FontSize + g.Style.FramePadding.y * 2.0f)
        alpha = ImSaturate((bb_frame_height - g.FontSize) / (g.Style.FramePadding.y * 2.0f));
    if (alpha <= 0.0f)
        return false;

    const ImGuiStyle& style = g.Style;
    const bool allow_interaction = (alpha >= 1.0f);

    ImRect bb = bb_frame;
    bb.Expand(ImVec2(-ImClamp(IM_FLOOR((bb_frame_width - 2.0f) * 0.5f), 0.0f, 3.0f), -ImClamp(IM_FLOOR((bb_frame_height - 2.0f) * 0.5f), 0.0f, 3.0f)));

    // V denote the main, longer axis of the scrollbar (= height for a vertical scrollbar)
    const float scrollbar_size_v = (axis == ImGuiAxis_X) ? bb.GetWidth() : bb.GetHeight();

    // Calculate the height of our grabbable box. It generally represent the amount visible (vs the total scrollable amount)
    // But we maintain a minimum size in pixel to allow for the user to still aim inside.
    IM_ASSERT(ImMax(size_contents_v, size_avail_v) > 0.0f); // Adding this assert to check if the ImMax(XXX,1.0f) is still needed. PLEASE CONTACT ME if this triggers.
    const float win_size_v = ImMax(ImMax(size_contents_v, size_avail_v), 1.0f);
    const float grab_h_pixels = ImClamp(scrollbar_size_v * (size_avail_v / win_size_v), style.GrabMinSize, scrollbar_size_v);
    const float grab_h_norm = grab_h_pixels / scrollbar_size_v;

    // Handle input right away. None of the code of Begin() is relying on scrolling position before calling Scrollbar().
    bool held = false;
    bool hovered = false;
    ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_NoNavFocus);

    float scroll_max = ImMax(1.0f, size_contents_v - size_avail_v);
    float scroll_ratio = ImSaturate(*p_scroll_v / scroll_max);
    float grab_v_norm = scroll_ratio * (scrollbar_size_v - grab_h_pixels) / scrollbar_size_v; // Grab position in normalized space
    if (held && allow_interaction && grab_h_norm < 1.0f)
    {
        float scrollbar_pos_v = bb.Min[axis];
        float mouse_pos_v = g.IO.MousePos[axis];

        // Click position in scrollbar normalized space (0.0f->1.0f)
        const float clicked_v_norm = ImSaturate((mouse_pos_v - scrollbar_pos_v) / scrollbar_size_v);
        SetHoveredID(id);

        bool seek_absolute = false;
        if (g.ActiveIdIsJustActivated)
        {
            // On initial click calculate the distance between mouse and the center of the grab
            seek_absolute = (clicked_v_norm < grab_v_norm || clicked_v_norm > grab_v_norm + grab_h_norm);
            if (seek_absolute)
                g.ScrollbarClickDeltaToGrabCenter = 0.0f;
            else
                g.ScrollbarClickDeltaToGrabCenter = clicked_v_norm - grab_v_norm - grab_h_norm * 0.5f;
        }

        // Apply scroll (p_scroll_v will generally point on one member of window->Scroll)
        // It is ok to modify Scroll here because we are being called in Begin() after the calculation of ContentSize and before setting up our starting position
        const float scroll_v_norm = ImSaturate((clicked_v_norm - g.ScrollbarClickDeltaToGrabCenter - grab_h_norm * 0.5f) / (1.0f - grab_h_norm));
        *p_scroll_v = IM_ROUND(scroll_v_norm * scroll_max);//(win_size_contents_v - win_size_v));

        // Update values for rendering
        scroll_ratio = ImSaturate(*p_scroll_v / scroll_max);
        grab_v_norm = scroll_ratio * (scrollbar_size_v - grab_h_pixels) / scrollbar_size_v;

        // Update distance to grab now that we have seeked and saturated
        if (seek_absolute)
            g.ScrollbarClickDeltaToGrabCenter = clicked_v_norm - grab_v_norm - grab_h_norm * 0.5f;
    }

    // Render
    const ImU32 bg_col = GetColorU32(ImGuiCol_ScrollbarBg);
    const ImU32 grab_col = GetColorU32(held ? ImGuiCol_ScrollbarGrabActive : hovered ? ImGuiCol_ScrollbarGrabHovered : ImGuiCol_ScrollbarGrab, alpha);
    window->DrawList->AddRectFilled(bb_frame.Min, bb_frame.Max, bg_col, window->WindowRounding, rounding_corners);
    ImRect grab_rect;
    if (axis == ImGuiAxis_X)
        grab_rect = ImRect(ImLerp(bb.Min.x, bb.Max.x, grab_v_norm), bb.Min.y, ImLerp(bb.Min.x, bb.Max.x, grab_v_norm) + grab_h_pixels, bb.Max.y);
    else
        grab_rect = ImRect(bb.Min.x, ImLerp(bb.Min.y, bb.Max.y, grab_v_norm), bb.Max.x, ImLerp(bb.Min.y, bb.Max.y, grab_v_norm) + grab_h_pixels);
    window->DrawList->AddRectFilled(grab_rect.Min, grab_rect.Max, grab_col, style.ScrollbarRounding);

    return held;
}

void ImGui::Image(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tint_col, const ImVec4& border_col)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    if (border_col.w > 0.0f)
        bb.Max += ImVec2(2, 2);
    ItemSize(bb);
    if (!ItemAdd(bb, 0))
        return;

    if (border_col.w > 0.0f)
    {
        window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(border_col), 0.0f);
        window->DrawList->AddImage(user_texture_id, bb.Min + ImVec2(1, 1), bb.Max - ImVec2(1, 1), uv0, uv1, GetColorU32(tint_col));
    }
    else
    {
        window->DrawList->AddImage(user_texture_id, bb.Min, bb.Max, uv0, uv1, GetColorU32(tint_col));
    }
}

// ImageButton() is flawed as 'id' is always derived from 'texture_id' (see #2464 #1390)
// We provide this internal helper to write your own variant while we figure out how to redesign the public ImageButton() API.
bool ImGui::ImageButtonEx(ImGuiID id, ImTextureID texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, const ImVec2& padding, const ImVec4& bg_col, const ImVec4& tint_col)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size + padding * 2);
    ItemSize(bb);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    // Render
    const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    RenderNavHighlight(bb, id);
    RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, g.Style.FrameRounding));
    if (bg_col.w > 0.0f)
        window->DrawList->AddRectFilled(bb.Min + padding, bb.Max - padding, GetColorU32(bg_col));
    window->DrawList->AddImage(texture_id, bb.Min + padding, bb.Max - padding, uv0, uv1, GetColorU32(tint_col));

    return pressed;
}

// frame_padding < 0: uses FramePadding from style (default)
// frame_padding = 0: no framing
// frame_padding > 0: set framing size
bool ImGui::ImageButton(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1, int frame_padding, const ImVec4& bg_col, const ImVec4& tint_col)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    // Default to using texture ID as ID. User can still push string/integer prefixes.
    PushID((void*)(intptr_t)user_texture_id);
    const ImGuiID id = window->GetID("#image");
    PopID();

    const ImVec2 padding = (frame_padding >= 0) ? ImVec2((float)frame_padding, (float)frame_padding) : g.Style.FramePadding;
    return ImageButtonEx(id, user_texture_id, size, uv0, uv1, padding, bg_col, tint_col);
}

bool ImGui::Checkbox(const char* label, bool* v)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    const float square_sz = GetFrameHeight();
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
    {
        *v = !(*v);
        MarkItemEdited(id);
    }

    const ImRect check_bb(pos, pos + ImVec2(square_sz, square_sz));
    RenderNavHighlight(total_bb, id);
    RenderFrame(check_bb.Min, check_bb.Max, GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), true, style.FrameRounding);
    ImU32 check_col = GetColorU32(ImGuiCol_CheckMark);
    bool mixed_value = (window->DC.ItemFlags & ImGuiItemFlags_MixedValue) != 0;
    if (mixed_value)
    {
        // Undocumented tristate/mixed/indeterminate checkbox (#2644)
        // This may seem awkwardly designed because the aim is to make ImGuiItemFlags_MixedValue supported by all widgets (not just checkbox)
        ImVec2 pad(ImMax(1.0f, IM_FLOOR(square_sz / 3.6f)), ImMax(1.0f, IM_FLOOR(square_sz / 3.6f)));
        window->DrawList->AddRectFilled(check_bb.Min + pad, check_bb.Max - pad, check_col, style.FrameRounding);
    }
    else if (*v)
    {
        const float pad = ImMax(1.0f, IM_FLOOR(square_sz / 6.0f));
        RenderCheckMark(window->DrawList, check_bb.Min + ImVec2(pad, pad), check_col, square_sz - pad * 2.0f);
    }

    if (g.LogEnabled)
        LogRenderedText(&total_bb.Min, mixed_value ? "[~]" : *v ? "[x]" : "[ ]");
    if (label_size.x > 0.0f)
        RenderText(ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y), label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
    return pressed;
}

template<typename T>
bool ImGui::CheckboxFlagsT(const char* label, T* flags, T flags_value)
{
    bool all_on = (*flags & flags_value) == flags_value;
    bool any_on = (*flags & flags_value) != 0;
    bool pressed;
    if (!all_on && any_on)
    {
        ImGuiWindow* window = GetCurrentWindow();
        ImGuiItemFlags backup_item_flags = window->DC.ItemFlags;
        window->DC.ItemFlags |= ImGuiItemFlags_MixedValue;
        pressed = Checkbox(label, &all_on);
        window->DC.ItemFlags = backup_item_flags;
    }
    else
    {
        pressed = Checkbox(label, &all_on);

    }
    if (pressed)
    {
        if (all_on)
            *flags |= flags_value;
        else
            *flags &= ~flags_value;
    }
    return pressed;
}

bool ImGui::CheckboxFlags(const char* label, int* flags, int flags_value)
{
    return CheckboxFlagsT(label, flags, flags_value);
}

bool ImGui::CheckboxFlags(const char* label, unsigned int* flags, unsigned int flags_value)
{
    return CheckboxFlagsT(label, flags, flags_value);
}

bool ImGui::RadioButton(const char* label, bool active)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    const float square_sz = GetFrameHeight();
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect check_bb(pos, pos + ImVec2(square_sz, square_sz));
    const ImRect total_bb(pos, pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id))
        return false;

    ImVec2 center = check_bb.GetCenter();
    center.x = IM_ROUND(center.x);
    center.y = IM_ROUND(center.y);
    const float radius = (square_sz - 1.0f) * 0.5f;

    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
        MarkItemEdited(id);

    RenderNavHighlight(total_bb, id);
    window->DrawList->AddCircleFilled(center, radius, GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), 16);
    if (active)
    {
        const float pad = ImMax(1.0f, IM_FLOOR(square_sz / 6.0f));
        window->DrawList->AddCircleFilled(center, radius - pad, GetColorU32(ImGuiCol_CheckMark), 16);
    }

    if (style.FrameBorderSize > 0.0f)
    {
        window->DrawList->AddCircle(center + ImVec2(1, 1), radius, GetColorU32(ImGuiCol_BorderShadow), 16, style.FrameBorderSize);
        window->DrawList->AddCircle(center, radius, GetColorU32(ImGuiCol_Border), 16, style.FrameBorderSize);
    }

    if (g.LogEnabled)
        LogRenderedText(&total_bb.Min, active ? "(x)" : "( )");
    if (label_size.x > 0.0f)
        RenderText(ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y), label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags);
    return pressed;
}

// FIXME: This would work nicely if it was a public template, e.g. 'template<T> RadioButton(const char* label, T* v, T v_button)', but I'm not sure how we would expose it..
bool ImGui::RadioButton(const char* label, int* v, int v_button)
{
    const bool pressed = RadioButton(label, *v == v_button);
    if (pressed)
        *v = v_button;
    return pressed;
}

// size_arg (for each axis) < 0.0f: align to end, 0.0f: auto, > 0.0f: specified size
void ImGui::ProgressBar(float fraction, const ImVec2& size_arg, const char* overlay)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), g.FontSize + style.FramePadding.y * 2.0f);
    ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, 0))
        return;

    // Render
    fraction = ImSaturate(fraction);
    RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    bb.Expand(ImVec2(-style.FrameBorderSize, -style.FrameBorderSize));
    const ImVec2 fill_br = ImVec2(ImLerp(bb.Min.x, bb.Max.x, fraction), bb.Max.y);
    RenderRectFilledRangeH(window->DrawList, bb, GetColorU32(ImGuiCol_PlotHistogram), 0.0f, fraction, style.FrameRounding);

    // Default displaying the fraction as percentage string, but user can override it
    char overlay_buf[32];
    if (!overlay)
    {
        ImFormatString(overlay_buf, IM_ARRAYSIZE(overlay_buf), "%.0f%%", fraction * 100 + 0.01f);
        overlay = overlay_buf;
    }

    ImVec2 overlay_size = CalcTextSize(overlay, NULL);
    if (overlay_size.x > 0.0f)
        RenderTextClipped(ImVec2(ImClamp(fill_br.x + style.ItemSpacing.x, bb.Min.x, bb.Max.x - overlay_size.x - style.ItemInnerSpacing.x), bb.Min.y), bb.Max, overlay, NULL, &overlay_size, ImVec2(0.0f, 0.5f), &bb);
}

void ImGui::Bullet()
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float line_height = ImMax(ImMin(window->DC.CurrLineSize.y, g.FontSize + g.Style.FramePadding.y * 2), g.FontSize);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(g.FontSize, line_height));
    ItemSize(bb);
    if (!ItemAdd(bb, 0))
    {
        SameLine(0, style.FramePadding.x * 2);
        return;
    }

    // Render and stay on same line
    ImU32 text_col = GetColorU32(ImGuiCol_Text);
    RenderBullet(window->DrawList, bb.Min + ImVec2(style.FramePadding.x + g.FontSize * 0.5f, line_height * 0.5f), text_col);
    SameLine(0, style.FramePadding.x * 2.0f);
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Low-level Layout helpers
//-------------------------------------------------------------------------
// - Spacing()
// - Dummy()
// - NewLine()
// - AlignTextToFramePadding()
// - SeparatorEx() [Internal]
// - Separator()
// - SplitterBehavior() [Internal]
// - ShrinkWidths() [Internal]
//-------------------------------------------------------------------------

void ImGui::Spacing()
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;
    ItemSize(ImVec2(0, 0));
}

void ImGui::Dummy(const ImVec2& size)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ItemSize(size);
    ItemAdd(bb, 0);
}

void ImGui::NewLine()
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiLayoutType backup_layout_type = window->DC.LayoutType;
    window->DC.LayoutType = ImGuiLayoutType_Vertical;
    if (window->DC.CurrLineSize.y > 0.0f)     // In the event that we are on a line with items that is smaller that FontSize high, we will preserve its height.
        ItemSize(ImVec2(0, 0));
    else
        ItemSize(ImVec2(0.0f, g.FontSize));
    window->DC.LayoutType = backup_layout_type;
}

void ImGui::AlignTextToFramePadding()
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    window->DC.CurrLineSize.y = ImMax(window->DC.CurrLineSize.y, g.FontSize + g.Style.FramePadding.y * 2);
    window->DC.CurrLineTextBaseOffset = ImMax(window->DC.CurrLineTextBaseOffset, g.Style.FramePadding.y);
}

// Horizontal/vertical separating line
void ImGui::SeparatorEx(ImGuiSeparatorFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    IM_ASSERT(ImIsPowerOfTwo(flags & (ImGuiSeparatorFlags_Horizontal | ImGuiSeparatorFlags_Vertical)));   // Check that only 1 option is selected

    float thickness_draw = 1.0f;
    float thickness_layout = 0.0f;
    if (flags & ImGuiSeparatorFlags_Vertical)
    {
        // Vertical separator, for menu bars (use current line height). Not exposed because it is misleading and it doesn't have an effect on regular layout.
        float y1 = window->DC.CursorPos.y;
        float y2 = window->DC.CursorPos.y + window->DC.CurrLineSize.y;
        const ImRect bb(ImVec2(window->DC.CursorPos.x, y1), ImVec2(window->DC.CursorPos.x + thickness_draw, y2));
        ItemSize(ImVec2(thickness_layout, 0.0f));
        if (!ItemAdd(bb, 0))
            return;

        // Draw
        window->DrawList->AddLine(ImVec2(bb.Min.x, bb.Min.y), ImVec2(bb.Min.x, bb.Max.y), GetColorU32(ImGuiCol_Separator));
        if (g.LogEnabled)
            LogText(" |");
    }
    else if (flags & ImGuiSeparatorFlags_Horizontal)
    {
        // Horizontal Separator
        float x1 = window->Pos.x;
        float x2 = window->Pos.x + window->Size.x;

        // FIXME-WORKRECT: old hack (#205) until we decide of consistent behavior with WorkRect/Indent and Separator
        if (g.GroupStack.Size > 0 && g.GroupStack.back().WindowID == window->ID)
            x1 += window->DC.Indent.x;

        ImGuiColumns* columns = (flags & ImGuiSeparatorFlags_SpanAllColumns) ? window->DC.CurrentColumns : NULL;
        if (columns)
            PushColumnsBackground();

        // We don't provide our width to the layout so that it doesn't get feed back into AutoFit
        const ImRect bb(ImVec2(x1, window->DC.CursorPos.y), ImVec2(x2, window->DC.CursorPos.y + thickness_draw));
        ItemSize(ImVec2(0.0f, thickness_layout));
        const bool item_visible = ItemAdd(bb, 0);
        if (item_visible)
        {
            // Draw
            window->DrawList->AddLine(bb.Min, ImVec2(bb.Max.x, bb.Min.y), GetColorU32(ImGuiCol_Separator));
            if (g.LogEnabled)
                LogRenderedText(&bb.Min, "--------------------------------");
        }
        if (columns)
        {
            PopColumnsBackground();
            columns->LineMinY = window->DC.CursorPos.y;
        }
    }
}

void ImGui::Separator()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    // Those flags should eventually be overridable by the user
    ImGuiSeparatorFlags flags = (window->DC.LayoutType == ImGuiLayoutType_Horizontal) ? ImGuiSeparatorFlags_Vertical : ImGuiSeparatorFlags_Horizontal;
    flags |= ImGuiSeparatorFlags_SpanAllColumns;
    SeparatorEx(flags);
}

// Using 'hover_visibility_delay' allows us to hide the highlight and mouse cursor for a short time, which can be convenient to reduce visual noise.
bool ImGui::SplitterBehavior(const ImRect& bb, ImGuiID id, ImGuiAxis axis, float* size1, float* size2, float min_size1, float min_size2, float hover_extend, float hover_visibility_delay)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    const ImGuiItemFlags item_flags_backup = window->DC.ItemFlags;
    window->DC.ItemFlags |= ImGuiItemFlags_NoNav | ImGuiItemFlags_NoNavDefaultFocus;
    bool item_add = ItemAdd(bb, id);
    window->DC.ItemFlags = item_flags_backup;
    if (!item_add)
        return false;

    bool hovered, held;
    ImRect bb_interact = bb;
    bb_interact.Expand(axis == ImGuiAxis_Y ? ImVec2(0.0f, hover_extend) : ImVec2(hover_extend, 0.0f));
    ButtonBehavior(bb_interact, id, &hovered, &held, ImGuiButtonFlags_FlattenChildren | ImGuiButtonFlags_AllowItemOverlap);
    if (g.ActiveId != id)
        SetItemAllowOverlap();

    if (held || (g.HoveredId == id && g.HoveredIdPreviousFrame == id && g.HoveredIdTimer >= hover_visibility_delay))
        SetMouseCursor(axis == ImGuiAxis_Y ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW);

    ImRect bb_render = bb;
    if (held)
    {
        ImVec2 mouse_delta_2d = g.IO.MousePos - g.ActiveIdClickOffset - bb_interact.Min;
        float mouse_delta = (axis == ImGuiAxis_Y) ? mouse_delta_2d.y : mouse_delta_2d.x;

        // Minimum pane size
        float size_1_maximum_delta = ImMax(0.0f, *size1 - min_size1);
        float size_2_maximum_delta = ImMax(0.0f, *size2 - min_size2);
        if (mouse_delta < -size_1_maximum_delta)
            mouse_delta = -size_1_maximum_delta;
        if (mouse_delta > size_2_maximum_delta)
            mouse_delta = size_2_maximum_delta;

        // Apply resize
        if (mouse_delta != 0.0f)
        {
            if (mouse_delta < 0.0f)
                IM_ASSERT(*size1 + mouse_delta >= min_size1);
            if (mouse_delta > 0.0f)
                IM_ASSERT(*size2 - mouse_delta >= min_size2);
            *size1 += mouse_delta;
            *size2 -= mouse_delta;
            bb_render.Translate((axis == ImGuiAxis_X) ? ImVec2(mouse_delta, 0.0f) : ImVec2(0.0f, mouse_delta));
            MarkItemEdited(id);
        }
    }

    // Render
    const ImU32 col = GetColorU32(held ? ImGuiCol_SeparatorActive : (hovered && g.HoveredIdTimer >= hover_visibility_delay) ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
    window->DrawList->AddRectFilled(bb_render.Min, bb_render.Max, col, 0.0f);

    return held;
}

static int IMGUI_CDECL ShrinkWidthItemComparer(const void* lhs, const void* rhs)
{
    const ImGuiShrinkWidthItem* a = (const ImGuiShrinkWidthItem*)lhs;
    const ImGuiShrinkWidthItem* b = (const ImGuiShrinkWidthItem*)rhs;
    if (int d = (int)(b->Width - a->Width))
        return d;
    return (b->Index - a->Index);
}

// Shrink excess width from a set of item, by removing width from the larger items first.
// Set items Width to -1.0f to disable shrinking this item.
void ImGui::ShrinkWidths(ImGuiShrinkWidthItem* items, int count, float width_excess)
{
    if (count == 1)
    {
        if (items[0].Width >= 0.0f)
            items[0].Width = ImMax(items[0].Width - width_excess, 1.0f);
        return;
    }
    ImQsort(items, (size_t)count, sizeof(ImGuiShrinkWidthItem), ShrinkWidthItemComparer);
    int count_same_width = 1;
    while (width_excess > 0.0f && count_same_width < count)
    {
        while (count_same_width < count && items[0].Width <= items[count_same_width].Width)
            count_same_width++;
        float max_width_to_remove_per_item = (count_same_width < count && items[count_same_width].Width >= 0.0f) ? (items[0].Width - items[count_same_width].Width) : (items[0].Width - 1.0f);
        if (max_width_to_remove_per_item <= 0.0f)
            break;
        float width_to_remove_per_item = ImMin(width_excess / count_same_width, max_width_to_remove_per_item);
        for (int item_n = 0; item_n < count_same_width; item_n++)
            items[item_n].Width -= width_to_remove_per_item;
        width_excess -= width_to_remove_per_item * count_same_width;
    }

    // Round width and redistribute remainder left-to-right (could make it an option of the function?)
    // Ensure that e.g. the right-most tab of a shrunk tab-bar always reaches exactly at the same distance from the right-most edge of the tab bar separator.
    width_excess = 0.0f;
    for (int n = 0; n < count; n++)
    {
        float width_rounded = ImFloor(items[n].Width);
        width_excess += items[n].Width - width_rounded;
        items[n].Width = width_rounded;
    }
    if (width_excess > 0.0f)
        for (int n = 0; n < count; n++)
            if (items[n].Index < (int)(width_excess + 0.01f))
                items[n].Width += 1.0f;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: ComboBox
//-------------------------------------------------------------------------
// - BeginCombo()
// - EndCombo()
// - Combo()
//-------------------------------------------------------------------------

static float CalcMaxPopupHeightFromItemCount(int items_count)
{
    ImGuiContext& g = *GImGui;
    if (items_count <= 0)
        return FLT_MAX;
    return (g.FontSize + g.Style.ItemSpacing.y) * items_count - g.Style.ItemSpacing.y + (g.Style.WindowPadding.y * 2);
}

bool ImGui::BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags)
{
    // Always consume the SetNextWindowSizeConstraint() call in our early return paths
    ImGuiContext& g = *GImGui;
    bool has_window_size_constraint = (g.NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint) != 0;
    g.NextWindowData.Flags &= ~ImGuiNextWindowDataFlags_HasSizeConstraint;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) != (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together

    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float arrow_size = (flags & ImGuiComboFlags_NoArrowButton) ? 0.0f : GetFrameHeight();
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const float expected_w = CalcItemWidth();
    const float w = (flags & ImGuiComboFlags_NoPreview) ? arrow_size : expected_w;
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(frame_bb, id, &hovered, &held);
    bool popup_open = IsPopupOpen(id, ImGuiPopupFlags_None);

    const ImU32 frame_col = GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    const float value_x2 = ImMax(frame_bb.Min.x, frame_bb.Max.x - arrow_size);
    RenderNavHighlight(frame_bb, id);
    if (!(flags & ImGuiComboFlags_NoPreview))
        window->DrawList->AddRectFilled(frame_bb.Min, ImVec2(value_x2, frame_bb.Max.y), frame_col, style.FrameRounding, (flags & ImGuiComboFlags_NoArrowButton) ? ImDrawCornerFlags_All : ImDrawCornerFlags_Left);
    if (!(flags & ImGuiComboFlags_NoArrowButton))
    {
        ImU32 bg_col = GetColorU32((popup_open || hovered) ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        ImU32 text_col = GetColorU32(ImGuiCol_Text);
        window->DrawList->AddRectFilled(ImVec2(value_x2, frame_bb.Min.y), frame_bb.Max, bg_col, style.FrameRounding, (w <= arrow_size) ? ImDrawCornerFlags_All : ImDrawCornerFlags_Right);
        if (value_x2 + arrow_size - style.FramePadding.x <= frame_bb.Max.x)
            RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, frame_bb.Min.y + style.FramePadding.y), text_col, ImGuiDir_Down, 1.0f);
    }
    RenderFrameBorder(frame_bb.Min, frame_bb.Max, style.FrameRounding);
    if (preview_value != NULL && !(flags & ImGuiComboFlags_NoPreview))
        RenderTextClipped(frame_bb.Min + style.FramePadding, ImVec2(value_x2, frame_bb.Max.y), preview_value, NULL, NULL, ImVec2(0.0f, 0.0f));
    if (label_size.x > 0)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    if ((pressed || g.NavActivateId == id) && !popup_open)
    {
        if (window->DC.NavLayerCurrent == 0)
            window->NavLastIds[0] = id;
        OpenPopupEx(id, ImGuiPopupFlags_None);
        popup_open = true;
    }

    if (!popup_open)
        return false;

    if (has_window_size_constraint)
    {
        g.NextWindowData.Flags |= ImGuiNextWindowDataFlags_HasSizeConstraint;
        g.NextWindowData.SizeConstraintRect.Min.x = ImMax(g.NextWindowData.SizeConstraintRect.Min.x, w);
    }
    else
    {
        if ((flags & ImGuiComboFlags_HeightMask_) == 0)
            flags |= ImGuiComboFlags_HeightRegular;
        IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiComboFlags_HeightMask_));    // Only one
        int popup_max_height_in_items = -1;
        if (flags & ImGuiComboFlags_HeightRegular)     popup_max_height_in_items = 8;
        else if (flags & ImGuiComboFlags_HeightSmall)  popup_max_height_in_items = 4;
        else if (flags & ImGuiComboFlags_HeightLarge)  popup_max_height_in_items = 20;
        SetNextWindowSizeConstraints(ImVec2(w, 0.0f), ImVec2(FLT_MAX, CalcMaxPopupHeightFromItemCount(popup_max_height_in_items)));
    }

    char name[16];
    ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g.BeginPopupStack.Size); // Recycle windows based on depth

    // Peak into expected window size so we can position it
    if (ImGuiWindow* popup_window = FindWindowByName(name))
        if (popup_window->WasActive)
        {
            ImVec2 size_expected = CalcWindowExpectedSize(popup_window);
            if (flags & ImGuiComboFlags_PopupAlignLeft)
                popup_window->AutoPosLastDirection = ImGuiDir_Left;
            ImRect r_outer = GetWindowAllowedExtentRect(popup_window);
            ImVec2 pos = FindBestWindowPosForPopupEx(frame_bb.GetBL(), size_expected, &popup_window->AutoPosLastDirection, r_outer, frame_bb, ImGuiPopupPositionPolicy_ComboBox);
            SetNextWindowPos(pos);
        }

    // We don't use BeginPopupEx() solely because we have a custom name string, which we could make an argument to BeginPopupEx()
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;

    // Horizontally align ourselves with the framed text
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.FramePadding.x, style.WindowPadding.y));
    bool ret = Begin(name, NULL, window_flags);
    PopStyleVar();
    if (!ret)
    {
        EndPopup();
        IM_ASSERT(0);   // This should never happen as we tested for IsPopupOpen() above
        return false;
    }
    return true;
}

void ImGui::EndCombo()
{
    EndPopup();
}

// Getter for the old Combo() API: const char*[]
static bool Items_ArrayGetter(void* data, int idx, const char** out_text)
{
    const char* const* items = (const char* const*)data;
    if (out_text)
        *out_text = items[idx];
    return true;
}

// Getter for the old Combo() API: "item1\0item2\0item3\0"
static bool Items_SingleStringGetter(void* data, int idx, const char** out_text)
{
    // FIXME-OPT: we could pre-compute the indices to fasten this. But only 1 active combo means the waste is limited.
    const char* items_separated_by_zeros = (const char*)data;
    int items_count = 0;
    const char* p = items_separated_by_zeros;
    while (*p)
    {
        if (idx == items_count)
            break;
        p += strlen(p) + 1;
        items_count++;
    }
    if (!*p)
        return false;
    if (out_text)
        *out_text = p;
    return true;
}

// Old API, prefer using BeginCombo() nowadays if you can.
bool ImGui::Combo(const char* label, int* current_item, bool (*items_getter)(void*, int, const char**), void* data, int items_count, int popup_max_height_in_items)
{
    ImGuiContext& g = *GImGui;

    // Call the getter to obtain the preview string which is a parameter to BeginCombo()
    const char* preview_value = NULL;
    if (*current_item >= 0 && *current_item < items_count)
        items_getter(data, *current_item, &preview_value);

    // The old Combo() API exposed "popup_max_height_in_items". The new more general BeginCombo() API doesn't have/need it, but we emulate it here.
    if (popup_max_height_in_items != -1 && !(g.NextWindowData.Flags & ImGuiNextWindowDataFlags_HasSizeConstraint))
        SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, CalcMaxPopupHeightFromItemCount(popup_max_height_in_items)));

    if (!BeginCombo(label, preview_value, ImGuiComboFlags_None))
        return false;

    // Display items
    // FIXME-OPT: Use clipper (but we need to disable it on the appearing frame to make sure our call to SetItemDefaultFocus() is processed)
    bool value_changed = false;
    for (int i = 0; i < items_count; i++)
    {
        PushID((void*)(intptr_t)i);
        const bool item_selected = (i == *current_item);
        const char* item_text;
        if (!items_getter(data, i, &item_text))
            item_text = "*Unknown item*";
        if (Selectable(item_text, item_selected))
        {
            value_changed = true;
            *current_item = i;
        }
        if (item_selected)
            SetItemDefaultFocus();
        PopID();
    }

    EndCombo();
    return value_changed;
}

// Combo box helper allowing to pass an array of strings.
bool ImGui::Combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items)
{
    const bool value_changed = Combo(label, current_item, Items_ArrayGetter, (void*)items, items_count, height_in_items);
    return value_changed;
}

// Combo box helper allowing to pass all items in a single string literal holding multiple zero-terminated items "item1\0item2\0"
bool ImGui::Combo(const char* label, int* current_item, const char* items_separated_by_zeros, int height_in_items)
{
    int items_count = 0;
    const char* p = items_separated_by_zeros;       // FIXME-OPT: Avoid computing this, or at least only when combo is open
    while (*p)
    {
        p += strlen(p) + 1;
        items_count++;
    }
    bool value_changed = Combo(label, current_item, Items_SingleStringGetter, (void*)items_separated_by_zeros, items_count, height_in_items);
    return value_changed;
}

//-------------------------------------------------------------------------
// [SECTION] Data Type and Data Formatting Helpers [Internal]
//-------------------------------------------------------------------------
// - PatchFormatStringFloatToInt()
// - DataTypeGetInfo()
// - DataTypeFormatString()
// - DataTypeApplyOp()
// - DataTypeApplyOpFromText()
// - DataTypeClamp()
// - GetMinimumStepAtDecimalPrecision
// - RoundScalarWithFormat<>()
//-------------------------------------------------------------------------

static const ImGuiDataTypeInfo GDataTypeInfo[] =
{
    { sizeof(char),             "S8",   "%d",   "%d"    },  // ImGuiDataType_S8
    { sizeof(unsigned char),    "U8",   "%u",   "%u"    },
    { sizeof(short),            "S16",  "%d",   "%d"    },  // ImGuiDataType_S16
    { sizeof(unsigned short),   "U16",  "%u",   "%u"    },
    { sizeof(int),              "S32",  "%d",   "%d"    },  // ImGuiDataType_S32
    { sizeof(unsigned int),     "U32",  "%u",   "%u"    },
#ifdef _MSC_VER
    { sizeof(ImS64),            "S64",  "%I64d","%I64d" },  // ImGuiDataType_S64
    { sizeof(ImU64),            "U64",  "%I64u","%I64u" },
#else
    { sizeof(ImS64),            "S64",  "%lld", "%lld"  },  // ImGuiDataType_S64
    { sizeof(ImU64),            "U64",  "%llu", "%llu"  },
#endif
    { sizeof(float),            "float", "%f",  "%f"    },  // ImGuiDataType_Float (float are promoted to double in va_arg)
    { sizeof(double),           "double","%f",  "%lf"   },  // ImGuiDataType_Double
};
IM_STATIC_ASSERT(IM_ARRAYSIZE(GDataTypeInfo) == ImGuiDataType_COUNT);

// FIXME-LEGACY: Prior to 1.61 our DragInt() function internally used floats and because of this the compile-time default value for format was "%.0f".
// Even though we changed the compile-time default, we expect users to have carried %f around, which would break the display of DragInt() calls.
// To honor backward compatibility we are rewriting the format string, unless IMGUI_DISABLE_OBSOLETE_FUNCTIONS is enabled. What could possibly go wrong?!
static const char* PatchFormatStringFloatToInt(const char* fmt)
{
    if (fmt[0] == '%' && fmt[1] == '.' && fmt[2] == '0' && fmt[3] == 'f' && fmt[4] == 0) // Fast legacy path for "%.0f" which is expected to be the most common case.
        return "%d";
    const char* fmt_start = ImParseFormatFindStart(fmt);    // Find % (if any, and ignore %%)
    const char* fmt_end = ImParseFormatFindEnd(fmt_start);  // Find end of format specifier, which itself is an exercise of confidence/recklessness (because snprintf is dependent on libc or user).
    if (fmt_end > fmt_start && fmt_end[-1] == 'f')
    {
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        if (fmt_start == fmt && fmt_end[0] == 0)
            return "%d";
        ImGuiContext& g = *GImGui;
        ImFormatString(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), "%.*s%%d%s", (int)(fmt_start - fmt), fmt, fmt_end); // Honor leading and trailing decorations, but lose alignment/precision.
        return g.TempBuffer;
#else
        IM_ASSERT(0 && "DragInt(): Invalid format string!"); // Old versions used a default parameter of "%.0f", please replace with e.g. "%d"
#endif
    }
    return fmt;
}

const ImGuiDataTypeInfo* ImGui::DataTypeGetInfo(ImGuiDataType data_type)
{
    IM_ASSERT(data_type >= 0 && data_type < ImGuiDataType_COUNT);
    return &GDataTypeInfo[data_type];
}

int ImGui::DataTypeFormatString(char* buf, int buf_size, ImGuiDataType data_type, const void* p_data, const char* format)
{
    // Signedness doesn't matter when pushing integer arguments
    if (data_type == ImGuiDataType_S32 || data_type == ImGuiDataType_U32)
        return ImFormatString(buf, buf_size, format, *(const ImU32*)p_data);
    if (data_type == ImGuiDataType_S64 || data_type == ImGuiDataType_U64)
        return ImFormatString(buf, buf_size, format, *(const ImU64*)p_data);
    if (data_type == ImGuiDataType_Float)
        return ImFormatString(buf, buf_size, format, *(const float*)p_data);
    if (data_type == ImGuiDataType_Double)
        return ImFormatString(buf, buf_size, format, *(const double*)p_data);
    if (data_type == ImGuiDataType_S8)
        return ImFormatString(buf, buf_size, format, *(const ImS8*)p_data);
    if (data_type == ImGuiDataType_U8)
        return ImFormatString(buf, buf_size, format, *(const ImU8*)p_data);
    if (data_type == ImGuiDataType_S16)
        return ImFormatString(buf, buf_size, format, *(const ImS16*)p_data);
    if (data_type == ImGuiDataType_U16)
        return ImFormatString(buf, buf_size, format, *(const ImU16*)p_data);
    IM_ASSERT(0);
    return 0;
}

void ImGui::DataTypeApplyOp(ImGuiDataType data_type, int op, void* output, const void* arg1, const void* arg2)
{
    IM_ASSERT(op == '+' || op == '-');
    switch (data_type)
    {
        case ImGuiDataType_S8:
            if (op == '+') { *(ImS8*)output  = ImAddClampOverflow(*(const ImS8*)arg1,  *(const ImS8*)arg2,  IM_S8_MIN,  IM_S8_MAX); }
            if (op == '-') { *(ImS8*)output  = ImSubClampOverflow(*(const ImS8*)arg1,  *(const ImS8*)arg2,  IM_S8_MIN,  IM_S8_MAX); }
            return;
        case ImGuiDataType_U8:
            if (op == '+') { *(ImU8*)output  = ImAddClampOverflow(*(const ImU8*)arg1,  *(const ImU8*)arg2,  IM_U8_MIN,  IM_U8_MAX); }
            if (op == '-') { *(ImU8*)output  = ImSubClampOverflow(*(const ImU8*)arg1,  *(const ImU8*)arg2,  IM_U8_MIN,  IM_U8_MAX); }
            return;
        case ImGuiDataType_S16:
            if (op == '+') { *(ImS16*)output = ImAddClampOverflow(*(const ImS16*)arg1, *(const ImS16*)arg2, IM_S16_MIN, IM_S16_MAX); }
            if (op == '-') { *(ImS16*)output = ImSubClampOverflow(*(const ImS16*)arg1, *(const ImS16*)arg2, IM_S16_MIN, IM_S16_MAX); }
            return;
        case ImGuiDataType_U16:
            if (op == '+') { *(ImU16*)output = ImAddClampOverflow(*(const ImU16*)arg1, *(const ImU16*)arg2, IM_U16_MIN, IM_U16_MAX); }
            if (op == '-') { *(ImU16*)output = ImSubClampOverflow(*(const ImU16*)arg1, *(const ImU16*)arg2, IM_U16_MIN, IM_U16_MAX); }
            return;
        case ImGuiDataType_S32:
            if (op == '+') { *(ImS32*)output = ImAddClampOverflow(*(const ImS32*)arg1, *(const ImS32*)arg2, IM_S32_MIN, IM_S32_MAX); }
            if (op == '-') { *(ImS32*)output = ImSubClampOverflow(*(const ImS32*)arg1, *(const ImS32*)arg2, IM_S32_MIN, IM_S32_MAX); }
            return;
        case ImGuiDataType_U32:
            if (op == '+') { *(ImU32*)output = ImAddClampOverflow(*(const ImU32*)arg1, *(const ImU32*)arg2, IM_U32_MIN, IM_U32_MAX); }
            if (op == '-') { *(ImU32*)output = ImSubClampOverflow(*(const ImU32*)arg1, *(const ImU32*)arg2, IM_U32_MIN, IM_U32_MAX); }
            return;
        case ImGuiDataType_S64:
            if (op == '+') { *(ImS64*)output = ImAddClampOverflow(*(const ImS64*)arg1, *(const ImS64*)arg2, IM_S64_MIN, IM_S64_MAX); }
            if (op == '-') { *(ImS64*)output = ImSubClampOverflow(*(const ImS64*)arg1, *(const ImS64*)arg2, IM_S64_MIN, IM_S64_MAX); }
            return;
        case ImGuiDataType_U64:
            if (op == '+') { *(ImU64*)output = ImAddClampOverflow(*(const ImU64*)arg1, *(const ImU64*)arg2, IM_U64_MIN, IM_U64_MAX); }
            if (op == '-') { *(ImU64*)output = ImSubClampOverflow(*(const ImU64*)arg1, *(const ImU64*)arg2, IM_U64_MIN, IM_U64_MAX); }
            return;
        case ImGuiDataType_Float:
            if (op == '+') { *(float*)output = *(const float*)arg1 + *(const float*)arg2; }
            if (op == '-') { *(float*)output = *(const float*)arg1 - *(const float*)arg2; }
            return;
        case ImGuiDataType_Double:
            if (op == '+') { *(double*)output = *(const double*)arg1 + *(const double*)arg2; }
            if (op == '-') { *(double*)output = *(const double*)arg1 - *(const double*)arg2; }
            return;
        case ImGuiDataType_COUNT: break;
    }
    IM_ASSERT(0);
}

// User can input math operators (e.g. +100) to edit a numerical values.
// NB: This is _not_ a full expression evaluator. We should probably add one and replace this dumb mess..
bool ImGui::DataTypeApplyOpFromText(const char* buf, const char* initial_value_buf, ImGuiDataType data_type, void* p_data, const char* format)
{
    while (ImCharIsBlankA(*buf))
        buf++;

    // We don't support '-' op because it would conflict with inputing negative value.
    // Instead you can use +-100 to subtract from an existing value
    char op = buf[0];
    if (op == '+' || op == '*' || op == '/')
    {
        buf++;
        while (ImCharIsBlankA(*buf))
            buf++;
    }
    else
    {
        op = 0;
    }
    if (!buf[0])
        return false;

    // Copy the value in an opaque buffer so we can compare at the end of the function if it changed at all.
    const ImGuiDataTypeInfo* type_info = DataTypeGetInfo(data_type);
    ImGuiDataTypeTempStorage data_backup;
    memcpy(&data_backup, p_data, type_info->Size);

    if (format == NULL)
        format = type_info->ScanFmt;

    // FIXME-LEGACY: The aim is to remove those operators and write a proper expression evaluator at some point..
    int arg1i = 0;
    if (data_type == ImGuiDataType_S32)
    {
        int* v = (int*)p_data;
        int arg0i = *v;
        float arg1f = 0.0f;
        if (op && sscanf(initial_value_buf, format, &arg0i) < 1)
            return false;
        // Store operand in a float so we can use fractional value for multipliers (*1.1), but constant always parsed as integer so we can fit big integers (e.g. 2000000003) past float precision
        if (op == '+')      { if (sscanf(buf, "%d", &arg1i)) *v = (int)(arg0i + arg1i); }                   // Add (use "+-" to subtract)
        else if (op == '*') { if (sscanf(buf, "%f", &arg1f)) *v = (int)(arg0i * arg1f); }                   // Multiply
        else if (op == '/') { if (sscanf(buf, "%f", &arg1f) && arg1f != 0.0f) *v = (int)(arg0i / arg1f); }  // Divide
        else                { if (sscanf(buf, format, &arg1i) == 1) *v = arg1i; }                           // Assign constant
    }
    else if (data_type == ImGuiDataType_Float)
    {
        // For floats we have to ignore format with precision (e.g. "%.2f") because sscanf doesn't take them in
        format = "%f";
        float* v = (float*)p_data;
        float arg0f = *v, arg1f = 0.0f;
        if (op && sscanf(initial_value_buf, format, &arg0f) < 1)
            return false;
        if (sscanf(buf, format, &arg1f) < 1)
            return false;
        if (op == '+')      { *v = arg0f + arg1f; }                    // Add (use "+-" to subtract)
        else if (op == '*') { *v = arg0f * arg1f; }                    // Multiply
        else if (op == '/') { if (arg1f != 0.0f) *v = arg0f / arg1f; } // Divide
        else                { *v = arg1f; }                            // Assign constant
    }
    else if (data_type == ImGuiDataType_Double)
    {
        format = "%lf"; // scanf differentiate float/double unlike printf which forces everything to double because of ellipsis
        double* v = (double*)p_data;
        double arg0f = *v, arg1f = 0.0;
        if (op && sscanf(initial_value_buf, format, &arg0f) < 1)
            return false;
        if (sscanf(buf, format, &arg1f) < 1)
            return false;
        if (op == '+')      { *v = arg0f + arg1f; }                    // Add (use "+-" to subtract)
        else if (op == '*') { *v = arg0f * arg1f; }                    // Multiply
        else if (op == '/') { if (arg1f != 0.0f) *v = arg0f / arg1f; } // Divide
        else                { *v = arg1f; }                            // Assign constant
    }
    else if (data_type == ImGuiDataType_U32 || data_type == ImGuiDataType_S64 || data_type == ImGuiDataType_U64)
    {
        // All other types assign constant
        // We don't bother handling support for legacy operators since they are a little too crappy. Instead we will later implement a proper expression evaluator in the future.
        sscanf(buf, format, p_data);
    }
    else
    {
        // Small types need a 32-bit buffer to receive the result from scanf()
        int v32;
        sscanf(buf, format, &v32);
        if (data_type == ImGuiDataType_S8)
            *(ImS8*)p_data = (ImS8)ImClamp(v32, (int)IM_S8_MIN, (int)IM_S8_MAX);
        else if (data_type == ImGuiDataType_U8)
            *(ImU8*)p_data = (ImU8)ImClamp(v32, (int)IM_U8_MIN, (int)IM_U8_MAX);
        else if (data_type == ImGuiDataType_S16)
            *(ImS16*)p_data = (ImS16)ImClamp(v32, (int)IM_S16_MIN, (int)IM_S16_MAX);
        else if (data_type == ImGuiDataType_U16)
            *(ImU16*)p_data = (ImU16)ImClamp(v32, (int)IM_U16_MIN, (int)IM_U16_MAX);
        else
            IM_ASSERT(0);
    }

    return memcmp(&data_backup, p_data, type_info->Size) != 0;
}

template<typename T>
static int DataTypeCompareT(const T* lhs, const T* rhs)
{
    if (*lhs < *rhs) return -1;
    if (*lhs > *rhs) return +1;
    return 0;
}

int ImGui::DataTypeCompare(ImGuiDataType data_type, const void* arg_1, const void* arg_2)
{
    switch (data_type)
    {
    case ImGuiDataType_S8:     return DataTypeCompareT<ImS8  >((const ImS8*  )arg_1, (const ImS8*  )arg_2);
    case ImGuiDataType_U8:     return DataTypeCompareT<ImU8  >((const ImU8*  )arg_1, (const ImU8*  )arg_2);
    case ImGuiDataType_S16:    return DataTypeCompareT<ImS16 >((const ImS16* )arg_1, (const ImS16* )arg_2);
    case ImGuiDataType_U16:    return DataTypeCompareT<ImU16 >((const ImU16* )arg_1, (const ImU16* )arg_2);
    case ImGuiDataType_S32:    return DataTypeCompareT<ImS32 >((const ImS32* )arg_1, (const ImS32* )arg_2);
    case ImGuiDataType_U32:    return DataTypeCompareT<ImU32 >((const ImU32* )arg_1, (const ImU32* )arg_2);
    case ImGuiDataType_S64:    return DataTypeCompareT<ImS64 >((const ImS64* )arg_1, (const ImS64* )arg_2);
    case ImGuiDataType_U64:    return DataTypeCompareT<ImU64 >((const ImU64* )arg_1, (const ImU64* )arg_2);
    case ImGuiDataType_Float:  return DataTypeCompareT<float >((const float* )arg_1, (const float* )arg_2);
    case ImGuiDataType_Double: return DataTypeCompareT<double>((const double*)arg_1, (const double*)arg_2);
    case ImGuiDataType_COUNT:  break;
    }
    IM_ASSERT(0);
    return 0;
}

template<typename T>
static bool DataTypeClampT(T* v, const T* v_min, const T* v_max)
{
    // Clamp, both sides are optional, return true if modified
    if (v_min && *v < *v_min) { *v = *v_min; return true; }
    if (v_max && *v > *v_max) { *v = *v_max; return true; }
    return false;
}

bool ImGui::DataTypeClamp(ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max)
{
    switch (data_type)
    {
    case ImGuiDataType_S8:     return DataTypeClampT<ImS8  >((ImS8*  )p_data, (const ImS8*  )p_min, (const ImS8*  )p_max);
    case ImGuiDataType_U8:     return DataTypeClampT<ImU8  >((ImU8*  )p_data, (const ImU8*  )p_min, (const ImU8*  )p_max);
    case ImGuiDataType_S16:    return DataTypeClampT<ImS16 >((ImS16* )p_data, (const ImS16* )p_min, (const ImS16* )p_max);
    case ImGuiDataType_U16:    return DataTypeClampT<ImU16 >((ImU16* )p_data, (const ImU16* )p_min, (const ImU16* )p_max);
    case ImGuiDataType_S32:    return DataTypeClampT<ImS32 >((ImS32* )p_data, (const ImS32* )p_min, (const ImS32* )p_max);
    case ImGuiDataType_U32:    return DataTypeClampT<ImU32 >((ImU32* )p_data, (const ImU32* )p_min, (const ImU32* )p_max);
    case ImGuiDataType_S64:    return DataTypeClampT<ImS64 >((ImS64* )p_data, (const ImS64* )p_min, (const ImS64* )p_max);
    case ImGuiDataType_U64:    return DataTypeClampT<ImU64 >((ImU64* )p_data, (const ImU64* )p_min, (const ImU64* )p_max);
    case ImGuiDataType_Float:  return DataTypeClampT<float >((float* )p_data, (const float* )p_min, (const float* )p_max);
    case ImGuiDataType_Double: return DataTypeClampT<double>((double*)p_data, (const double*)p_min, (const double*)p_max);
    case ImGuiDataType_COUNT:  break;
    }
    IM_ASSERT(0);
    return false;
}

static float GetMinimumStepAtDecimalPrecision(int decimal_precision)
{
    static const float min_steps[10] = { 1.0f, 0.1f, 0.01f, 0.001f, 0.0001f, 0.00001f, 0.000001f, 0.0000001f, 0.00000001f, 0.000000001f };
    if (decimal_precision < 0)
        return FLT_MIN;
    return (decimal_precision < IM_ARRAYSIZE(min_steps)) ? min_steps[decimal_precision] : ImPow(10.0f, (float)-decimal_precision);
}

template<typename TYPE>
static const char* ImAtoi(const char* src, TYPE* output)
{
    int negative = 0;
    if (*src == '-') { negative = 1; src++; }
    if (*src == '+') { src++; }
    TYPE v = 0;
    while (*src >= '0' && *src <= '9')
        v = (v * 10) + (*src++ - '0');
    *output = negative ? -v : v;
    return src;
}

template<typename TYPE, typename SIGNEDTYPE>
TYPE ImGui::RoundScalarWithFormatT(const char* format, ImGuiDataType data_type, TYPE v)
{
    const char* fmt_start = ImParseFormatFindStart(format);
    if (fmt_start[0] != '%' || fmt_start[1] == '%') // Don't apply if the value is not visible in the format string
        return v;
    char v_str[64];
    ImFormatString(v_str, IM_ARRAYSIZE(v_str), fmt_start, v);
    const char* p = v_str;
    while (*p == ' ')
        p++;
    if (data_type == ImGuiDataType_Float || data_type == ImGuiDataType_Double)
        v = (TYPE)ImAtof(p);
    else
        ImAtoi(p, (SIGNEDTYPE*)&v);
    return v;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: DragScalar, DragFloat, DragInt, etc.
//-------------------------------------------------------------------------
// - DragBehaviorT<>() [Internal]
// - DragBehavior() [Internal]
// - DragScalar()
// - DragScalarN()
// - DragFloat()
// - DragFloat2()
// - DragFloat3()
// - DragFloat4()
// - DragFloatRange2()
// - DragInt()
// - DragInt2()
// - DragInt3()
// - DragInt4()
// - DragIntRange2()
//-------------------------------------------------------------------------

// This is called by DragBehavior() when the widget is active (held by mouse or being manipulated with Nav controls)
template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
bool ImGui::DragBehaviorT(ImGuiDataType data_type, TYPE* v, float v_speed, const TYPE v_min, const TYPE v_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiContext& g = *GImGui;
    const ImGuiAxis axis = (flags & ImGuiSliderFlags_Vertical) ? ImGuiAxis_Y : ImGuiAxis_X;
    const bool is_decimal = (data_type == ImGuiDataType_Float) || (data_type == ImGuiDataType_Double);
    const bool is_clamped = (v_min < v_max);
    const bool is_logarithmic = (flags & ImGuiSliderFlags_Logarithmic) && is_decimal;

    // Default tweak speed
    if (v_speed == 0.0f && is_clamped && (v_max - v_min < FLT_MAX))
        v_speed = (float)((v_max - v_min) * g.DragSpeedDefaultRatio);

    // Inputs accumulates into g.DragCurrentAccum, which is flushed into the current value as soon as it makes a difference with our precision settings
    float adjust_delta = 0.0f;
    if (g.ActiveIdSource == ImGuiInputSource_Mouse && IsMousePosValid() && g.IO.MouseDragMaxDistanceSqr[0] > 1.0f * 1.0f)
    {
        adjust_delta = g.IO.MouseDelta[axis];
        if (g.IO.KeyAlt)
            adjust_delta *= 1.0f / 100.0f;
        if (g.IO.KeyShift)
            adjust_delta *= 10.0f;
    }
    else if (g.ActiveIdSource == ImGuiInputSource_Nav)
    {
        int decimal_precision = is_decimal ? ImParseFormatPrecision(format, 3) : 0;
        adjust_delta = GetNavInputAmount2d(ImGuiNavDirSourceFlags_Keyboard | ImGuiNavDirSourceFlags_PadDPad, ImGuiInputReadMode_RepeatFast, 1.0f / 10.0f, 10.0f)[axis];
        v_speed = ImMax(v_speed, GetMinimumStepAtDecimalPrecision(decimal_precision));
    }
    adjust_delta *= v_speed;

    // For vertical drag we currently assume that Up=higher value (like we do with vertical sliders). This may become a parameter.
    if (axis == ImGuiAxis_Y)
        adjust_delta = -adjust_delta;

    // For logarithmic use our range is effectively 0..1 so scale the delta into that range
    if (is_logarithmic && (v_max - v_min < FLT_MAX) && ((v_max - v_min) > 0.000001f)) // Epsilon to avoid /0
        adjust_delta /= (float)(v_max - v_min);

    // Clear current value on activation
    // Avoid altering values and clamping when we are _already_ past the limits and heading in the same direction, so e.g. if range is 0..255, current value is 300 and we are pushing to the right side, keep the 300.
    bool is_just_activated = g.ActiveIdIsJustActivated;
    bool is_already_past_limits_and_pushing_outward = is_clamped && ((*v >= v_max && adjust_delta > 0.0f) || (*v <= v_min && adjust_delta < 0.0f));
    if (is_just_activated || is_already_past_limits_and_pushing_outward)
    {
        g.DragCurrentAccum = 0.0f;
        g.DragCurrentAccumDirty = false;
    }
    else if (adjust_delta != 0.0f)
    {
        g.DragCurrentAccum += adjust_delta;
        g.DragCurrentAccumDirty = true;
    }

    if (!g.DragCurrentAccumDirty)
        return false;

    TYPE v_cur = *v;
    FLOATTYPE v_old_ref_for_accum_remainder = (FLOATTYPE)0.0f;

    float logarithmic_zero_epsilon = 0.0f; // Only valid when is_logarithmic is true
    const float zero_deadzone_halfsize = 0.0f; // Drag widgets have no deadzone (as it doesn't make sense)
    if (is_logarithmic)
    {
        // When using logarithmic sliders, we need to clamp to avoid hitting zero, but our choice of clamp value greatly affects slider precision. We attempt to use the specified precision to estimate a good lower bound.
        const int decimal_precision = is_decimal ? ImParseFormatPrecision(format, 3) : 1;
        logarithmic_zero_epsilon = ImPow(0.1f, (float)decimal_precision);

        // Convert to parametric space, apply delta, convert back
        float v_old_parametric = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, v_cur, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
        float v_new_parametric = v_old_parametric + g.DragCurrentAccum;
        v_cur = ScaleValueFromRatioT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, v_new_parametric, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
        v_old_ref_for_accum_remainder = v_old_parametric;
    }
    else
    {
        v_cur += (SIGNEDTYPE)g.DragCurrentAccum;
    }

    // Round to user desired precision based on format string
    if (!(flags & ImGuiSliderFlags_NoRoundToFormat))
        v_cur = RoundScalarWithFormatT<TYPE, SIGNEDTYPE>(format, data_type, v_cur);

    // Preserve remainder after rounding has been applied. This also allow slow tweaking of values.
    g.DragCurrentAccumDirty = false;
    if (is_logarithmic)
    {
        // Convert to parametric space, apply delta, convert back
        float v_new_parametric = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, v_cur, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
        g.DragCurrentAccum -= (float)(v_new_parametric - v_old_ref_for_accum_remainder);
    }
    else
    {
        g.DragCurrentAccum -= (float)((SIGNEDTYPE)v_cur - (SIGNEDTYPE)*v);
    }

    // Lose zero sign for float/double
    if (v_cur == (TYPE)-0)
        v_cur = (TYPE)0;

    // Clamp values (+ handle overflow/wrap-around for integer types)
    if (*v != v_cur && is_clamped)
    {
        if (v_cur < v_min || (v_cur > *v && adjust_delta < 0.0f && !is_decimal))
            v_cur = v_min;
        if (v_cur > v_max || (v_cur < *v && adjust_delta > 0.0f && !is_decimal))
            v_cur = v_max;
    }

    // Apply result
    if (*v == v_cur)
        return false;
    *v = v_cur;
    return true;
}

bool ImGui::DragBehavior(ImGuiID id, ImGuiDataType data_type, void* p_v, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
{
    // Read imgui.cpp "API BREAKING CHANGES" section for 1.78 if you hit this assert.
    IM_ASSERT((flags == 1 || (flags & ImGuiSliderFlags_InvalidMask_) == 0) && "Invalid ImGuiSliderFlags flags! Has the 'float power' argument been mistakenly cast to flags? Call function with ImGuiSliderFlags_Logarithmic flags instead.");

    ImGuiContext& g = *GImGui;
    if (g.ActiveId == id)
    {
        if (g.ActiveIdSource == ImGuiInputSource_Mouse && !g.IO.MouseDown[0])
            ClearActiveID();
        else if (g.ActiveIdSource == ImGuiInputSource_Nav && g.NavActivatePressedId == id && !g.ActiveIdIsJustActivated)
            ClearActiveID();
    }
    if (g.ActiveId != id)
        return false;
    if ((g.CurrentWindow->DC.ItemFlags & ImGuiItemFlags_ReadOnly) || (flags & ImGuiSliderFlags_ReadOnly))
        return false;

    switch (data_type)
    {
    case ImGuiDataType_S8:     { ImS32 v32 = (ImS32)*(ImS8*)p_v;  bool r = DragBehaviorT<ImS32, ImS32, float>(ImGuiDataType_S32, &v32, v_speed, p_min ? *(const ImS8*) p_min : IM_S8_MIN,  p_max ? *(const ImS8*)p_max  : IM_S8_MAX,  format, flags); if (r) *(ImS8*)p_v = (ImS8)v32; return r; }
    case ImGuiDataType_U8:     { ImU32 v32 = (ImU32)*(ImU8*)p_v;  bool r = DragBehaviorT<ImU32, ImS32, float>(ImGuiDataType_U32, &v32, v_speed, p_min ? *(const ImU8*) p_min : IM_U8_MIN,  p_max ? *(const ImU8*)p_max  : IM_U8_MAX,  format, flags); if (r) *(ImU8*)p_v = (ImU8)v32; return r; }
    case ImGuiDataType_S16:    { ImS32 v32 = (ImS32)*(ImS16*)p_v; bool r = DragBehaviorT<ImS32, ImS32, float>(ImGuiDataType_S32, &v32, v_speed, p_min ? *(const ImS16*)p_min : IM_S16_MIN, p_max ? *(const ImS16*)p_max : IM_S16_MAX, format, flags); if (r) *(ImS16*)p_v = (ImS16)v32; return r; }
    case ImGuiDataType_U16:    { ImU32 v32 = (ImU32)*(ImU16*)p_v; bool r = DragBehaviorT<ImU32, ImS32, float>(ImGuiDataType_U32, &v32, v_speed, p_min ? *(const ImU16*)p_min : IM_U16_MIN, p_max ? *(const ImU16*)p_max : IM_U16_MAX, format, flags); if (r) *(ImU16*)p_v = (ImU16)v32; return r; }
    case ImGuiDataType_S32:    return DragBehaviorT<ImS32, ImS32, float >(data_type, (ImS32*)p_v,  v_speed, p_min ? *(const ImS32* )p_min : IM_S32_MIN, p_max ? *(const ImS32* )p_max : IM_S32_MAX, format, flags);
    case ImGuiDataType_U32:    return DragBehaviorT<ImU32, ImS32, float >(data_type, (ImU32*)p_v,  v_speed, p_min ? *(const ImU32* )p_min : IM_U32_MIN, p_max ? *(const ImU32* )p_max : IM_U32_MAX, format, flags);
    case ImGuiDataType_S64:    return DragBehaviorT<ImS64, ImS64, double>(data_type, (ImS64*)p_v,  v_speed, p_min ? *(const ImS64* )p_min : IM_S64_MIN, p_max ? *(const ImS64* )p_max : IM_S64_MAX, format, flags);
    case ImGuiDataType_U64:    return DragBehaviorT<ImU64, ImS64, double>(data_type, (ImU64*)p_v,  v_speed, p_min ? *(const ImU64* )p_min : IM_U64_MIN, p_max ? *(const ImU64* )p_max : IM_U64_MAX, format, flags);
    case ImGuiDataType_Float:  return DragBehaviorT<float, float, float >(data_type, (float*)p_v,  v_speed, p_min ? *(const float* )p_min : -FLT_MAX,   p_max ? *(const float* )p_max : FLT_MAX,    format, flags);
    case ImGuiDataType_Double: return DragBehaviorT<double,double,double>(data_type, (double*)p_v, v_speed, p_min ? *(const double*)p_min : -DBL_MAX,   p_max ? *(const double*)p_max : DBL_MAX,    format, flags);
    case ImGuiDataType_COUNT:  break;
    }
    IM_ASSERT(0);
    return false;
}

// Note: p_data, p_min and p_max are _pointers_ to a memory address holding the data. For a Drag widget, p_min and p_max are optional.
// Read code of e.g. DragFloat(), DragInt() etc. or examples in 'Demo->Widgets->Data Types' to understand how to use this function directly.
bool ImGui::DragScalar(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w = CalcItemWidth();
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb))
        return false;

    // Default format string when passing NULL
    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;
    else if (data_type == ImGuiDataType_S32 && strcmp(format, "%d") != 0) // (FIXME-LEGACY: Patch old "%.0f" format string to use "%d", read function more details.)
        format = PatchFormatStringFloatToInt(format);

    // Tabbing or CTRL-clicking on Drag turns it into an input box
    const bool hovered = ItemHoverable(frame_bb, id);
    const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
    bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        const bool focus_requested = temp_input_allowed && FocusableItemRegister(window, id);
        const bool clicked = (hovered && g.IO.MouseClicked[0]);
        const bool double_clicked = (hovered && g.IO.MouseDoubleClicked[0]);
        if (focus_requested || clicked || double_clicked || g.NavActivateId == id || g.NavInputId == id)
        {
            SetActiveID(id, window);
            SetFocusID(id, window);
            FocusWindow(window);
            g.ActiveIdUsingNavDirMask = (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
            if (temp_input_allowed && (focus_requested || (clicked && g.IO.KeyCtrl) || double_clicked || g.NavInputId == id))
            {
                temp_input_is_active = true;
                FocusableItemUnregister(window);
            }
        }
    }

    if (temp_input_is_active)
    {
        // Only clamp CTRL+Click input when ImGuiSliderFlags_AlwaysClamp is set
        const bool is_clamp_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0 && (p_min == NULL || p_max == NULL || DataTypeCompare(data_type, p_min, p_max) < 0);
        return TempInputScalar(frame_bb, id, label, data_type, p_data, format, is_clamp_input ? p_min : NULL, is_clamp_input ? p_max : NULL);
    }

    // Draw frame
    const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    RenderNavHighlight(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, style.FrameRounding);

    // Drag behavior
    const bool value_changed = DragBehavior(id, data_type, p_data, v_speed, p_min, p_max, format, flags);
    if (value_changed)
        MarkItemEdited(id);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
    RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags);
    return value_changed;
}

bool ImGui::DragScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    bool value_changed = false;
    BeginGroup();
    PushID(label);
    PushMultiItemsWidths(components, CalcItemWidth());
    size_t type_size = GDataTypeInfo[data_type].Size;
    for (int i = 0; i < components; i++)
    {
        PushID(i);
        if (i > 0)
            SameLine(0, g.Style.ItemInnerSpacing.x);
        value_changed |= DragScalar("", data_type, p_data, v_speed, p_min, p_max, format, flags);
        PopID();
        PopItemWidth();
        p_data = (void*)((char*)p_data + type_size);
    }
    PopID();

    const char* label_end = FindRenderedTextEnd(label);
    if (label != label_end)
    {
        SameLine(0, g.Style.ItemInnerSpacing.x);
        TextEx(label, label_end);
    }

    EndGroup();
    return value_changed;
}

bool ImGui::DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return DragScalar(label, ImGuiDataType_Float, v, v_speed, &v_min, &v_max, format, flags);
}

bool ImGui::DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return DragScalarN(label, ImGuiDataType_Float, v, 2, v_speed, &v_min, &v_max, format, flags);
}

bool ImGui::DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return DragScalarN(label, ImGuiDataType_Float, v, 3, v_speed, &v_min, &v_max, format, flags);
}

bool ImGui::DragFloat4(const char* label, float v[4], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return DragScalarN(label, ImGuiDataType_Float, v, 4, v_speed, &v_min, &v_max, format, flags);
}

// NB: You likely want to specify the ImGuiSliderFlags_AlwaysClamp when using this.
bool ImGui::DragFloatRange2(const char* label, float* v_current_min, float* v_current_max, float v_speed, float v_min, float v_max, const char* format, const char* format_max, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    PushID(label);
    BeginGroup();
    PushMultiItemsWidths(2, CalcItemWidth());

    float min_min = (v_min >= v_max) ? -FLT_MAX : v_min;
    float min_max = (v_min >= v_max) ? *v_current_max : ImMin(v_max, *v_current_max);
    ImGuiSliderFlags min_flags = flags | ((min_min == min_max) ? ImGuiSliderFlags_ReadOnly : 0);
    bool value_changed = DragScalar("##min", ImGuiDataType_Float, v_current_min, v_speed, &min_min, &min_max, format, min_flags);
    PopItemWidth();
    SameLine(0, g.Style.ItemInnerSpacing.x);

    float max_min = (v_min >= v_max) ? *v_current_min : ImMax(v_min, *v_current_min);
    float max_max = (v_min >= v_max) ? FLT_MAX : v_max;
    ImGuiSliderFlags max_flags = flags | ((max_min == max_max) ? ImGuiSliderFlags_ReadOnly : 0);
    value_changed |= DragScalar("##max", ImGuiDataType_Float, v_current_max, v_speed, &max_min, &max_max, format_max ? format_max : format, max_flags);
    PopItemWidth();
    SameLine(0, g.Style.ItemInnerSpacing.x);

    TextEx(label, FindRenderedTextEnd(label));
    EndGroup();
    PopID();
    return value_changed;
}

// NB: v_speed is float to allow adjusting the drag speed with more precision
bool ImGui::DragInt(const char* label, int* v, float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return DragScalar(label, ImGuiDataType_S32, v, v_speed, &v_min, &v_max, format, flags);
}

bool ImGui::DragInt2(const char* label, int v[2], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return DragScalarN(label, ImGuiDataType_S32, v, 2, v_speed, &v_min, &v_max, format, flags);
}

bool ImGui::DragInt3(const char* label, int v[3], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return DragScalarN(label, ImGuiDataType_S32, v, 3, v_speed, &v_min, &v_max, format, flags);
}

bool ImGui::DragInt4(const char* label, int v[4], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return DragScalarN(label, ImGuiDataType_S32, v, 4, v_speed, &v_min, &v_max, format, flags);
}

// NB: You likely want to specify the ImGuiSliderFlags_AlwaysClamp when using this.
bool ImGui::DragIntRange2(const char* label, int* v_current_min, int* v_current_max, float v_speed, int v_min, int v_max, const char* format, const char* format_max, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    PushID(label);
    BeginGroup();
    PushMultiItemsWidths(2, CalcItemWidth());

    int min_min = (v_min >= v_max) ? INT_MIN : v_min;
    int min_max = (v_min >= v_max) ? *v_current_max : ImMin(v_max, *v_current_max);
    ImGuiSliderFlags min_flags = flags | ((min_min == min_max) ? ImGuiSliderFlags_ReadOnly : 0);
    bool value_changed = DragInt("##min", v_current_min, v_speed, min_min, min_max, format, min_flags);
    PopItemWidth();
    SameLine(0, g.Style.ItemInnerSpacing.x);

    int max_min = (v_min >= v_max) ? *v_current_min : ImMax(v_min, *v_current_min);
    int max_max = (v_min >= v_max) ? INT_MAX : v_max;
    ImGuiSliderFlags max_flags = flags | ((max_min == max_max) ? ImGuiSliderFlags_ReadOnly : 0);
    value_changed |= DragInt("##max", v_current_max, v_speed, max_min, max_max, format_max ? format_max : format, max_flags);
    PopItemWidth();
    SameLine(0, g.Style.ItemInnerSpacing.x);

    TextEx(label, FindRenderedTextEnd(label));
    EndGroup();
    PopID();

    return value_changed;
}

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS

// Obsolete versions with power parameter. See https://github.com/ocornut/imgui/issues/3361 for details.
bool ImGui::DragScalar(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, float power)
{
    ImGuiSliderFlags drag_flags = ImGuiSliderFlags_None;
    if (power != 1.0f)
    {
        IM_ASSERT(power == 1.0f && "Call function with ImGuiSliderFlags_Logarithmic flags instead of using the old 'float power' function!");
        IM_ASSERT(p_min != NULL && p_max != NULL);  // When using a power curve the drag needs to have known bounds
        drag_flags |= ImGuiSliderFlags_Logarithmic;   // Fallback for non-asserting paths
    }
    return DragScalar(label, data_type, p_data, v_speed, p_min, p_max, format, drag_flags);
}

bool ImGui::DragScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, float v_speed, const void* p_min, const void* p_max, const char* format, float power)
{
    ImGuiSliderFlags drag_flags = ImGuiSliderFlags_None;
    if (power != 1.0f)
    {
        IM_ASSERT(power == 1.0f && "Call function with ImGuiSliderFlags_Logarithmic flags instead of using the old 'float power' function!");
        IM_ASSERT(p_min != NULL && p_max != NULL);  // When using a power curve the drag needs to have known bounds
        drag_flags |= ImGuiSliderFlags_Logarithmic;   // Fallback for non-asserting paths
    }
    return DragScalarN(label, data_type, p_data, components, v_speed, p_min, p_max, format, drag_flags);
}

#endif // IMGUI_DISABLE_OBSOLETE_FUNCTIONS

//-------------------------------------------------------------------------
// [SECTION] Widgets: SliderScalar, SliderFloat, SliderInt, etc.
//-------------------------------------------------------------------------
// - ScaleRatioFromValueT<> [Internal]
// - ScaleValueFromRatioT<> [Internal]
// - SliderBehaviorT<>() [Internal]
// - SliderBehavior() [Internal]
// - SliderScalar()
// - SliderScalarN()
// - SliderFloat()
// - SliderFloat2()
// - SliderFloat3()
// - SliderFloat4()
// - SliderAngle()
// - SliderInt()
// - SliderInt2()
// - SliderInt3()
// - SliderInt4()
// - VSliderScalar()
// - VSliderFloat()
// - VSliderInt()
//-------------------------------------------------------------------------

// Convert a value v in the output space of a slider into a parametric position on the slider itself (the logical opposite of ScaleValueFromRatioT)
template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
float ImGui::ScaleRatioFromValueT(ImGuiDataType data_type, TYPE v, TYPE v_min, TYPE v_max, bool is_logarithmic, float logarithmic_zero_epsilon, float zero_deadzone_halfsize)
{
    if (v_min == v_max)
        return 0.0f;
    IM_UNUSED(data_type);

    const TYPE v_clamped = (v_min < v_max) ? ImClamp(v, v_min, v_max) : ImClamp(v, v_max, v_min);
    if (is_logarithmic)
    {
        bool flipped = v_max < v_min;

        if (flipped) // Handle the case where the range is backwards
            ImSwap(v_min, v_max);

        // Fudge min/max to avoid getting close to log(0)
        FLOATTYPE v_min_fudged = (ImAbs((FLOATTYPE)v_min) < logarithmic_zero_epsilon) ? ((v_min < 0.0f) ? -logarithmic_zero_epsilon : logarithmic_zero_epsilon) : (FLOATTYPE)v_min;
        FLOATTYPE v_max_fudged = (ImAbs((FLOATTYPE)v_max) < logarithmic_zero_epsilon) ? ((v_max < 0.0f) ? -logarithmic_zero_epsilon : logarithmic_zero_epsilon) : (FLOATTYPE)v_max;

        // Awkward special cases - we need ranges of the form (-100 .. 0) to convert to (-100 .. -epsilon), not (-100 .. epsilon)
        if ((v_min == 0.0f) && (v_max < 0.0f))
            v_min_fudged = -logarithmic_zero_epsilon;
        else if ((v_max == 0.0f) && (v_min < 0.0f))
            v_max_fudged = -logarithmic_zero_epsilon;

        float result;

        if (v_clamped <= v_min_fudged)
            result = 0.0f; // Workaround for values that are in-range but below our fudge
        else if (v_clamped >= v_max_fudged)
            result = 1.0f; // Workaround for values that are in-range but above our fudge
        else if ((v_min * v_max) < 0.0f) // Range crosses zero, so split into two portions
        {
            float zero_point_center = (-(float)v_min) / ((float)v_max - (float)v_min); // The zero point in parametric space.  There's an argument we should take the logarithmic nature into account when calculating this, but for now this should do (and the most common case of a symmetrical range works fine)
            float zero_point_snap_L = zero_point_center - zero_deadzone_halfsize;
            float zero_point_snap_R = zero_point_center + zero_deadzone_halfsize;
            if (v == 0.0f)
                result = zero_point_center; // Special case for exactly zero
            else if (v < 0.0f)
                result = (1.0f - (float)(ImLog(-(FLOATTYPE)v_clamped / logarithmic_zero_epsilon) / ImLog(-v_min_fudged / logarithmic_zero_epsilon))) * zero_point_snap_L;
            else
                result = zero_point_snap_R + ((float)(ImLog((FLOATTYPE)v_clamped / logarithmic_zero_epsilon) / ImLog(v_max_fudged / logarithmic_zero_epsilon)) * (1.0f - zero_point_snap_R));
        }
        else if ((v_min < 0.0f) || (v_max < 0.0f)) // Entirely negative slider
            result = 1.0f - (float)(ImLog(-(FLOATTYPE)v_clamped / -v_max_fudged) / ImLog(-v_min_fudged / -v_max_fudged));
        else
            result = (float)(ImLog((FLOATTYPE)v_clamped / v_min_fudged) / ImLog(v_max_fudged / v_min_fudged));

        return flipped ? (1.0f - result) : result;
    }

    // Linear slider
    return (float)((FLOATTYPE)(SIGNEDTYPE)(v_clamped - v_min) / (FLOATTYPE)(SIGNEDTYPE)(v_max - v_min));
}

// Convert a parametric position on a slider into a value v in the output space (the logical opposite of ScaleRatioFromValueT)
template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
TYPE ImGui::ScaleValueFromRatioT(ImGuiDataType data_type, float t, TYPE v_min, TYPE v_max, bool is_logarithmic, float logarithmic_zero_epsilon, float zero_deadzone_halfsize)
{
    if (v_min == v_max)
        return (TYPE)0.0f;
    const bool is_decimal = (data_type == ImGuiDataType_Float) || (data_type == ImGuiDataType_Double);

    TYPE result;
    if (is_logarithmic)
    {
        // We special-case the extents because otherwise our fudging can lead to "mathematically correct" but non-intuitive behaviors like a fully-left slider not actually reaching the minimum value
        if (t <= 0.0f)
            result = v_min;
        else if (t >= 1.0f)
            result = v_max;
        else
        {
            bool flipped = v_max < v_min; // Check if range is "backwards"

            // Fudge min/max to avoid getting silly results close to zero
            FLOATTYPE v_min_fudged = (ImAbs((FLOATTYPE)v_min) < logarithmic_zero_epsilon) ? ((v_min < 0.0f) ? -logarithmic_zero_epsilon : logarithmic_zero_epsilon) : (FLOATTYPE)v_min;
            FLOATTYPE v_max_fudged = (ImAbs((FLOATTYPE)v_max) < logarithmic_zero_epsilon) ? ((v_max < 0.0f) ? -logarithmic_zero_epsilon : logarithmic_zero_epsilon) : (FLOATTYPE)v_max;

            if (flipped)
                ImSwap(v_min_fudged, v_max_fudged);

            // Awkward special case - we need ranges of the form (-100 .. 0) to convert to (-100 .. -epsilon), not (-100 .. epsilon)
            if ((v_max == 0.0f) && (v_min < 0.0f))
                v_max_fudged = -logarithmic_zero_epsilon;

            float t_with_flip = flipped ? (1.0f - t) : t; // t, but flipped if necessary to account for us flipping the range

            if ((v_min * v_max) < 0.0f) // Range crosses zero, so we have to do this in two parts
            {
                float zero_point_center = (-(float)ImMin(v_min, v_max)) / ImAbs((float)v_max - (float)v_min); // The zero point in parametric space
                float zero_point_snap_L = zero_point_center - zero_deadzone_halfsize;
                float zero_point_snap_R = zero_point_center + zero_deadzone_halfsize;
                if (t_with_flip >= zero_point_snap_L && t_with_flip <= zero_point_snap_R)
                    result = (TYPE)0.0f; // Special case to make getting exactly zero possible (the epsilon prevents it otherwise)
                else if (t_with_flip < zero_point_center)
                    result = (TYPE)-(logarithmic_zero_epsilon * ImPow(-v_min_fudged / logarithmic_zero_epsilon, (FLOATTYPE)(1.0f - (t_with_flip / zero_point_snap_L))));
                else
                    result = (TYPE)(logarithmic_zero_epsilon * ImPow(v_max_fudged / logarithmic_zero_epsilon, (FLOATTYPE)((t_with_flip - zero_point_snap_R) / (1.0f - zero_point_snap_R))));
            }
            else if ((v_min < 0.0f) || (v_max < 0.0f)) // Entirely negative slider
                result = (TYPE)-(-v_max_fudged * ImPow(-v_min_fudged / -v_max_fudged, (FLOATTYPE)(1.0f - t_with_flip)));
            else
                result = (TYPE)(v_min_fudged * ImPow(v_max_fudged / v_min_fudged, (FLOATTYPE)t_with_flip));
        }
    }
    else
    {
        // Linear slider
        if (is_decimal)
        {
            result = ImLerp(v_min, v_max, t);
        }
        else
        {
            // - For integer values we want the clicking position to match the grab box so we round above
            //   This code is carefully tuned to work with large values (e.g. high ranges of U64) while preserving this property..
            // - Not doing a *1.0 multiply at the end of a range as it tends to be lossy. While absolute aiming at a large s64/u64
            //   range is going to be imprecise anyway, with this check we at least make the edge values matches expected limits.
            if (t < 1.0)
            {
                FLOATTYPE v_new_off_f = (SIGNEDTYPE)(v_max - v_min) * t;
                result = (TYPE)((SIGNEDTYPE)v_min + (SIGNEDTYPE)(v_new_off_f + (FLOATTYPE)(v_min > v_max ? -0.5 : 0.5)));
            }
            else
            {
                result = v_max;
            }
        }
    }

    return result;
}

// FIXME: Move more of the code into SliderBehavior()
template<typename TYPE, typename SIGNEDTYPE, typename FLOATTYPE>
bool ImGui::SliderBehaviorT(const ImRect& bb, ImGuiID id, ImGuiDataType data_type, TYPE* v, const TYPE v_min, const TYPE v_max, const char* format, ImGuiSliderFlags flags, ImRect* out_grab_bb)
{
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImGuiAxis axis = (flags & ImGuiSliderFlags_Vertical) ? ImGuiAxis_Y : ImGuiAxis_X;
    const bool is_decimal = (data_type == ImGuiDataType_Float) || (data_type == ImGuiDataType_Double);
    const bool is_logarithmic = (flags & ImGuiSliderFlags_Logarithmic) && is_decimal;

    const float grab_padding = 2.0f;
    const float slider_sz = (bb.Max[axis] - bb.Min[axis]) - grab_padding * 2.0f;
    float grab_sz = style.GrabMinSize;
    SIGNEDTYPE v_range = (v_min < v_max ? v_max - v_min : v_min - v_max);
    if (!is_decimal && v_range >= 0)                                             // v_range < 0 may happen on integer overflows
        grab_sz = ImMax((float)(slider_sz / (v_range + 1)), style.GrabMinSize);  // For integer sliders: if possible have the grab size represent 1 unit
    grab_sz = ImMin(grab_sz, slider_sz);
    const float slider_usable_sz = slider_sz - grab_sz;
    const float slider_usable_pos_min = bb.Min[axis] + grab_padding + grab_sz * 0.5f;
    const float slider_usable_pos_max = bb.Max[axis] - grab_padding - grab_sz * 0.5f;

    float logarithmic_zero_epsilon = 0.0f; // Only valid when is_logarithmic is true
    float zero_deadzone_halfsize = 0.0f; // Only valid when is_logarithmic is true
    if (is_logarithmic)
    {
        // When using logarithmic sliders, we need to clamp to avoid hitting zero, but our choice of clamp value greatly affects slider precision. We attempt to use the specified precision to estimate a good lower bound.
        const int decimal_precision = is_decimal ? ImParseFormatPrecision(format, 3) : 1;
        logarithmic_zero_epsilon = ImPow(0.1f, (float)decimal_precision);
        zero_deadzone_halfsize = (style.LogSliderDeadzone * 0.5f) / ImMax(slider_usable_sz, 1.0f);
    }

    // Process interacting with the slider
    bool value_changed = false;
    if (g.ActiveId == id)
    {
        bool set_new_value = false;
        float clicked_t = 0.0f;
        if (g.ActiveIdSource == ImGuiInputSource_Mouse)
        {
            if (!g.IO.MouseDown[0])
            {
                ClearActiveID();
            }
            else
            {
                const float mouse_abs_pos = g.IO.MousePos[axis];
                clicked_t = (slider_usable_sz > 0.0f) ? ImClamp((mouse_abs_pos - slider_usable_pos_min) / slider_usable_sz, 0.0f, 1.0f) : 0.0f;
                if (axis == ImGuiAxis_Y)
                    clicked_t = 1.0f - clicked_t;
                set_new_value = true;
            }
        }
        else if (g.ActiveIdSource == ImGuiInputSource_Nav)
        {
            if (g.ActiveIdIsJustActivated)
            {
                g.SliderCurrentAccum = 0.0f; // Reset any stored nav delta upon activation
                g.SliderCurrentAccumDirty = false;
            }

            const ImVec2 input_delta2 = GetNavInputAmount2d(ImGuiNavDirSourceFlags_Keyboard | ImGuiNavDirSourceFlags_PadDPad, ImGuiInputReadMode_RepeatFast, 0.0f, 0.0f);
            float input_delta = (axis == ImGuiAxis_X) ? input_delta2.x : -input_delta2.y;
            if (input_delta != 0.0f)
            {
                const int decimal_precision = is_decimal ? ImParseFormatPrecision(format, 3) : 0;
                if (decimal_precision > 0)
                {
                    input_delta /= 100.0f;    // Gamepad/keyboard tweak speeds in % of slider bounds
                    if (IsNavInputDown(ImGuiNavInput_TweakSlow))
                        input_delta /= 10.0f;
                }
                else
                {
                    if ((v_range >= -100.0f && v_range <= 100.0f) || IsNavInputDown(ImGuiNavInput_TweakSlow))
                        input_delta = ((input_delta < 0.0f) ? -1.0f : +1.0f) / (float)v_range; // Gamepad/keyboard tweak speeds in integer steps
                    else
                        input_delta /= 100.0f;
                }
                if (IsNavInputDown(ImGuiNavInput_TweakFast))
                    input_delta *= 10.0f;

                g.SliderCurrentAccum += input_delta;
                g.SliderCurrentAccumDirty = true;
            }

            float delta = g.SliderCurrentAccum;
            if (g.NavActivatePressedId == id && !g.ActiveIdIsJustActivated)
            {
                ClearActiveID();
            }
            else if (g.SliderCurrentAccumDirty)
            {
                clicked_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, *v, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

                if ((clicked_t >= 1.0f && delta > 0.0f) || (clicked_t <= 0.0f && delta < 0.0f)) // This is to avoid applying the saturation when already past the limits
                {
                    set_new_value = false;
                    g.SliderCurrentAccum = 0.0f; // If pushing up against the limits, don't continue to accumulate
                }
                else
                {
                    set_new_value = true;
                    float old_clicked_t = clicked_t;
                    clicked_t = ImSaturate(clicked_t + delta);

                    // Calculate what our "new" clicked_t will be, and thus how far we actually moved the slider, and subtract this from the accumulator
                    TYPE v_new = ScaleValueFromRatioT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, clicked_t, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
                    if (!(flags & ImGuiSliderFlags_NoRoundToFormat))
                        v_new = RoundScalarWithFormatT<TYPE, SIGNEDTYPE>(format, data_type, v_new);
                    float new_clicked_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, v_new, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

                    if (delta > 0)
                        g.SliderCurrentAccum -= ImMin(new_clicked_t - old_clicked_t, delta);
                    else
                        g.SliderCurrentAccum -= ImMax(new_clicked_t - old_clicked_t, delta);
                }

                g.SliderCurrentAccumDirty = false;
            }
        }

        if (set_new_value)
        {
            TYPE v_new = ScaleValueFromRatioT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, clicked_t, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);

            // Round to user desired precision based on format string
            if (!(flags & ImGuiSliderFlags_NoRoundToFormat))
                v_new = RoundScalarWithFormatT<TYPE, SIGNEDTYPE>(format, data_type, v_new);

            // Apply result
            if (*v != v_new)
            {
                *v = v_new;
                value_changed = true;
            }
        }
    }

    if (slider_sz < 1.0f)
    {
        *out_grab_bb = ImRect(bb.Min, bb.Min);
    }
    else
    {
        // Output grab position so it can be displayed by the caller
        float grab_t = ScaleRatioFromValueT<TYPE, SIGNEDTYPE, FLOATTYPE>(data_type, *v, v_min, v_max, is_logarithmic, logarithmic_zero_epsilon, zero_deadzone_halfsize);
        if (axis == ImGuiAxis_Y)
            grab_t = 1.0f - grab_t;
        const float grab_pos = ImLerp(slider_usable_pos_min, slider_usable_pos_max, grab_t);
        if (axis == ImGuiAxis_X)
            *out_grab_bb = ImRect(grab_pos - grab_sz * 0.5f, bb.Min.y + grab_padding, grab_pos + grab_sz * 0.5f, bb.Max.y - grab_padding);
        else
            *out_grab_bb = ImRect(bb.Min.x + grab_padding, grab_pos - grab_sz * 0.5f, bb.Max.x - grab_padding, grab_pos + grab_sz * 0.5f);
    }

    return value_changed;
}

// For 32-bit and larger types, slider bounds are limited to half the natural type range.
// So e.g. an integer Slider between INT_MAX-10 and INT_MAX will fail, but an integer Slider between INT_MAX/2-10 and INT_MAX/2 will be ok.
// It would be possible to lift that limitation with some work but it doesn't seem to be worth it for sliders.
bool ImGui::SliderBehavior(const ImRect& bb, ImGuiID id, ImGuiDataType data_type, void* p_v, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags, ImRect* out_grab_bb)
{
    // Read imgui.cpp "API BREAKING CHANGES" section for 1.78 if you hit this assert.
    IM_ASSERT((flags == 1 || (flags & ImGuiSliderFlags_InvalidMask_) == 0) && "Invalid ImGuiSliderFlags flag!  Has the 'float power' argument been mistakenly cast to flags? Call function with ImGuiSliderFlags_Logarithmic flags instead.");

    ImGuiContext& g = *GImGui;
    if ((g.CurrentWindow->DC.ItemFlags & ImGuiItemFlags_ReadOnly) || (flags & ImGuiSliderFlags_ReadOnly))
        return false;

    switch (data_type)
    {
    case ImGuiDataType_S8:  { ImS32 v32 = (ImS32)*(ImS8*)p_v;  bool r = SliderBehaviorT<ImS32, ImS32, float>(bb, id, ImGuiDataType_S32, &v32, *(const ImS8*)p_min,  *(const ImS8*)p_max,  format, flags, out_grab_bb); if (r) *(ImS8*)p_v  = (ImS8)v32;  return r; }
    case ImGuiDataType_U8:  { ImU32 v32 = (ImU32)*(ImU8*)p_v;  bool r = SliderBehaviorT<ImU32, ImS32, float>(bb, id, ImGuiDataType_U32, &v32, *(const ImU8*)p_min,  *(const ImU8*)p_max,  format, flags, out_grab_bb); if (r) *(ImU8*)p_v  = (ImU8)v32;  return r; }
    case ImGuiDataType_S16: { ImS32 v32 = (ImS32)*(ImS16*)p_v; bool r = SliderBehaviorT<ImS32, ImS32, float>(bb, id, ImGuiDataType_S32, &v32, *(const ImS16*)p_min, *(const ImS16*)p_max, format, flags, out_grab_bb); if (r) *(ImS16*)p_v = (ImS16)v32; return r; }
    case ImGuiDataType_U16: { ImU32 v32 = (ImU32)*(ImU16*)p_v; bool r = SliderBehaviorT<ImU32, ImS32, float>(bb, id, ImGuiDataType_U32, &v32, *(const ImU16*)p_min, *(const ImU16*)p_max, format, flags, out_grab_bb); if (r) *(ImU16*)p_v = (ImU16)v32; return r; }
    case ImGuiDataType_S32:
        IM_ASSERT(*(const ImS32*)p_min >= IM_S32_MIN / 2 && *(const ImS32*)p_max <= IM_S32_MAX / 2);
        return SliderBehaviorT<ImS32, ImS32, float >(bb, id, data_type, (ImS32*)p_v,  *(const ImS32*)p_min,  *(const ImS32*)p_max,  format, flags, out_grab_bb);
    case ImGuiDataType_U32:
        IM_ASSERT(*(const ImU32*)p_max <= IM_U32_MAX / 2);
        return SliderBehaviorT<ImU32, ImS32, float >(bb, id, data_type, (ImU32*)p_v,  *(const ImU32*)p_min,  *(const ImU32*)p_max,  format, flags, out_grab_bb);
    case ImGuiDataType_S64:
        IM_ASSERT(*(const ImS64*)p_min >= IM_S64_MIN / 2 && *(const ImS64*)p_max <= IM_S64_MAX / 2);
        return SliderBehaviorT<ImS64, ImS64, double>(bb, id, data_type, (ImS64*)p_v,  *(const ImS64*)p_min,  *(const ImS64*)p_max,  format, flags, out_grab_bb);
    case ImGuiDataType_U64:
        IM_ASSERT(*(const ImU64*)p_max <= IM_U64_MAX / 2);
        return SliderBehaviorT<ImU64, ImS64, double>(bb, id, data_type, (ImU64*)p_v,  *(const ImU64*)p_min,  *(const ImU64*)p_max,  format, flags, out_grab_bb);
    case ImGuiDataType_Float:
        IM_ASSERT(*(const float*)p_min >= -FLT_MAX / 2.0f && *(const float*)p_max <= FLT_MAX / 2.0f);
        return SliderBehaviorT<float, float, float >(bb, id, data_type, (float*)p_v,  *(const float*)p_min,  *(const float*)p_max,  format, flags, out_grab_bb);
    case ImGuiDataType_Double:
        IM_ASSERT(*(const double*)p_min >= -DBL_MAX / 2.0f && *(const double*)p_max <= DBL_MAX / 2.0f);
        return SliderBehaviorT<double, double, double>(bb, id, data_type, (double*)p_v, *(const double*)p_min, *(const double*)p_max, format, flags, out_grab_bb);
    case ImGuiDataType_COUNT: break;
    }
    IM_ASSERT(0);
    return false;
}

// Note: p_data, p_min and p_max are _pointers_ to a memory address holding the data. For a slider, they are all required.
// Read code of e.g. SliderFloat(), SliderInt() etc. or examples in 'Demo->Widgets->Data Types' to understand how to use this function directly.
bool ImGui::SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float w = CalcItemWidth();

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &frame_bb))
        return false;

    // Default format string when passing NULL
    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;
    else if (data_type == ImGuiDataType_S32 && strcmp(format, "%d") != 0) // (FIXME-LEGACY: Patch old "%.0f" format string to use "%d", read function more details.)
        format = PatchFormatStringFloatToInt(format);

    // Tabbing or CTRL-clicking on Slider turns it into an input box
    const bool hovered = ItemHoverable(frame_bb, id);
    const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
    bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);
    if (!temp_input_is_active)
    {
        const bool focus_requested = temp_input_allowed && FocusableItemRegister(window, id);
        const bool clicked = (hovered && g.IO.MouseClicked[0]);
        if (focus_requested || clicked || g.NavActivateId == id || g.NavInputId == id)
        {
            SetActiveID(id, window);
            SetFocusID(id, window);
            FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
            if (temp_input_allowed && (focus_requested || (clicked && g.IO.KeyCtrl) || g.NavInputId == id))
            {
                temp_input_is_active = true;
                FocusableItemUnregister(window);
            }
        }
    }

    if (temp_input_is_active)
    {
        // Only clamp CTRL+Click input when ImGuiSliderFlags_AlwaysClamp is set
        const bool is_clamp_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0;
        return TempInputScalar(frame_bb, id, label, data_type, p_data, format, is_clamp_input ? p_min : NULL, is_clamp_input ? p_max : NULL);
    }

    // Draw frame
    const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    RenderNavHighlight(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    // Slider behavior
    ImRect grab_bb;
    const bool value_changed = SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, flags, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    // Render grab
    if (grab_bb.Max.x > grab_bb.Min.x)
        window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
    RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.5f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags);
    return value_changed;
}

// Add multiple sliders on 1 line for compact edition of multiple components
bool ImGui::SliderScalarN(const char* label, ImGuiDataType data_type, void* v, int components, const void* v_min, const void* v_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    bool value_changed = false;
    BeginGroup();
    PushID(label);
    PushMultiItemsWidths(components, CalcItemWidth());
    size_t type_size = GDataTypeInfo[data_type].Size;
    for (int i = 0; i < components; i++)
    {
        PushID(i);
        if (i > 0)
            SameLine(0, g.Style.ItemInnerSpacing.x);
        value_changed |= SliderScalar("", data_type, v, v_min, v_max, format, flags);
        PopID();
        PopItemWidth();
        v = (void*)((char*)v + type_size);
    }
    PopID();

    const char* label_end = FindRenderedTextEnd(label);
    if (label != label_end)
    {
        SameLine(0, g.Style.ItemInnerSpacing.x);
        TextEx(label, label_end);
    }

    EndGroup();
    return value_changed;
}

bool ImGui::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
}

bool ImGui::SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_Float, v, 2, &v_min, &v_max, format, flags);
}

bool ImGui::SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_Float, v, 3, &v_min, &v_max, format, flags);
}

bool ImGui::SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_Float, v, 4, &v_min, &v_max, format, flags);
}

bool ImGui::SliderAngle(const char* label, float* v_rad, float v_degrees_min, float v_degrees_max, const char* format, ImGuiSliderFlags flags)
{
    if (format == NULL)
        format = "%.0f deg";
    float v_deg = (*v_rad) * 360.0f / (2 * IM_PI);
    bool value_changed = SliderFloat(label, &v_deg, v_degrees_min, v_degrees_max, format, flags);
    *v_rad = v_deg * (2 * IM_PI) / 360.0f;
    return value_changed;
}

bool ImGui::SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format, flags);
}

bool ImGui::SliderInt2(const char* label, int v[2], int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_S32, v, 2, &v_min, &v_max, format, flags);
}

bool ImGui::SliderInt3(const char* label, int v[3], int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_S32, v, 3, &v_min, &v_max, format, flags);
}

bool ImGui::SliderInt4(const char* label, int v[4], int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_S32, v, 4, &v_min, &v_max, format, flags);
}

bool ImGui::VSliderScalar(const char* label, const ImVec2& size, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + size);
    const ImRect bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

    ItemSize(bb, style.FramePadding.y);
    if (!ItemAdd(frame_bb, id))
        return false;

    // Default format string when passing NULL
    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;
    else if (data_type == ImGuiDataType_S32 && strcmp(format, "%d") != 0) // (FIXME-LEGACY: Patch old "%.0f" format string to use "%d", read function more details.)
        format = PatchFormatStringFloatToInt(format);

    const bool hovered = ItemHoverable(frame_bb, id);
    if ((hovered && g.IO.MouseClicked[0]) || g.NavActivateId == id || g.NavInputId == id)
    {
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
    }

    // Draw frame
    const ImU32 frame_col = GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : g.HoveredId == id ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    RenderNavHighlight(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, frame_col, true, g.Style.FrameRounding);

    // Slider behavior
    ImRect grab_bb;
    const bool value_changed = SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, flags | ImGuiSliderFlags_Vertical, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    // Render grab
    if (grab_bb.Max.y > grab_bb.Min.y)
        window->DrawList->AddRectFilled(grab_bb.Min, grab_bb.Max, GetColorU32(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), style.GrabRounding);

    // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
    // For the vertical slider we allow centered text to overlap the frame padding
    char value_buf[64];
    const char* value_buf_end = value_buf + DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format);
    RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, value_buf, value_buf_end, NULL, ImVec2(0.5f, 0.0f));
    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    return value_changed;
}

bool ImGui::VSliderFloat(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return VSliderScalar(label, size, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
}

bool ImGui::VSliderInt(const char* label, const ImVec2& size, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return VSliderScalar(label, size, ImGuiDataType_S32, v, &v_min, &v_max, format, flags);
}

#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS

// Obsolete versions with power parameter. See https://github.com/ocornut/imgui/issues/3361 for details.
bool ImGui::SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, float power)
{
    ImGuiSliderFlags slider_flags = ImGuiSliderFlags_None;
    if (power != 1.0f)
    {
        IM_ASSERT(power == 1.0f && "Call function with ImGuiSliderFlags_Logarithmic flags instead of using the old 'float power' function!");
        slider_flags |= ImGuiSliderFlags_Logarithmic;   // Fallback for non-asserting paths
    }
    return SliderScalar(label, data_type, p_data, p_min, p_max, format, slider_flags);
}

bool ImGui::SliderScalarN(const char* label, ImGuiDataType data_type, void* v, int components, const void* v_min, const void* v_max, const char* format, float power)
{
    ImGuiSliderFlags slider_flags = ImGuiSliderFlags_None;
    if (power != 1.0f)
    {
        IM_ASSERT(power == 1.0f && "Call function with ImGuiSliderFlags_Logarithmic flags instead of using the old 'float power' function!");
        slider_flags |= ImGuiSliderFlags_Logarithmic;   // Fallback for non-asserting paths
    }
    return SliderScalarN(label, data_type, v, components, v_min, v_max, format, slider_flags);
}

#endif // IMGUI_DISABLE_OBSOLETE_FUNCTIONS

//-------------------------------------------------------------------------
// [SECTION] Widgets: InputScalar, InputFloat, InputInt, etc.
//-------------------------------------------------------------------------
// - ImParseFormatFindStart() [Internal]
// - ImParseFormatFindEnd() [Internal]
// - ImParseFormatTrimDecorations() [Internal]
// - ImParseFormatPrecision() [Internal]
// - TempInputTextScalar() [Internal]
// - InputScalar()
// - InputScalarN()
// - InputFloat()
// - InputFloat2()
// - InputFloat3()
// - InputFloat4()
// - InputInt()
// - InputInt2()
// - InputInt3()
// - InputInt4()
// - InputDouble()
//-------------------------------------------------------------------------

// We don't use strchr() because our strings are usually very short and often start with '%'
const char* ImParseFormatFindStart(const char* fmt)
{
    while (char c = fmt[0])
    {
        if (c == '%' && fmt[1] != '%')
            return fmt;
        else if (c == '%')
            fmt++;
        fmt++;
    }
    return fmt;
}

const char* ImParseFormatFindEnd(const char* fmt)
{
    // Printf/scanf types modifiers: I/L/h/j/l/t/w/z. Other uppercase letters qualify as types aka end of the format.
    if (fmt[0] != '%')
        return fmt;
    const unsigned int ignored_uppercase_mask = (1 << ('I'-'A')) | (1 << ('L'-'A'));
    const unsigned int ignored_lowercase_mask = (1 << ('h'-'a')) | (1 << ('j'-'a')) | (1 << ('l'-'a')) | (1 << ('t'-'a')) | (1 << ('w'-'a')) | (1 << ('z'-'a'));
    for (char c; (c = *fmt) != 0; fmt++)
    {
        if (c >= 'A' && c <= 'Z' && ((1 << (c - 'A')) & ignored_uppercase_mask) == 0)
            return fmt + 1;
        if (c >= 'a' && c <= 'z' && ((1 << (c - 'a')) & ignored_lowercase_mask) == 0)
            return fmt + 1;
    }
    return fmt;
}

// Extract the format out of a format string with leading or trailing decorations
//  fmt = "blah blah"  -> return fmt
//  fmt = "%.3f"       -> return fmt
//  fmt = "hello %.3f" -> return fmt + 6
//  fmt = "%.3f hello" -> return buf written with "%.3f"
const char* ImParseFormatTrimDecorations(const char* fmt, char* buf, size_t buf_size)
{
    const char* fmt_start = ImParseFormatFindStart(fmt);
    if (fmt_start[0] != '%')
        return fmt;
    const char* fmt_end = ImParseFormatFindEnd(fmt_start);
    if (fmt_end[0] == 0) // If we only have leading decoration, we don't need to copy the data.
        return fmt_start;
    ImStrncpy(buf, fmt_start, ImMin((size_t)(fmt_end - fmt_start) + 1, buf_size));
    return buf;
}

// Parse display precision back from the display format string
// FIXME: This is still used by some navigation code path to infer a minimum tweak step, but we should aim to rework widgets so it isn't needed.
int ImParseFormatPrecision(const char* fmt, int default_precision)
{
    fmt = ImParseFormatFindStart(fmt);
    if (fmt[0] != '%')
        return default_precision;
    fmt++;
    while (*fmt >= '0' && *fmt <= '9')
        fmt++;
    int precision = INT_MAX;
    if (*fmt == '.')
    {
        fmt = ImAtoi<int>(fmt + 1, &precision);
        if (precision < 0 || precision > 99)
            precision = default_precision;
    }
    if (*fmt == 'e' || *fmt == 'E') // Maximum precision with scientific notation
        precision = -1;
    if ((*fmt == 'g' || *fmt == 'G') && precision == INT_MAX)
        precision = -1;
    return (precision == INT_MAX) ? default_precision : precision;
}

// Create text input in place of another active widget (e.g. used when doing a CTRL+Click on drag/slider widgets)
// FIXME: Facilitate using this in variety of other situations.
bool ImGui::TempInputText(const ImRect& bb, ImGuiID id, const char* label, char* buf, int buf_size, ImGuiInputTextFlags flags)
{
    // On the first frame, g.TempInputTextId == 0, then on subsequent frames it becomes == id.
    // We clear ActiveID on the first frame to allow the InputText() taking it back.
    ImGuiContext& g = *GImGui;
    const bool init = (g.TempInputId != id);
    if (init)
        ClearActiveID();

    g.CurrentWindow->DC.CursorPos = bb.Min;
    bool value_changed = InputTextEx(label, NULL, buf, buf_size, bb.GetSize(), flags);
    if (init)
    {
        // First frame we started displaying the InputText widget, we expect it to take the active id.
        IM_ASSERT(g.ActiveId == id);
        g.TempInputId = g.ActiveId;
    }
    return value_changed;
}

// Note that Drag/Slider functions are only forwarding the min/max values clamping values if the ImGuiSliderFlags_AlwaysClamp flag is set!
// This is intended: this way we allow CTRL+Click manual input to set a value out of bounds, for maximum flexibility.
// However this may not be ideal for all uses, as some user code may break on out of bound values.
bool ImGui::TempInputScalar(const ImRect& bb, ImGuiID id, const char* label, ImGuiDataType data_type, void* p_data, const char* format, const void* p_clamp_min, const void* p_clamp_max)
{
    ImGuiContext& g = *GImGui;

    char fmt_buf[32];
    char data_buf[32];
    format = ImParseFormatTrimDecorations(format, fmt_buf, IM_ARRAYSIZE(fmt_buf));
    DataTypeFormatString(data_buf, IM_ARRAYSIZE(data_buf), data_type, p_data, format);
    ImStrTrimBlanks(data_buf);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoMarkEdited;
    flags |= ((data_type == ImGuiDataType_Float || data_type == ImGuiDataType_Double) ? ImGuiInputTextFlags_CharsScientific : ImGuiInputTextFlags_CharsDecimal);
    bool value_changed = false;
    if (TempInputText(bb, id, label, data_buf, IM_ARRAYSIZE(data_buf), flags))
    {
        // Backup old value
        size_t data_type_size = DataTypeGetInfo(data_type)->Size;
        ImGuiDataTypeTempStorage data_backup;
        memcpy(&data_backup, p_data, data_type_size);

        // Apply new value (or operations) then clamp
        DataTypeApplyOpFromText(data_buf, g.InputTextState.InitialTextA.Data, data_type, p_data, NULL);
        if (p_clamp_min || p_clamp_max)
        {
            if (DataTypeCompare(data_type, p_clamp_min, p_clamp_max) > 0)
                ImSwap(p_clamp_min, p_clamp_max);
            DataTypeClamp(data_type, p_data, p_clamp_min, p_clamp_max);
        }

        // Only mark as edited if new value is different
        value_changed = memcmp(&data_backup, p_data, data_type_size) != 0;
        if (value_changed)
            MarkItemEdited(id);
    }
    return value_changed;
}

// Note: p_data, p_step, p_step_fast are _pointers_ to a memory address holding the data. For an Input widget, p_step and p_step_fast are optional.
// Read code of e.g. InputFloat(), InputInt() etc. or examples in 'Demo->Widgets->Data Types' to understand how to use this function directly.
bool ImGui::InputScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;

    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;

    char buf[64];
    DataTypeFormatString(buf, IM_ARRAYSIZE(buf), data_type, p_data, format);

    bool value_changed = false;
    if ((flags & (ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsScientific)) == 0)
        flags |= ImGuiInputTextFlags_CharsDecimal;
    flags |= ImGuiInputTextFlags_AutoSelectAll;
    flags |= ImGuiInputTextFlags_NoMarkEdited;  // We call MarkItemEdited() ourselves by comparing the actual data rather than the string.

    if (p_step != NULL)
    {
        const float button_size = GetFrameHeight();

        BeginGroup(); // The only purpose of the group here is to allow the caller to query item data e.g. IsItemActive()
        PushID(label);
        SetNextItemWidth(ImMax(1.0f, CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * 2));
        if (InputText("", buf, IM_ARRAYSIZE(buf), flags)) // PushId(label) + "" gives us the expected ID from outside point of view
            value_changed = DataTypeApplyOpFromText(buf, g.InputTextState.InitialTextA.Data, data_type, p_data, format);

        // Step buttons
        const ImVec2 backup_frame_padding = style.FramePadding;
        style.FramePadding.x = style.FramePadding.y;
        ImGuiButtonFlags button_flags = ImGuiButtonFlags_Repeat | ImGuiButtonFlags_DontClosePopups;
        if (flags & ImGuiInputTextFlags_ReadOnly)
            button_flags |= ImGuiButtonFlags_Disabled;
        SameLine(0, style.ItemInnerSpacing.x);
        if (ButtonEx("-", ImVec2(button_size, button_size), button_flags))
        {
            DataTypeApplyOp(data_type, '-', p_data, p_data, g.IO.KeyCtrl && p_step_fast ? p_step_fast : p_step);
            value_changed = true;
        }
        SameLine(0, style.ItemInnerSpacing.x);
        if (ButtonEx("+", ImVec2(button_size, button_size), button_flags))
        {
            DataTypeApplyOp(data_type, '+', p_data, p_data, g.IO.KeyCtrl && p_step_fast ? p_step_fast : p_step);
            value_changed = true;
        }

        const char* label_end = FindRenderedTextEnd(label);
        if (label != label_end)
        {
            SameLine(0, style.ItemInnerSpacing.x);
            TextEx(label, label_end);
        }
        style.FramePadding = backup_frame_padding;

        PopID();
        EndGroup();
    }
    else
    {
        if (InputText(label, buf, IM_ARRAYSIZE(buf), flags))
            value_changed = DataTypeApplyOpFromText(buf, g.InputTextState.InitialTextA.Data, data_type, p_data, format);
    }
    if (value_changed)
        MarkItemEdited(window->DC.LastItemId);

    return value_changed;
}

bool ImGui::InputScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    bool value_changed = false;
    BeginGroup();
    PushID(label);
    PushMultiItemsWidths(components, CalcItemWidth());
    size_t type_size = GDataTypeInfo[data_type].Size;
    for (int i = 0; i < components; i++)
    {
        PushID(i);
        if (i > 0)
            SameLine(0, g.Style.ItemInnerSpacing.x);
        value_changed |= InputScalar("", data_type, p_data, p_step, p_step_fast, format, flags);
        PopID();
        PopItemWidth();
        p_data = (void*)((char*)p_data + type_size);
    }
    PopID();

    const char* label_end = FindRenderedTextEnd(label);
    if (label != label_end)
    {
        SameLine(0.0f, g.Style.ItemInnerSpacing.x);
        TextEx(label, label_end);
    }

    EndGroup();
    return value_changed;
}

bool ImGui::InputFloat(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags)
{
    flags |= ImGuiInputTextFlags_CharsScientific;
    return InputScalar(label, ImGuiDataType_Float, (void*)v, (void*)(step > 0.0f ? &step : NULL), (void*)(step_fast > 0.0f ? &step_fast : NULL), format, flags);
}

bool ImGui::InputFloat2(const char* label, float v[2], const char* format, ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_Float, v, 2, NULL, NULL, format, flags);
}

bool ImGui::InputFloat3(const char* label, float v[3], const char* format, ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_Float, v, 3, NULL, NULL, format, flags);
}

bool ImGui::InputFloat4(const char* label, float v[4], const char* format, ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_Float, v, 4, NULL, NULL, format, flags);
}

bool ImGui::InputInt(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags)
{
    // Hexadecimal input provided as a convenience but the flag name is awkward. Typically you'd use InputText() to parse your own data, if you want to handle prefixes.
    const char* format = (flags & ImGuiInputTextFlags_CharsHexadecimal) ? "%08X" : "%d";
    return InputScalar(label, ImGuiDataType_S32, (void*)v, (void*)(step > 0 ? &step : NULL), (void*)(step_fast > 0 ? &step_fast : NULL), format, flags);
}

bool ImGui::InputInt2(const char* label, int v[2], ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_S32, v, 2, NULL, NULL, "%d", flags);
}

bool ImGui::InputInt3(const char* label, int v[3], ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_S32, v, 3, NULL, NULL, "%d", flags);
}

bool ImGui::InputInt4(const char* label, int v[4], ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_S32, v, 4, NULL, NULL, "%d", flags);
}

bool ImGui::InputDouble(const char* label, double* v, double step, double step_fast, const char* format, ImGuiInputTextFlags flags)
{
    flags |= ImGuiInputTextFlags_CharsScientific;
    return InputScalar(label, ImGuiDataType_Double, (void*)v, (void*)(step > 0.0 ? &step : NULL), (void*)(step_fast > 0.0 ? &step_fast : NULL), format, flags);
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: InputText, InputTextMultiline, InputTextWithHint
//-------------------------------------------------------------------------
// - InputText()
// - InputTextWithHint()
// - InputTextMultiline()
// - InputTextEx() [Internal]
//-------------------------------------------------------------------------

bool ImGui::InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT(!(flags & ImGuiInputTextFlags_Multiline)); // call InputTextMultiline()
    return InputTextEx(label, NULL, buf, (int)buf_size, ImVec2(0, 0), flags, callback, user_data);
}

bool ImGui::InputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    return InputTextEx(label, NULL, buf, (int)buf_size, size, flags | ImGuiInputTextFlags_Multiline, callback, user_data);
}

bool ImGui::InputTextWithHint(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    IM_ASSERT(!(flags & ImGuiInputTextFlags_Multiline)); // call InputTextMultiline()
    return InputTextEx(label, hint, buf, (int)buf_size, ImVec2(0, 0), flags, callback, user_data);
}

static int InputTextCalcTextLenAndLineCount(const char* text_begin, const char** out_text_end)
{
    int line_count = 0;
    const char* s = text_begin;
    while (char c = *s++) // We are only matching for \n so we can ignore UTF-8 decoding
        if (c == '\n')
            line_count++;
    s--;
    if (s[0] != '\n' && s[0] != '\r')
        line_count++;
    *out_text_end = s;
    return line_count;
}

static ImVec2 InputTextCalcTextSizeW(const ImWchar* text_begin, const ImWchar* text_end, const ImWchar** remaining, ImVec2* out_offset, bool stop_on_new_line)
{
    ImGuiContext& g = *GImGui;
    ImFont* font = g.Font;
    const float line_height = g.FontSize;
    const float scale = line_height / font->FontSize;

    ImVec2 text_size = ImVec2(0, 0);
    float line_width = 0.0f;

    const ImWchar* s = text_begin;
    while (s < text_end)
    {
        unsigned int c = (unsigned int)(*s++);
        if (c == '\n')
        {
            text_size.x = ImMax(text_size.x, line_width);
            text_size.y += line_height;
            line_width = 0.0f;
            if (stop_on_new_line)
                break;
            continue;
        }
        if (c == '\r')
            continue;

        const float char_width = font->GetCharAdvance((ImWchar)c) * scale;
        line_width += char_width;
    }

    if (text_size.x < line_width)
        text_size.x = line_width;

    if (out_offset)
        *out_offset = ImVec2(line_width, text_size.y + line_height);  // offset allow for the possibility of sitting after a trailing \n

    if (line_width > 0 || text_size.y == 0.0f)                        // whereas size.y will ignore the trailing \n
        text_size.y += line_height;

    if (remaining)
        *remaining = s;

    return text_size;
}

// Wrapper for stb_textedit.h to edit text (our wrapper is for: statically sized buffer, single-line, wchar characters. InputText converts between UTF-8 and wchar)
namespace ImStb
{

static int     STB_TEXTEDIT_STRINGLEN(const STB_TEXTEDIT_STRING* obj)                             { return obj->CurLenW; }
static ImWchar STB_TEXTEDIT_GETCHAR(const STB_TEXTEDIT_STRING* obj, int idx)                      { return obj->TextW[idx]; }
static float   STB_TEXTEDIT_GETWIDTH(STB_TEXTEDIT_STRING* obj, int line_start_idx, int char_idx)  { ImWchar c = obj->TextW[line_start_idx + char_idx]; if (c == '\n') return STB_TEXTEDIT_GETWIDTH_NEWLINE; ImGuiContext& g = *GImGui; return g.Font->GetCharAdvance(c) * (g.FontSize / g.Font->FontSize); }
static int     STB_TEXTEDIT_KEYTOTEXT(int key)                                                    { return key >= 0x200000 ? 0 : key; }
static ImWchar STB_TEXTEDIT_NEWLINE = '\n';
static void    STB_TEXTEDIT_LAYOUTROW(StbTexteditRow* r, STB_TEXTEDIT_STRING* obj, int line_start_idx)
{
    const ImWchar* text = obj->TextW.Data;
    const ImWchar* text_remaining = NULL;
    const ImVec2 size = InputTextCalcTextSizeW(text + line_start_idx, text + obj->CurLenW, &text_remaining, NULL, true);
    r->x0 = 0.0f;
    r->x1 = size.x;
    r->baseline_y_delta = size.y;
    r->ymin = 0.0f;
    r->ymax = size.y;
    r->num_chars = (int)(text_remaining - (text + line_start_idx));
}

static bool is_separator(unsigned int c)                                        { return ImCharIsBlankW(c) || c==',' || c==';' || c=='(' || c==')' || c=='{' || c=='}' || c=='[' || c==']' || c=='|'; }
static int  is_word_boundary_from_right(STB_TEXTEDIT_STRING* obj, int idx)      { return idx > 0 ? (is_separator(obj->TextW[idx - 1]) && !is_separator(obj->TextW[idx]) ) : 1; }
static int  STB_TEXTEDIT_MOVEWORDLEFT_IMPL(STB_TEXTEDIT_STRING* obj, int idx)   { idx--; while (idx >= 0 && !is_word_boundary_from_right(obj, idx)) idx--; return idx < 0 ? 0 : idx; }
#ifdef __APPLE__    // FIXME: Move setting to IO structure
static int  is_word_boundary_from_left(STB_TEXTEDIT_STRING* obj, int idx)       { return idx > 0 ? (!is_separator(obj->TextW[idx - 1]) && is_separator(obj->TextW[idx]) ) : 1; }
static int  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL(STB_TEXTEDIT_STRING* obj, int idx)  { idx++; int len = obj->CurLenW; while (idx < len && !is_word_boundary_from_left(obj, idx)) idx++; return idx > len ? len : idx; }
#else
static int  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL(STB_TEXTEDIT_STRING* obj, int idx)  { idx++; int len = obj->CurLenW; while (idx < len && !is_word_boundary_from_right(obj, idx)) idx++; return idx > len ? len : idx; }
#endif
#define STB_TEXTEDIT_MOVEWORDLEFT   STB_TEXTEDIT_MOVEWORDLEFT_IMPL    // They need to be #define for stb_textedit.h
#define STB_TEXTEDIT_MOVEWORDRIGHT  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL

static void STB_TEXTEDIT_DELETECHARS(STB_TEXTEDIT_STRING* obj, int pos, int n)
{
    ImWchar* dst = obj->TextW.Data + pos;

    // We maintain our buffer length in both UTF-8 and wchar formats
    obj->Edited = true;
    obj->CurLenA -= ImTextCountUtf8BytesFromStr(dst, dst + n);
    obj->CurLenW -= n;

    // Offset remaining text (FIXME-OPT: Use memmove)
    const ImWchar* src = obj->TextW.Data + pos + n;
    while (ImWchar c = *src++)
        *dst++ = c;
    *dst = '\0';
}

static bool STB_TEXTEDIT_INSERTCHARS(STB_TEXTEDIT_STRING* obj, int pos, const ImWchar* new_text, int new_text_len)
{
    const bool is_resizable = (obj->UserFlags & ImGuiInputTextFlags_CallbackResize) != 0;
    const int text_len = obj->CurLenW;
    IM_ASSERT(pos <= text_len);

    const int new_text_len_utf8 = ImTextCountUtf8BytesFromStr(new_text, new_text + new_text_len);
    if (!is_resizable && (new_text_len_utf8 + obj->CurLenA + 1 > obj->BufCapacityA))
        return false;

    // Grow internal buffer if needed
    if (new_text_len + text_len + 1 > obj->TextW.Size)
    {
        if (!is_resizable)
            return false;
        IM_ASSERT(text_len < obj->TextW.Size);
        obj->TextW.resize(text_len + ImClamp(new_text_len * 4, 32, ImMax(256, new_text_len)) + 1);
    }

    ImWchar* text = obj->TextW.Data;
    if (pos != text_len)
        memmove(text + pos + new_text_len, text + pos, (size_t)(text_len - pos) * sizeof(ImWchar));
    memcpy(text + pos, new_text, (size_t)new_text_len * sizeof(ImWchar));

    obj->Edited = true;
    obj->CurLenW += new_text_len;
    obj->CurLenA += new_text_len_utf8;
    obj->TextW[obj->CurLenW] = '\0';

    return true;
}

// We don't use an enum so we can build even with conflicting symbols (if another user of stb_textedit.h leak their STB_TEXTEDIT_K_* symbols)
#define STB_TEXTEDIT_K_LEFT         0x200000 // keyboard input to move cursor left
#define STB_TEXTEDIT_K_RIGHT        0x200001 // keyboard input to move cursor right
#define STB_TEXTEDIT_K_UP           0x200002 // keyboard input to move cursor up
#define STB_TEXTEDIT_K_DOWN         0x200003 // keyboard input to move cursor down
#define STB_TEXTEDIT_K_LINESTART    0x200004 // keyboard input to move cursor to start of line
#define STB_TEXTEDIT_K_LINEEND      0x200005 // keyboard input to move cursor to end of line
#define STB_TEXTEDIT_K_TEXTSTART    0x200006 // keyboard input to move cursor to start of text
#define STB_TEXTEDIT_K_TEXTEND      0x200007 // keyboard input to move cursor to end of text
#define STB_TEXTEDIT_K_DELETE       0x200008 // keyboard input to delete selection or character under cursor
#define STB_TEXTEDIT_K_BACKSPACE    0x200009 // keyboard input to delete selection or character left of cursor
#define STB_TEXTEDIT_K_UNDO         0x20000A // keyboard input to perform undo
#define STB_TEXTEDIT_K_REDO         0x20000B // keyboard input to perform redo
#define STB_TEXTEDIT_K_WORDLEFT     0x20000C // keyboard input to move cursor left one word
#define STB_TEXTEDIT_K_WORDRIGHT    0x20000D // keyboard input to move cursor right one word
#define STB_TEXTEDIT_K_PGUP         0x20000E // keyboard input to move cursor up a page
#define STB_TEXTEDIT_K_PGDOWN       0x20000F // keyboard input to move cursor down a page
#define STB_TEXTEDIT_K_SHIFT        0x400000

#define STB_TEXTEDIT_IMPLEMENTATION
#include "imstb_textedit.h"

// stb_textedit internally allows for a single undo record to do addition and deletion, but somehow, calling
// the stb_textedit_paste() function creates two separate records, so we perform it manually. (FIXME: Report to nothings/stb?)
static void stb_textedit_replace(STB_TEXTEDIT_STRING* str, STB_TexteditState* state, const STB_TEXTEDIT_CHARTYPE* text, int text_len)
{
    stb_text_makeundo_replace(str, state, 0, str->CurLenW, text_len);
    ImStb::STB_TEXTEDIT_DELETECHARS(str, 0, str->CurLenW);
    if (text_len <= 0)
        return;
    if (ImStb::STB_TEXTEDIT_INSERTCHARS(str, 0, text, text_len))
    {
        state->cursor = text_len;
        state->has_preferred_x = 0;
        return;
    }
    IM_ASSERT(0); // Failed to insert character, normally shouldn't happen because of how we currently use stb_textedit_replace()
}

} // namespace ImStb

void ImGuiInputTextState::OnKeyPressed(int key)
{
    stb_textedit_key(this, &Stb, key);
    CursorFollow = true;
    CursorAnimReset();
}

ImGuiInputTextCallbackData::ImGuiInputTextCallbackData()
{
    memset(this, 0, sizeof(*this));
}

// Public API to manipulate UTF-8 text
// We expose UTF-8 to the user (unlike the STB_TEXTEDIT_* functions which are manipulating wchar)
// FIXME: The existence of this rarely exercised code path is a bit of a nuisance.
void ImGuiInputTextCallbackData::DeleteChars(int pos, int bytes_count)
{
    IM_ASSERT(pos + bytes_count <= BufTextLen);
    char* dst = Buf + pos;
    const char* src = Buf + pos + bytes_count;
    while (char c = *src++)
        *dst++ = c;
    *dst = '\0';

    if (CursorPos >= pos + bytes_count)
        CursorPos -= bytes_count;
    else if (CursorPos >= pos)
        CursorPos = pos;
    SelectionStart = SelectionEnd = CursorPos;
    BufDirty = true;
    BufTextLen -= bytes_count;
}

void ImGuiInputTextCallbackData::InsertChars(int pos, const char* new_text, const char* new_text_end)
{
    const bool is_resizable = (Flags & ImGuiInputTextFlags_CallbackResize) != 0;
    const int new_text_len = new_text_end ? (int)(new_text_end - new_text) : (int)strlen(new_text);
    if (new_text_len + BufTextLen >= BufSize)
    {
        if (!is_resizable)
            return;

        // Contrary to STB_TEXTEDIT_INSERTCHARS() this is working in the UTF8 buffer, hence the mildly similar code (until we remove the U16 buffer altogether!)
        ImGuiContext& g = *GImGui;
        ImGuiInputTextState* edit_state = &g.InputTextState;
        IM_ASSERT(edit_state->ID != 0 && g.ActiveId == edit_state->ID);
        IM_ASSERT(Buf == edit_state->TextA.Data);
        int new_buf_size = BufTextLen + ImClamp(new_text_len * 4, 32, ImMax(256, new_text_len)) + 1;
        edit_state->TextA.reserve(new_buf_size + 1);
        Buf = edit_state->TextA.Data;
        BufSize = edit_state->BufCapacityA = new_buf_size;
    }

    if (BufTextLen != pos)
        memmove(Buf + pos + new_text_len, Buf + pos, (size_t)(BufTextLen - pos));
    memcpy(Buf + pos, new_text, (size_t)new_text_len * sizeof(char));
    Buf[BufTextLen + new_text_len] = '\0';

    if (CursorPos >= pos)
        CursorPos += new_text_len;
    SelectionStart = SelectionEnd = CursorPos;
    BufDirty = true;
    BufTextLen += new_text_len;
}

// Return false to discard a character.
static bool InputTextFilterCharacter(unsigned int* p_char, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
    unsigned int c = *p_char;

    // Filter non-printable (NB: isprint is unreliable! see #2467)
    if (c < 0x20)
    {
        bool pass = false;
        pass |= (c == '\n' && (flags & ImGuiInputTextFlags_Multiline));
        pass |= (c == '\t' && (flags & ImGuiInputTextFlags_AllowTabInput));
        if (!pass)
            return false;
    }

    // We ignore Ascii representation of delete (emitted from Backspace on OSX, see #2578, #2817)
    if (c == 127)
        return false;

    // Filter private Unicode range. GLFW on OSX seems to send private characters for special keys like arrow keys (FIXME)
    if (c >= 0xE000 && c <= 0xF8FF)
        return false;

    // Filter Unicode ranges we are not handling in this build.
    if (c > IM_UNICODE_CODEPOINT_MAX)
        return false;

    // Generic named filters
    if (flags & (ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsScientific))
    {
        // The libc allows overriding locale, with e.g. 'setlocale(LC_NUMERIC, "de_DE.UTF-8");' which affect the output/input of printf/scanf.
        // The standard mandate that programs starts in the "C" locale where the decimal point is '.'.
        // We don't really intend to provide widespread support for it, but out of empathy for people stuck with using odd API, we support the bare minimum aka overriding the decimal point.
        // Change the default decimal_point with:
        //   ImGui::GetCurrentContext()->PlatformLocaleDecimalPoint = *localeconv()->decimal_point;
        ImGuiContext& g = *GImGui;
        const unsigned c_decimal_point = (unsigned int)g.PlatformLocaleDecimalPoint;

        // Allow 0-9 . - + * /
        if (flags & ImGuiInputTextFlags_CharsDecimal)
            if (!(c >= '0' && c <= '9') && (c != c_decimal_point) && (c != '-') && (c != '+') && (c != '*') && (c != '/'))
                return false;

        // Allow 0-9 . - + * / e E
        if (flags & ImGuiInputTextFlags_CharsScientific)
            if (!(c >= '0' && c <= '9') && (c != c_decimal_point) && (c != '-') && (c != '+') && (c != '*') && (c != '/') && (c != 'e') && (c != 'E'))
                return false;

        // Allow 0-9 a-F A-F
        if (flags & ImGuiInputTextFlags_CharsHexadecimal)
            if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F'))
                return false;

        // Turn a-z into A-Z
        if (flags & ImGuiInputTextFlags_CharsUppercase)
            if (c >= 'a' && c <= 'z')
                *p_char = (c += (unsigned int)('A' - 'a'));

        if (flags & ImGuiInputTextFlags_CharsNoBlank)
            if (ImCharIsBlankW(c))
                return false;
    }

    // Custom callback filter
    if (flags & ImGuiInputTextFlags_CallbackCharFilter)
    {
        ImGuiInputTextCallbackData callback_data;
        memset(&callback_data, 0, sizeof(ImGuiInputTextCallbackData));
        callback_data.EventFlag = ImGuiInputTextFlags_CallbackCharFilter;
        callback_data.EventChar = (ImWchar)c;
        callback_data.Flags = flags;
        callback_data.UserData = user_data;
        if (callback(&callback_data) != 0)
            return false;
        *p_char = callback_data.EventChar;
        if (!callback_data.EventChar)
            return false;
    }

    return true;
}

// Edit a string of text
// - buf_size account for the zero-terminator, so a buf_size of 6 can hold "Hello" but not "Hello!".
//   This is so we can easily call InputText() on static arrays using ARRAYSIZE() and to match
//   Note that in std::string world, capacity() would omit 1 byte used by the zero-terminator.
// - When active, hold on a privately held copy of the text (and apply back to 'buf'). So changing 'buf' while the InputText is active has no effect.
// - If you want to use ImGui::InputText() with std::string, see misc/cpp/imgui_stdlib.h
// (FIXME: Rather confusing and messy function, among the worse part of our codebase, expecting to rewrite a V2 at some point.. Partly because we are
//  doing UTF8 > U16 > UTF8 conversions on the go to easily interface with stb_textedit. Ideally should stay in UTF-8 all the time. See https://github.com/nothings/stb/issues/188)
bool ImGui::InputTextEx(const char* label, const char* hint, char* buf, int buf_size, const ImVec2& size_arg, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* callback_user_data)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    IM_ASSERT(buf != NULL && buf_size >= 0);
    IM_ASSERT(!((flags & ImGuiInputTextFlags_CallbackHistory) && (flags & ImGuiInputTextFlags_Multiline)));        // Can't use both together (they both use up/down keys)
    IM_ASSERT(!((flags & ImGuiInputTextFlags_CallbackCompletion) && (flags & ImGuiInputTextFlags_AllowTabInput))); // Can't use both together (they both use tab key)

    ImGuiContext& g = *GImGui;
    ImGuiIO& io = g.IO;
    const ImGuiStyle& style = g.Style;

    const bool RENDER_SELECTION_WHEN_INACTIVE = false;
    const bool is_multiline = (flags & ImGuiInputTextFlags_Multiline) != 0;
    const bool is_readonly = (flags & ImGuiInputTextFlags_ReadOnly) != 0;
    const bool is_password = (flags & ImGuiInputTextFlags_Password) != 0;
    const bool is_undoable = (flags & ImGuiInputTextFlags_NoUndoRedo) == 0;
    const bool is_resizable = (flags & ImGuiInputTextFlags_CallbackResize) != 0;
    if (is_resizable)
        IM_ASSERT(callback != NULL); // Must provide a callback if you set the ImGuiInputTextFlags_CallbackResize flag!

    if (is_multiline) // Open group before calling GetID() because groups tracks id created within their scope,
        BeginGroup();
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    const ImVec2 frame_size = CalcItemSize(size_arg, CalcItemWidth(), (is_multiline ? g.FontSize * 8.0f : label_size.y) + style.FramePadding.y * 2.0f); // Arbitrary default of 8 lines high for multi-line
    const ImVec2 total_size = ImVec2(frame_size.x + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), frame_size.y);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    const ImRect total_bb(frame_bb.Min, frame_bb.Min + total_size);

    ImGuiWindow* draw_window = window;
    ImVec2 inner_size = frame_size;
    if (is_multiline)
    {
        if (!ItemAdd(total_bb, id, &frame_bb))
        {
            ItemSize(total_bb, style.FramePadding.y);
            EndGroup();
            return false;
        }

        // We reproduce the contents of BeginChildFrame() in order to provide 'label' so our window internal data are easier to read/debug.
        PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_FrameBg]);
        PushStyleVar(ImGuiStyleVar_ChildRounding, style.FrameRounding);
        PushStyleVar(ImGuiStyleVar_ChildBorderSize, style.FrameBorderSize);
        PushStyleVar(ImGuiStyleVar_WindowPadding, style.FramePadding);
        bool child_visible = BeginChildEx(label, id, frame_bb.GetSize(), true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysUseWindowPadding);
        PopStyleVar(3);
        PopStyleColor();
        if (!child_visible)
        {
            EndChild();
            EndGroup();
            return false;
        }
        draw_window = g.CurrentWindow; // Child window
        draw_window->DC.NavLayerActiveMaskNext |= (1 << draw_window->DC.NavLayerCurrent); // This is to ensure that EndChild() will display a navigation highlight so we can "enter" into it.
        inner_size.x -= draw_window->ScrollbarSizes.x;
    }
    else
    {
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id, &frame_bb))
            return false;
    }
    const bool hovered = ItemHoverable(frame_bb, id);
    if (hovered)
        g.MouseCursor = ImGuiMouseCursor_TextInput;

    // We are only allowed to access the state if we are already the active widget.
    ImGuiInputTextState* state = GetInputTextState(id);

    const bool focus_requested = FocusableItemRegister(window, id);
    const bool focus_requested_by_code = focus_requested && (g.FocusRequestCurrWindow == window && g.FocusRequestCurrCounterRegular == window->DC.FocusCounterRegular);
    const bool focus_requested_by_tab = focus_requested && !focus_requested_by_code;

    const bool user_clicked = hovered && io.MouseClicked[0];
    const bool user_nav_input_start = (g.ActiveId != id) && ((g.NavInputId == id) || (g.NavActivateId == id && g.NavInputSource == ImGuiInputSource_NavKeyboard));
    const bool user_scroll_finish = is_multiline && state != NULL && g.ActiveId == 0 && g.ActiveIdPreviousFrame == GetWindowScrollbarID(draw_window, ImGuiAxis_Y);
    const bool user_scroll_active = is_multiline && state != NULL && g.ActiveId == GetWindowScrollbarID(draw_window, ImGuiAxis_Y);

    bool clear_active_id = false;
    bool select_all = (g.ActiveId != id) && ((flags & ImGuiInputTextFlags_AutoSelectAll) != 0 || user_nav_input_start) && (!is_multiline);

    float scroll_y = is_multiline ? draw_window->Scroll.y : FLT_MAX;

    const bool init_make_active = (focus_requested || user_clicked || user_scroll_finish || user_nav_input_start);
    const bool init_state = (init_make_active || user_scroll_active);
    if (init_state && g.ActiveId != id)
    {
        // Access state even if we don't own it yet.
        state = &g.InputTextState;
        state->CursorAnimReset();

        // Take a copy of the initial buffer value (both in original UTF-8 format and converted to wchar)
        // From the moment we focused we are ignoring the content of 'buf' (unless we are in read-only mode)
        const int buf_len = (int)strlen(buf);
        state->InitialTextA.resize(buf_len + 1);    // UTF-8. we use +1 to make sure that .Data is always pointing to at least an empty string.
        memcpy(state->InitialTextA.Data, buf, buf_len + 1);

        // Start edition
        const char* buf_end = NULL;
        state->TextW.resize(buf_size + 1);          // wchar count <= UTF-8 count. we use +1 to make sure that .Data is always pointing to at least an empty string.
        state->TextA.resize(0);
        state->TextAIsValid = false;                // TextA is not valid yet (we will display buf until then)
        state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, buf_size, buf, NULL, &buf_end);
        state->CurLenA = (int)(buf_end - buf);      // We can't get the result from ImStrncpy() above because it is not UTF-8 aware. Here we'll cut off malformed UTF-8.

        // Preserve cursor position and undo/redo stack if we come back to same widget
        // FIXME: For non-readonly widgets we might be able to require that TextAIsValid && TextA == buf ? (untested) and discard undo stack if user buffer has changed.
        const bool recycle_state = (state->ID == id);
        if (recycle_state)
        {
            // Recycle existing cursor/selection/undo stack but clamp position
            // Note a single mouse click will override the cursor/position immediately by calling stb_textedit_click handler.
            state->CursorClamp();
        }
        else
        {
            state->ID = id;
            state->ScrollX = 0.0f;
            stb_textedit_initialize_state(&state->Stb, !is_multiline);
            if (!is_multiline && focus_requested_by_code)
                select_all = true;
        }
        if (flags & ImGuiInputTextFlags_AlwaysInsertMode)
            state->Stb.insert_mode = 1;
        if (!is_multiline && (focus_requested_by_tab || (user_clicked && io.KeyCtrl)))
            select_all = true;
    }

    if (g.ActiveId != id && init_make_active)
    {
        IM_ASSERT(state && state->ID == id);
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);

        // Declare our inputs
        IM_ASSERT(ImGuiNavInput_COUNT < 32);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        if (is_multiline || (flags & ImGuiInputTextFlags_CallbackHistory))
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
        g.ActiveIdUsingNavInputMask |= (1 << ImGuiNavInput_Cancel);
        g.ActiveIdUsingKeyInputMask |= ((ImU64)1 << ImGuiKey_Home) | ((ImU64)1 << ImGuiKey_End);
        if (is_multiline)
            g.ActiveIdUsingKeyInputMask |= ((ImU64)1 << ImGuiKey_PageUp) | ((ImU64)1 << ImGuiKey_PageDown);
        if (flags & (ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_AllowTabInput))  // Disable keyboard tabbing out as we will use the \t character.
            g.ActiveIdUsingKeyInputMask |= ((ImU64)1 << ImGuiKey_Tab);
    }

    // We have an edge case if ActiveId was set through another widget (e.g. widget being swapped), clear id immediately (don't wait until the end of the function)
    if (g.ActiveId == id && state == NULL)
        ClearActiveID();

    // Release focus when we click outside
    if (g.ActiveId == id && io.MouseClicked[0] && !init_state && !init_make_active) //-V560
        clear_active_id = true;

    // Lock the decision of whether we are going to take the path displaying the cursor or selection
    const bool render_cursor = (g.ActiveId == id) || (state && user_scroll_active);
    bool render_selection = state && state->HasSelection() && (RENDER_SELECTION_WHEN_INACTIVE || render_cursor);
    bool value_changed = false;
    bool enter_pressed = false;

    // When read-only we always use the live data passed to the function
    // FIXME-OPT: Because our selection/cursor code currently needs the wide text we need to convert it when active, which is not ideal :(
    if (is_readonly && state != NULL && (render_cursor || render_selection))
    {
        const char* buf_end = NULL;
        state->TextW.resize(buf_size + 1);
        state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, state->TextW.Size, buf, NULL, &buf_end);
        state->CurLenA = (int)(buf_end - buf);
        state->CursorClamp();
        render_selection &= state->HasSelection();
    }

    // Select the buffer to render.
    const bool buf_display_from_state = (render_cursor || render_selection || g.ActiveId == id) && !is_readonly && state && state->TextAIsValid;
    const bool is_displaying_hint = (hint != NULL && (buf_display_from_state ? state->TextA.Data : buf)[0] == 0);

    // Password pushes a temporary font with only a fallback glyph
    if (is_password && !is_displaying_hint)
    {
        const ImFontGlyph* glyph = g.Font->FindGlyph('*');
        ImFont* password_font = &g.InputTextPasswordFont;
        password_font->FontSize = g.Font->FontSize;
        password_font->Scale = g.Font->Scale;
        password_font->Ascent = g.Font->Ascent;
        password_font->Descent = g.Font->Descent;
        password_font->ContainerAtlas = g.Font->ContainerAtlas;
        password_font->FallbackGlyph = glyph;
        password_font->FallbackAdvanceX = glyph->AdvanceX;
        IM_ASSERT(password_font->Glyphs.empty() && password_font->IndexAdvanceX.empty() && password_font->IndexLookup.empty());
        PushFont(password_font);
    }

    // Process mouse inputs and character inputs
    int backup_current_text_length = 0;
    if (g.ActiveId == id)
    {
        IM_ASSERT(state != NULL);
        backup_current_text_length = state->CurLenA;
        state->Edited = false;
        state->BufCapacityA = buf_size;
        state->UserFlags = flags;
        state->UserCallback = callback;
        state->UserCallbackData = callback_user_data;

        // Although we are active we don't prevent mouse from hovering other elements unless we are interacting right now with the widget.
        // Down the line we should have a cleaner library-wide concept of Selected vs Active.
        g.ActiveIdAllowOverlap = !io.MouseDown[0];
        g.WantTextInputNextFrame = 1;

        // Edit in progress
        const float mouse_x = (io.MousePos.x - frame_bb.Min.x - style.FramePadding.x) + state->ScrollX;
        const float mouse_y = (is_multiline ? (io.MousePos.y - draw_window->DC.CursorPos.y - style.FramePadding.y) : (g.FontSize * 0.5f));

        const bool is_osx = io.ConfigMacOSXBehaviors;
        if (select_all || (hovered && !is_osx && io.MouseDoubleClicked[0]))
        {
            state->SelectAll();
            state->SelectedAllMouseLock = true;
        }
        else if (hovered && is_osx && io.MouseDoubleClicked[0])
        {
            // Double-click select a word only, OS X style (by simulating keystrokes)
            state->OnKeyPressed(STB_TEXTEDIT_K_WORDLEFT);
            state->OnKeyPressed(STB_TEXTEDIT_K_WORDRIGHT | STB_TEXTEDIT_K_SHIFT);
        }
        else if (io.MouseClicked[0] && !state->SelectedAllMouseLock)
        {
            if (hovered)
            {
                stb_textedit_click(state, &state->Stb, mouse_x, mouse_y);
                state->CursorAnimReset();
            }
        }
        else if (io.MouseDown[0] && !state->SelectedAllMouseLock && (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
        {
            stb_textedit_drag(state, &state->Stb, mouse_x, mouse_y);
            state->CursorAnimReset();
            state->CursorFollow = true;
        }
        if (state->SelectedAllMouseLock && !io.MouseDown[0])
            state->SelectedAllMouseLock = false;

        // It is ill-defined whether the backend needs to send a \t character when pressing the TAB keys.
        // Win32 and GLFW naturally do it but not SDL.
        const bool ignore_char_inputs = (io.KeyCtrl && !io.KeyAlt) || (is_osx && io.KeySuper);
        if ((flags & ImGuiInputTextFlags_AllowTabInput) && IsKeyPressedMap(ImGuiKey_Tab) && !ignore_char_inputs && !io.KeyShift && !is_readonly)
            if (!io.InputQueueCharacters.contains('\t'))
            {
                unsigned int c = '\t'; // Insert TAB
                if (InputTextFilterCharacter(&c, flags, callback, callback_user_data))
                    state->OnKeyPressed((int)c);
            }

        // Process regular text input (before we check for Return because using some IME will effectively send a Return?)
        // We ignore CTRL inputs, but need to allow ALT+CTRL as some keyboards (e.g. German) use AltGR (which _is_ Alt+Ctrl) to input certain characters.
        if (io.InputQueueCharacters.Size > 0)
        {
            if (!ignore_char_inputs && !is_readonly && !user_nav_input_start)
                for (int n = 0; n < io.InputQueueCharacters.Size; n++)
                {
                    // Insert character if they pass filtering
                    unsigned int c = (unsigned int)io.InputQueueCharacters[n];
                    if (c == '\t' && io.KeyShift)
                        continue;
                    if (InputTextFilterCharacter(&c, flags, callback, callback_user_data))
                        state->OnKeyPressed((int)c);
                }

            // Consume characters
            io.InputQueueCharacters.resize(0);
        }
    }

    // Process other shortcuts/key-presses
    bool cancel_edit = false;
    if (g.ActiveId == id && !g.ActiveIdIsJustActivated && !clear_active_id)
    {
        IM_ASSERT(state != NULL);
        IM_ASSERT(io.KeyMods == GetMergedKeyModFlags() && "Mismatching io.KeyCtrl/io.KeyShift/io.KeyAlt/io.KeySuper vs io.KeyMods"); // We rarely do this check, but if anything let's do it here.

        const int row_count_per_page = ImMax((int)((inner_size.y - style.FramePadding.y) / g.FontSize), 1);
        state->Stb.row_count_per_page = row_count_per_page;

        const int k_mask = (io.KeyShift ? STB_TEXTEDIT_K_SHIFT : 0);
        const bool is_osx = io.ConfigMacOSXBehaviors;
        const bool is_osx_shift_shortcut = is_osx && (io.KeyMods == (ImGuiKeyModFlags_Super | ImGuiKeyModFlags_Shift));
        const bool is_wordmove_key_down = is_osx ? io.KeyAlt : io.KeyCtrl;                     // OS X style: Text editing cursor movement using Alt instead of Ctrl
        const bool is_startend_key_down = is_osx && io.KeySuper && !io.KeyCtrl && !io.KeyAlt;  // OS X style: Line/Text Start and End using Cmd+Arrows instead of Home/End
        const bool is_ctrl_key_only = (io.KeyMods == ImGuiKeyModFlags_Ctrl);
        const bool is_shift_key_only = (io.KeyMods == ImGuiKeyModFlags_Shift);
        const bool is_shortcut_key = g.IO.ConfigMacOSXBehaviors ? (io.KeyMods == ImGuiKeyModFlags_Super) : (io.KeyMods == ImGuiKeyModFlags_Ctrl);

        const bool is_cut   = ((is_shortcut_key && IsKeyPressedMap(ImGuiKey_X)) || (is_shift_key_only && IsKeyPressedMap(ImGuiKey_Delete))) && !is_readonly && !is_password && (!is_multiline || state->HasSelection());
        const bool is_copy  = ((is_shortcut_key && IsKeyPressedMap(ImGuiKey_C)) || (is_ctrl_key_only  && IsKeyPressedMap(ImGuiKey_Insert))) && !is_password && (!is_multiline || state->HasSelection());
        const bool is_paste = ((is_shortcut_key && IsKeyPressedMap(ImGuiKey_V)) || (is_shift_key_only && IsKeyPressedMap(ImGuiKey_Insert))) && !is_readonly;
        const bool is_undo  = ((is_shortcut_key && IsKeyPressedMap(ImGuiKey_Z)) && !is_readonly && is_undoable);
        const bool is_redo  = ((is_shortcut_key && IsKeyPressedMap(ImGuiKey_Y)) || (is_osx_shift_shortcut && IsKeyPressedMap(ImGuiKey_Z))) && !is_readonly && is_undoable;

        if (IsKeyPressedMap(ImGuiKey_LeftArrow))                        { state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_LINESTART : is_wordmove_key_down ? STB_TEXTEDIT_K_WORDLEFT : STB_TEXTEDIT_K_LEFT) | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_RightArrow))                  { state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_LINEEND : is_wordmove_key_down ? STB_TEXTEDIT_K_WORDRIGHT : STB_TEXTEDIT_K_RIGHT) | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_UpArrow) && is_multiline)     { if (io.KeyCtrl) SetScrollY(draw_window, ImMax(draw_window->Scroll.y - g.FontSize, 0.0f)); else state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_TEXTSTART : STB_TEXTEDIT_K_UP) | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_DownArrow) && is_multiline)   { if (io.KeyCtrl) SetScrollY(draw_window, ImMin(draw_window->Scroll.y + g.FontSize, GetScrollMaxY())); else state->OnKeyPressed((is_startend_key_down ? STB_TEXTEDIT_K_TEXTEND : STB_TEXTEDIT_K_DOWN) | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_PageUp) && is_multiline)      { state->OnKeyPressed(STB_TEXTEDIT_K_PGUP | k_mask); scroll_y -= row_count_per_page * g.FontSize; }
        else if (IsKeyPressedMap(ImGuiKey_PageDown) && is_multiline)    { state->OnKeyPressed(STB_TEXTEDIT_K_PGDOWN | k_mask); scroll_y += row_count_per_page * g.FontSize; }
        else if (IsKeyPressedMap(ImGuiKey_Home))                        { state->OnKeyPressed(io.KeyCtrl ? STB_TEXTEDIT_K_TEXTSTART | k_mask : STB_TEXTEDIT_K_LINESTART | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_End))                         { state->OnKeyPressed(io.KeyCtrl ? STB_TEXTEDIT_K_TEXTEND | k_mask : STB_TEXTEDIT_K_LINEEND | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_Delete) && !is_readonly)      { state->OnKeyPressed(STB_TEXTEDIT_K_DELETE | k_mask); }
        else if (IsKeyPressedMap(ImGuiKey_Backspace) && !is_readonly)
        {
            if (!state->HasSelection())
            {
                if (is_wordmove_key_down)
                    state->OnKeyPressed(STB_TEXTEDIT_K_WORDLEFT | STB_TEXTEDIT_K_SHIFT);
                else if (is_osx && io.KeySuper && !io.KeyAlt && !io.KeyCtrl)
                    state->OnKeyPressed(STB_TEXTEDIT_K_LINESTART | STB_TEXTEDIT_K_SHIFT);
            }
            state->OnKeyPressed(STB_TEXTEDIT_K_BACKSPACE | k_mask);
        }
        else if (IsKeyPressedMap(ImGuiKey_Enter) || IsKeyPressedMap(ImGuiKey_KeyPadEnter))
        {
            bool ctrl_enter_for_new_line = (flags & ImGuiInputTextFlags_CtrlEnterForNewLine) != 0;
            if (!is_multiline || (ctrl_enter_for_new_line && !io.KeyCtrl) || (!ctrl_enter_for_new_line && io.KeyCtrl))
            {
                enter_pressed = clear_active_id = true;
            }
            else if (!is_readonly)
            {
                unsigned int c = '\n'; // Insert new line
                if (InputTextFilterCharacter(&c, flags, callback, callback_user_data))
                    state->OnKeyPressed((int)c);
            }
        }
        else if (IsKeyPressedMap(ImGuiKey_Escape))
        {
            clear_active_id = cancel_edit = true;
        }
        else if (is_undo || is_redo)
        {
            state->OnKeyPressed(is_undo ? STB_TEXTEDIT_K_UNDO : STB_TEXTEDIT_K_REDO);
            state->ClearSelection();
        }
        else if (is_shortcut_key && IsKeyPressedMap(ImGuiKey_A))
        {
            state->SelectAll();
            state->CursorFollow = true;
        }
        else if (is_cut || is_copy)
        {
            // Cut, Copy
            if (io.SetClipboardTextFn)
            {
                const int ib = state->HasSelection() ? ImMin(state->Stb.select_start, state->Stb.select_end) : 0;
                const int ie = state->HasSelection() ? ImMax(state->Stb.select_start, state->Stb.select_end) : state->CurLenW;
                const int clipboard_data_len = ImTextCountUtf8BytesFromStr(state->TextW.Data + ib, state->TextW.Data + ie) + 1;
                char* clipboard_data = (char*)IM_ALLOC(clipboard_data_len * sizeof(char));
                ImTextStrToUtf8(clipboard_data, clipboard_data_len, state->TextW.Data + ib, state->TextW.Data + ie);
                SetClipboardText(clipboard_data);
                MemFree(clipboard_data);
            }
            if (is_cut)
            {
                if (!state->HasSelection())
                    state->SelectAll();
                state->CursorFollow = true;
                stb_textedit_cut(state, &state->Stb);
            }
        }
        else if (is_paste)
        {
            if (const char* clipboard = GetClipboardText())
            {
                // Filter pasted buffer
                const int clipboard_len = (int)strlen(clipboard);
                ImWchar* clipboard_filtered = (ImWchar*)IM_ALLOC((clipboard_len + 1) * sizeof(ImWchar));
                int clipboard_filtered_len = 0;
                for (const char* s = clipboard; *s; )
                {
                    unsigned int c;
                    s += ImTextCharFromUtf8(&c, s, NULL);
                    if (c == 0)
                        break;
                    if (!InputTextFilterCharacter(&c, flags, callback, callback_user_data))
                        continue;
                    clipboard_filtered[clipboard_filtered_len++] = (ImWchar)c;
                }
                clipboard_filtered[clipboard_filtered_len] = 0;
                if (clipboard_filtered_len > 0) // If everything was filtered, ignore the pasting operation
                {
                    stb_textedit_paste(state, &state->Stb, clipboard_filtered, clipboard_filtered_len);
                    state->CursorFollow = true;
                }
                MemFree(clipboard_filtered);
            }
        }

        // Update render selection flag after events have been handled, so selection highlight can be displayed during the same frame.
        render_selection |= state->HasSelection() && (RENDER_SELECTION_WHEN_INACTIVE || render_cursor);
    }

    // Process callbacks and apply result back to user's buffer.
    if (g.ActiveId == id)
    {
        IM_ASSERT(state != NULL);
        const char* apply_new_text = NULL;
        int apply_new_text_length = 0;
        if (cancel_edit)
        {
            // Restore initial value. Only return true if restoring to the initial value changes the current buffer contents.
            if (!is_readonly && strcmp(buf, state->InitialTextA.Data) != 0)
            {
                // Push records into the undo stack so we can CTRL+Z the revert operation itself
                apply_new_text = state->InitialTextA.Data;
                apply_new_text_length = state->InitialTextA.Size - 1;
                ImVector<ImWchar> w_text;
                if (apply_new_text_length > 0)
                {
                    w_text.resize(ImTextCountCharsFromUtf8(apply_new_text, apply_new_text + apply_new_text_length) + 1);
                    ImTextStrFromUtf8(w_text.Data, w_text.Size, apply_new_text, apply_new_text + apply_new_text_length);
                }
                stb_textedit_replace(state, &state->Stb, w_text.Data, (apply_new_text_length > 0) ? (w_text.Size - 1) : 0);
            }
        }

        // When using 'ImGuiInputTextFlags_EnterReturnsTrue' as a special case we reapply the live buffer back to the input buffer before clearing ActiveId, even though strictly speaking it wasn't modified on this frame.
        // If we didn't do that, code like InputInt() with ImGuiInputTextFlags_EnterReturnsTrue would fail.
        // This also allows the user to use InputText() with ImGuiInputTextFlags_EnterReturnsTrue without maintaining any user-side storage (please note that if you use this property along ImGuiInputTextFlags_CallbackResize you can end up with your temporary string object unnecessarily allocating once a frame, either store your string data, either if you don't then don't use ImGuiInputTextFlags_CallbackResize).
        bool apply_edit_back_to_user_buffer = !cancel_edit || (enter_pressed && (flags & ImGuiInputTextFlags_EnterReturnsTrue) != 0);
        if (apply_edit_back_to_user_buffer)
        {
            // Apply new value immediately - copy modified buffer back
            // Note that as soon as the input box is active, the in-widget value gets priority over any underlying modification of the input buffer
            // FIXME: We actually always render 'buf' when calling DrawList->AddText, making the comment above incorrect.
            // FIXME-OPT: CPU waste to do this every time the widget is active, should mark dirty state from the stb_textedit callbacks.
            if (!is_readonly)
            {
                state->TextAIsValid = true;
                state->TextA.resize(state->TextW.Size * 4 + 1);
                ImTextStrToUtf8(state->TextA.Data, state->TextA.Size, state->TextW.Data, NULL);
            }

            // User callback
            if ((flags & (ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackAlways)) != 0)
            {
                IM_ASSERT(callback != NULL);

                // The reason we specify the usage semantic (Completion/History) is that Completion needs to disable keyboard TABBING at the moment.
                ImGuiInputTextFlags event_flag = 0;
                ImGuiKey event_key = ImGuiKey_COUNT;
                if ((flags & ImGuiInputTextFlags_CallbackCompletion) != 0 && IsKeyPressedMap(ImGuiKey_Tab))
                {
                    event_flag = ImGuiInputTextFlags_CallbackCompletion;
                    event_key = ImGuiKey_Tab;
                }
                else if ((flags & ImGuiInputTextFlags_CallbackHistory) != 0 && IsKeyPressedMap(ImGuiKey_UpArrow))
                {
                    event_flag = ImGuiInputTextFlags_CallbackHistory;
                    event_key = ImGuiKey_UpArrow;
                }
                else if ((flags & ImGuiInputTextFlags_CallbackHistory) != 0 && IsKeyPressedMap(ImGuiKey_DownArrow))
                {
                    event_flag = ImGuiInputTextFlags_CallbackHistory;
                    event_key = ImGuiKey_DownArrow;
                }
                else if ((flags & ImGuiInputTextFlags_CallbackEdit) && state->Edited)
                {
                    event_flag = ImGuiInputTextFlags_CallbackEdit;
                }
                else if (flags & ImGuiInputTextFlags_CallbackAlways)
                {
                    event_flag = ImGuiInputTextFlags_CallbackAlways;
                }

                if (event_flag)
                {
                    ImGuiInputTextCallbackData callback_data;
                    memset(&callback_data, 0, sizeof(ImGuiInputTextCallbackData));
                    callback_data.EventFlag = event_flag;
                    callback_data.Flags = flags;
                    callback_data.UserData = callback_user_data;

                    callback_data.EventKey = event_key;
                    callback_data.Buf = state->TextA.Data;
                    callback_data.BufTextLen = state->CurLenA;
                    callback_data.BufSize = state->BufCapacityA;
                    callback_data.BufDirty = false;

                    // We have to convert from wchar-positions to UTF-8-positions, which can be pretty slow (an incentive to ditch the ImWchar buffer, see https://github.com/nothings/stb/issues/188)
                    ImWchar* text = state->TextW.Data;
                    const int utf8_cursor_pos = callback_data.CursorPos = ImTextCountUtf8BytesFromStr(text, text + state->Stb.cursor);
                    const int utf8_selection_start = callback_data.SelectionStart = ImTextCountUtf8BytesFromStr(text, text + state->Stb.select_start);
                    const int utf8_selection_end = callback_data.SelectionEnd = ImTextCountUtf8BytesFromStr(text, text + state->Stb.select_end);

                    // Call user code
                    callback(&callback_data);

                    // Read back what user may have modified
                    IM_ASSERT(callback_data.Buf == state->TextA.Data);  // Invalid to modify those fields
                    IM_ASSERT(callback_data.BufSize == state->BufCapacityA);
                    IM_ASSERT(callback_data.Flags == flags);
                    const bool buf_dirty = callback_data.BufDirty;
                    if (callback_data.CursorPos != utf8_cursor_pos || buf_dirty)            { state->Stb.cursor = ImTextCountCharsFromUtf8(callback_data.Buf, callback_data.Buf + callback_data.CursorPos); state->CursorFollow = true; }
                    if (callback_data.SelectionStart != utf8_selection_start || buf_dirty)  { state->Stb.select_start = (callback_data.SelectionStart == callback_data.CursorPos) ? state->Stb.cursor : ImTextCountCharsFromUtf8(callback_data.Buf, callback_data.Buf + callback_data.SelectionStart); }
                    if (callback_data.SelectionEnd != utf8_selection_end || buf_dirty)      { state->Stb.select_end = (callback_data.SelectionEnd == callback_data.SelectionStart) ? state->Stb.select_start : ImTextCountCharsFromUtf8(callback_data.Buf, callback_data.Buf + callback_data.SelectionEnd); }
                    if (buf_dirty)
                    {
                        IM_ASSERT(callback_data.BufTextLen == (int)strlen(callback_data.Buf)); // You need to maintain BufTextLen if you change the text!
                        if (callback_data.BufTextLen > backup_current_text_length && is_resizable)
                            state->TextW.resize(state->TextW.Size + (callback_data.BufTextLen - backup_current_text_length));
                        state->CurLenW = ImTextStrFromUtf8(state->TextW.Data, state->TextW.Size, callback_data.Buf, NULL);
                        state->CurLenA = callback_data.BufTextLen;  // Assume correct length and valid UTF-8 from user, saves us an extra strlen()
                        state->CursorAnimReset();
                    }
                }
            }

            // Will copy result string if modified
            if (!is_readonly && strcmp(state->TextA.Data, buf) != 0)
            {
                apply_new_text = state->TextA.Data;
                apply_new_text_length = state->CurLenA;
            }
        }

        // Copy result to user buffer
        if (apply_new_text)
        {
            // We cannot test for 'backup_current_text_length != apply_new_text_length' here because we have no guarantee that the size
            // of our owned buffer matches the size of the string object held by the user, and by design we allow InputText() to be used
            // without any storage on user's side.
            IM_ASSERT(apply_new_text_length >= 0);
            if (is_resizable)
            {
                ImGuiInputTextCallbackData callback_data;
                callback_data.EventFlag = ImGuiInputTextFlags_CallbackResize;
                callback_data.Flags = flags;
                callback_data.Buf = buf;
                callback_data.BufTextLen = apply_new_text_length;
                callback_data.BufSize = ImMax(buf_size, apply_new_text_length + 1);
                callback_data.UserData = callback_user_data;
                callback(&callback_data);
                buf = callback_data.Buf;
                buf_size = callback_data.BufSize;
                apply_new_text_length = ImMin(callback_data.BufTextLen, buf_size - 1);
                IM_ASSERT(apply_new_text_length <= buf_size);
            }
            //IMGUI_DEBUG_LOG("InputText(\"%s\"): apply_new_text length %d\n", label, apply_new_text_length);

            // If the underlying buffer resize was denied or not carried to the next frame, apply_new_text_length+1 may be >= buf_size.
            ImStrncpy(buf, apply_new_text, ImMin(apply_new_text_length + 1, buf_size));
            value_changed = true;
        }

        // Clear temporary user storage
        state->UserFlags = 0;
        state->UserCallback = NULL;
        state->UserCallbackData = NULL;
    }

    // Release active ID at the end of the function (so e.g. pressing Return still does a final application of the value)
    if (clear_active_id && g.ActiveId == id)
        ClearActiveID();

    // Render frame
    if (!is_multiline)
    {
        RenderNavHighlight(frame_bb, id);
        RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    }

    const ImVec4 clip_rect(frame_bb.Min.x, frame_bb.Min.y, frame_bb.Min.x + inner_size.x, frame_bb.Min.y + inner_size.y); // Not using frame_bb.Max because we have adjusted size
    ImVec2 draw_pos = is_multiline ? draw_window->DC.CursorPos : frame_bb.Min + style.FramePadding;
    ImVec2 text_size(0.0f, 0.0f);

    // Set upper limit of single-line InputTextEx() at 2 million characters strings. The current pathological worst case is a long line
    // without any carriage return, which would makes ImFont::RenderText() reserve too many vertices and probably crash. Avoid it altogether.
    // Note that we only use this limit on single-line InputText(), so a pathologically large line on a InputTextMultiline() would still crash.
    const int buf_display_max_length = 2 * 1024 * 1024;
    const char* buf_display = buf_display_from_state ? state->TextA.Data : buf; //-V595
    const char* buf_display_end = NULL; // We have specialized paths below for setting the length
    if (is_displaying_hint)
    {
        buf_display = hint;
        buf_display_end = hint + strlen(hint);
    }

    // Render text. We currently only render selection when the widget is active or while scrolling.
    // FIXME: We could remove the '&& render_cursor' to keep rendering selection when inactive.
    if (render_cursor || render_selection)
    {
        IM_ASSERT(state != NULL);
        if (!is_displaying_hint)
            buf_display_end = buf_display + state->CurLenA;

        // Render text (with cursor and selection)
        // This is going to be messy. We need to:
        // - Display the text (this alone can be more easily clipped)
        // - Handle scrolling, highlight selection, display cursor (those all requires some form of 1d->2d cursor position calculation)
        // - Measure text height (for scrollbar)
        // We are attempting to do most of that in **one main pass** to minimize the computation cost (non-negligible for large amount of text) + 2nd pass for selection rendering (we could merge them by an extra refactoring effort)
        // FIXME: This should occur on buf_display but we'd need to maintain cursor/select_start/select_end for UTF-8.
        const ImWchar* text_begin = state->TextW.Data;
        ImVec2 cursor_offset, select_start_offset;

        {
            // Find lines numbers straddling 'cursor' (slot 0) and 'select_start' (slot 1) positions.
            const ImWchar* searches_input_ptr[2] = { NULL, NULL };
            int searches_result_line_no[2] = { -1000, -1000 };
            int searches_remaining = 0;
            if (render_cursor)
            {
                searches_input_ptr[0] = text_begin + state->Stb.cursor;
                searches_result_line_no[0] = -1;
                searches_remaining++;
            }
            if (render_selection)
            {
                searches_input_ptr[1] = text_begin + ImMin(state->Stb.select_start, state->Stb.select_end);
                searches_result_line_no[1] = -1;
                searches_remaining++;
            }

            // Iterate all lines to find our line numbers
            // In multi-line mode, we never exit the loop until all lines are counted, so add one extra to the searches_remaining counter.
            searches_remaining += is_multiline ? 1 : 0;
            int line_count = 0;
            //for (const ImWchar* s = text_begin; (s = (const ImWchar*)wcschr((const wchar_t*)s, (wchar_t)'\n')) != NULL; s++)  // FIXME-OPT: Could use this when wchar_t are 16-bit
            for (const ImWchar* s = text_begin; *s != 0; s++)
                if (*s == '\n')
                {
                    line_count++;
                    if (searches_result_line_no[0] == -1 && s >= searches_input_ptr[0]) { searches_result_line_no[0] = line_count; if (--searches_remaining <= 0) break; }
                    if (searches_result_line_no[1] == -1 && s >= searches_input_ptr[1]) { searches_result_line_no[1] = line_count; if (--searches_remaining <= 0) break; }
                }
            line_count++;
            if (searches_result_line_no[0] == -1)
                searches_result_line_no[0] = line_count;
            if (searches_result_line_no[1] == -1)
                searches_result_line_no[1] = line_count;

            // Calculate 2d position by finding the beginning of the line and measuring distance
            cursor_offset.x = InputTextCalcTextSizeW(ImStrbolW(searches_input_ptr[0], text_begin), searches_input_ptr[0]).x;
            cursor_offset.y = searches_result_line_no[0] * g.FontSize;
            if (searches_result_line_no[1] >= 0)
            {
                select_start_offset.x = InputTextCalcTextSizeW(ImStrbolW(searches_input_ptr[1], text_begin), searches_input_ptr[1]).x;
                select_start_offset.y = searches_result_line_no[1] * g.FontSize;
            }

            // Store text height (note that we haven't calculated text width at all, see GitHub issues #383, #1224)
            if (is_multiline)
                text_size = ImVec2(inner_size.x, line_count * g.FontSize);
        }

        // Scroll
        if (render_cursor && state->CursorFollow)
        {
            // Horizontal scroll in chunks of quarter width
            if (!(flags & ImGuiInputTextFlags_NoHorizontalScroll))
            {
                const float scroll_increment_x = inner_size.x * 0.25f;
                if (cursor_offset.x < state->ScrollX)
                    state->ScrollX = IM_FLOOR(ImMax(0.0f, cursor_offset.x - scroll_increment_x));
                else if (cursor_offset.x - inner_size.x >= state->ScrollX)
                    state->ScrollX = IM_FLOOR(cursor_offset.x - inner_size.x + scroll_increment_x);
            }
            else
            {
                state->ScrollX = 0.0f;
            }

            // Vertical scroll
            if (is_multiline)
            {
                // Test if cursor is vertically visible
                if (cursor_offset.y - g.FontSize < scroll_y)
                    scroll_y = ImMax(0.0f, cursor_offset.y - g.FontSize);
                else if (cursor_offset.y - inner_size.y >= scroll_y)
                    scroll_y = cursor_offset.y - inner_size.y + style.FramePadding.y * 2.0f;
                const float scroll_max_y = ImMax((text_size.y + style.FramePadding.y * 2.0f) - inner_size.y, 0.0f);
                scroll_y = ImClamp(scroll_y, 0.0f, scroll_max_y);
                draw_pos.y += (draw_window->Scroll.y - scroll_y);   // Manipulate cursor pos immediately avoid a frame of lag
                draw_window->Scroll.y = scroll_y;
            }

            state->CursorFollow = false;
        }

        // Draw selection
        const ImVec2 draw_scroll = ImVec2(state->ScrollX, 0.0f);
        if (render_selection)
        {
            const ImWchar* text_selected_begin = text_begin + ImMin(state->Stb.select_start, state->Stb.select_end);
            const ImWchar* text_selected_end = text_begin + ImMax(state->Stb.select_start, state->Stb.select_end);

            ImU32 bg_color = GetColorU32(ImGuiCol_TextSelectedBg, render_cursor ? 1.0f : 0.6f); // FIXME: current code flow mandate that render_cursor is always true here, we are leaving the transparent one for tests.
            float bg_offy_up = is_multiline ? 0.0f : -1.0f;    // FIXME: those offsets should be part of the style? they don't play so well with multi-line selection.
            float bg_offy_dn = is_multiline ? 0.0f : 2.0f;
            ImVec2 rect_pos = draw_pos + select_start_offset - draw_scroll;
            for (const ImWchar* p = text_selected_begin; p < text_selected_end; )
            {
                if (rect_pos.y > clip_rect.w + g.FontSize)
                    break;
                if (rect_pos.y < clip_rect.y)
                {
                    //p = (const ImWchar*)wmemchr((const wchar_t*)p, '\n', text_selected_end - p);  // FIXME-OPT: Could use this when wchar_t are 16-bit
                    //p = p ? p + 1 : text_selected_end;
                    while (p < text_selected_end)
                        if (*p++ == '\n')
                            break;
                }
                else
                {
                    ImVec2 rect_size = InputTextCalcTextSizeW(p, text_selected_end, &p, NULL, true);
                    if (rect_size.x <= 0.0f) rect_size.x = IM_FLOOR(g.Font->GetCharAdvance((ImWchar)' ') * 0.50f); // So we can see selected empty lines
                    ImRect rect(rect_pos + ImVec2(0.0f, bg_offy_up - g.FontSize), rect_pos + ImVec2(rect_size.x, bg_offy_dn));
                    rect.ClipWith(clip_rect);
                    if (rect.Overlaps(clip_rect))
                        draw_window->DrawList->AddRectFilled(rect.Min, rect.Max, bg_color);
                }
                rect_pos.x = draw_pos.x - draw_scroll.x;
                rect_pos.y += g.FontSize;
            }
        }

        // We test for 'buf_display_max_length' as a way to avoid some pathological cases (e.g. single-line 1 MB string) which would make ImDrawList crash.
        if (is_multiline || (buf_display_end - buf_display) < buf_display_max_length)
        {
            ImU32 col = GetColorU32(is_displaying_hint ? ImGuiCol_TextDisabled : ImGuiCol_Text);
            draw_window->DrawList->AddText(g.Font, g.FontSize, draw_pos - draw_scroll, col, buf_display, buf_display_end, 0.0f, is_multiline ? NULL : &clip_rect);
        }

        // Draw blinking cursor
        if (render_cursor)
        {
            state->CursorAnim += io.DeltaTime;
            bool cursor_is_visible = (!g.IO.ConfigInputTextCursorBlink) || (state->CursorAnim <= 0.0f) || ImFmod(state->CursorAnim, 1.20f) <= 0.80f;
            ImVec2 cursor_screen_pos = draw_pos + cursor_offset - draw_scroll;
            ImRect cursor_screen_rect(cursor_screen_pos.x, cursor_screen_pos.y - g.FontSize + 0.5f, cursor_screen_pos.x + 1.0f, cursor_screen_pos.y - 1.5f);
            if (cursor_is_visible && cursor_screen_rect.Overlaps(clip_rect))
                draw_window->DrawList->AddLine(cursor_screen_rect.Min, cursor_screen_rect.GetBL(), GetColorU32(ImGuiCol_Text));

            // Notify OS of text input position for advanced IME (-1 x offset so that Windows IME can cover our cursor. Bit of an extra nicety.)
            if (!is_readonly)
            {
                g.PlatformImePos = ImVec2(cursor_screen_pos.x - 1, cursor_screen_pos.y - g.FontSize);
                g.PlatformImePosViewport = window->Viewport;
            }
        }
    }
    else
    {
        // Render text only (no selection, no cursor)
        if (is_multiline)
            text_size = ImVec2(inner_size.x, InputTextCalcTextLenAndLineCount(buf_display, &buf_display_end) * g.FontSize); // We don't need width
        else if (!is_displaying_hint && g.ActiveId == id)
            buf_display_end = buf_display + state->CurLenA;
        else if (!is_displaying_hint)
            buf_display_end = buf_display + strlen(buf_display);

        if (is_multiline || (buf_display_end - buf_display) < buf_display_max_length)
        {
            ImU32 col = GetColorU32(is_displaying_hint ? ImGuiCol_TextDisabled : ImGuiCol_Text);
            draw_window->DrawList->AddText(g.Font, g.FontSize, draw_pos, col, buf_display, buf_display_end, 0.0f, is_multiline ? NULL : &clip_rect);
        }
    }

    if (is_password && !is_displaying_hint)
        PopFont();

    if (is_multiline)
    {
        Dummy(text_size);
        EndChild();
        EndGroup();
    }

    // Log as text
    if (g.LogEnabled && (!is_password || is_displaying_hint))
        LogRenderedText(&draw_pos, buf_display, buf_display_end);

    if (label_size.x > 0)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    if (value_changed && !(flags & ImGuiInputTextFlags_NoMarkEdited))
        MarkItemEdited(id);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags);
    if ((flags & ImGuiInputTextFlags_EnterReturnsTrue) != 0)
        return enter_pressed;
    else
        return value_changed;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: ColorEdit, ColorPicker, ColorButton, etc.
//-------------------------------------------------------------------------
// - ColorEdit3()
// - ColorEdit4()
// - ColorPicker3()
// - RenderColorRectWithAlphaCheckerboard() [Internal]
// - ColorPicker4()
// - ColorButton()
// - SetColorEditOptions()
// - ColorTooltip() [Internal]
// - ColorEditOptionsPopup() [Internal]
// - ColorPickerOptionsPopup() [Internal]
//-------------------------------------------------------------------------

bool ImGui::ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags)
{
    return ColorEdit4(label, col, flags | ImGuiColorEditFlags_NoAlpha);
}

// Edit colors components (each component in 0.0f..1.0f range).
// See enum ImGuiColorEditFlags_ for available options. e.g. Only access 3 floats if ImGuiColorEditFlags_NoAlpha flag is set.
// With typical options: Left-click on color square to open color picker. Right-click to open option menu. CTRL-Click over input fields to edit them and TAB to go to next item.
bool ImGui::ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const float square_sz = GetFrameHeight();
    const float w_full = CalcItemWidth();
    const float w_button = (flags & ImGuiColorEditFlags_NoSmallPreview) ? 0.0f : (square_sz + style.ItemInnerSpacing.x);
    const float w_inputs = w_full - w_button;
    const char* label_display_end = FindRenderedTextEnd(label);
    g.NextItemData.ClearFlags();

    BeginGroup();
    PushID(label);

    // If we're not showing any slider there's no point in doing any HSV conversions
    const ImGuiColorEditFlags flags_untouched = flags;
    if (flags & ImGuiColorEditFlags_NoInputs)
        flags = (flags & (~ImGuiColorEditFlags__DisplayMask)) | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoOptions;

    // Context menu: display and modify options (before defaults are applied)
    if (!(flags & ImGuiColorEditFlags_NoOptions))
        ColorEditOptionsPopup(col, flags);

    // Read stored options
    if (!(flags & ImGuiColorEditFlags__DisplayMask))
        flags |= (g.ColorEditOptions & ImGuiColorEditFlags__DisplayMask);
    if (!(flags & ImGuiColorEditFlags__DataTypeMask))
        flags |= (g.ColorEditOptions & ImGuiColorEditFlags__DataTypeMask);
    if (!(flags & ImGuiColorEditFlags__PickerMask))
        flags |= (g.ColorEditOptions & ImGuiColorEditFlags__PickerMask);
    if (!(flags & ImGuiColorEditFlags__InputMask))
        flags |= (g.ColorEditOptions & ImGuiColorEditFlags__InputMask);
    flags |= (g.ColorEditOptions & ~(ImGuiColorEditFlags__DisplayMask | ImGuiColorEditFlags__DataTypeMask | ImGuiColorEditFlags__PickerMask | ImGuiColorEditFlags__InputMask));
    IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags__DisplayMask)); // Check that only 1 is selected
    IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags__InputMask));   // Check that only 1 is selected

    const bool alpha = (flags & ImGuiColorEditFlags_NoAlpha) == 0;
    const bool hdr = (flags & ImGuiColorEditFlags_HDR) != 0;
    const int components = alpha ? 4 : 3;

    // Convert to the formats we need
    float f[4] = { col[0], col[1], col[2], alpha ? col[3] : 1.0f };
    if ((flags & ImGuiColorEditFlags_InputHSV) && (flags & ImGuiColorEditFlags_DisplayRGB))
        ColorConvertHSVtoRGB(f[0], f[1], f[2], f[0], f[1], f[2]);
    else if ((flags & ImGuiColorEditFlags_InputRGB) && (flags & ImGuiColorEditFlags_DisplayHSV))
    {
        // Hue is lost when converting from greyscale rgb (saturation=0). Restore it.
        ColorConvertRGBtoHSV(f[0], f[1], f[2], f[0], f[1], f[2]);
        if (memcmp(g.ColorEditLastColor, col, sizeof(float) * 3) == 0)
        {
            if (f[1] == 0)
                f[0] = g.ColorEditLastHue;
            if (f[2] == 0)
                f[1] = g.ColorEditLastSat;
        }
    }
    int i[4] = { IM_F32_TO_INT8_UNBOUND(f[0]), IM_F32_TO_INT8_UNBOUND(f[1]), IM_F32_TO_INT8_UNBOUND(f[2]), IM_F32_TO_INT8_UNBOUND(f[3]) };

    bool value_changed = false;
    bool value_changed_as_float = false;

    const ImVec2 pos = window->DC.CursorPos;
    const float inputs_offset_x = (style.ColorButtonPosition == ImGuiDir_Left) ? w_button : 0.0f;
    window->DC.CursorPos.x = pos.x + inputs_offset_x;

    if ((flags & (ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV)) != 0 && (flags & ImGuiColorEditFlags_NoInputs) == 0)
    {
        // RGB/HSV 0..255 Sliders
        const float w_item_one  = ImMax(1.0f, IM_FLOOR((w_inputs - (style.ItemInnerSpacing.x) * (components - 1)) / (float)components));
        const float w_item_last = ImMax(1.0f, IM_FLOOR(w_inputs - (w_item_one + style.ItemInnerSpacing.x) * (components - 1)));

        const bool hide_prefix = (w_item_one <= CalcTextSize((flags & ImGuiColorEditFlags_Float) ? "M:0.000" : "M:000").x);
        static const char* ids[4] = { "##X", "##Y", "##Z", "##W" };
        static const char* fmt_table_int[3][4] =
        {
            {   "%3d",   "%3d",   "%3d",   "%3d" }, // Short display
            { "R:%3d", "G:%3d", "B:%3d", "A:%3d" }, // Long display for RGBA
            { "H:%3d", "S:%3d", "V:%3d", "A:%3d" }  // Long display for HSVA
        };
        static const char* fmt_table_float[3][4] =
        {
            {   "%0.3f",   "%0.3f",   "%0.3f",   "%0.3f" }, // Short display
            { "R:%0.3f", "G:%0.3f", "B:%0.3f", "A:%0.3f" }, // Long display for RGBA
            { "H:%0.3f", "S:%0.3f", "V:%0.3f", "A:%0.3f" }  // Long display for HSVA
        };
        const int fmt_idx = hide_prefix ? 0 : (flags & ImGuiColorEditFlags_DisplayHSV) ? 2 : 1;

        for (int n = 0; n < components; n++)
        {
            if (n > 0)
                SameLine(0, style.ItemInnerSpacing.x);
            SetNextItemWidth((n + 1 < components) ? w_item_one : w_item_last);

            // FIXME: When ImGuiColorEditFlags_HDR flag is passed HS values snap in weird ways when SV values go below 0.
            if (flags & ImGuiColorEditFlags_Float)
            {
                value_changed |= DragFloat(ids[n], &f[n], 1.0f / 255.0f, 0.0f, hdr ? 0.0f : 1.0f, fmt_table_float[fmt_idx][n]);
                value_changed_as_float |= value_changed;
            }
            else
            {
                value_changed |= DragInt(ids[n], &i[n], 1.0f, 0, hdr ? 0 : 255, fmt_table_int[fmt_idx][n]);
            }
            if (!(flags & ImGuiColorEditFlags_NoOptions))
                OpenPopupOnItemClick("context");
        }
    }
    else if ((flags & ImGuiColorEditFlags_DisplayHex) != 0 && (flags & ImGuiColorEditFlags_NoInputs) == 0)
    {
        // RGB Hexadecimal Input
        char buf[64];
        if (alpha)
            ImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X%02X", ImClamp(i[0], 0, 255), ImClamp(i[1], 0, 255), ImClamp(i[2], 0, 255), ImClamp(i[3], 0, 255));
        else
            ImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", ImClamp(i[0], 0, 255), ImClamp(i[1], 0, 255), ImClamp(i[2], 0, 255));
        SetNextItemWidth(w_inputs);
        if (InputText("##Text", buf, IM_ARRAYSIZE(buf), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase))
        {
            value_changed = true;
            char* p = buf;
            while (*p == '#' || ImCharIsBlankA(*p))
                p++;
            i[0] = i[1] = i[2] = i[3] = 0;
            if (alpha)
                sscanf(p, "%02X%02X%02X%02X", (unsigned int*)&i[0], (unsigned int*)&i[1], (unsigned int*)&i[2], (unsigned int*)&i[3]); // Treat at unsigned (%X is unsigned)
            else
                sscanf(p, "%02X%02X%02X", (unsigned int*)&i[0], (unsigned int*)&i[1], (unsigned int*)&i[2]);
        }
        if (!(flags & ImGuiColorEditFlags_NoOptions))
            OpenPopupOnItemClick("context");
    }

    ImGuiWindow* picker_active_window = NULL;
    if (!(flags & ImGuiColorEditFlags_NoSmallPreview))
    {
        const float button_offset_x = ((flags & ImGuiColorEditFlags_NoInputs) || (style.ColorButtonPosition == ImGuiDir_Left)) ? 0.0f : w_inputs + style.ItemInnerSpacing.x;
        window->DC.CursorPos = ImVec2(pos.x + button_offset_x, pos.y);

        const ImVec4 col_v4(col[0], col[1], col[2], alpha ? col[3] : 1.0f);
        if (ColorButton("##ColorButton", col_v4, flags))
        {
            if (!(flags & ImGuiColorEditFlags_NoPicker))
            {
                // Store current color and open a picker
                g.ColorPickerRef = col_v4;
                OpenPopup("picker");
                SetNextWindowPos(window->DC.LastItemRect.GetBL() + ImVec2(-1, style.ItemSpacing.y));
            }
        }
        if (!(flags & ImGuiColorEditFlags_NoOptions))
            OpenPopupOnItemClick("context");

        if (BeginPopup("picker"))
        {
            picker_active_window = g.CurrentWindow;
            if (label != label_display_end)
            {
                TextEx(label, label_display_end);
                Spacing();
            }
            ImGuiColorEditFlags picker_flags_to_forward = ImGuiColorEditFlags__DataTypeMask | ImGuiColorEditFlags__PickerMask | ImGuiColorEditFlags__InputMask | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaBar;
            ImGuiColorEditFlags picker_flags = (flags_untouched & picker_flags_to_forward) | ImGuiColorEditFlags__DisplayMask | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf;
            SetNextItemWidth(square_sz * 12.0f); // Use 256 + bar sizes?
            value_changed |= ColorPicker4("##picker", col, picker_flags, &g.ColorPickerRef.x);
            EndPopup();
        }
    }

    if (label != label_display_end && !(flags & ImGuiColorEditFlags_NoLabel))
    {
        const float text_offset_x = (flags & ImGuiColorEditFlags_NoInputs) ? w_button : w_full + style.ItemInnerSpacing.x;
        window->DC.CursorPos = ImVec2(pos.x + text_offset_x, pos.y + style.FramePadding.y);
        TextEx(label, label_display_end);
    }

    // Convert back
    if (value_changed && picker_active_window == NULL)
    {
        if (!value_changed_as_float)
            for (int n = 0; n < 4; n++)
                f[n] = i[n] / 255.0f;
        if ((flags & ImGuiColorEditFlags_DisplayHSV) && (flags & ImGuiColorEditFlags_InputRGB))
        {
            g.ColorEditLastHue = f[0];
            g.ColorEditLastSat = f[1];
            ColorConvertHSVtoRGB(f[0], f[1], f[2], f[0], f[1], f[2]);
            memcpy(g.ColorEditLastColor, f, sizeof(float) * 3);
        }
        if ((flags & ImGuiColorEditFlags_DisplayRGB) && (flags & ImGuiColorEditFlags_InputHSV))
            ColorConvertRGBtoHSV(f[0], f[1], f[2], f[0], f[1], f[2]);

        col[0] = f[0];
        col[1] = f[1];
        col[2] = f[2];
        if (alpha)
            col[3] = f[3];
    }

    PopID();
    EndGroup();

    // Drag and Drop Target
    // NB: The flag test is merely an optional micro-optimization, BeginDragDropTarget() does the same test.
    if ((window->DC.LastItemStatusFlags & ImGuiItemStatusFlags_HoveredRect) && !(flags & ImGuiColorEditFlags_NoDragDrop) && BeginDragDropTarget())
    {
        bool accepted_drag_drop = false;
        if (const ImGuiPayload* payload = AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
        {
            memcpy((float*)col, payload->Data, sizeof(float) * 3); // Preserve alpha if any //-V512
            value_changed = accepted_drag_drop = true;
        }
        if (const ImGuiPayload* payload = AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
        {
            memcpy((float*)col, payload->Data, sizeof(float) * components);
            value_changed = accepted_drag_drop = true;
        }

        // Drag-drop payloads are always RGB
        if (accepted_drag_drop && (flags & ImGuiColorEditFlags_InputHSV))
            ColorConvertRGBtoHSV(col[0], col[1], col[2], col[0], col[1], col[2]);
        EndDragDropTarget();
    }

    // When picker is being actively used, use its active id so IsItemActive() will function on ColorEdit4().
    if (picker_active_window && g.ActiveId != 0 && g.ActiveIdWindow == picker_active_window)
        window->DC.LastItemId = g.ActiveId;

    if (value_changed)
        MarkItemEdited(window->DC.LastItemId);

    return value_changed;
}

bool ImGui::ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags)
{
    float col4[4] = { col[0], col[1], col[2], 1.0f };
    if (!ColorPicker4(label, col4, flags | ImGuiColorEditFlags_NoAlpha))
        return false;
    col[0] = col4[0]; col[1] = col4[1]; col[2] = col4[2];
    return true;
}

// Helper for ColorPicker4()
static void RenderArrowsForVerticalBar(ImDrawList* draw_list, ImVec2 pos, ImVec2 half_sz, float bar_w, float alpha)
{
    ImU32 alpha8 = IM_F32_TO_INT8_SAT(alpha);
    ImGui::RenderArrowPointingAt(draw_list, ImVec2(pos.x + half_sz.x + 1,         pos.y), ImVec2(half_sz.x + 2, half_sz.y + 1), ImGuiDir_Right, IM_COL32(0,0,0,alpha8));
    ImGui::RenderArrowPointingAt(draw_list, ImVec2(pos.x + half_sz.x,             pos.y), half_sz,                              ImGuiDir_Right, IM_COL32(255,255,255,alpha8));
    ImGui::RenderArrowPointingAt(draw_list, ImVec2(pos.x + bar_w - half_sz.x - 1, pos.y), ImVec2(half_sz.x + 2, half_sz.y + 1), ImGuiDir_Left,  IM_COL32(0,0,0,alpha8));
    ImGui::RenderArrowPointingAt(draw_list, ImVec2(pos.x + bar_w - half_sz.x,     pos.y), half_sz,                              ImGuiDir_Left,  IM_COL32(255,255,255,alpha8));
}

// Note: ColorPicker4() only accesses 3 floats if ImGuiColorEditFlags_NoAlpha flag is set.
// (In C++ the 'float col[4]' notation for a function argument is equivalent to 'float* col', we only specify a size to facilitate understanding of the code.)
// FIXME: we adjust the big color square height based on item width, which may cause a flickering feedback loop (if automatic height makes a vertical scrollbar appears, affecting automatic width..)
// FIXME: this is trying to be aware of style.Alpha but not fully correct. Also, the color wheel will have overlapping glitches with (style.Alpha < 1.0)
bool ImGui::ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags, const float* ref_col)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImDrawList* draw_list = window->DrawList;
    ImGuiStyle& style = g.Style;
    ImGuiIO& io = g.IO;

    const float width = CalcItemWidth();
    g.NextItemData.ClearFlags();

    PushID(label);
    BeginGroup();

    if (!(flags & ImGuiColorEditFlags_NoSidePreview))
        flags |= ImGuiColorEditFlags_NoSmallPreview;

    // Context menu: display and store options.
    if (!(flags & ImGuiColorEditFlags_NoOptions))
        ColorPickerOptionsPopup(col, flags);

    // Read stored options
    if (!(flags & ImGuiColorEditFlags__PickerMask))
        flags |= ((g.ColorEditOptions & ImGuiColorEditFlags__PickerMask) ? g.ColorEditOptions : ImGuiColorEditFlags__OptionsDefault) & ImGuiColorEditFlags__PickerMask;
    if (!(flags & ImGuiColorEditFlags__InputMask))
        flags |= ((g.ColorEditOptions & ImGuiColorEditFlags__InputMask) ? g.ColorEditOptions : ImGuiColorEditFlags__OptionsDefault) & ImGuiColorEditFlags__InputMask;
    IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags__PickerMask)); // Check that only 1 is selected
    IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags__InputMask));  // Check that only 1 is selected
    if (!(flags & ImGuiColorEditFlags_NoOptions))
        flags |= (g.ColorEditOptions & ImGuiColorEditFlags_AlphaBar);

    // Setup
    int components = (flags & ImGuiColorEditFlags_NoAlpha) ? 3 : 4;
    bool alpha_bar = (flags & ImGuiColorEditFlags_AlphaBar) && !(flags & ImGuiColorEditFlags_NoAlpha);
    ImVec2 picker_pos = window->DC.CursorPos;
    float square_sz = GetFrameHeight();
    float bars_width = square_sz; // Arbitrary smallish width of Hue/Alpha picking bars
    float sv_picker_size = ImMax(bars_width * 1, width - (alpha_bar ? 2 : 1) * (bars_width + style.ItemInnerSpacing.x)); // Saturation/Value picking box
    float bar0_pos_x = picker_pos.x + sv_picker_size + style.ItemInnerSpacing.x;
    float bar1_pos_x = bar0_pos_x + bars_width + style.ItemInnerSpacing.x;
    float bars_triangles_half_sz = IM_FLOOR(bars_width * 0.20f);

    float backup_initial_col[4];
    memcpy(backup_initial_col, col, components * sizeof(float));

    float wheel_thickness = sv_picker_size * 0.08f;
    float wheel_r_outer = sv_picker_size * 0.50f;
    float wheel_r_inner = wheel_r_outer - wheel_thickness;
    ImVec2 wheel_center(picker_pos.x + (sv_picker_size + bars_width)*0.5f, picker_pos.y + sv_picker_size * 0.5f);

    // Note: the triangle is displayed rotated with triangle_pa pointing to Hue, but most coordinates stays unrotated for logic.
    float triangle_r = wheel_r_inner - (int)(sv_picker_size * 0.027f);
    ImVec2 triangle_pa = ImVec2(triangle_r, 0.0f); // Hue point.
    ImVec2 triangle_pb = ImVec2(triangle_r * -0.5f, triangle_r * -0.866025f); // Black point.
    ImVec2 triangle_pc = ImVec2(triangle_r * -0.5f, triangle_r * +0.866025f); // White point.

    float H = col[0], S = col[1], V = col[2];
    float R = col[0], G = col[1], B = col[2];
    if (flags & ImGuiColorEditFlags_InputRGB)
    {
        // Hue is lost when converting from greyscale rgb (saturation=0). Restore it.
        ColorConvertRGBtoHSV(R, G, B, H, S, V);
        if (memcmp(g.ColorEditLastColor, col, sizeof(float) * 3) == 0)
        {
            if (S == 0)
                H = g.ColorEditLastHue;
            if (V == 0)
                S = g.ColorEditLastSat;
        }
    }
    else if (flags & ImGuiColorEditFlags_InputHSV)
    {
        ColorConvertHSVtoRGB(H, S, V, R, G, B);
    }

    bool value_changed = false, value_changed_h = false, value_changed_sv = false;

    PushItemFlag(ImGuiItemFlags_NoNav, true);
    if (flags & ImGuiColorEditFlags_PickerHueWheel)
    {
        // Hue wheel + SV triangle logic
        InvisibleButton("hsv", ImVec2(sv_picker_size + style.ItemInnerSpacing.x + bars_width, sv_picker_size));
        if (IsItemActive())
        {
            ImVec2 initial_off = g.IO.MouseClickedPos[0] - wheel_center;
            ImVec2 current_off = g.IO.MousePos - wheel_center;
            float initial_dist2 = ImLengthSqr(initial_off);
            if (initial_dist2 >= (wheel_r_inner - 1) * (wheel_r_inner - 1) && initial_dist2 <= (wheel_r_outer + 1) * (wheel_r_outer + 1))
            {
                // Interactive with Hue wheel
                H = ImAtan2(current_off.y, current_off.x) / IM_PI * 0.5f;
                if (H < 0.0f)
                    H += 1.0f;
                value_changed = value_changed_h = true;
            }
            float cos_hue_angle = ImCos(-H * 2.0f * IM_PI);
            float sin_hue_angle = ImSin(-H * 2.0f * IM_PI);
            if (ImTriangleContainsPoint(triangle_pa, triangle_pb, triangle_pc, ImRotate(initial_off, cos_hue_angle, sin_hue_angle)))
            {
                // Interacting with SV triangle
                ImVec2 current_off_unrotated = ImRotate(current_off, cos_hue_angle, sin_hue_angle);
                if (!ImTriangleContainsPoint(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated))
                    current_off_unrotated = ImTriangleClosestPoint(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated);
                float uu, vv, ww;
                ImTriangleBarycentricCoords(triangle_pa, triangle_pb, triangle_pc, current_off_unrotated, uu, vv, ww);
                V = ImClamp(1.0f - vv, 0.0001f, 1.0f);
                S = ImClamp(uu / V, 0.0001f, 1.0f);
                value_changed = value_changed_sv = true;
            }
        }
        if (!(flags & ImGuiColorEditFlags_NoOptions))
            OpenPopupOnItemClick("context");
    }
    else if (flags & ImGuiColorEditFlags_PickerHueBar)
    {
        // SV rectangle logic
        InvisibleButton("sv", ImVec2(sv_picker_size, sv_picker_size));
        if (IsItemActive())
        {
            S = ImSaturate((io.MousePos.x - picker_pos.x) / (sv_picker_size - 1));
            V = 1.0f - ImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size - 1));
            value_changed = value_changed_sv = true;
        }
        if (!(flags & ImGuiColorEditFlags_NoOptions))
            OpenPopupOnItemClick("context");

        // Hue bar logic
        SetCursorScreenPos(ImVec2(bar0_pos_x, picker_pos.y));
        InvisibleButton("hue", ImVec2(bars_width, sv_picker_size));
        if (IsItemActive())
        {
            H = ImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size - 1));
            value_changed = value_changed_h = true;
        }
    }

    // Alpha bar logic
    if (alpha_bar)
    {
        SetCursorScreenPos(ImVec2(bar1_pos_x, picker_pos.y));
        InvisibleButton("alpha", ImVec2(bars_width, sv_picker_size));
        if (IsItemActive())
        {
            col[3] = 1.0f - ImSaturate((io.MousePos.y - picker_pos.y) / (sv_picker_size - 1));
            value_changed = true;
        }
    }
    PopItemFlag(); // ImGuiItemFlags_NoNav

    if (!(flags & ImGuiColorEditFlags_NoSidePreview))
    {
        SameLine(0, style.ItemInnerSpacing.x);
        BeginGroup();
    }

    if (!(flags & ImGuiColorEditFlags_NoLabel))
    {
        const char* label_display_end = FindRenderedTextEnd(label);
        if (label != label_display_end)
        {
            if ((flags & ImGuiColorEditFlags_NoSidePreview))
                SameLine(0, style.ItemInnerSpacing.x);
            TextEx(label, label_display_end);
        }
    }

    if (!(flags & ImGuiColorEditFlags_NoSidePreview))
    {
        PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
        ImVec4 col_v4(col[0], col[1], col[2], (flags & ImGuiColorEditFlags_NoAlpha) ? 1.0f : col[3]);
        if ((flags & ImGuiColorEditFlags_NoLabel))
            Text("Current");

        ImGuiColorEditFlags sub_flags_to_forward = ImGuiColorEditFlags__InputMask | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip;
        ColorButton("##current", col_v4, (flags & sub_flags_to_forward), ImVec2(square_sz * 3, square_sz * 2));
        if (ref_col != NULL)
        {
            Text("Original");
            ImVec4 ref_col_v4(ref_col[0], ref_col[1], ref_col[2], (flags & ImGuiColorEditFlags_NoAlpha) ? 1.0f : ref_col[3]);
            if (ColorButton("##original", ref_col_v4, (flags & sub_flags_to_forward), ImVec2(square_sz * 3, square_sz * 2)))
            {
                memcpy(col, ref_col, components * sizeof(float));
                value_changed = true;
            }
        }
        PopItemFlag();
        EndGroup();
    }

    // Convert back color to RGB
    if (value_changed_h || value_changed_sv)
    {
        if (flags & ImGuiColorEditFlags_InputRGB)
        {
            ColorConvertHSVtoRGB(H >= 1.0f ? H - 10 * 1e-6f : H, S > 0.0f ? S : 10 * 1e-6f, V > 0.0f ? V : 1e-6f, col[0], col[1], col[2]);
            g.ColorEditLastHue = H;
            g.ColorEditLastSat = S;
            memcpy(g.ColorEditLastColor, col, sizeof(float) * 3);
        }
        else if (flags & ImGuiColorEditFlags_InputHSV)
        {
            col[0] = H;
            col[1] = S;
            col[2] = V;
        }
    }

    // R,G,B and H,S,V slider color editor
    bool value_changed_fix_hue_wrap = false;
    if ((flags & ImGuiColorEditFlags_NoInputs) == 0)
    {
        PushItemWidth((alpha_bar ? bar1_pos_x : bar0_pos_x) + bars_width - picker_pos.x);
        ImGuiColorEditFlags sub_flags_to_forward = ImGuiColorEditFlags__DataTypeMask | ImGuiColorEditFlags__InputMask | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf;
        ImGuiColorEditFlags sub_flags = (flags & sub_flags_to_forward) | ImGuiColorEditFlags_NoPicker;
        if (flags & ImGuiColorEditFlags_DisplayRGB || (flags & ImGuiColorEditFlags__DisplayMask) == 0)
            if (ColorEdit4("##rgb", col, sub_flags | ImGuiColorEditFlags_DisplayRGB))
            {
                // FIXME: Hackily differentiating using the DragInt (ActiveId != 0 && !ActiveIdAllowOverlap) vs. using the InputText or DropTarget.
                // For the later we don't want to run the hue-wrap canceling code. If you are well versed in HSV picker please provide your input! (See #2050)
                value_changed_fix_hue_wrap = (g.ActiveId != 0 && !g.ActiveIdAllowOverlap);
                value_changed = true;
            }
        if (flags & ImGuiColorEditFlags_DisplayHSV || (flags & ImGuiColorEditFlags__DisplayMask) == 0)
            value_changed |= ColorEdit4("##hsv", col, sub_flags | ImGuiColorEditFlags_DisplayHSV);
        if (flags & ImGuiColorEditFlags_DisplayHex || (flags & ImGuiColorEditFlags__DisplayMask) == 0)
            value_changed |= ColorEdit4("##hex", col, sub_flags | ImGuiColorEditFlags_DisplayHex);
        PopItemWidth();
    }

    // Try to cancel hue wrap (after ColorEdit4 call), if any
    if (value_changed_fix_hue_wrap && (flags & ImGuiColorEditFlags_InputRGB))
    {
        float new_H, new_S, new_V;
        ColorConvertRGBtoHSV(col[0], col[1], col[2], new_H, new_S, new_V);
        if (new_H <= 0 && H > 0)
        {
            if (new_V <= 0 && V != new_V)
                ColorConvertHSVtoRGB(H, S, new_V <= 0 ? V * 0.5f : new_V, col[0], col[1], col[2]);
            else if (new_S <= 0)
                ColorConvertHSVtoRGB(H, new_S <= 0 ? S * 0.5f : new_S, new_V, col[0], col[1], col[2]);
        }
    }

    if (value_changed)
    {
        if (flags & ImGuiColorEditFlags_InputRGB)
        {
            R = col[0];
            G = col[1];
            B = col[2];
            ColorConvertRGBtoHSV(R, G, B, H, S, V);
            if (memcmp(g.ColorEditLastColor, col, sizeof(float) * 3) == 0) // Fix local Hue as display below will use it immediately.
            {
                if (S == 0)
                    H = g.ColorEditLastHue;
                if (V == 0)
                    S = g.ColorEditLastSat;
            }
        }
        else if (flags & ImGuiColorEditFlags_InputHSV)
        {
            H = col[0];
            S = col[1];
            V = col[2];
            ColorConvertHSVtoRGB(H, S, V, R, G, B);
        }
    }

    const int style_alpha8 = IM_F32_TO_INT8_SAT(style.Alpha);
    const ImU32 col_black = IM_COL32(0,0,0,style_alpha8);
    const ImU32 col_white = IM_COL32(255,255,255,style_alpha8);
    const ImU32 col_midgrey = IM_COL32(128,128,128,style_alpha8);
    const ImU32 col_hues[6 + 1] = { IM_COL32(255,0,0,style_alpha8), IM_COL32(255,255,0,style_alpha8), IM_COL32(0,255,0,style_alpha8), IM_COL32(0,255,255,style_alpha8), IM_COL32(0,0,255,style_alpha8), IM_COL32(255,0,255,style_alpha8), IM_COL32(255,0,0,style_alpha8) };

    ImVec4 hue_color_f(1, 1, 1, style.Alpha); ColorConvertHSVtoRGB(H, 1, 1, hue_color_f.x, hue_color_f.y, hue_color_f.z);
    ImU32 hue_color32 = ColorConvertFloat4ToU32(hue_color_f);
    ImU32 user_col32_striped_of_alpha = ColorConvertFloat4ToU32(ImVec4(R, G, B, style.Alpha)); // Important: this is still including the main rendering/style alpha!!

    ImVec2 sv_cursor_pos;

    if (flags & ImGuiColorEditFlags_PickerHueWheel)
    {
        // Render Hue Wheel
        const float aeps = 0.5f / wheel_r_outer; // Half a pixel arc length in radians (2pi cancels out).
        const int segment_per_arc = ImMax(4, (int)wheel_r_outer / 12);
        for (int n = 0; n < 6; n++)
        {
            const float a0 = (n)     /6.0f * 2.0f * IM_PI - aeps;
            const float a1 = (n+1.0f)/6.0f * 2.0f * IM_PI + aeps;
            const int vert_start_idx = draw_list->VtxBuffer.Size;
            draw_list->PathArcTo(wheel_center, (wheel_r_inner + wheel_r_outer)*0.5f, a0, a1, segment_per_arc);
            draw_list->PathStroke(col_white, false, wheel_thickness);
            const int vert_end_idx = draw_list->VtxBuffer.Size;

            // Paint colors over existing vertices
            ImVec2 gradient_p0(wheel_center.x + ImCos(a0) * wheel_r_inner, wheel_center.y + ImSin(a0) * wheel_r_inner);
            ImVec2 gradient_p1(wheel_center.x + ImCos(a1) * wheel_r_inner, wheel_center.y + ImSin(a1) * wheel_r_inner);
            ShadeVertsLinearColorGradientKeepAlpha(draw_list, vert_start_idx, vert_end_idx, gradient_p0, gradient_p1, col_hues[n], col_hues[n + 1]);
        }

        // Render Cursor + preview on Hue Wheel
        float cos_hue_angle = ImCos(H * 2.0f * IM_PI);
        float sin_hue_angle = ImSin(H * 2.0f * IM_PI);
        ImVec2 hue_cursor_pos(wheel_center.x + cos_hue_angle * (wheel_r_inner + wheel_r_outer) * 0.5f, wheel_center.y + sin_hue_angle * (wheel_r_inner + wheel_r_outer) * 0.5f);
        float hue_cursor_rad = value_changed_h ? wheel_thickness * 0.65f : wheel_thickness * 0.55f;
        int hue_cursor_segments = ImClamp((int)(hue_cursor_rad / 1.4f), 9, 32);
        draw_list->AddCircleFilled(hue_cursor_pos, hue_cursor_rad, hue_color32, hue_cursor_segments);
        draw_list->AddCircle(hue_cursor_pos, hue_cursor_rad + 1, col_midgrey, hue_cursor_segments);
        draw_list->AddCircle(hue_cursor_pos, hue_cursor_rad, col_white, hue_cursor_segments);

        // Render SV triangle (rotated according to hue)
        ImVec2 tra = wheel_center + ImRotate(triangle_pa, cos_hue_angle, sin_hue_angle);
        ImVec2 trb = wheel_center + ImRotate(triangle_pb, cos_hue_angle, sin_hue_angle);
        ImVec2 trc = wheel_center + ImRotate(triangle_pc, cos_hue_angle, sin_hue_angle);
        ImVec2 uv_white = GetFontTexUvWhitePixel();
        draw_list->PrimReserve(6, 6);
        draw_list->PrimVtx(tra, uv_white, hue_color32);
        draw_list->PrimVtx(trb, uv_white, hue_color32);
        draw_list->PrimVtx(trc, uv_white, col_white);
        draw_list->PrimVtx(tra, uv_white, 0);
        draw_list->PrimVtx(trb, uv_white, col_black);
        draw_list->PrimVtx(trc, uv_white, 0);
        draw_list->AddTriangle(tra, trb, trc, col_midgrey, 1.5f);
        sv_cursor_pos = ImLerp(ImLerp(trc, tra, ImSaturate(S)), trb, ImSaturate(1 - V));
    }
    else if (flags & ImGuiColorEditFlags_PickerHueBar)
    {
        // Render SV Square
        draw_list->AddRectFilledMultiColor(picker_pos, picker_pos + ImVec2(sv_picker_size, sv_picker_size), col_white, hue_color32, hue_color32, col_white);
        draw_list->AddRectFilledMultiColor(picker_pos, picker_pos + ImVec2(sv_picker_size, sv_picker_size), 0, 0, col_black, col_black);
        RenderFrameBorder(picker_pos, picker_pos + ImVec2(sv_picker_size, sv_picker_size), 0.0f);
        sv_cursor_pos.x = ImClamp(IM_ROUND(picker_pos.x + ImSaturate(S)     * sv_picker_size), picker_pos.x + 2, picker_pos.x + sv_picker_size - 2); // Sneakily prevent the circle to stick out too much
        sv_cursor_pos.y = ImClamp(IM_ROUND(picker_pos.y + ImSaturate(1 - V) * sv_picker_size), picker_pos.y + 2, picker_pos.y + sv_picker_size - 2);

        // Render Hue Bar
        for (int i = 0; i < 6; ++i)
            draw_list->AddRectFilledMultiColor(ImVec2(bar0_pos_x, picker_pos.y + i * (sv_picker_size / 6)), ImVec2(bar0_pos_x + bars_width, picker_pos.y + (i + 1) * (sv_picker_size / 6)), col_hues[i], col_hues[i], col_hues[i + 1], col_hues[i + 1]);
        float bar0_line_y = IM_ROUND(picker_pos.y + H * sv_picker_size);
        RenderFrameBorder(ImVec2(bar0_pos_x, picker_pos.y), ImVec2(bar0_pos_x + bars_width, picker_pos.y + sv_picker_size), 0.0f);
        RenderArrowsForVerticalBar(draw_list, ImVec2(bar0_pos_x - 1, bar0_line_y), ImVec2(bars_triangles_half_sz + 1, bars_triangles_half_sz), bars_width + 2.0f, style.Alpha);
    }

    // Render cursor/preview circle (clamp S/V within 0..1 range because floating points colors may lead HSV values to be out of range)
    float sv_cursor_rad = value_changed_sv ? 10.0f : 6.0f;
    draw_list->AddCircleFilled(sv_cursor_pos, sv_cursor_rad, user_col32_striped_of_alpha, 12);
    draw_list->AddCircle(sv_cursor_pos, sv_cursor_rad + 1, col_midgrey, 12);
    draw_list->AddCircle(sv_cursor_pos, sv_cursor_rad, col_white, 12);

    // Render alpha bar
    if (alpha_bar)
    {
        float alpha = ImSaturate(col[3]);
        ImRect bar1_bb(bar1_pos_x, picker_pos.y, bar1_pos_x + bars_width, picker_pos.y + sv_picker_size);
        RenderColorRectWithAlphaCheckerboard(draw_list, bar1_bb.Min, bar1_bb.Max, 0, bar1_bb.GetWidth() / 2.0f, ImVec2(0.0f, 0.0f));
        draw_list->AddRectFilledMultiColor(bar1_bb.Min, bar1_bb.Max, user_col32_striped_of_alpha, user_col32_striped_of_alpha, user_col32_striped_of_alpha & ~IM_COL32_A_MASK, user_col32_striped_of_alpha & ~IM_COL32_A_MASK);
        float bar1_line_y = IM_ROUND(picker_pos.y + (1.0f - alpha) * sv_picker_size);
        RenderFrameBorder(bar1_bb.Min, bar1_bb.Max, 0.0f);
        RenderArrowsForVerticalBar(draw_list, ImVec2(bar1_pos_x - 1, bar1_line_y), ImVec2(bars_triangles_half_sz + 1, bars_triangles_half_sz), bars_width + 2.0f, style.Alpha);
    }

    EndGroup();

    if (value_changed && memcmp(backup_initial_col, col, components * sizeof(float)) == 0)
        value_changed = false;
    if (value_changed)
        MarkItemEdited(window->DC.LastItemId);

    PopID();

    return value_changed;
}

// A little color square. Return true when clicked.
// FIXME: May want to display/ignore the alpha component in the color display? Yet show it in the tooltip.
// 'desc_id' is not called 'label' because we don't display it next to the button, but only in the tooltip.
// Note that 'col' may be encoded in HSV if ImGuiColorEditFlags_InputHSV is set.
bool ImGui::ColorButton(const char* desc_id, const ImVec4& col, ImGuiColorEditFlags flags, ImVec2 size)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(desc_id);
    float default_size = GetFrameHeight();
    if (size.x == 0.0f)
        size.x = default_size;
    if (size.y == 0.0f)
        size.y = default_size;
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ItemSize(bb, (size.y >= default_size) ? g.Style.FramePadding.y : 0.0f);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    if (flags & ImGuiColorEditFlags_NoAlpha)
        flags &= ~(ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf);

    ImVec4 col_rgb = col;
    if (flags & ImGuiColorEditFlags_InputHSV)
        ColorConvertHSVtoRGB(col_rgb.x, col_rgb.y, col_rgb.z, col_rgb.x, col_rgb.y, col_rgb.z);

    ImVec4 col_rgb_without_alpha(col_rgb.x, col_rgb.y, col_rgb.z, 1.0f);
    float grid_step = ImMin(size.x, size.y) / 2.99f;
    float rounding = ImMin(g.Style.FrameRounding, grid_step * 0.5f);
    ImRect bb_inner = bb;
    float off = 0.0f;
    if ((flags & ImGuiColorEditFlags_NoBorder) == 0)
    {
        off = -0.75f; // The border (using Col_FrameBg) tends to look off when color is near-opaque and rounding is enabled. This offset seemed like a good middle ground to reduce those artifacts.
        bb_inner.Expand(off);
    }
    if ((flags & ImGuiColorEditFlags_AlphaPreviewHalf) && col_rgb.w < 1.0f)
    {
        float mid_x = IM_ROUND((bb_inner.Min.x + bb_inner.Max.x) * 0.5f);
        RenderColorRectWithAlphaCheckerboard(window->DrawList, ImVec2(bb_inner.Min.x + grid_step, bb_inner.Min.y), bb_inner.Max, GetColorU32(col_rgb), grid_step, ImVec2(-grid_step + off, off), rounding, ImDrawCornerFlags_TopRight | ImDrawCornerFlags_BotRight);
        window->DrawList->AddRectFilled(bb_inner.Min, ImVec2(mid_x, bb_inner.Max.y), GetColorU32(col_rgb_without_alpha), rounding, ImDrawCornerFlags_TopLeft | ImDrawCornerFlags_BotLeft);
    }
    else
    {
        // Because GetColorU32() multiplies by the global style Alpha and we don't want to display a checkerboard if the source code had no alpha
        ImVec4 col_source = (flags & ImGuiColorEditFlags_AlphaPreview) ? col_rgb : col_rgb_without_alpha;
        if (col_source.w < 1.0f)
            RenderColorRectWithAlphaCheckerboard(window->DrawList, bb_inner.Min, bb_inner.Max, GetColorU32(col_source), grid_step, ImVec2(off, off), rounding);
        else
            window->DrawList->AddRectFilled(bb_inner.Min, bb_inner.Max, GetColorU32(col_source), rounding, ImDrawCornerFlags_All);
    }
    RenderNavHighlight(bb, id);
    if ((flags & ImGuiColorEditFlags_NoBorder) == 0)
    {
        if (g.Style.FrameBorderSize > 0.0f)
            RenderFrameBorder(bb.Min, bb.Max, rounding);
        else
            window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), rounding); // Color button are often in need of some sort of border
    }

    // Drag and Drop Source
    // NB: The ActiveId test is merely an optional micro-optimization, BeginDragDropSource() does the same test.
    if (g.ActiveId == id && !(flags & ImGuiColorEditFlags_NoDragDrop) && BeginDragDropSource())
    {
        if (flags & ImGuiColorEditFlags_NoAlpha)
            SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F, &col_rgb, sizeof(float) * 3, ImGuiCond_Once);
        else
            SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F, &col_rgb, sizeof(float) * 4, ImGuiCond_Once);
        ColorButton(desc_id, col, flags);
        SameLine();
        TextEx("Color");
        EndDragDropSource();
    }

    // Tooltip
    if (!(flags & ImGuiColorEditFlags_NoTooltip) && hovered)
        ColorTooltip(desc_id, &col.x, flags & (ImGuiColorEditFlags__InputMask | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf));

    return pressed;
}

// Initialize/override default color options
void ImGui::SetColorEditOptions(ImGuiColorEditFlags flags)
{
    ImGuiContext& g = *GImGui;
    if ((flags & ImGuiColorEditFlags__DisplayMask) == 0)
        flags |= ImGuiColorEditFlags__OptionsDefault & ImGuiColorEditFlags__DisplayMask;
    if ((flags & ImGuiColorEditFlags__DataTypeMask) == 0)
        flags |= ImGuiColorEditFlags__OptionsDefault & ImGuiColorEditFlags__DataTypeMask;
    if ((flags & ImGuiColorEditFlags__PickerMask) == 0)
        flags |= ImGuiColorEditFlags__OptionsDefault & ImGuiColorEditFlags__PickerMask;
    if ((flags & ImGuiColorEditFlags__InputMask) == 0)
        flags |= ImGuiColorEditFlags__OptionsDefault & ImGuiColorEditFlags__InputMask;
    IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags__DisplayMask));    // Check only 1 option is selected
    IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags__DataTypeMask));   // Check only 1 option is selected
    IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags__PickerMask));     // Check only 1 option is selected
    IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiColorEditFlags__InputMask));      // Check only 1 option is selected
    g.ColorEditOptions = flags;
}

// Note: only access 3 floats if ImGuiColorEditFlags_NoAlpha flag is set.
void ImGui::ColorTooltip(const char* text, const float* col, ImGuiColorEditFlags flags)
{
    ImGuiContext& g = *GImGui;

    BeginTooltipEx(0, ImGuiTooltipFlags_OverridePreviousTooltip);
    const char* text_end = text ? FindRenderedTextEnd(text, NULL) : text;
    if (text_end > text)
    {
        TextEx(text, text_end);
        Separator();
    }

    ImVec2 sz(g.FontSize * 3 + g.Style.FramePadding.y * 2, g.FontSize * 3 + g.Style.FramePadding.y * 2);
    ImVec4 cf(col[0], col[1], col[2], (flags & ImGuiColorEditFlags_NoAlpha) ? 1.0f : col[3]);
    int cr = IM_F32_TO_INT8_SAT(col[0]), cg = IM_F32_TO_INT8_SAT(col[1]), cb = IM_F32_TO_INT8_SAT(col[2]), ca = (flags & ImGuiColorEditFlags_NoAlpha) ? 255 : IM_F32_TO_INT8_SAT(col[3]);
    ColorButton("##preview", cf, (flags & (ImGuiColorEditFlags__InputMask | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf)) | ImGuiColorEditFlags_NoTooltip, sz);
    SameLine();
    if ((flags & ImGuiColorEditFlags_InputRGB) || !(flags & ImGuiColorEditFlags__InputMask))
    {
        if (flags & ImGuiColorEditFlags_NoAlpha)
            Text("#%02X%02X%02X\nR: %d, G: %d, B: %d\n(%.3f, %.3f, %.3f)", cr, cg, cb, cr, cg, cb, col[0], col[1], col[2]);
        else
            Text("#%02X%02X%02X%02X\nR:%d, G:%d, B:%d, A:%d\n(%.3f, %.3f, %.3f, %.3f)", cr, cg, cb, ca, cr, cg, cb, ca, col[0], col[1], col[2], col[3]);
    }
    else if (flags & ImGuiColorEditFlags_InputHSV)
    {
        if (flags & ImGuiColorEditFlags_NoAlpha)
            Text("H: %.3f, S: %.3f, V: %.3f", col[0], col[1], col[2]);
        else
            Text("H: %.3f, S: %.3f, V: %.3f, A: %.3f", col[0], col[1], col[2], col[3]);
    }
    EndTooltip();
}

void ImGui::ColorEditOptionsPopup(const float* col, ImGuiColorEditFlags flags)
{
    bool allow_opt_inputs = !(flags & ImGuiColorEditFlags__DisplayMask);
    bool allow_opt_datatype = !(flags & ImGuiColorEditFlags__DataTypeMask);
    if ((!allow_opt_inputs && !allow_opt_datatype) || !BeginPopup("context"))
        return;
    ImGuiContext& g = *GImGui;
    ImGuiColorEditFlags opts = g.ColorEditOptions;
    if (allow_opt_inputs)
    {
        if (RadioButton("RGB", (opts & ImGuiColorEditFlags_DisplayRGB) != 0)) opts = (opts & ~ImGuiColorEditFlags__DisplayMask) | ImGuiColorEditFlags_DisplayRGB;
        if (RadioButton("HSV", (opts & ImGuiColorEditFlags_DisplayHSV) != 0)) opts = (opts & ~ImGuiColorEditFlags__DisplayMask) | ImGuiColorEditFlags_DisplayHSV;
        if (RadioButton("Hex", (opts & ImGuiColorEditFlags_DisplayHex) != 0)) opts = (opts & ~ImGuiColorEditFlags__DisplayMask) | ImGuiColorEditFlags_DisplayHex;
    }
    if (allow_opt_datatype)
    {
        if (allow_opt_inputs) Separator();
        if (RadioButton("0..255",     (opts & ImGuiColorEditFlags_Uint8) != 0)) opts = (opts & ~ImGuiColorEditFlags__DataTypeMask) | ImGuiColorEditFlags_Uint8;
        if (RadioButton("0.00..1.00", (opts & ImGuiColorEditFlags_Float) != 0)) opts = (opts & ~ImGuiColorEditFlags__DataTypeMask) | ImGuiColorEditFlags_Float;
    }

    if (allow_opt_inputs || allow_opt_datatype)
        Separator();
    if (Button("Copy as..", ImVec2(-1, 0)))
        OpenPopup("Copy");
    if (BeginPopup("Copy"))
    {
        int cr = IM_F32_TO_INT8_SAT(col[0]), cg = IM_F32_TO_INT8_SAT(col[1]), cb = IM_F32_TO_INT8_SAT(col[2]), ca = (flags & ImGuiColorEditFlags_NoAlpha) ? 255 : IM_F32_TO_INT8_SAT(col[3]);
        char buf[64];
        ImFormatString(buf, IM_ARRAYSIZE(buf), "(%.3ff, %.3ff, %.3ff, %.3ff)", col[0], col[1], col[2], (flags & ImGuiColorEditFlags_NoAlpha) ? 1.0f : col[3]);
        if (Selectable(buf))
            SetClipboardText(buf);
        ImFormatString(buf, IM_ARRAYSIZE(buf), "(%d,%d,%d,%d)", cr, cg, cb, ca);
        if (Selectable(buf))
            SetClipboardText(buf);
        ImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", cr, cg, cb);
        if (Selectable(buf))
            SetClipboardText(buf);
        if (!(flags & ImGuiColorEditFlags_NoAlpha))
        {
            ImFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X%02X", cr, cg, cb, ca);
            if (Selectable(buf))
                SetClipboardText(buf);
        }
        EndPopup();
    }

    g.ColorEditOptions = opts;
    EndPopup();
}

void ImGui::ColorPickerOptionsPopup(const float* ref_col, ImGuiColorEditFlags flags)
{
    bool allow_opt_picker = !(flags & ImGuiColorEditFlags__PickerMask);
    bool allow_opt_alpha_bar = !(flags & ImGuiColorEditFlags_NoAlpha) && !(flags & ImGuiColorEditFlags_AlphaBar);
    if ((!allow_opt_picker && !allow_opt_alpha_bar) || !BeginPopup("context"))
        return;
    ImGuiContext& g = *GImGui;
    if (allow_opt_picker)
    {
        ImVec2 picker_size(g.FontSize * 8, ImMax(g.FontSize * 8 - (GetFrameHeight() + g.Style.ItemInnerSpacing.x), 1.0f)); // FIXME: Picker size copied from main picker function
        PushItemWidth(picker_size.x);
        for (int picker_type = 0; picker_type < 2; picker_type++)
        {
            // Draw small/thumbnail version of each picker type (over an invisible button for selection)
            if (picker_type > 0) Separator();
            PushID(picker_type);
            ImGuiColorEditFlags picker_flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview | (flags & ImGuiColorEditFlags_NoAlpha);
            if (picker_type == 0) picker_flags |= ImGuiColorEditFlags_PickerHueBar;
            if (picker_type == 1) picker_flags |= ImGuiColorEditFlags_PickerHueWheel;
            ImVec2 backup_pos = GetCursorScreenPos();
            if (Selectable("##selectable", false, 0, picker_size)) // By default, Selectable() is closing popup
                g.ColorEditOptions = (g.ColorEditOptions & ~ImGuiColorEditFlags__PickerMask) | (picker_flags & ImGuiColorEditFlags__PickerMask);
            SetCursorScreenPos(backup_pos);
            ImVec4 previewing_ref_col;
            memcpy(&previewing_ref_col, ref_col, sizeof(float) * ((picker_flags & ImGuiColorEditFlags_NoAlpha) ? 3 : 4));
            ColorPicker4("##previewing_picker", &previewing_ref_col.x, picker_flags);
            PopID();
        }
        PopItemWidth();
    }
    if (allow_opt_alpha_bar)
    {
        if (allow_opt_picker) Separator();
        CheckboxFlags("Alpha Bar", &g.ColorEditOptions, ImGuiColorEditFlags_AlphaBar);
    }
    EndPopup();
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: TreeNode, CollapsingHeader, etc.
//-------------------------------------------------------------------------
// - TreeNode()
// - TreeNodeV()
// - TreeNodeEx()
// - TreeNodeExV()
// - TreeNodeBehavior() [Internal]
// - TreePush()
// - TreePop()
// - GetTreeNodeToLabelSpacing()
// - SetNextItemOpen()
// - CollapsingHeader()
//-------------------------------------------------------------------------

bool ImGui::TreeNode(const char* str_id, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool is_open = TreeNodeExV(str_id, 0, fmt, args);
    va_end(args);
    return is_open;
}

bool ImGui::TreeNode(const void* ptr_id, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool is_open = TreeNodeExV(ptr_id, 0, fmt, args);
    va_end(args);
    return is_open;
}

bool ImGui::TreeNode(const char* label)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;
    return TreeNodeBehavior(window->GetID(label), 0, label, NULL);
}

bool ImGui::TreeNodeV(const char* str_id, const char* fmt, va_list args)
{
    return TreeNodeExV(str_id, 0, fmt, args);
}

bool ImGui::TreeNodeV(const void* ptr_id, const char* fmt, va_list args)
{
    return TreeNodeExV(ptr_id, 0, fmt, args);
}

bool ImGui::TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    return TreeNodeBehavior(window->GetID(label), flags, label, NULL);
}

bool ImGui::TreeNodeEx(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool is_open = TreeNodeExV(str_id, flags, fmt, args);
    va_end(args);
    return is_open;
}

bool ImGui::TreeNodeEx(const void* ptr_id, ImGuiTreeNodeFlags flags, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    bool is_open = TreeNodeExV(ptr_id, flags, fmt, args);
    va_end(args);
    return is_open;
}

bool ImGui::TreeNodeExV(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt, va_list args)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const char* label_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);
    return TreeNodeBehavior(window->GetID(str_id), flags, g.TempBuffer, label_end);
}

bool ImGui::TreeNodeExV(const void* ptr_id, ImGuiTreeNodeFlags flags, const char* fmt, va_list args)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const char* label_end = g.TempBuffer + ImFormatStringV(g.TempBuffer, IM_ARRAYSIZE(g.TempBuffer), fmt, args);
    return TreeNodeBehavior(window->GetID(ptr_id), flags, g.TempBuffer, label_end);
}

bool ImGui::TreeNodeBehaviorIsOpen(ImGuiID id, ImGuiTreeNodeFlags flags)
{
    if (flags & ImGuiTreeNodeFlags_Leaf)
        return true;

    // We only write to the tree storage if the user clicks (or explicitly use the SetNextItemOpen function)
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiStorage* storage = window->DC.StateStorage;

    bool is_open;
    if (g.NextItemData.Flags & ImGuiNextItemDataFlags_HasOpen)
    {
        if (g.NextItemData.OpenCond & ImGuiCond_Always)
        {
            is_open = g.NextItemData.OpenVal;
            storage->SetInt(id, is_open);
        }
        else
        {
            // We treat ImGuiCond_Once and ImGuiCond_FirstUseEver the same because tree node state are not saved persistently.
            const int stored_value = storage->GetInt(id, -1);
            if (stored_value == -1)
            {
                is_open = g.NextItemData.OpenVal;
                storage->SetInt(id, is_open);
            }
            else
            {
                is_open = stored_value != 0;
            }
        }
    }
    else
    {
        is_open = storage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) ? 1 : 0) != 0;
    }

    // When logging is enabled, we automatically expand tree nodes (but *NOT* collapsing headers.. seems like sensible behavior).
    // NB- If we are above max depth we still allow manually opened nodes to be logged.
    if (g.LogEnabled && !(flags & ImGuiTreeNodeFlags_NoAutoOpenOnLog) && (window->DC.TreeDepth - g.LogDepthRef) < g.LogDepthToExpand)
        is_open = true;

    return is_open;
}

bool ImGui::TreeNodeBehavior(ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const bool display_frame = (flags & ImGuiTreeNodeFlags_Framed) != 0;
    const ImVec2 padding = (display_frame || (flags & ImGuiTreeNodeFlags_FramePadding)) ? style.FramePadding : ImVec2(style.FramePadding.x, ImMin(window->DC.CurrLineTextBaseOffset, style.FramePadding.y));

    if (!label_end)
        label_end = FindRenderedTextEnd(label);
    const ImVec2 label_size = CalcTextSize(label, label_end, false);

    // We vertically grow up to current line height up the typical widget height.
    const float frame_height = ImMax(ImMin(window->DC.CurrLineSize.y, g.FontSize + style.FramePadding.y * 2), label_size.y + padding.y * 2);
    ImRect frame_bb;
    frame_bb.Min.x = (flags & ImGuiTreeNodeFlags_SpanFullWidth) ? window->WorkRect.Min.x : window->DC.CursorPos.x;
    frame_bb.Min.y = window->DC.CursorPos.y;
    frame_bb.Max.x = window->WorkRect.Max.x;
    frame_bb.Max.y = window->DC.CursorPos.y + frame_height;
    if (display_frame)
    {
        // Framed header expand a little outside the default padding, to the edge of InnerClipRect
        // (FIXME: May remove this at some point and make InnerClipRect align with WindowPadding.x instead of WindowPadding.x*0.5f)
        frame_bb.Min.x -= IM_FLOOR(window->WindowPadding.x * 0.5f - 1.0f);
        frame_bb.Max.x += IM_FLOOR(window->WindowPadding.x * 0.5f);
    }

    const float text_offset_x = g.FontSize + (display_frame ? padding.x * 3 : padding.x * 2);           // Collapser arrow width + Spacing
    const float text_offset_y = ImMax(padding.y, window->DC.CurrLineTextBaseOffset);                    // Latch before ItemSize changes it
    const float text_width = g.FontSize + (label_size.x > 0.0f ? label_size.x + padding.x * 2 : 0.0f);  // Include collapser
    ImVec2 text_pos(window->DC.CursorPos.x + text_offset_x, window->DC.CursorPos.y + text_offset_y);
    ItemSize(ImVec2(text_width, frame_height), padding.y);

    // For regular tree nodes, we arbitrary allow to click past 2 worth of ItemSpacing
    ImRect interact_bb = frame_bb;
    if (!display_frame && (flags & (ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth)) == 0)
        interact_bb.Max.x = frame_bb.Min.x + text_width + style.ItemSpacing.x * 2.0f;

    // Store a flag for the current depth to tell if we will allow closing this node when navigating one of its child.
    // For this purpose we essentially compare if g.NavIdIsAlive went from 0 to 1 between TreeNode() and TreePop().
    // This is currently only support 32 level deep and we are fine with (1 << Depth) overflowing into a zero.
    const bool is_leaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;
    bool is_open = TreeNodeBehaviorIsOpen(id, flags);
    if (is_open && !g.NavIdIsAlive && (flags & ImGuiTreeNodeFlags_NavLeftJumpsBackHere) && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
        window->DC.TreeJumpToParentOnPopMask |= (1 << window->DC.TreeDepth);

    bool item_add = ItemAdd(interact_bb, id);
    window->DC.LastItemStatusFlags |= ImGuiItemStatusFlags_HasDisplayRect;
    window->DC.LastItemDisplayRect = frame_bb;

    if (!item_add)
    {
        if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
            TreePushOverrideID(id);
        IMGUI_TEST_ENGINE_ITEM_INFO(window->DC.LastItemId, label, window->DC.ItemFlags | (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) | (is_open ? ImGuiItemStatusFlags_Opened : 0));
        return is_open;
    }

    ImGuiButtonFlags button_flags = ImGuiTreeNodeFlags_None;
    if (flags & ImGuiTreeNodeFlags_AllowItemOverlap)
        button_flags |= ImGuiButtonFlags_AllowItemOverlap;
    if (!is_leaf)
        button_flags |= ImGuiButtonFlags_PressedOnDragDropHold;

    // We allow clicking on the arrow section with keyboard modifiers held, in order to easily
    // allow browsing a tree while preserving selection with code implementing multi-selection patterns.
    // When clicking on the rest of the tree node we always disallow keyboard modifiers.
    const float arrow_hit_x1 = (text_pos.x - text_offset_x) - style.TouchExtraPadding.x;
    const float arrow_hit_x2 = (text_pos.x - text_offset_x) + (g.FontSize + padding.x * 2.0f) + style.TouchExtraPadding.x;
    const bool is_mouse_x_over_arrow = (g.IO.MousePos.x >= arrow_hit_x1 && g.IO.MousePos.x < arrow_hit_x2);
    if (window != g.HoveredWindow || !is_mouse_x_over_arrow)
        button_flags |= ImGuiButtonFlags_NoKeyModifiers;

    // Open behaviors can be altered with the _OpenOnArrow and _OnOnDoubleClick flags.
    // Some alteration have subtle effects (e.g. toggle on MouseUp vs MouseDown events) due to requirements for multi-selection and drag and drop support.
    // - Single-click on label = Toggle on MouseUp (default, when _OpenOnArrow=0)
    // - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=0)
    // - Single-click on arrow = Toggle on MouseDown (when _OpenOnArrow=1)
    // - Double-click on label = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1)
    // - Double-click on arrow = Toggle on MouseDoubleClick (when _OpenOnDoubleClick=1 and _OpenOnArrow=0)
    // It is rather standard that arrow click react on Down rather than Up.
    // We set ImGuiButtonFlags_PressedOnClickRelease on OpenOnDoubleClick because we want the item to be active on the initial MouseDown in order for drag and drop to work.
    if (is_mouse_x_over_arrow)
        button_flags |= ImGuiButtonFlags_PressedOnClick;
    else if (flags & ImGuiTreeNodeFlags_OpenOnDoubleClick)
        button_flags |= ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnDoubleClick;
    else
        button_flags |= ImGuiButtonFlags_PressedOnClickRelease;

    bool selected = (flags & ImGuiTreeNodeFlags_Selected) != 0;
    const bool was_selected = selected;

    bool hovered, held;
    bool pressed = ButtonBehavior(interact_bb, id, &hovered, &held, button_flags);
    bool toggled = false;
    if (!is_leaf)
    {
        if (pressed && g.DragDropHoldJustPressedId != id)
        {
            if ((flags & (ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick)) == 0 || (g.NavActivateId == id))
                toggled = true;
            if (flags & ImGuiTreeNodeFlags_OpenOnArrow)
                toggled |= is_mouse_x_over_arrow && !g.NavDisableMouseHover; // Lightweight equivalent of IsMouseHoveringRect() since ButtonBehavior() already did the job
            if ((flags & ImGuiTreeNodeFlags_OpenOnDoubleClick) && g.IO.MouseDoubleClicked[0])
                toggled = true;
        }
        else if (pressed && g.DragDropHoldJustPressedId == id)
        {
            IM_ASSERT(button_flags & ImGuiButtonFlags_PressedOnDragDropHold);
            if (!is_open) // When using Drag and Drop "hold to open" we keep the node highlighted after opening, but never close it again.
                toggled = true;
        }

        if (g.NavId == id && g.NavMoveRequest && g.NavMoveDir == ImGuiDir_Left && is_open)
        {
            toggled = true;
            NavMoveRequestCancel();
        }
        if (g.NavId == id && g.NavMoveRequest && g.NavMoveDir == ImGuiDir_Right && !is_open) // If there's something upcoming on the line we may want to give it the priority?
        {
            toggled = true;
            NavMoveRequestCancel();
        }

        if (toggled)
        {
            is_open = !is_open;
            window->DC.StateStorage->SetInt(id, is_open);
            window->DC.LastItemStatusFlags |= ImGuiItemStatusFlags_ToggledOpen;
        }
    }
    if (flags & ImGuiTreeNodeFlags_AllowItemOverlap)
        SetItemAllowOverlap();

    // In this branch, TreeNodeBehavior() cannot toggle the selection so this will never trigger.
    if (selected != was_selected) //-V547
        window->DC.LastItemStatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

    // Render
    const ImU32 text_col = GetColorU32(ImGuiCol_Text);
    ImGuiNavHighlightFlags nav_highlight_flags = ImGuiNavHighlightFlags_TypeThin;
    if (display_frame)
    {
        // Framed type
        const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
        RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, true, style.FrameRounding);
        RenderNavHighlight(frame_bb, id, nav_highlight_flags);
        if (flags & ImGuiTreeNodeFlags_Bullet)
            RenderBullet(window->DrawList, ImVec2(text_pos.x - text_offset_x * 0.60f, text_pos.y + g.FontSize * 0.5f), text_col);
        else if (!is_leaf)
            RenderArrow(window->DrawList, ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y), text_col, is_open ? ImGuiDir_Down : ImGuiDir_Right, 1.0f);
        else // Leaf without bullet, left-adjusted text
            text_pos.x -= text_offset_x;
        if (flags & ImGuiTreeNodeFlags_ClipLabelForTrailingButton)
            frame_bb.Max.x -= g.FontSize + style.FramePadding.x;
        if (g.LogEnabled)
        {
            // NB: '##' is normally used to hide text (as a library-wide feature), so we need to specify the text range to make sure the ## aren't stripped out here.
            const char log_prefix[] = "\n##";
            const char log_suffix[] = "##";
            LogRenderedText(&text_pos, log_prefix, log_prefix + 3);
            RenderTextClipped(text_pos, frame_bb.Max, label, label_end, &label_size);
            LogRenderedText(&text_pos, log_suffix, log_suffix + 2);
        }
        else
        {
            RenderTextClipped(text_pos, frame_bb.Max, label, label_end, &label_size);
        }
    }
    else
    {
        // Unframed typed for tree nodes
        if (hovered || selected)
        {
            const ImU32 bg_col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
            RenderFrame(frame_bb.Min, frame_bb.Max, bg_col, false);
            RenderNavHighlight(frame_bb, id, nav_highlight_flags);
        }
        if (flags & ImGuiTreeNodeFlags_Bullet)
            RenderBullet(window->DrawList, ImVec2(text_pos.x - text_offset_x * 0.5f, text_pos.y + g.FontSize * 0.5f), text_col);
        else if (!is_leaf)
            RenderArrow(window->DrawList, ImVec2(text_pos.x - text_offset_x + padding.x, text_pos.y + g.FontSize * 0.15f), text_col, is_open ? ImGuiDir_Down : ImGuiDir_Right, 0.70f);
        if (g.LogEnabled)
            LogRenderedText(&text_pos, ">");
        RenderText(text_pos, label, label_end, false);
    }

    if (is_open && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
        TreePushOverrideID(id);
    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags | (is_leaf ? 0 : ImGuiItemStatusFlags_Openable) | (is_open ? ImGuiItemStatusFlags_Opened : 0));
    return is_open;
}

void ImGui::TreePush(const char* str_id)
{
    ImGuiWindow* window = GetCurrentWindow();
    Indent();
    window->DC.TreeDepth++;
    PushID(str_id ? str_id : "#TreePush");
}

void ImGui::TreePush(const void* ptr_id)
{
    ImGuiWindow* window = GetCurrentWindow();
    Indent();
    window->DC.TreeDepth++;
    PushID(ptr_id ? ptr_id : (const void*)"#TreePush");
}

void ImGui::TreePushOverrideID(ImGuiID id)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    Indent();
    window->DC.TreeDepth++;
    window->IDStack.push_back(id);
}

void ImGui::TreePop()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    Unindent();

    window->DC.TreeDepth--;
    ImU32 tree_depth_mask = (1 << window->DC.TreeDepth);

    // Handle Left arrow to move to parent tree node (when ImGuiTreeNodeFlags_NavLeftJumpsBackHere is enabled)
    if (g.NavMoveDir == ImGuiDir_Left && g.NavWindow == window && NavMoveRequestButNoResultYet())
        if (g.NavIdIsAlive && (window->DC.TreeJumpToParentOnPopMask & tree_depth_mask))
        {
            SetNavID(window->IDStack.back(), g.NavLayer, 0);
            NavMoveRequestCancel();
        }
    window->DC.TreeJumpToParentOnPopMask &= tree_depth_mask - 1;

    IM_ASSERT(window->IDStack.Size > 1); // There should always be 1 element in the IDStack (pushed during window creation). If this triggers you called TreePop/PopID too much.
    PopID();
}

// Horizontal distance preceding label when using TreeNode() or Bullet()
float ImGui::GetTreeNodeToLabelSpacing()
{
    ImGuiContext& g = *GImGui;
    return g.FontSize + (g.Style.FramePadding.x * 2.0f);
}

// Set next TreeNode/CollapsingHeader open state.
void ImGui::SetNextItemOpen(bool is_open, ImGuiCond cond)
{
    ImGuiContext& g = *GImGui;
    if (g.CurrentWindow->SkipItems)
        return;
    g.NextItemData.Flags |= ImGuiNextItemDataFlags_HasOpen;
    g.NextItemData.OpenVal = is_open;
    g.NextItemData.OpenCond = cond ? cond : ImGuiCond_Always;
}

// CollapsingHeader returns true when opened but do not indent nor push into the ID stack (because of the ImGuiTreeNodeFlags_NoTreePushOnOpen flag).
// This is basically the same as calling TreeNodeEx(label, ImGuiTreeNodeFlags_CollapsingHeader). You can remove the _NoTreePushOnOpen flag if you want behavior closer to normal TreeNode().
bool ImGui::CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    return TreeNodeBehavior(window->GetID(label), flags | ImGuiTreeNodeFlags_CollapsingHeader, label);
}

bool ImGui::CollapsingHeader(const char* label, bool* p_open, ImGuiTreeNodeFlags flags)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    if (p_open && !*p_open)
        return false;

    ImGuiID id = window->GetID(label);
    flags |= ImGuiTreeNodeFlags_CollapsingHeader;
    if (p_open)
        flags |= ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_ClipLabelForTrailingButton;
    bool is_open = TreeNodeBehavior(id, flags, label);
    if (p_open != NULL)
    {
        // Create a small overlapping close button
        // FIXME: We can evolve this into user accessible helpers to add extra buttons on title bars, headers, etc.
        // FIXME: CloseButton can overlap into text, need find a way to clip the text somehow.
        ImGuiContext& g = *GImGui;
        ImGuiLastItemDataBackup last_item_backup;
        float button_size = g.FontSize;
        float button_x = ImMax(window->DC.LastItemRect.Min.x, window->DC.LastItemRect.Max.x - g.Style.FramePadding.x * 2.0f - button_size);
        float button_y = window->DC.LastItemRect.Min.y;
        ImGuiID close_button_id = GetIDWithSeed("#CLOSE", NULL, id);
        if (CloseButton(close_button_id, ImVec2(button_x, button_y)))
            *p_open = false;
        last_item_backup.Restore();
    }

    return is_open;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Selectable
//-------------------------------------------------------------------------
// - Selectable()
//-------------------------------------------------------------------------

// Tip: pass a non-visible label (e.g. "##hello") then you can use the space to draw other text or image.
// But you need to make sure the ID is unique, e.g. enclose calls in PushID/PopID or use ##unique_id.
// With this scheme, ImGuiSelectableFlags_SpanAllColumns and ImGuiSelectableFlags_AllowItemOverlap are also frequently used flags.
// FIXME: Selectable() with (size.x == 0.0f) and (SelectableTextAlign.x > 0.0f) followed by SameLine() is currently not supported.
bool ImGui::Selectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    // Submit label or explicit size to ItemSize(), whereas ItemAdd() will submit a larger/spanning rectangle.
    ImGuiID id = window->GetID(label);
    ImVec2 label_size = CalcTextSize(label, NULL, true);
    ImVec2 size(size_arg.x != 0.0f ? size_arg.x : label_size.x, size_arg.y != 0.0f ? size_arg.y : label_size.y);
    ImVec2 pos = window->DC.CursorPos;
    pos.y += window->DC.CurrLineTextBaseOffset;
    ItemSize(size, 0.0f);

    // Fill horizontal space
    // We don't support (size < 0.0f) in Selectable() because the ItemSpacing extension would make explicitely right-aligned sizes not visibly match other widgets.
    const bool span_all_columns = (flags & ImGuiSelectableFlags_SpanAllColumns) != 0;
    const float min_x = span_all_columns ? window->ParentWorkRect.Min.x : pos.x;
    const float max_x = span_all_columns ? window->ParentWorkRect.Max.x : window->WorkRect.Max.x;
    if (size_arg.x == 0.0f || (flags & ImGuiSelectableFlags_SpanAvailWidth))
        size.x = ImMax(label_size.x, max_x - min_x);

    // Text stays at the submission position, but bounding box may be extended on both sides
    const ImVec2 text_min = pos;
    const ImVec2 text_max(min_x + size.x, pos.y + size.y);

    // Selectables are meant to be tightly packed together with no click-gap, so we extend their box to cover spacing between selectable.
    ImRect bb(min_x, pos.y, text_max.x, text_max.y);
    if ((flags & ImGuiSelectableFlags_NoPadWithHalfSpacing) == 0)
    {
        const float spacing_x = span_all_columns ? 0.0f : style.ItemSpacing.x;
        const float spacing_y = style.ItemSpacing.y;
        const float spacing_L = IM_FLOOR(spacing_x * 0.50f);
        const float spacing_U = IM_FLOOR(spacing_y * 0.50f);
        bb.Min.x -= spacing_L;
        bb.Min.y -= spacing_U;
        bb.Max.x += (spacing_x - spacing_L);
        bb.Max.y += (spacing_y - spacing_U);
    }
    //if (g.IO.KeyCtrl) { GetForegroundDrawList()->AddRect(bb.Min, bb.Max, IM_COL32(0, 255, 0, 255)); }

    // Modify ClipRect for the ItemAdd(), faster than doing a PushColumnsBackground/PushTableBackground for every Selectable..
    const float backup_clip_rect_min_x = window->ClipRect.Min.x;
    const float backup_clip_rect_max_x = window->ClipRect.Max.x;
    if (span_all_columns)
    {
        window->ClipRect.Min.x = window->ParentWorkRect.Min.x;
        window->ClipRect.Max.x = window->ParentWorkRect.Max.x;
    }

    bool item_add;
    if (flags & ImGuiSelectableFlags_Disabled)
    {
        ImGuiItemFlags backup_item_flags = window->DC.ItemFlags;
        window->DC.ItemFlags |= ImGuiItemFlags_Disabled | ImGuiItemFlags_NoNavDefaultFocus;
        item_add = ItemAdd(bb, id);
        window->DC.ItemFlags = backup_item_flags;
    }
    else
    {
        item_add = ItemAdd(bb, id);
    }

    if (span_all_columns)
    {
        window->ClipRect.Min.x = backup_clip_rect_min_x;
        window->ClipRect.Max.x = backup_clip_rect_max_x;
    }

    if (!item_add)
        return false;

    // FIXME: We can standardize the behavior of those two, we could also keep the fast path of override ClipRect + full push on render only,
    // which would be advantageous since most selectable are not selected.
    if (span_all_columns && window->DC.CurrentColumns)
        PushColumnsBackground();
    else if (span_all_columns && g.CurrentTable)
        PushTableBackground();

    // We use NoHoldingActiveID on menus so user can click and _hold_ on a menu then drag to browse child entries
    ImGuiButtonFlags button_flags = 0;
    if (flags & ImGuiSelectableFlags_NoHoldingActiveID) { button_flags |= ImGuiButtonFlags_NoHoldingActiveId; }
    if (flags & ImGuiSelectableFlags_SelectOnClick)     { button_flags |= ImGuiButtonFlags_PressedOnClick; }
    if (flags & ImGuiSelectableFlags_SelectOnRelease)   { button_flags |= ImGuiButtonFlags_PressedOnRelease; }
    if (flags & ImGuiSelectableFlags_Disabled)          { button_flags |= ImGuiButtonFlags_Disabled; }
    if (flags & ImGuiSelectableFlags_AllowDoubleClick)  { button_flags |= ImGuiButtonFlags_PressedOnClickRelease | ImGuiButtonFlags_PressedOnDoubleClick; }
    if (flags & ImGuiSelectableFlags_AllowItemOverlap)  { button_flags |= ImGuiButtonFlags_AllowItemOverlap; }

    if (flags & ImGuiSelectableFlags_Disabled)
        selected = false;

    const bool was_selected = selected;
    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, button_flags);

    // Update NavId when clicking or when Hovering (this doesn't happen on most widgets), so navigation can be resumed with gamepad/keyboard
    if (pressed || (hovered && (flags & ImGuiSelectableFlags_SetNavIdOnHover)))
    {
        if (!g.NavDisableMouseHover && g.NavWindow == window && g.NavLayer == window->DC.NavLayerCurrent)
        {
            g.NavDisableHighlight = true;
            SetNavID(id, window->DC.NavLayerCurrent, window->DC.NavFocusScopeIdCurrent);
        }
    }
    if (pressed)
        MarkItemEdited(id);

    if (flags & ImGuiSelectableFlags_AllowItemOverlap)
        SetItemAllowOverlap();

    // In this branch, Selectable() cannot toggle the selection so this will never trigger.
    if (selected != was_selected) //-V547
        window->DC.LastItemStatusFlags |= ImGuiItemStatusFlags_ToggledSelection;

    // Render
    if (held && (flags & ImGuiSelectableFlags_DrawHoveredWhenHeld))
        hovered = true;
    if (hovered || selected)
    {
        const ImU32 col = GetColorU32((held && hovered) ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
        RenderFrame(bb.Min, bb.Max, col, false, 0.0f);
        RenderNavHighlight(bb, id, ImGuiNavHighlightFlags_TypeThin | ImGuiNavHighlightFlags_NoRounding);
    }

    if (span_all_columns && window->DC.CurrentColumns)
        PopColumnsBackground();
    else if (span_all_columns && g.CurrentTable)
        PopTableBackground();

    if (flags & ImGuiSelectableFlags_Disabled) PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
    RenderTextClipped(text_min, text_max, label, NULL, &label_size, style.SelectableTextAlign, &bb);
    if (flags & ImGuiSelectableFlags_Disabled) PopStyleColor();

    // Automatically close popups
    if (pressed && (window->Flags & ImGuiWindowFlags_Popup) && !(flags & ImGuiSelectableFlags_DontClosePopups) && !(window->DC.ItemFlags & ImGuiItemFlags_SelectableDontClosePopup))
        CloseCurrentPopup();

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags);
    return pressed;
}

bool ImGui::Selectable(const char* label, bool* p_selected, ImGuiSelectableFlags flags, const ImVec2& size_arg)
{
    if (Selectable(label, *p_selected, flags, size_arg))
    {
        *p_selected = !*p_selected;
        return true;
    }
    return false;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: ListBox
//-------------------------------------------------------------------------
// - ListBox()
// - ListBoxHeader()
// - ListBoxFooter()
//-------------------------------------------------------------------------
// FIXME: This is an old API. We should redesign some of it, rename ListBoxHeader->BeginListBox, ListBoxFooter->EndListBox
// and promote using them over existing ListBox() functions, similarly to change with combo boxes.
//-------------------------------------------------------------------------

// FIXME: In principle this function should be called BeginListBox(). We should rename it after re-evaluating if we want to keep the same signature.
// Helper to calculate the size of a listbox and display a label on the right.
// Tip: To have a list filling the entire window width, PushItemWidth(-1) and pass an non-visible label e.g. "##empty"
bool ImGui::ListBoxHeader(const char* label, const ImVec2& size_arg)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImGuiStyle& style = g.Style;
    const ImGuiID id = GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    // Size default to hold ~7 items. Fractional number of items helps seeing that we can scroll down/up without looking at scrollbar.
    ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), GetTextLineHeightWithSpacing() * 7.4f + style.ItemSpacing.y);
    ImVec2 frame_size = ImVec2(size.x, ImMax(size.y, label_size.y));
    ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    ImRect bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
    window->DC.LastItemRect = bb; // Forward storage for ListBoxFooter.. dodgy.
    g.NextItemData.ClearFlags();

    if (!IsRectVisible(bb.Min, bb.Max))
    {
        ItemSize(bb.GetSize(), style.FramePadding.y);
        ItemAdd(bb, 0, &frame_bb);
        return false;
    }

    BeginGroup();
    if (label_size.x > 0)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);

    BeginChildFrame(id, frame_bb.GetSize());
    return true;
}

// FIXME: In principle this function should be called EndListBox(). We should rename it after re-evaluating if we want to keep the same signature.
bool ImGui::ListBoxHeader(const char* label, int items_count, int height_in_items)
{
    // Size default to hold ~7.25 items.
    // We add +25% worth of item height to allow the user to see at a glance if there are more items up/down, without looking at the scrollbar.
    // We don't add this extra bit if items_count <= height_in_items. It is slightly dodgy, because it means a dynamic list of items will make the widget resize occasionally when it crosses that size.
    // I am expecting that someone will come and complain about this behavior in a remote future, then we can advise on a better solution.
    if (height_in_items < 0)
        height_in_items = ImMin(items_count, 7);
    const ImGuiStyle& style = GetStyle();
    float height_in_items_f = (height_in_items < items_count) ? (height_in_items + 0.25f) : (height_in_items + 0.00f);

    // We include ItemSpacing.y so that a list sized for the exact number of items doesn't make a scrollbar appears. We could also enforce that by passing a flag to BeginChild().
    ImVec2 size;
    size.x = 0.0f;
    size.y = ImFloor(GetTextLineHeightWithSpacing() * height_in_items_f + style.FramePadding.y * 2.0f);
    return ListBoxHeader(label, size);
}

// FIXME: In principle this function should be called EndListBox(). We should rename it after re-evaluating if we want to keep the same signature.
void ImGui::ListBoxFooter()
{
    ImGuiWindow* parent_window = GetCurrentWindow()->ParentWindow;
    const ImRect bb = parent_window->DC.LastItemRect;
    const ImGuiStyle& style = GetStyle();

    EndChildFrame();

    // Redeclare item size so that it includes the label (we have stored the full size in LastItemRect)
    // We call SameLine() to restore DC.CurrentLine* data
    SameLine();
    parent_window->DC.CursorPos = bb.Min;
    ItemSize(bb, style.FramePadding.y);
    EndGroup();
}

bool ImGui::ListBox(const char* label, int* current_item, const char* const items[], int items_count, int height_items)
{
    const bool value_changed = ListBox(label, current_item, Items_ArrayGetter, (void*)items, items_count, height_items);
    return value_changed;
}

bool ImGui::ListBox(const char* label, int* current_item, bool (*items_getter)(void*, int, const char**), void* data, int items_count, int height_in_items)
{
    if (!ListBoxHeader(label, items_count, height_in_items))
        return false;

    // Assume all items have even height (= 1 line of text). If you need items of different or variable sizes you can create a custom version of ListBox() in your code without using the clipper.
    ImGuiContext& g = *GImGui;
    bool value_changed = false;
    ImGuiListClipper clipper;
    clipper.Begin(items_count, GetTextLineHeightWithSpacing()); // We know exactly our line height here so we pass it as a minor optimization, but generally you don't need to.
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            const bool item_selected = (i == *current_item);
            const char* item_text;
            if (!items_getter(data, i, &item_text))
                item_text = "*Unknown item*";

            PushID(i);
            if (Selectable(item_text, item_selected))
            {
                *current_item = i;
                value_changed = true;
            }
            if (item_selected)
                SetItemDefaultFocus();
            PopID();
        }
    ListBoxFooter();
    if (value_changed)
        MarkItemEdited(g.CurrentWindow->DC.LastItemId);

    return value_changed;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: PlotLines, PlotHistogram
//-------------------------------------------------------------------------
// - PlotEx() [Internal]
// - PlotLines()
// - PlotHistogram()
//-------------------------------------------------------------------------

int ImGui::PlotEx(ImGuiPlotType plot_type, const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 frame_size)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return -1;

    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    if (frame_size.x == 0.0f)
        frame_size.x = CalcItemWidth();
    if (frame_size.y == 0.0f)
        frame_size.y = label_size.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, 0, &frame_bb))
        return -1;
    const bool hovered = ItemHoverable(frame_bb, id);

    // Determine scale from values if not specified
    if (scale_min == FLT_MAX || scale_max == FLT_MAX)
    {
        float v_min = FLT_MAX;
        float v_max = -FLT_MAX;
        for (int i = 0; i < values_count; i++)
        {
            const float v = values_getter(data, i);
            if (v != v) // Ignore NaN values
                continue;
            v_min = ImMin(v_min, v);
            v_max = ImMax(v_max, v);
        }
        if (scale_min == FLT_MAX)
            scale_min = v_min;
        if (scale_max == FLT_MAX)
            scale_max = v_max;
    }

    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    const int values_count_min = (plot_type == ImGuiPlotType_Lines) ? 2 : 1;
    int idx_hovered = -1;
    if (values_count >= values_count_min)
    {
        int res_w = ImMin((int)frame_size.x, values_count) + ((plot_type == ImGuiPlotType_Lines) ? -1 : 0);
        int item_count = values_count + ((plot_type == ImGuiPlotType_Lines) ? -1 : 0);

        // Tooltip on hover
        if (hovered && inner_bb.Contains(g.IO.MousePos))
        {
            const float t = ImClamp((g.IO.MousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f);
            const int v_idx = (int)(t * item_count);
            IM_ASSERT(v_idx >= 0 && v_idx < values_count);

            const float v0 = values_getter(data, (v_idx + values_offset) % values_count);
            const float v1 = values_getter(data, (v_idx + 1 + values_offset) % values_count);
            if (plot_type == ImGuiPlotType_Lines)
                SetTooltip("%d: %8.4g\n%d: %8.4g", v_idx, v0, v_idx + 1, v1);
            else if (plot_type == ImGuiPlotType_Histogram)
                SetTooltip("%d: %8.4g", v_idx, v0);
            idx_hovered = v_idx;
        }

        const float t_step = 1.0f / (float)res_w;
        const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));

        float v0 = values_getter(data, (0 + values_offset) % values_count);
        float t0 = 0.0f;
        ImVec2 tp0 = ImVec2( t0, 1.0f - ImSaturate((v0 - scale_min) * inv_scale) );                       // Point in the normalized space of our target rectangle
        float histogram_zero_line_t = (scale_min * scale_max < 0.0f) ? (-scale_min * inv_scale) : (scale_min < 0.0f ? 0.0f : 1.0f);   // Where does the zero line stands

        const ImU32 col_base = GetColorU32((plot_type == ImGuiPlotType_Lines) ? ImGuiCol_PlotLines : ImGuiCol_PlotHistogram);
        const ImU32 col_hovered = GetColorU32((plot_type == ImGuiPlotType_Lines) ? ImGuiCol_PlotLinesHovered : ImGuiCol_PlotHistogramHovered);

        for (int n = 0; n < res_w; n++)
        {
            const float t1 = t0 + t_step;
            const int v1_idx = (int)(t0 * item_count + 0.5f);
            IM_ASSERT(v1_idx >= 0 && v1_idx < values_count);
            const float v1 = values_getter(data, (v1_idx + values_offset + 1) % values_count);
            const ImVec2 tp1 = ImVec2( t1, 1.0f - ImSaturate((v1 - scale_min) * inv_scale) );

            // NB: Draw calls are merged together by the DrawList system. Still, we should render our batch are lower level to save a bit of CPU.
            ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
            ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, (plot_type == ImGuiPlotType_Lines) ? tp1 : ImVec2(tp1.x, histogram_zero_line_t));
            if (plot_type == ImGuiPlotType_Lines)
            {
                window->DrawList->AddLine(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);
            }
            else if (plot_type == ImGuiPlotType_Histogram)
            {
                if (pos1.x >= pos0.x + 2.0f)
                    pos1.x -= 1.0f;
                window->DrawList->AddRectFilled(pos0, pos1, idx_hovered == v1_idx ? col_hovered : col_base);
            }

            t0 = t1;
            tp0 = tp1;
        }
    }

    // Text overlay
    if (overlay_text)
        RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL, ImVec2(0.5f, 0.0f));

    if (label_size.x > 0.0f)
        RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);

    // Return hovered index or -1 if none are hovered.
    // This is currently not exposed in the public API because we need a larger redesign of the whole thing, but in the short-term we are making it available in PlotEx().
    return idx_hovered;
}

struct ImGuiPlotArrayGetterData
{
    const float* Values;
    int Stride;

    ImGuiPlotArrayGetterData(const float* values, int stride) { Values = values; Stride = stride; }
};

static float Plot_ArrayGetter(void* data, int idx)
{
    ImGuiPlotArrayGetterData* plot_data = (ImGuiPlotArrayGetterData*)data;
    const float v = *(const float*)(const void*)((const unsigned char*)plot_data->Values + (size_t)idx * plot_data->Stride);
    return v;
}

void ImGui::PlotLines(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride)
{
    ImGuiPlotArrayGetterData data(values, stride);
    PlotEx(ImGuiPlotType_Lines, label, &Plot_ArrayGetter, (void*)&data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
}

void ImGui::PlotLines(const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size)
{
    PlotEx(ImGuiPlotType_Lines, label, values_getter, data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
}

void ImGui::PlotHistogram(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride)
{
    ImGuiPlotArrayGetterData data(values, stride);
    PlotEx(ImGuiPlotType_Histogram, label, &Plot_ArrayGetter, (void*)&data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
}

void ImGui::PlotHistogram(const char* label, float (*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size)
{
    PlotEx(ImGuiPlotType_Histogram, label, values_getter, data, values_count, values_offset, overlay_text, scale_min, scale_max, graph_size);
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: Value helpers
// Those is not very useful, legacy API.
//-------------------------------------------------------------------------
// - Value()
//-------------------------------------------------------------------------

void ImGui::Value(const char* prefix, bool b)
{
    Text("%s: %s", prefix, (b ? "true" : "false"));
}

void ImGui::Value(const char* prefix, int v)
{
    Text("%s: %d", prefix, v);
}

void ImGui::Value(const char* prefix, unsigned int v)
{
    Text("%s: %d", prefix, v);
}

void ImGui::Value(const char* prefix, float v, const char* float_format)
{
    if (float_format)
    {
        char fmt[64];
        ImFormatString(fmt, IM_ARRAYSIZE(fmt), "%%s: %s", float_format);
        Text(fmt, prefix, v);
    }
    else
    {
        Text("%s: %.3f", prefix, v);
    }
}

//-------------------------------------------------------------------------
// [SECTION] MenuItem, BeginMenu, EndMenu, etc.
//-------------------------------------------------------------------------
// - ImGuiMenuColumns [Internal]
// - BeginMenuBar()
// - EndMenuBar()
// - BeginMainMenuBar()
// - EndMainMenuBar()
// - BeginMenu()
// - EndMenu()
// - MenuItem()
//-------------------------------------------------------------------------

// Helpers for internal use
void ImGuiMenuColumns::Update(int count, float spacing, bool clear)
{
    IM_ASSERT(count == IM_ARRAYSIZE(Pos));
    IM_UNUSED(count);
    Width = NextWidth = 0.0f;
    Spacing = spacing;
    if (clear)
        memset(NextWidths, 0, sizeof(NextWidths));
    for (int i = 0; i < IM_ARRAYSIZE(Pos); i++)
    {
        if (i > 0 && NextWidths[i] > 0.0f)
            Width += Spacing;
        Pos[i] = IM_FLOOR(Width);
        Width += NextWidths[i];
        NextWidths[i] = 0.0f;
    }
}

float ImGuiMenuColumns::DeclColumns(float w0, float w1, float w2) // not using va_arg because they promote float to double
{
    NextWidth = 0.0f;
    NextWidths[0] = ImMax(NextWidths[0], w0);
    NextWidths[1] = ImMax(NextWidths[1], w1);
    NextWidths[2] = ImMax(NextWidths[2], w2);
    for (int i = 0; i < IM_ARRAYSIZE(Pos); i++)
        NextWidth += NextWidths[i] + ((i > 0 && NextWidths[i] > 0.0f) ? Spacing : 0.0f);
    return ImMax(Width, NextWidth);
}

float ImGuiMenuColumns::CalcExtraSpace(float avail_w) const
{
    return ImMax(0.0f, avail_w - Width);
}

// FIXME: Provided a rectangle perhaps e.g. a BeginMenuBarEx() could be used anywhere..
// Currently the main responsibility of this function being to setup clip-rect + horizontal layout + menu navigation layer.
// Ideally we also want this to be responsible for claiming space out of the main window scrolling rectangle, in which case ImGuiWindowFlags_MenuBar will become unnecessary.
// Then later the same system could be used for multiple menu-bars, scrollbars, side-bars.
bool ImGui::BeginMenuBar()
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;
    if (!(window->Flags & ImGuiWindowFlags_MenuBar))
        return false;

    IM_ASSERT(!window->DC.MenuBarAppending);
    BeginGroup(); // Backup position on layer 0 // FIXME: Misleading to use a group for that backup/restore
    PushID("##menubar");

    // We don't clip with current window clipping rectangle as it is already set to the area below. However we clip with window full rect.
    // We remove 1 worth of rounding to Max.x to that text in long menus and small windows don't tend to display over the lower-right rounded area, which looks particularly glitchy.
    ImRect bar_rect = window->MenuBarRect();
    ImRect clip_rect(IM_ROUND(bar_rect.Min.x + window->WindowBorderSize), IM_ROUND(bar_rect.Min.y + window->WindowBorderSize), IM_ROUND(ImMax(bar_rect.Min.x, bar_rect.Max.x - ImMax(window->WindowRounding, window->WindowBorderSize))), IM_ROUND(bar_rect.Max.y));
    clip_rect.ClipWith(window->OuterRectClipped);
    PushClipRect(clip_rect.Min, clip_rect.Max, false);

    // We overwrite CursorMaxPos because BeginGroup sets it to CursorPos (essentially the .EmitItem hack in EndMenuBar() would need something analoguous here, maybe a BeginGroupEx() with flags).
    window->DC.CursorPos = window->DC.CursorMaxPos = ImVec2(bar_rect.Min.x + window->DC.MenuBarOffset.x, bar_rect.Min.y + window->DC.MenuBarOffset.y);
    window->DC.LayoutType = ImGuiLayoutType_Horizontal;
    window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;
    window->DC.MenuBarAppending = true;
    AlignTextToFramePadding();
    return true;
}

void ImGui::EndMenuBar()
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;
    ImGuiContext& g = *GImGui;

    // Nav: When a move request within one of our child menu failed, capture the request to navigate among our siblings.
    if (NavMoveRequestButNoResultYet() && (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right) && (g.NavWindow->Flags & ImGuiWindowFlags_ChildMenu))
    {
        ImGuiWindow* nav_earliest_child = g.NavWindow;
        while (nav_earliest_child->ParentWindow && (nav_earliest_child->ParentWindow->Flags & ImGuiWindowFlags_ChildMenu))
            nav_earliest_child = nav_earliest_child->ParentWindow;
        if (nav_earliest_child->ParentWindow == window && nav_earliest_child->DC.ParentLayoutType == ImGuiLayoutType_Horizontal && g.NavMoveRequestForward == ImGuiNavForward_None)
        {
            // To do so we claim focus back, restore NavId and then process the movement request for yet another frame.
            // This involve a one-frame delay which isn't very problematic in this situation. We could remove it by scoring in advance for multiple window (probably not worth the hassle/cost)
            const ImGuiNavLayer layer = ImGuiNavLayer_Menu;
            IM_ASSERT(window->DC.NavLayerActiveMaskNext & (1 << layer)); // Sanity check
            FocusWindow(window);
            SetNavIDWithRectRel(window->NavLastIds[layer], layer, 0, window->NavRectRel[layer]);
            g.NavLayer = layer;
            g.NavDisableHighlight = true; // Hide highlight for the current frame so we don't see the intermediary selection.
            g.NavMoveRequestForward = ImGuiNavForward_ForwardQueued;
            NavMoveRequestCancel();
        }
    }

    IM_ASSERT(window->Flags & ImGuiWindowFlags_MenuBar);
    IM_ASSERT(window->DC.MenuBarAppending);
    PopClipRect();
    PopID();
    window->DC.MenuBarOffset.x = window->DC.CursorPos.x - window->MenuBarRect().Min.x; // Save horizontal position so next append can reuse it. This is kinda equivalent to a per-layer CursorPos.
    g.GroupStack.back().EmitItem = false;
    EndGroup(); // Restore position on layer 0
    window->DC.LayoutType = ImGuiLayoutType_Vertical;
    window->DC.NavLayerCurrent = ImGuiNavLayer_Main;
    window->DC.MenuBarAppending = false;
}

bool ImGui::BeginMainMenuBar()
{
    ImGuiContext& g = *GImGui;
    ImGuiViewportP* viewport = g.Viewports[0];
    ImGuiWindow* menu_bar_window = FindWindowByName("##MainMenuBar");

    // For the main menu bar, which cannot be moved, we honor g.Style.DisplaySafeAreaPadding to ensure text can be visible on a TV set.
    g.NextWindowData.MenuBarOffsetMinVal = ImVec2(g.Style.DisplaySafeAreaPadding.x, ImMax(g.Style.DisplaySafeAreaPadding.y - g.Style.FramePadding.y, 0.0f));

    // Get our rectangle at the top of the work area
    if (menu_bar_window == NULL || menu_bar_window->BeginCount == 0)
    {
        // Set window position
        // We don't attempt to calculate our height ahead, as it depends on the per-viewport font size. However menu-bar will affect the minimum window size so we'll get the right height.
        ImVec2 menu_bar_pos = viewport->Pos + viewport->CurrWorkOffsetMin;
        ImVec2 menu_bar_size = ImVec2(viewport->Size.x - viewport->CurrWorkOffsetMin.x + viewport->CurrWorkOffsetMax.x, 1.0f);
        SetNextWindowPos(menu_bar_pos);
        SetNextWindowSize(menu_bar_size);
    }

    // Create window
    SetNextWindowViewport(viewport->ID); // Enforce viewport so we don't create our own viewport when ImGuiConfigFlags_ViewportsNoMerge is set.
    PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));    // Lift normal size constraint, however the presence of a menu-bar will give us the minimum height we want.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    bool is_open = Begin("##MainMenuBar", NULL, window_flags) && BeginMenuBar();
    PopStyleVar(2);

    // Report our size into work area (for next frame) using actual window size
    menu_bar_window = GetCurrentWindow();
    if (menu_bar_window->BeginCount == 1)
        viewport->CurrWorkOffsetMin.y += menu_bar_window->Size.y;

    g.NextWindowData.MenuBarOffsetMinVal = ImVec2(0.0f, 0.0f);
    if (!is_open)
    {
        End();
        return false;
    }
    return true; //-V1020
}

void ImGui::EndMainMenuBar()
{
    EndMenuBar();

    // When the user has left the menu layer (typically: closed menus through activation of an item), we restore focus to the previous window
    // FIXME: With this strategy we won't be able to restore a NULL focus.
    ImGuiContext& g = *GImGui;
    if (g.CurrentWindow == g.NavWindow && g.NavLayer == ImGuiNavLayer_Main && !g.NavAnyRequest)
        FocusTopMostWindowUnderOne(g.NavWindow, NULL);

    End();
}

bool ImGui::BeginMenu(const char* label, bool enabled)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    bool menu_is_open = IsPopupOpen(id, ImGuiPopupFlags_None);

    // Sub-menus are ChildWindow so that mouse can be hovering across them (otherwise top-most popup menu would steal focus and not allow hovering on parent menu)
    ImGuiWindowFlags flags = ImGuiWindowFlags_ChildMenu | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
    if (window->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_ChildMenu))
        flags |= ImGuiWindowFlags_ChildWindow;

    // If a menu with same the ID was already submitted, we will append to it, matching the behavior of Begin().
    // We are relying on a O(N) search - so O(N log N) over the frame - which seems like the most efficient for the expected small amount of BeginMenu() calls per frame.
    // If somehow this is ever becoming a problem we can switch to use e.g. ImGuiStorage mapping key to last frame used.
    if (g.MenusIdSubmittedThisFrame.contains(id))
    {
        if (menu_is_open)
            menu_is_open = BeginPopupEx(id, flags); // menu_is_open can be 'false' when the popup is completely clipped (e.g. zero size display)
        else
            g.NextWindowData.ClearFlags();          // we behave like Begin() and need to consume those values
        return menu_is_open;
    }

    // Tag menu as used. Next time BeginMenu() with same ID is called it will append to existing menu
    g.MenusIdSubmittedThisFrame.push_back(id);

    ImVec2 label_size = CalcTextSize(label, NULL, true);
    bool pressed;
    bool menuset_is_open = !(window->Flags & ImGuiWindowFlags_Popup) && (g.OpenPopupStack.Size > g.BeginPopupStack.Size && g.OpenPopupStack[g.BeginPopupStack.Size].OpenParentId == window->IDStack.back());
    ImGuiWindow* backed_nav_window = g.NavWindow;
    if (menuset_is_open)
        g.NavWindow = window;  // Odd hack to allow hovering across menus of a same menu-set (otherwise we wouldn't be able to hover parent)

    // The reference position stored in popup_pos will be used by Begin() to find a suitable position for the child menu,
    // However the final position is going to be different! It is chosen by FindBestWindowPosForPopup().
    // e.g. Menus tend to overlap each other horizontally to amplify relative Z-ordering.
    ImVec2 popup_pos, pos = window->DC.CursorPos;
    if (window->DC.LayoutType == ImGuiLayoutType_Horizontal)
    {
        // Menu inside an horizontal menu bar
        // Selectable extend their highlight by half ItemSpacing in each direction.
        // For ChildMenu, the popup position will be overwritten by the call to FindBestWindowPosForPopup() in Begin()
        popup_pos = ImVec2(pos.x - 1.0f - IM_FLOOR(style.ItemSpacing.x * 0.5f), pos.y - style.FramePadding.y + window->MenuBarHeight());
        window->DC.CursorPos.x += IM_FLOOR(style.ItemSpacing.x * 0.5f);
        PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x * 2.0f, style.ItemSpacing.y));
        float w = label_size.x;
        pressed = Selectable(label, menu_is_open, ImGuiSelectableFlags_NoHoldingActiveID | ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_DontClosePopups | (!enabled ? ImGuiSelectableFlags_Disabled : 0), ImVec2(w, 0.0f));
        PopStyleVar();
        window->DC.CursorPos.x += IM_FLOOR(style.ItemSpacing.x * (-1.0f + 0.5f)); // -1 spacing to compensate the spacing added when Selectable() did a SameLine(). It would also work to call SameLine() ourselves after the PopStyleVar().
    }
    else
    {
        // Menu inside a menu
        // (In a typical menu window where all items are BeginMenu() or MenuItem() calls, extra_w will always be 0.0f.
        //  Only when they are other items sticking out we're going to add spacing, yet only register minimum width into the layout system.
        popup_pos = ImVec2(pos.x, pos.y - style.WindowPadding.y);
        float min_w = window->DC.MenuColumns.DeclColumns(label_size.x, 0.0f, IM_FLOOR(g.FontSize * 1.20f)); // Feedback to next frame
        float extra_w = ImMax(0.0f, GetContentRegionAvail().x - min_w);
        pressed = Selectable(label, menu_is_open, ImGuiSelectableFlags_NoHoldingActiveID | ImGuiSelectableFlags_SelectOnClick | ImGuiSelectableFlags_DontClosePopups | ImGuiSelectableFlags_SpanAvailWidth | (!enabled ? ImGuiSelectableFlags_Disabled : 0), ImVec2(min_w, 0.0f));
        ImU32 text_col = GetColorU32(enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled);
        RenderArrow(window->DrawList, pos + ImVec2(window->DC.MenuColumns.Pos[2] + extra_w + g.FontSize * 0.30f, 0.0f), text_col, ImGuiDir_Right);
    }

    const bool hovered = enabled && ItemHoverable(window->DC.LastItemRect, id);
    if (menuset_is_open)
        g.NavWindow = backed_nav_window;

    bool want_open = false;
    bool want_close = false;
    if (window->DC.LayoutType == ImGuiLayoutType_Vertical) // (window->Flags & (ImGuiWindowFlags_Popup|ImGuiWindowFlags_ChildMenu))
    {
        // Close menu when not hovering it anymore unless we are moving roughly in the direction of the menu
        // Implement http://bjk5.com/post/44698559168/breaking-down-amazons-mega-dropdown to avoid using timers, so menus feels more reactive.
        bool moving_toward_other_child_menu = false;

        ImGuiWindow* child_menu_window = (g.BeginPopupStack.Size < g.OpenPopupStack.Size && g.OpenPopupStack[g.BeginPopupStack.Size].SourceWindow == window) ? g.OpenPopupStack[g.BeginPopupStack.Size].Window : NULL;
        if (g.HoveredWindow == window && child_menu_window != NULL && !(window->Flags & ImGuiWindowFlags_MenuBar))
        {
            // FIXME-DPI: Values should be derived from a master "scale" factor.
            ImRect next_window_rect = child_menu_window->Rect();
            ImVec2 ta = g.IO.MousePos - g.IO.MouseDelta;
            ImVec2 tb = (window->Pos.x < child_menu_window->Pos.x) ? next_window_rect.GetTL() : next_window_rect.GetTR();
            ImVec2 tc = (window->Pos.x < child_menu_window->Pos.x) ? next_window_rect.GetBL() : next_window_rect.GetBR();
            float extra = ImClamp(ImFabs(ta.x - tb.x) * 0.30f, 5.0f, 30.0f);    // add a bit of extra slack.
            ta.x += (window->Pos.x < child_menu_window->Pos.x) ? -0.5f : +0.5f; // to avoid numerical issues
            tb.y = ta.y + ImMax((tb.y - extra) - ta.y, -100.0f);                // triangle is maximum 200 high to limit the slope and the bias toward large sub-menus // FIXME: Multiply by fb_scale?
            tc.y = ta.y + ImMin((tc.y + extra) - ta.y, +100.0f);
            moving_toward_other_child_menu = ImTriangleContainsPoint(ta, tb, tc, g.IO.MousePos);
            //GetForegroundDrawList()->AddTriangleFilled(ta, tb, tc, moving_within_opened_triangle ? IM_COL32(0,128,0,128) : IM_COL32(128,0,0,128)); // [DEBUG]
        }
        if (menu_is_open && !hovered && g.HoveredWindow == window && g.HoveredIdPreviousFrame != 0 && g.HoveredIdPreviousFrame != id && !moving_toward_other_child_menu)
            want_close = true;

        if (!menu_is_open && hovered && pressed) // Click to open
            want_open = true;
        else if (!menu_is_open && hovered && !moving_toward_other_child_menu) // Hover to open
            want_open = true;

        if (g.NavActivateId == id)
        {
            want_close = menu_is_open;
            want_open = !menu_is_open;
        }
        if (g.NavId == id && g.NavMoveRequest && g.NavMoveDir == ImGuiDir_Right) // Nav-Right to open
        {
            want_open = true;
            NavMoveRequestCancel();
        }
    }
    else
    {
        // Menu bar
        if (menu_is_open && pressed && menuset_is_open) // Click an open menu again to close it
        {
            want_close = true;
            want_open = menu_is_open = false;
        }
        else if (pressed || (hovered && menuset_is_open && !menu_is_open)) // First click to open, then hover to open others
        {
            want_open = true;
        }
        else if (g.NavId == id && g.NavMoveRequest && g.NavMoveDir == ImGuiDir_Down) // Nav-Down to open
        {
            want_open = true;
            NavMoveRequestCancel();
        }
    }

    if (!enabled) // explicitly close if an open menu becomes disabled, facilitate users code a lot in pattern such as 'if (BeginMenu("options", has_object)) { ..use object.. }'
        want_close = true;
    if (want_close && IsPopupOpen(id, ImGuiPopupFlags_None))
        ClosePopupToLevel(g.BeginPopupStack.Size, true);

    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags | ImGuiItemStatusFlags_Openable | (menu_is_open ? ImGuiItemStatusFlags_Opened : 0));

    if (!menu_is_open && want_open && g.OpenPopupStack.Size > g.BeginPopupStack.Size)
    {
        // Don't recycle same menu level in the same frame, first close the other menu and yield for a frame.
        OpenPopup(label);
        return false;
    }

    menu_is_open |= want_open;
    if (want_open)
        OpenPopup(label);

    if (menu_is_open)
    {
        SetNextWindowPos(popup_pos, ImGuiCond_Always);
        menu_is_open = BeginPopupEx(id, flags); // menu_is_open can be 'false' when the popup is completely clipped (e.g. zero size display)
    }
    else
    {
        g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
    }

    return menu_is_open;
}

void ImGui::EndMenu()
{
    // Nav: When a left move request _within our child menu_ failed, close ourselves (the _parent_ menu).
    // A menu doesn't close itself because EndMenuBar() wants the catch the last Left<>Right inputs.
    // However, it means that with the current code, a BeginMenu() from outside another menu or a menu-bar won't be closable with the Left direction.
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (g.NavWindow && g.NavWindow->ParentWindow == window && g.NavMoveDir == ImGuiDir_Left && NavMoveRequestButNoResultYet() && window->DC.LayoutType == ImGuiLayoutType_Vertical)
    {
        ClosePopupToLevel(g.BeginPopupStack.Size, true);
        NavMoveRequestCancel();
    }

    EndPopup();
}

bool ImGui::MenuItem(const char* label, const char* shortcut, bool selected, bool enabled)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = g.Style;
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 label_size = CalcTextSize(label, NULL, true);

    // We've been using the equivalent of ImGuiSelectableFlags_SetNavIdOnHover on all Selectable() since early Nav system days (commit 43ee5d73),
    // but I am unsure whether this should be kept at all. For now moved it to be an opt-in feature used by menus only.
    ImGuiSelectableFlags flags = ImGuiSelectableFlags_SelectOnRelease | ImGuiSelectableFlags_SetNavIdOnHover | (enabled ? 0 : ImGuiSelectableFlags_Disabled);
    bool pressed;
    if (window->DC.LayoutType == ImGuiLayoutType_Horizontal)
    {
        // Mimic the exact layout spacing of BeginMenu() to allow MenuItem() inside a menu bar, which is a little misleading but may be useful
        // Note that in this situation we render neither the shortcut neither the selected tick mark
        float w = label_size.x;
        window->DC.CursorPos.x += IM_FLOOR(style.ItemSpacing.x * 0.5f);
        PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x * 2.0f, style.ItemSpacing.y));
        pressed = Selectable(label, false, flags, ImVec2(w, 0.0f));
        PopStyleVar();
        window->DC.CursorPos.x += IM_FLOOR(style.ItemSpacing.x * (-1.0f + 0.5f)); // -1 spacing to compensate the spacing added when Selectable() did a SameLine(). It would also work to call SameLine() ourselves after the PopStyleVar().
    }
    else
    {
        // Menu item inside a vertical menu
        // (In a typical menu window where all items are BeginMenu() or MenuItem() calls, extra_w will always be 0.0f.
        //  Only when they are other items sticking out we're going to add spacing, yet only register minimum width into the layout system.
        float shortcut_w = shortcut ? CalcTextSize(shortcut, NULL).x : 0.0f;
        float min_w = window->DC.MenuColumns.DeclColumns(label_size.x, shortcut_w, IM_FLOOR(g.FontSize * 1.20f)); // Feedback for next frame
        float extra_w = ImMax(0.0f, GetContentRegionAvail().x - min_w);
        pressed = Selectable(label, false, flags | ImGuiSelectableFlags_SpanAvailWidth, ImVec2(min_w, 0.0f));
        if (shortcut_w > 0.0f)
        {
            PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
            RenderText(pos + ImVec2(window->DC.MenuColumns.Pos[1] + extra_w, 0.0f), shortcut, NULL, false);
            PopStyleColor();
        }
        if (selected)
            RenderCheckMark(window->DrawList, pos + ImVec2(window->DC.MenuColumns.Pos[2] + extra_w + g.FontSize * 0.40f, g.FontSize * 0.134f * 0.5f), GetColorU32(enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled), g.FontSize  * 0.866f);
    }

    IMGUI_TEST_ENGINE_ITEM_INFO(window->DC.LastItemId, label, window->DC.ItemFlags | ImGuiItemStatusFlags_Checkable | (selected ? ImGuiItemStatusFlags_Checked : 0));
    return pressed;
}

bool ImGui::MenuItem(const char* label, const char* shortcut, bool* p_selected, bool enabled)
{
    if (MenuItem(label, shortcut, p_selected ? *p_selected : false, enabled))
    {
        if (p_selected)
            *p_selected = !*p_selected;
        return true;
    }
    return false;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: BeginTabBar, EndTabBar, etc.
//-------------------------------------------------------------------------
// - BeginTabBar()
// - BeginTabBarEx() [Internal]
// - EndTabBar()
// - TabBarLayout() [Internal]
// - TabBarCalcTabID() [Internal]
// - TabBarCalcMaxTabWidth() [Internal]
// - TabBarFindTabById() [Internal]
// - TabBarAddTab() [Internal]
// - TabBarRemoveTab() [Internal]
// - TabBarCloseTab() [Internal]
// - TabBarScrollClamp() [Internal]
// - TabBarScrollToTab() [Internal]
// - TabBarQueueChangeTabOrder() [Internal]
// - TabBarScrollingButtons() [Internal]
// - TabBarTabListPopupButton() [Internal]
//-------------------------------------------------------------------------

struct ImGuiTabBarSection
{
    int                 TabCount;               // Number of tabs in this section.
    float               Width;                  // Sum of width of tabs in this section (after shrinking down)
    float               Spacing;                // Horizontal spacing at the end of the section.

    ImGuiTabBarSection() { memset(this, 0, sizeof(*this)); }
};

namespace ImGui
{
    static void             TabBarLayout(ImGuiTabBar* tab_bar);
    static ImU32            TabBarCalcTabID(ImGuiTabBar* tab_bar, const char* label, ImGuiWindow* docked_window);
    static float            TabBarCalcMaxTabWidth();
    static float            TabBarScrollClamp(ImGuiTabBar* tab_bar, float scrolling);
    static void             TabBarScrollToTab(ImGuiTabBar* tab_bar, ImGuiTabItem* tab, ImGuiTabBarSection* sections);
    static ImGuiTabItem*    TabBarScrollingButtons(ImGuiTabBar* tab_bar);
    static ImGuiTabItem*    TabBarTabListPopupButton(ImGuiTabBar* tab_bar);
}

ImGuiTabBar::ImGuiTabBar()
{
    memset(this, 0, sizeof(*this));
    CurrFrameVisible = PrevFrameVisible = -1;
    LastTabItemIdx = -1;
}

static int IMGUI_CDECL TabItemComparerBySection(const void* lhs, const void* rhs)
{
    const ImGuiTabItem* a = (const ImGuiTabItem*)lhs;
    const ImGuiTabItem* b = (const ImGuiTabItem*)rhs;
    const int a_section = (a->Flags & ImGuiTabItemFlags_Leading) ? 0 : (a->Flags & ImGuiTabItemFlags_Trailing) ? 2 : 1;
    const int b_section = (b->Flags & ImGuiTabItemFlags_Leading) ? 0 : (b->Flags & ImGuiTabItemFlags_Trailing) ? 2 : 1;
    if (a_section != b_section)
        return a_section - b_section;
    return (int)(a->IndexDuringLayout - b->IndexDuringLayout);
}

static int IMGUI_CDECL TabItemComparerByBeginOrder(const void* lhs, const void* rhs)
{
    const ImGuiTabItem* a = (const ImGuiTabItem*)lhs;
    const ImGuiTabItem* b = (const ImGuiTabItem*)rhs;
    return (int)(a->BeginOrder - b->BeginOrder);
}

static ImGuiTabBar* GetTabBarFromTabBarRef(const ImGuiPtrOrIndex& ref)
{
    ImGuiContext& g = *GImGui;
    return ref.Ptr ? (ImGuiTabBar*)ref.Ptr : g.TabBars.GetByIndex(ref.Index);
}

static ImGuiPtrOrIndex GetTabBarRefFromTabBar(ImGuiTabBar* tab_bar)
{
    ImGuiContext& g = *GImGui;
    if (g.TabBars.Contains(tab_bar))
        return ImGuiPtrOrIndex(g.TabBars.GetIndex(tab_bar));
    return ImGuiPtrOrIndex(tab_bar);
}

bool    ImGui::BeginTabBar(const char* str_id, ImGuiTabBarFlags flags)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    ImGuiID id = window->GetID(str_id);
    ImGuiTabBar* tab_bar = g.TabBars.GetOrAddByKey(id);
    ImRect tab_bar_bb = ImRect(window->DC.CursorPos.x, window->DC.CursorPos.y, window->WorkRect.Max.x, window->DC.CursorPos.y + g.FontSize + g.Style.FramePadding.y * 2);
    tab_bar->ID = id;
    return BeginTabBarEx(tab_bar, tab_bar_bb, flags | ImGuiTabBarFlags_IsFocused, NULL);
}

bool    ImGui::BeginTabBarEx(ImGuiTabBar* tab_bar, const ImRect& tab_bar_bb, ImGuiTabBarFlags flags, ImGuiDockNode* dock_node)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    if ((flags & ImGuiTabBarFlags_DockNode) == 0)
        PushOverrideID(tab_bar->ID);

    // Add to stack
    g.CurrentTabBarStack.push_back(GetTabBarRefFromTabBar(tab_bar));
    g.CurrentTabBar = tab_bar;

    // Append with multiple BeginTabBar()/EndTabBar() pairs.
    tab_bar->BackupCursorPos = window->DC.CursorPos;
    if (tab_bar->CurrFrameVisible == g.FrameCount)
    {
        window->DC.CursorPos = ImVec2(tab_bar->BarRect.Min.x, tab_bar->BarRect.Max.y + tab_bar->ItemSpacingY);
        tab_bar->BeginCount++;
        return true;
    }

    // Ensure correct ordering when toggling ImGuiTabBarFlags_Reorderable flag, or when a new tab was added while being not reorderable
    if ((flags & ImGuiTabBarFlags_Reorderable) != (tab_bar->Flags & ImGuiTabBarFlags_Reorderable) || (tab_bar->TabsAddedNew && !(flags & ImGuiTabBarFlags_Reorderable)))
        if (tab_bar->Tabs.Size > 1 && (flags & ImGuiTabBarFlags_DockNode) == 0) // FIXME: TabBar with DockNode can now be hybrid
            ImQsort(tab_bar->Tabs.Data, tab_bar->Tabs.Size, sizeof(ImGuiTabItem), TabItemComparerByBeginOrder);
    tab_bar->TabsAddedNew = false;

    // Flags
    if ((flags & ImGuiTabBarFlags_FittingPolicyMask_) == 0)
        flags |= ImGuiTabBarFlags_FittingPolicyDefault_;

    tab_bar->Flags = flags;
    tab_bar->BarRect = tab_bar_bb;
    tab_bar->WantLayout = true; // Layout will be done on the first call to ItemTab()
    tab_bar->PrevFrameVisible = tab_bar->CurrFrameVisible;
    tab_bar->CurrFrameVisible = g.FrameCount;
    tab_bar->PrevTabsContentsHeight = tab_bar->CurrTabsContentsHeight;
    tab_bar->CurrTabsContentsHeight = 0.0f;
    tab_bar->ItemSpacingY = g.Style.ItemSpacing.y;
    tab_bar->FramePadding = g.Style.FramePadding;
    tab_bar->TabsActiveCount = 0;
    tab_bar->BeginCount = 1;

    // Set cursor pos in a way which only be used in the off-chance the user erroneously submits item before BeginTabItem(): items will overlap
    window->DC.CursorPos = ImVec2(tab_bar->BarRect.Min.x, tab_bar->BarRect.Max.y + tab_bar->ItemSpacingY);

    // Draw separator
    const ImU32 col = GetColorU32((flags & ImGuiTabBarFlags_IsFocused) ? ImGuiCol_TabActive : ImGuiCol_TabUnfocusedActive);
    const float y = tab_bar->BarRect.Max.y - 1.0f;
    if (dock_node != NULL)
    {
        const float separator_min_x = dock_node->Pos.x + window->WindowBorderSize;
        const float separator_max_x = dock_node->Pos.x + dock_node->Size.x - window->WindowBorderSize;
        window->DrawList->AddLine(ImVec2(separator_min_x, y), ImVec2(separator_max_x, y), col, 1.0f);
    }
    else
    {
        const float separator_min_x = tab_bar->BarRect.Min.x - IM_FLOOR(window->WindowPadding.x * 0.5f);
        const float separator_max_x = tab_bar->BarRect.Max.x + IM_FLOOR(window->WindowPadding.x * 0.5f);
        window->DrawList->AddLine(ImVec2(separator_min_x, y), ImVec2(separator_max_x, y), col, 1.0f);
    }
    return true;
}

void    ImGui::EndTabBar()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    ImGuiTabBar* tab_bar = g.CurrentTabBar;
    if (tab_bar == NULL)
    {
        IM_ASSERT_USER_ERROR(tab_bar != NULL, "Mismatched BeginTabBar()/EndTabBar()!");
        return;
    }

    // Fallback in case no TabItem have been submitted
    if (tab_bar->WantLayout)
        TabBarLayout(tab_bar);

    // Restore the last visible height if no tab is visible, this reduce vertical flicker/movement when a tabs gets removed without calling SetTabItemClosed().
    const bool tab_bar_appearing = (tab_bar->PrevFrameVisible + 1 < g.FrameCount);
    if (tab_bar->VisibleTabWasSubmitted || tab_bar->VisibleTabId == 0 || tab_bar_appearing)
    {
        tab_bar->CurrTabsContentsHeight = ImMax(window->DC.CursorPos.y - tab_bar->BarRect.Max.y, tab_bar->CurrTabsContentsHeight);
        window->DC.CursorPos.y = tab_bar->BarRect.Max.y + tab_bar->CurrTabsContentsHeight;
    }
    else
    {
        window->DC.CursorPos.y = tab_bar->BarRect.Max.y + tab_bar->PrevTabsContentsHeight;
    }
    if (tab_bar->BeginCount > 1)
        window->DC.CursorPos = tab_bar->BackupCursorPos;

    if ((tab_bar->Flags & ImGuiTabBarFlags_DockNode) == 0)
        PopID();

    g.CurrentTabBarStack.pop_back();
    g.CurrentTabBar = g.CurrentTabBarStack.empty() ? NULL : GetTabBarFromTabBarRef(g.CurrentTabBarStack.back());
}

// This is called only once a frame before by the first call to ItemTab()
// The reason we're not calling it in BeginTabBar() is to leave a chance to the user to call the SetTabItemClosed() functions.
static void ImGui::TabBarLayout(ImGuiTabBar* tab_bar)
{
    ImGuiContext& g = *GImGui;
    tab_bar->WantLayout = false;

    // Garbage collect by compacting list
    // Detect if we need to sort out tab list (e.g. in rare case where a tab changed section)
    int tab_dst_n = 0;
    bool need_sort_by_section = false;
    ImGuiTabBarSection sections[3]; // Layout sections: Leading, Central, Trailing
    for (int tab_src_n = 0; tab_src_n < tab_bar->Tabs.Size; tab_src_n++)
    {
        ImGuiTabItem* tab = &tab_bar->Tabs[tab_src_n];
        if (tab->LastFrameVisible < tab_bar->PrevFrameVisible || tab->WantClose)
        {
            // Remove tab
            if (tab_bar->VisibleTabId == tab->ID) { tab_bar->VisibleTabId = 0; }
            if (tab_bar->SelectedTabId == tab->ID) { tab_bar->SelectedTabId = 0; }
            if (tab_bar->NextSelectedTabId == tab->ID) { tab_bar->NextSelectedTabId = 0; }
            continue;
        }
        if (tab_dst_n != tab_src_n)
            tab_bar->Tabs[tab_dst_n] = tab_bar->Tabs[tab_src_n];

        tab = &tab_bar->Tabs[tab_dst_n];
        tab->IndexDuringLayout = (ImS16)tab_dst_n;

        // We will need sorting if tabs have changed section (e.g. moved from one of Leading/Central/Trailing to another)
        int curr_tab_section_n = (tab->Flags & ImGuiTabItemFlags_Leading) ? 0 : (tab->Flags & ImGuiTabItemFlags_Trailing) ? 2 : 1;
        if (tab_dst_n > 0)
        {
            ImGuiTabItem* prev_tab = &tab_bar->Tabs[tab_dst_n - 1];
            int prev_tab_section_n = (prev_tab->Flags & ImGuiTabItemFlags_Leading) ? 0 : (prev_tab->Flags & ImGuiTabItemFlags_Trailing) ? 2 : 1;
            if (curr_tab_section_n == 0 && prev_tab_section_n != 0)
                need_sort_by_section = true;
            if (prev_tab_section_n == 2 && curr_tab_section_n != 2)
                need_sort_by_section = true;
        }

        sections[curr_tab_section_n].TabCount++;
        tab_dst_n++;
    }
    if (tab_bar->Tabs.Size != tab_dst_n)
        tab_bar->Tabs.resize(tab_dst_n);

    if (need_sort_by_section)
        ImQsort(tab_bar->Tabs.Data, tab_bar->Tabs.Size, sizeof(ImGuiTabItem), TabItemComparerBySection);

    // Calculate spacing between sections
    sections[0].Spacing = sections[0].TabCount > 0 && (sections[1].TabCount + sections[2].TabCount) > 0 ? g.Style.ItemInnerSpacing.x : 0.0f;
    sections[1].Spacing = sections[1].TabCount > 0 && sections[2].TabCount > 0 ? g.Style.ItemInnerSpacing.x : 0.0f;

    // Setup next selected tab
    ImGuiID scroll_track_selected_tab_id = 0;
    if (tab_bar->NextSelectedTabId)
    {
        tab_bar->SelectedTabId = tab_bar->NextSelectedTabId;
        tab_bar->NextSelectedTabId = 0;
        scroll_track_selected_tab_id = tab_bar->SelectedTabId;
    }

    // Process order change request (we could probably process it when requested but it's just saner to do it in a single spot).
    if (tab_bar->ReorderRequestTabId != 0)
    {
        if (TabBarProcessReorder(tab_bar))
            if (tab_bar->ReorderRequestTabId == tab_bar->SelectedTabId)
                scroll_track_selected_tab_id = tab_bar->ReorderRequestTabId;
        tab_bar->ReorderRequestTabId = 0;
    }

    // Tab List Popup (will alter tab_bar->BarRect and therefore the available width!)
    const bool tab_list_popup_button = (tab_bar->Flags & ImGuiTabBarFlags_TabListPopupButton) != 0;
    if (tab_list_popup_button)
        if (ImGuiTabItem* tab_to_select = TabBarTabListPopupButton(tab_bar)) // NB: Will alter BarRect.Min.x!
            scroll_track_selected_tab_id = tab_bar->SelectedTabId = tab_to_select->ID;

    // Leading/Trailing tabs will be shrink only if central one aren't visible anymore, so layout the shrink data as: leading, trailing, central
    // (whereas our tabs are stored as: leading, central, trailing)
    int shrink_buffer_indexes[3] = { 0, sections[0].TabCount + sections[2].TabCount, sections[0].TabCount };
    g.ShrinkWidthBuffer.resize(tab_bar->Tabs.Size);

    // Compute ideal tabs widths + store them into shrink buffer
    ImGuiTabItem* most_recently_selected_tab = NULL;
    int curr_section_n = -1;
    bool found_selected_tab_id = false;
    for (int tab_n = 0; tab_n < tab_bar->Tabs.Size; tab_n++)
    {
        ImGuiTabItem* tab = &tab_bar->Tabs[tab_n];
        IM_ASSERT(tab->LastFrameVisible >= tab_bar->PrevFrameVisible);

        if ((most_recently_selected_tab == NULL || most_recently_selected_tab->LastFrameSelected < tab->LastFrameSelected) && !(tab->Flags & ImGuiTabItemFlags_Button))
            most_recently_selected_tab = tab;
        if (tab->ID == tab_bar->SelectedTabId)
            found_selected_tab_id = true;
        if (scroll_track_selected_tab_id == 0 && g.NavJustMovedToId == tab->ID)
            scroll_track_selected_tab_id = tab->ID;

        // Refresh tab width immediately, otherwise changes of style e.g. style.FramePadding.x would noticeably lag in the tab bar.
        // Additionally, when using TabBarAddTab() to manipulate tab bar order we occasionally insert new tabs that don't have a width yet,
        // and we cannot wait for the next BeginTabItem() call. We cannot compute this width within TabBarAddTab() because font size depends on the active window.
        const char* tab_name = tab_bar->GetTabName(tab);
        const bool has_close_button = (tab->Flags & ImGuiTabItemFlags_NoCloseButton) == 0;
        tab->ContentWidth = TabItemCalcSize(tab_name, has_close_button).x;

        int section_n = (tab->Flags & ImGuiTabItemFlags_Leading) ? 0 : (tab->Flags & ImGuiTabItemFlags_Trailing) ? 2 : 1;
        ImGuiTabBarSection* section = &sections[section_n];
        section->Width += tab->ContentWidth + (section_n == curr_section_n ? g.Style.ItemInnerSpacing.x : 0.0f);
        curr_section_n = section_n;

        // Store data so we can build an array sorted by width if we need to shrink tabs down
        int shrink_buffer_index = shrink_buffer_indexes[section_n]++;
        g.ShrinkWidthBuffer[shrink_buffer_index].Index = tab_n;
        g.ShrinkWidthBuffer[shrink_buffer_index].Width = tab->ContentWidth;

        IM_ASSERT(tab->ContentWidth > 0.0f);
        tab->Width = tab->ContentWidth;
    }

    // Compute total ideal width (used for e.g. auto-resizing a window)
    tab_bar->WidthAllTabsIdeal = 0.0f;
    for (int section_n = 0; section_n < 3; section_n++)
        tab_bar->WidthAllTabsIdeal += sections[section_n].Width + sections[section_n].Spacing;

    // Horizontal scrolling buttons
    // (note that TabBarScrollButtons() will alter BarRect.Max.x)
    if ((tab_bar->WidthAllTabsIdeal > tab_bar->BarRect.GetWidth() && tab_bar->Tabs.Size > 1) && !(tab_bar->Flags & ImGuiTabBarFlags_NoTabListScrollingButtons) && (tab_bar->Flags & ImGuiTabBarFlags_FittingPolicyScroll))
        if (ImGuiTabItem* scroll_track_selected_tab = TabBarScrollingButtons(tab_bar))
        {
            scroll_track_selected_tab_id = scroll_track_selected_tab->ID;
            if (!(scroll_track_selected_tab->Flags & ImGuiTabItemFlags_Button))
                tab_bar->SelectedTabId = scroll_track_selected_tab_id;
        }

    // Shrink widths if full tabs don't fit in their allocated space
    float section_0_w = sections[0].Width + sections[0].Spacing;
    float section_1_w = sections[1].Width + sections[1].Spacing;
    float section_2_w = sections[2].Width + sections[2].Spacing;
    bool central_section_is_visible = (section_0_w + section_2_w) < tab_bar->BarRect.GetWidth();
    float width_excess;
    if (central_section_is_visible)
        width_excess = ImMax(section_1_w - (tab_bar->BarRect.GetWidth() - section_0_w - section_2_w), 0.0f); // Excess used to shrink central section
    else
        width_excess = (section_0_w + section_2_w) - tab_bar->BarRect.GetWidth(); // Excess used to shrink leading/trailing section

    // With ImGuiTabBarFlags_FittingPolicyScroll policy, we will only shrink leading/trailing if the central section is not visible anymore
    if (width_excess > 0.0f && ((tab_bar->Flags & ImGuiTabBarFlags_FittingPolicyResizeDown) || !central_section_is_visible))
    {
        int shrink_data_count = (central_section_is_visible ? sections[1].TabCount : sections[0].TabCount + sections[2].TabCount);
        int shrink_data_offset = (central_section_is_visible ? sections[0].TabCount + sections[2].TabCount : 0);
        ShrinkWidths(g.ShrinkWidthBuffer.Data + shrink_data_offset, shrink_data_count, width_excess);

        // Apply shrunk values into tabs and sections
        for (int tab_n = shrink_data_offset; tab_n < shrink_data_offset + shrink_data_count; tab_n++)
        {
            ImGuiTabItem* tab = &tab_bar->Tabs[g.ShrinkWidthBuffer[tab_n].Index];
            float shrinked_width = IM_FLOOR(g.ShrinkWidthBuffer[tab_n].Width);
            if (shrinked_width < 0.0f)
                continue;

            int section_n = (tab->Flags & ImGuiTabItemFlags_Leading) ? 0 : (tab->Flags & ImGuiTabItemFlags_Trailing) ? 2 : 1;
            sections[section_n].Width -= (tab->Width - shrinked_width);
            tab->Width = shrinked_width;
        }
    }

    // Layout all active tabs
    int section_tab_index = 0;
    float tab_offset = 0.0f;
    tab_bar->WidthAllTabs = 0.0f;
    for (int section_n = 0; section_n < 3; section_n++)
    {
        ImGuiTabBarSection* section = &sections[section_n];
        if (section_n == 2)
            tab_offset = ImMin(ImMax(0.0f, tab_bar->BarRect.GetWidth() - section->Width), tab_offset);

        for (int tab_n = 0; tab_n < section->TabCount; tab_n++)
        {
            ImGuiTabItem* tab = &tab_bar->Tabs[section_tab_index + tab_n];
            tab->Offset = tab_offset;
            tab->NameOffset = -1;
            tab_offset += tab->Width + (tab_n < section->TabCount - 1 ? g.Style.ItemInnerSpacing.x : 0.0f);
        }
        tab_bar->WidthAllTabs += ImMax(section->Width + section->Spacing, 0.0f);
        tab_offset += section->Spacing;
        section_tab_index += section->TabCount;
    }

    // Clear name buffers
    tab_bar->TabsNames.Buf.resize(0);

    // If we have lost the selected tab, select the next most recently active one
    if (found_selected_tab_id == false)
        tab_bar->SelectedTabId = 0;
    if (tab_bar->SelectedTabId == 0 && tab_bar->NextSelectedTabId == 0 && most_recently_selected_tab != NULL)
        scroll_track_selected_tab_id = tab_bar->SelectedTabId = most_recently_selected_tab->ID;

    // Lock in visible tab
    tab_bar->VisibleTabId = tab_bar->SelectedTabId;
    tab_bar->VisibleTabWasSubmitted = false;

    // CTRL+TAB can override visible tab temporarily
    if (g.NavWindowingTarget != NULL && g.NavWindowingTarget->DockNode && g.NavWindowingTarget->DockNode->TabBar == tab_bar)
        tab_bar->VisibleTabId = scroll_track_selected_tab_id = g.NavWindowingTarget->ID;

    // Update scrolling
    if (scroll_track_selected_tab_id)
        if (ImGuiTabItem* scroll_track_selected_tab = TabBarFindTabByID(tab_bar, scroll_track_selected_tab_id))
            TabBarScrollToTab(tab_bar, scroll_track_selected_tab, sections);
    tab_bar->ScrollingAnim = TabBarScrollClamp(tab_bar, tab_bar->ScrollingAnim);
    tab_bar->ScrollingTarget = TabBarScrollClamp(tab_bar, tab_bar->ScrollingTarget);
    if (tab_bar->ScrollingAnim != tab_bar->ScrollingTarget)
    {
        // Scrolling speed adjust itself so we can always reach our target in 1/3 seconds.
        // Teleport if we are aiming far off the visible line
        tab_bar->ScrollingSpeed = ImMax(tab_bar->ScrollingSpeed, 70.0f * g.FontSize);
        tab_bar->ScrollingSpeed = ImMax(tab_bar->ScrollingSpeed, ImFabs(tab_bar->ScrollingTarget - tab_bar->ScrollingAnim) / 0.3f);
        const bool teleport = (tab_bar->PrevFrameVisible + 1 < g.FrameCount) || (tab_bar->ScrollingTargetDistToVisibility > 10.0f * g.FontSize);
        tab_bar->ScrollingAnim = teleport ? tab_bar->ScrollingTarget : ImLinearSweep(tab_bar->ScrollingAnim, tab_bar->ScrollingTarget, g.IO.DeltaTime * tab_bar->ScrollingSpeed);
    }
    else
    {
        tab_bar->ScrollingSpeed = 0.0f;
    }
    tab_bar->ScrollingRectMinX = tab_bar->BarRect.Min.x + sections[0].Width + sections[0].Spacing;
    tab_bar->ScrollingRectMaxX = tab_bar->BarRect.Max.x - sections[2].Width - sections[1].Spacing;

    // Actual layout in host window (we don't do it in BeginTabBar() so as not to waste an extra frame)
    ImGuiWindow* window = g.CurrentWindow;
    window->DC.CursorPos = tab_bar->BarRect.Min;
    ItemSize(ImVec2(tab_bar->WidthAllTabsIdeal, tab_bar->BarRect.GetHeight()), tab_bar->FramePadding.y);
}

// Dockable uses Name/ID in the global namespace. Non-dockable items use the ID stack.
static ImU32   ImGui::TabBarCalcTabID(ImGuiTabBar* tab_bar, const char* label, ImGuiWindow* docked_window)
{
    if (docked_window != NULL)
    {
        IM_ASSERT(tab_bar->Flags & ImGuiTabBarFlags_DockNode);
        ImGuiID id = ImHashStr(label);
        KeepAliveID(id);
        return id;
    }
    else
    {
        ImGuiWindow* window = GImGui->CurrentWindow;
        return window->GetID(label);
    }
}

static float ImGui::TabBarCalcMaxTabWidth()
{
    ImGuiContext& g = *GImGui;
    return g.FontSize * 20.0f;
}

ImGuiTabItem* ImGui::TabBarFindTabByID(ImGuiTabBar* tab_bar, ImGuiID tab_id)
{
    if (tab_id != 0)
        for (int n = 0; n < tab_bar->Tabs.Size; n++)
            if (tab_bar->Tabs[n].ID == tab_id)
                return &tab_bar->Tabs[n];
    return NULL;
}

// FIXME: See references to #2304 in TODO.txt
ImGuiTabItem* ImGui::TabBarFindMostRecentlySelectedTabForActiveWindow(ImGuiTabBar* tab_bar)
{
    ImGuiTabItem* most_recently_selected_tab = NULL;
    for (int tab_n = 0; tab_n < tab_bar->Tabs.Size; tab_n++)
    {
        ImGuiTabItem* tab = &tab_bar->Tabs[tab_n];
        if (most_recently_selected_tab == NULL || most_recently_selected_tab->LastFrameSelected < tab->LastFrameSelected)
            if (tab->Window && tab->Window->WasActive)
                most_recently_selected_tab = tab;
    }
    return most_recently_selected_tab;
}

// The purpose of this call is to register tab in advance so we can control their order at the time they appear.
// Otherwise calling this is unnecessary as tabs are appending as needed by the BeginTabItem() function.
void ImGui::TabBarAddTab(ImGuiTabBar* tab_bar, ImGuiTabItemFlags tab_flags, ImGuiWindow* window)
{
    ImGuiContext& g = *GImGui;
    IM_ASSERT(TabBarFindTabByID(tab_bar, window->ID) == NULL);
    IM_ASSERT(g.CurrentTabBar != tab_bar);  // Can't work while the tab bar is active as our tab doesn't have an X offset yet, in theory we could/should test something like (tab_bar->CurrFrameVisible < g.FrameCount) but we'd need to solve why triggers the commented early-out assert in BeginTabBarEx() (probably dock node going from implicit to explicit in same frame)

    ImGuiTabItem new_tab;
    new_tab.ID = window->ID;
    new_tab.Flags = tab_flags;
    new_tab.LastFrameVisible = tab_bar->CurrFrameVisible;   // Required so BeginTabBar() doesn't ditch the tab
    if (new_tab.LastFrameVisible == -1)
        new_tab.LastFrameVisible = g.FrameCount - 1;
    new_tab.Window = window;                                // Required so tab bar layout can compute the tab width before tab submission
    tab_bar->Tabs.push_back(new_tab);
}

// The *TabId fields be already set by the docking system _before_ the actual TabItem was created, so we clear them regardless.
void ImGui::TabBarRemoveTab(ImGuiTabBar* tab_bar, ImGuiID tab_id)
{
    if (ImGuiTabItem* tab = TabBarFindTabByID(tab_bar, tab_id))
        tab_bar->Tabs.erase(tab);
    if (tab_bar->VisibleTabId == tab_id)      { tab_bar->VisibleTabId = 0; }
    if (tab_bar->SelectedTabId == tab_id)     { tab_bar->SelectedTabId = 0; }
    if (tab_bar->NextSelectedTabId == tab_id) { tab_bar->NextSelectedTabId = 0; }
}

// Called on manual closure attempt
void ImGui::TabBarCloseTab(ImGuiTabBar* tab_bar, ImGuiTabItem* tab)
{
    IM_ASSERT(!(tab->Flags & ImGuiTabItemFlags_Button));
    if (!(tab->Flags & ImGuiTabItemFlags_UnsavedDocument))
    {
        // This will remove a frame of lag for selecting another tab on closure.
        // However we don't run it in the case where the 'Unsaved' flag is set, so user gets a chance to fully undo the closure
        tab->WantClose = true;
        if (tab_bar->VisibleTabId == tab->ID)
        {
            tab->LastFrameVisible = -1;
            tab_bar->SelectedTabId = tab_bar->NextSelectedTabId = 0;
        }
    }
    else
    {
        // Actually select before expecting closure attempt (on an UnsavedDocument tab user is expect to e.g. show a popup)
        if (tab_bar->VisibleTabId != tab->ID)
            tab_bar->NextSelectedTabId = tab->ID;
    }
}

static float ImGui::TabBarScrollClamp(ImGuiTabBar* tab_bar, float scrolling)
{
    scrolling = ImMin(scrolling, tab_bar->WidthAllTabs - tab_bar->BarRect.GetWidth());
    return ImMax(scrolling, 0.0f);
}

static void ImGui::TabBarScrollToTab(ImGuiTabBar* tab_bar, ImGuiTabItem* tab, ImGuiTabBarSection* sections)
{
    if (tab->Flags & (ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_Trailing))
        return;

    ImGuiContext& g = *GImGui;
    float margin = g.FontSize * 1.0f; // When to scroll to make Tab N+1 visible always make a bit of N visible to suggest more scrolling area (since we don't have a scrollbar)
    int order = tab_bar->GetTabOrder(tab);

    // Scrolling happens only in the central section (leading/trailing sections are not scrolling)
    // FIXME: This is all confusing.
    float scrollable_width = tab_bar->BarRect.GetWidth() - sections[0].Width - sections[2].Width - sections[1].Spacing;

    // We make all tabs positions all relative Sections[0].Width to make code simpler
    float tab_x1 = tab->Offset - sections[0].Width + (order > sections[0].TabCount - 1 ? -margin : 0.0f);
    float tab_x2 = tab->Offset - sections[0].Width + tab->Width + (order + 1 < tab_bar->Tabs.Size - sections[2].TabCount ? margin : 1.0f);
    tab_bar->ScrollingTargetDistToVisibility = 0.0f;
    if (tab_bar->ScrollingTarget > tab_x1 || (tab_x2 - tab_x1 >= scrollable_width))
    {
        // Scroll to the left
        tab_bar->ScrollingTargetDistToVisibility = ImMax(tab_bar->ScrollingAnim - tab_x2, 0.0f);
        tab_bar->ScrollingTarget = tab_x1;
    }
    else if (tab_bar->ScrollingTarget < tab_x2 - scrollable_width)
    {
        // Scroll to the right
        tab_bar->ScrollingTargetDistToVisibility = ImMax((tab_x1 - scrollable_width) - tab_bar->ScrollingAnim, 0.0f);
        tab_bar->ScrollingTarget = tab_x2 - scrollable_width;
    }
}

void ImGui::TabBarQueueReorder(ImGuiTabBar* tab_bar, const ImGuiTabItem* tab, int dir)
{
    IM_ASSERT(dir == -1 || dir == +1);
    IM_ASSERT(tab_bar->ReorderRequestTabId == 0);
    tab_bar->ReorderRequestTabId = tab->ID;
    tab_bar->ReorderRequestDir = (ImS8)dir;
}

bool ImGui::TabBarProcessReorder(ImGuiTabBar* tab_bar)
{
    ImGuiTabItem* tab1 = TabBarFindTabByID(tab_bar, tab_bar->ReorderRequestTabId);
    if (tab1 == NULL || (tab1->Flags & ImGuiTabItemFlags_NoReorder))
        return false;

    //IM_ASSERT(tab_bar->Flags & ImGuiTabBarFlags_Reorderable); // <- this may happen when using debug tools
    int tab2_order = tab_bar->GetTabOrder(tab1) + tab_bar->ReorderRequestDir;
    if (tab2_order < 0 || tab2_order >= tab_bar->Tabs.Size)
        return false;

    // Reordered TabItem must share the same position flags than target
    ImGuiTabItem* tab2 = &tab_bar->Tabs[tab2_order];
    if (tab2->Flags & ImGuiTabItemFlags_NoReorder)
        return false;
    if ((tab1->Flags & (ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_Trailing)) != (tab2->Flags & (ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_Trailing)))
        return false;

    ImGuiTabItem item_tmp = *tab1;
    *tab1 = *tab2;
    *tab2 = item_tmp;

    if (tab_bar->Flags & ImGuiTabBarFlags_SaveSettings)
        MarkIniSettingsDirty();
    return true;
}

static ImGuiTabItem* ImGui::TabBarScrollingButtons(ImGuiTabBar* tab_bar)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    const ImVec2 arrow_button_size(g.FontSize - 2.0f, g.FontSize + g.Style.FramePadding.y * 2.0f);
    const float scrolling_buttons_width = arrow_button_size.x * 2.0f;

    const ImVec2 backup_cursor_pos = window->DC.CursorPos;
    //window->DrawList->AddRect(ImVec2(tab_bar->BarRect.Max.x - scrolling_buttons_width, tab_bar->BarRect.Min.y), ImVec2(tab_bar->BarRect.Max.x, tab_bar->BarRect.Max.y), IM_COL32(255,0,0,255));

    int select_dir = 0;
    ImVec4 arrow_col = g.Style.Colors[ImGuiCol_Text];
    arrow_col.w *= 0.5f;

    PushStyleColor(ImGuiCol_Text, arrow_col);
    PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    const float backup_repeat_delay = g.IO.KeyRepeatDelay;
    const float backup_repeat_rate = g.IO.KeyRepeatRate;
    g.IO.KeyRepeatDelay = 0.250f;
    g.IO.KeyRepeatRate = 0.200f;
    float x = ImMax(tab_bar->BarRect.Min.x, tab_bar->BarRect.Max.x - scrolling_buttons_width);
    window->DC.CursorPos = ImVec2(x, tab_bar->BarRect.Min.y);
    if (ArrowButtonEx("##<", ImGuiDir_Left, arrow_button_size, ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_Repeat))
        select_dir = -1;
    window->DC.CursorPos = ImVec2(x + arrow_button_size.x, tab_bar->BarRect.Min.y);
    if (ArrowButtonEx("##>", ImGuiDir_Right, arrow_button_size, ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_Repeat))
        select_dir = +1;
    PopStyleColor(2);
    g.IO.KeyRepeatRate = backup_repeat_rate;
    g.IO.KeyRepeatDelay = backup_repeat_delay;

    ImGuiTabItem* tab_to_scroll_to = NULL;
    if (select_dir != 0)
        if (ImGuiTabItem* tab_item = TabBarFindTabByID(tab_bar, tab_bar->SelectedTabId))
        {
            int selected_order = tab_bar->GetTabOrder(tab_item);
            int target_order = selected_order + select_dir;

            // Skip tab item buttons until another tab item is found or end is reached
            while (tab_to_scroll_to == NULL)
            {
                // If we are at the end of the list, still scroll to make our tab visible
                tab_to_scroll_to = &tab_bar->Tabs[(target_order >= 0 && target_order < tab_bar->Tabs.Size) ? target_order : selected_order];

                // Cross through buttons
                // (even if first/last item is a button, return it so we can update the scroll)
                if (tab_to_scroll_to->Flags & ImGuiTabItemFlags_Button)
                {
                    target_order += select_dir;
                    selected_order += select_dir;
                    tab_to_scroll_to = (target_order < 0 || target_order >= tab_bar->Tabs.Size) ? tab_to_scroll_to : NULL;
                }
            }
        }
    window->DC.CursorPos = backup_cursor_pos;
    tab_bar->BarRect.Max.x -= scrolling_buttons_width + 1.0f;

    return tab_to_scroll_to;
}

static ImGuiTabItem* ImGui::TabBarTabListPopupButton(ImGuiTabBar* tab_bar)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    // We use g.Style.FramePadding.y to match the square ArrowButton size
    const float tab_list_popup_button_width = g.FontSize + g.Style.FramePadding.y;
    const ImVec2 backup_cursor_pos = window->DC.CursorPos;
    window->DC.CursorPos = ImVec2(tab_bar->BarRect.Min.x - g.Style.FramePadding.y, tab_bar->BarRect.Min.y);
    tab_bar->BarRect.Min.x += tab_list_popup_button_width;

    ImVec4 arrow_col = g.Style.Colors[ImGuiCol_Text];
    arrow_col.w *= 0.5f;
    PushStyleColor(ImGuiCol_Text, arrow_col);
    PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    bool open = BeginCombo("##v", NULL, ImGuiComboFlags_NoPreview | ImGuiComboFlags_HeightLargest);
    PopStyleColor(2);

    ImGuiTabItem* tab_to_select = NULL;
    if (open)
    {
        for (int tab_n = 0; tab_n < tab_bar->Tabs.Size; tab_n++)
        {
            ImGuiTabItem* tab = &tab_bar->Tabs[tab_n];
            if (tab->Flags & ImGuiTabItemFlags_Button)
                continue;

            const char* tab_name = tab_bar->GetTabName(tab);
            if (Selectable(tab_name, tab_bar->SelectedTabId == tab->ID))
                tab_to_select = tab;
        }
        EndCombo();
    }

    window->DC.CursorPos = backup_cursor_pos;
    return tab_to_select;
}

//-------------------------------------------------------------------------
// [SECTION] Widgets: BeginTabItem, EndTabItem, etc.
//-------------------------------------------------------------------------
// - BeginTabItem()
// - EndTabItem()
// - TabItemButton()
// - TabItemEx() [Internal]
// - SetTabItemClosed()
// - TabItemCalcSize() [Internal]
// - TabItemBackground() [Internal]
// - TabItemLabelAndCloseButton() [Internal]
//-------------------------------------------------------------------------

bool    ImGui::BeginTabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    ImGuiTabBar* tab_bar = g.CurrentTabBar;
    if (tab_bar == NULL)
    {
        IM_ASSERT_USER_ERROR(tab_bar, "Needs to be called between BeginTabBar() and EndTabBar()!");
        return false;
    }
    IM_ASSERT((flags & ImGuiTabItemFlags_Button) == 0);             // BeginTabItem() Can't be used with button flags, use TabItemButton() instead!

    bool ret = TabItemEx(tab_bar, label, p_open, flags, NULL);
    if (ret && !(flags & ImGuiTabItemFlags_NoPushId))
    {
        ImGuiTabItem* tab = &tab_bar->Tabs[tab_bar->LastTabItemIdx];
        PushOverrideID(tab->ID); // We already hashed 'label' so push into the ID stack directly instead of doing another hash through PushID(label)
    }
    return ret;
}

void    ImGui::EndTabItem()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    ImGuiTabBar* tab_bar = g.CurrentTabBar;
    if (tab_bar == NULL)
    {
        IM_ASSERT_USER_ERROR(tab_bar != NULL, "Needs to be called between BeginTabBar() and EndTabBar()!");
        return;
    }
    IM_ASSERT(tab_bar->LastTabItemIdx >= 0);
    ImGuiTabItem* tab = &tab_bar->Tabs[tab_bar->LastTabItemIdx];
    if (!(tab->Flags & ImGuiTabItemFlags_NoPushId))
        PopID();
}

bool    ImGui::TabItemButton(const char* label, ImGuiTabItemFlags flags)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    ImGuiTabBar* tab_bar = g.CurrentTabBar;
    if (tab_bar == NULL)
    {
        IM_ASSERT_USER_ERROR(tab_bar != NULL, "Needs to be called between BeginTabBar() and EndTabBar()!");
        return false;
    }
    return TabItemEx(tab_bar, label, NULL, flags | ImGuiTabItemFlags_Button | ImGuiTabItemFlags_NoReorder, NULL);
}

bool    ImGui::TabItemEx(ImGuiTabBar* tab_bar, const char* label, bool* p_open, ImGuiTabItemFlags flags, ImGuiWindow* docked_window)
{
    // Layout whole tab bar if not already done
    if (tab_bar->WantLayout)
        TabBarLayout(tab_bar);

    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return false;

    const ImGuiStyle& style = g.Style;
    const ImGuiID id = TabBarCalcTabID(tab_bar, label, docked_window);

    // If the user called us with *p_open == false, we early out and don't render.
    // We make a call to ItemAdd() so that attempts to use a contextual popup menu with an implicit ID won't use an older ID.
    IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.LastItemStatusFlags);
    if (p_open && !*p_open)
    {
        PushItemFlag(ImGuiItemFlags_NoNav | ImGuiItemFlags_NoNavDefaultFocus, true);
        ItemAdd(ImRect(), id);
        PopItemFlag();
        return false;
    }

    IM_ASSERT(!p_open || !(flags & ImGuiTabItemFlags_Button));
    IM_ASSERT((flags & (ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_Trailing)) != (ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_Trailing)); // Can't use both Leading and Trailing

    // Store into ImGuiTabItemFlags_NoCloseButton, also honor ImGuiTabItemFlags_NoCloseButton passed by user (although not documented)
    if (flags & ImGuiTabItemFlags_NoCloseButton)
        p_open = NULL;
    else if (p_open == NULL)
        flags |= ImGuiTabItemFlags_NoCloseButton;

    // Calculate tab contents size
    ImVec2 size = TabItemCalcSize(label, p_open != NULL);

    // Acquire tab data
    ImGuiTabItem* tab = TabBarFindTabByID(tab_bar, id);
    bool tab_is_new = false;
    if (tab == NULL)
    {
        tab_bar->Tabs.push_back(ImGuiTabItem());
        tab = &tab_bar->Tabs.back();
        tab->ID = id;
        tab->Width = size.x;
        tab_bar->TabsAddedNew = true;
        tab_is_new = true;
    }
    tab_bar->LastTabItemIdx = (ImS16)tab_bar->Tabs.index_from_ptr(tab);
    tab->ContentWidth = size.x;
    tab->BeginOrder = tab_bar->TabsActiveCount++;

    const bool tab_bar_appearing = (tab_bar->PrevFrameVisible + 1 < g.FrameCount);
    const bool tab_bar_focused = (tab_bar->Flags & ImGuiTabBarFlags_IsFocused) != 0;
    const bool tab_appearing = (tab->LastFrameVisible + 1 < g.FrameCount);
    const bool is_tab_button = (flags & ImGuiTabItemFlags_Button) != 0;
    tab->LastFrameVisible = g.FrameCount;
    tab->Flags = flags;
    tab->Window = docked_window;

    // Append name with zero-terminator
    // (regular tabs are permitted in a DockNode tab bar, but window tabs not permitted in a non-DockNode tab bar)
    if (tab->Window != NULL)
    {
        IM_ASSERT(tab_bar->Flags & ImGuiTabBarFlags_DockNode);
        tab->NameOffset = -1;
    }
    else
    {
        IM_ASSERT(tab->Window == NULL);
        tab->NameOffset = (ImS16)tab_bar->TabsNames.size();
        tab_bar->TabsNames.append(label, label + strlen(label) + 1); // Append name _with_ the zero-terminator.
    }

    // Update selected tab
    if (tab_appearing && (tab_bar->Flags & ImGuiTabBarFlags_AutoSelectNewTabs) && tab_bar->NextSelectedTabId == 0)
        if (!tab_bar_appearing || tab_bar->SelectedTabId == 0)
            if (!is_tab_button)
                tab_bar->NextSelectedTabId = id;  // New tabs gets activated
    if ((flags & ImGuiTabItemFlags_SetSelected) && (tab_bar->SelectedTabId != id)) // SetSelected can only be passed on explicit tab bar
        if (!is_tab_button)
            tab_bar->NextSelectedTabId = id;

    // Lock visibility
    // (Note: tab_contents_visible != tab_selected... because CTRL+TAB operations may preview some tabs without selecting them!)
    bool tab_contents_visible = (tab_bar->VisibleTabId == id);
    if (tab_contents_visible)
        tab_bar->VisibleTabWasSubmitted = true;

    // On the very first frame of a tab bar we let first tab contents be visible to minimize appearing glitches
    if (!tab_contents_visible && tab_bar->SelectedTabId == 0 && tab_bar_appearing && docked_window == NULL)
        if (tab_bar->Tabs.Size == 1 && !(tab_bar->Flags & ImGuiTabBarFlags_AutoSelectNewTabs))
            tab_contents_visible = true;

    // Note that tab_is_new is not necessarily the same as tab_appearing! When a tab bar stops being submitted
    // and then gets submitted again, the tabs will have 'tab_appearing=true' but 'tab_is_new=false'.
    if (tab_appearing && (!tab_bar_appearing || tab_is_new))
    {
        PushItemFlag(ImGuiItemFlags_NoNav | ImGuiItemFlags_NoNavDefaultFocus, true);
        ItemAdd(ImRect(), id);
        PopItemFlag();
        if (is_tab_button)
            return false;
        return tab_contents_visible;
    }

    if (tab_bar->SelectedTabId == id)
        tab->LastFrameSelected = g.FrameCount;

    // Backup current layout position
    const ImVec2 backup_main_cursor_pos = window->DC.CursorPos;

    // Layout
    const bool is_central_section = (tab->Flags & (ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_Trailing)) == 0;
    size.x = tab->Width;
    if (is_central_section)
        window->DC.CursorPos = tab_bar->BarRect.Min + ImVec2(IM_FLOOR(tab->Offset - tab_bar->ScrollingAnim), 0.0f);
    else
        window->DC.CursorPos = tab_bar->BarRect.Min + ImVec2(tab->Offset, 0.0f);
    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos, pos + size);

    // We don't have CPU clipping primitives to clip the CloseButton (until it becomes a texture), so need to add an extra draw call (temporary in the case of vertical animation)
    const bool want_clip_rect = is_central_section && (bb.Min.x < tab_bar->ScrollingRectMinX || bb.Max.x > tab_bar->ScrollingRectMaxX);
    if (want_clip_rect)
        PushClipRect(ImVec2(ImMax(bb.Min.x, tab_bar->ScrollingRectMinX), bb.Min.y - 1), ImVec2(tab_bar->ScrollingRectMaxX, bb.Max.y), true);

    ImVec2 backup_cursor_max_pos = window->DC.CursorMaxPos;
    ItemSize(bb.GetSize(), style.FramePadding.y);
    window->DC.CursorMaxPos = backup_cursor_max_pos;

    if (!ItemAdd(bb, id))
    {
        if (want_clip_rect)
            PopClipRect();
        window->DC.CursorPos = backup_main_cursor_pos;
        return tab_contents_visible;
    }

    // Click to Select a tab
    ImGuiButtonFlags button_flags = ((is_tab_button ? ImGuiButtonFlags_PressedOnClickRelease : ImGuiButtonFlags_PressedOnClick) | ImGuiButtonFlags_AllowItemOverlap);
    if (g.DragDropActive && !g.DragDropPayload.IsDataType(IMGUI_PAYLOAD_TYPE_WINDOW))
        button_flags |= ImGuiButtonFlags_PressedOnDragDropHold;
    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, button_flags);
    if (pressed && !is_tab_button)
        tab_bar->NextSelectedTabId = id;
    hovered |= (g.HoveredId == id);

    // Transfer active id window so the active id is not owned by the dock host (as StartMouseMovingWindow()
    // will only do it on the drag). This allows FocusWindow() to be more conservative in how it clears active id.
    if (held && docked_window && g.ActiveId == id && g.ActiveIdIsJustActivated)
        g.ActiveIdWindow = docked_window;

    // Allow the close button to overlap unless we are dragging (in which case we don't want any overlapping tabs to be hovered)
    if (!held)
        SetItemAllowOverlap();

    // Drag and drop a single floating window node moves it
    ImGuiDockNode* node = docked_window ? docked_window->DockNode : NULL;
    const bool single_floating_window_node = node && node->IsFloatingNode() && (node->Windows.Size == 1);
    if (held && single_floating_window_node && IsMouseDragging(0, 0.0f))
    {
        // Move
        StartMouseMovingWindow(docked_window);
    }
    else if (held && !tab_appearing && IsMouseDragging(0))
    {
        // Drag and drop: re-order tabs
        float drag_distance_from_edge_x = 0.0f;
        if (!g.DragDropActive && ((tab_bar->Flags & ImGuiTabBarFlags_Reorderable) || (docked_window != NULL)))
        {
            // While moving a tab it will jump on the other side of the mouse, so we also test for MouseDelta.x
            if (g.IO.MouseDelta.x < 0.0f && g.IO.MousePos.x < bb.Min.x)
            {
                drag_distance_from_edge_x = bb.Min.x - g.IO.MousePos.x;
                if (tab_bar->Flags & ImGuiTabBarFlags_Reorderable)
                    TabBarQueueReorder(tab_bar, tab, -1);
            }
            else if (g.IO.MouseDelta.x > 0.0f && g.IO.MousePos.x > bb.Max.x)
            {
                drag_distance_from_edge_x = g.IO.MousePos.x - bb.Max.x;
                if (tab_bar->Flags & ImGuiTabBarFlags_Reorderable)
                    TabBarQueueReorder(tab_bar, tab, +1);
            }
        }

        // Extract a Dockable window out of it's tab bar
        if (docked_window != NULL && !(docked_window->Flags & ImGuiWindowFlags_NoMove))
        {
            // We use a variable threshold to distinguish dragging tabs within a tab bar and extracting them out of the tab bar
            bool undocking_tab = (g.DragDropActive && g.DragDropPayload.SourceId == id);

            if (!undocking_tab) //&& (!g.IO.ConfigDockingWithShift || g.IO.KeyShift)
            {
                float threshold_base = g.FontSize;
                //float threshold_base = g.IO.ConfigDockingWithShift ? g.FontSize * 0.5f : g.FontSize;
                float threshold_x = (threshold_base * 2.2f);
                float threshold_y = (threshold_base * 1.5f) + ImClamp((ImFabs(g.IO.MouseDragMaxDistanceAbs[0].x) - threshold_base * 2.0f) * 0.20f, 0.0f, threshold_base * 4.0f);
                //GetForegroundDrawList()->AddRect(ImVec2(bb.Min.x - threshold_x, bb.Min.y - threshold_y), ImVec2(bb.Max.x + threshold_x, bb.Max.y + threshold_y), IM_COL32_WHITE); // [DEBUG]

                float distance_from_edge_y = ImMax(bb.Min.y - g.IO.MousePos.y, g.IO.MousePos.y - bb.Max.y);
                if (distance_from_edge_y >= threshold_y)
                    undocking_tab = true;
                else if (drag_distance_from_edge_x > threshold_x)
                    if ((tab_bar->ReorderRequestDir < 0 && tab_bar->GetTabOrder(tab) == 0) || (tab_bar->ReorderRequestDir > 0 && tab_bar->GetTabOrder(tab) == tab_bar->Tabs.Size - 1))
                        undocking_tab = true;
            }

            if (undocking_tab)
            {
                // Undock
                DockContextQueueUndockWindow(&g, docked_window);
                g.MovingWindow = docked_window;
                SetActiveID(g.MovingWindow->MoveId, g.MovingWindow);
                g.ActiveIdClickOffset -= g.MovingWindow->Pos - bb.Min;
                g.ActiveIdNoClearOnFocusLoss = true;
            }
        }
    }

#if 0
    if (hovered && g.HoveredIdNotActiveTimer > TOOLTIP_DELAY && bb.GetWidth() < tab->ContentWidth)
    {
        // Enlarge tab display when hovering
        bb.Max.x = bb.Min.x + IM_FLOOR(ImLerp(bb.GetWidth(), tab->ContentWidth, ImSaturate((g.HoveredIdNotActiveTimer - 0.40f) * 6.0f)));
        display_draw_list = GetForegroundDrawList(window);
        TabItemBackground(display_draw_list, bb, flags, GetColorU32(ImGuiCol_TitleBgActive));
    }
#endif

    // Render tab shape
    ImDrawList* display_draw_list = window->DrawList;
    const ImU32 tab_col = GetColorU32((held || hovered) ? ImGuiCol_TabHovered : tab_contents_visible ? (tab_bar_focused ? ImGuiCol_TabActive : ImGuiCol_TabUnfocusedActive) : (tab_bar_focused ? ImGuiCol_Tab : ImGuiCol_TabUnfocused));
    TabItemBackground(display_draw_list, bb, flags, tab_col);
    RenderNavHighlight(bb, id);

    // Select with right mouse button. This is so the common idiom for context menu automatically highlight the current widget.
    const bool hovered_unblocked = IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    if (hovered_unblocked && (IsMouseClicked(1) || IsMouseReleased(1)))
        if (!is_tab_button)
            tab_bar->NextSelectedTabId = id;

    if (tab_bar->Flags & ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)
        flags |= ImGuiTabItemFlags_NoCloseWithMiddleMouseButton;

    // Render tab label, process close button
    const ImGuiID close_button_id = p_open ? GetIDWithSeed("#CLOSE", NULL, id) : 0;
    bool just_closed;
    bool text_clipped;
    TabItemLabelAndCloseButton(display_draw_list, bb, flags, tab_bar->FramePadding, label, id, close_button_id, tab_contents_visible, &just_closed, &text_clipped);
    if (just_closed && p_open != NULL)
    {
        *p_open = false;
        TabBarCloseTab(tab_bar, tab);
    }

    // Restore main window position so user can draw there
    if (want_clip_rect)
        PopClipRect();
    window->DC.CursorPos = backup_main_cursor_pos;

    // Tooltip (FIXME: Won't work over the close button because ItemOverlap systems messes up with HoveredIdTimer)
    // We test IsItemHovered() to discard e.g. when another item is active or drag and drop over the tab bar (which g.HoveredId ignores)
    if (text_clipped && g.HoveredId == id && !held && g.HoveredIdNotActiveTimer > TOOLTIP_DELAY && IsItemHovered())
        if (!(tab_bar->Flags & ImGuiTabBarFlags_NoTooltip) && !(tab->Flags & ImGuiTabItemFlags_NoTooltip))
            SetTooltip("%.*s", (int)(FindRenderedTextEnd(label) - label), label);

    IM_ASSERT(!is_tab_button || !(tab_bar->SelectedTabId == tab->ID && is_tab_button)); // TabItemButton should not be selected
    if (is_tab_button)
        return pressed;
    return tab_contents_visible;
}

// [Public] This is call is 100% optional but it allows to remove some one-frame glitches when a tab has been unexpectedly removed.
// To use it to need to call the function SetTabItemClosed() between BeginTabBar() and EndTabBar().
// Tabs closed by the close button will automatically be flagged to avoid this issue.
void    ImGui::SetTabItemClosed(const char* label)
{
    ImGuiContext& g = *GImGui;
    bool is_within_manual_tab_bar = g.CurrentTabBar && !(g.CurrentTabBar->Flags & ImGuiTabBarFlags_DockNode);
    if (is_within_manual_tab_bar)
    {
        ImGuiTabBar* tab_bar = g.CurrentTabBar;
        ImGuiID tab_id = TabBarCalcTabID(tab_bar, label, NULL);
        if (ImGuiTabItem* tab = TabBarFindTabByID(tab_bar, tab_id))
            tab->WantClose = true; // Will be processed by next call to TabBarLayout()
    }
    else if (ImGuiWindow* window = FindWindowByName(label))
    {
        if (window->DockIsActive)
            if (ImGuiDockNode* node = window->DockNode)
            {
                ImGuiID tab_id = TabBarCalcTabID(node->TabBar, label, window);
                TabBarRemoveTab(node->TabBar, tab_id);
                window->DockTabWantClose = true;
            }
    }
}

ImVec2 ImGui::TabItemCalcSize(const char* label, bool has_close_button)
{
    ImGuiContext& g = *GImGui;
    ImVec2 label_size = CalcTextSize(label, NULL, true);
    ImVec2 size = ImVec2(label_size.x + g.Style.FramePadding.x, label_size.y + g.Style.FramePadding.y * 2.0f);
    if (has_close_button)
        size.x += g.Style.FramePadding.x + (g.Style.ItemInnerSpacing.x + g.FontSize); // We use Y intentionally to fit the close button circle.
    else
        size.x += g.Style.FramePadding.x + 1.0f;
    return ImVec2(ImMin(size.x, TabBarCalcMaxTabWidth()), size.y);
}

void ImGui::TabItemBackground(ImDrawList* draw_list, const ImRect& bb, ImGuiTabItemFlags flags, ImU32 col)
{
    // While rendering tabs, we trim 1 pixel off the top of our bounding box so they can fit within a regular frame height while looking "detached" from it.
    ImGuiContext& g = *GImGui;
    const float width = bb.GetWidth();
    IM_UNUSED(flags);
    IM_ASSERT(width > 0.0f);
    const float rounding = ImMax(0.0f, ImMin((flags & ImGuiTabItemFlags_Button) ? g.Style.FrameRounding : g.Style.TabRounding, width * 0.5f - 1.0f));
    const float y1 = bb.Min.y + 1.0f;
    const float y2 = bb.Max.y + ((flags & ImGuiTabItemFlags_Preview) ? 0.0f : -1.0f);
    draw_list->PathLineTo(ImVec2(bb.Min.x, y2));
    draw_list->PathArcToFast(ImVec2(bb.Min.x + rounding, y1 + rounding), rounding, 6, 9);
    draw_list->PathArcToFast(ImVec2(bb.Max.x - rounding, y1 + rounding), rounding, 9, 12);
    draw_list->PathLineTo(ImVec2(bb.Max.x, y2));
    draw_list->PathFillConvex(col);
    if (g.Style.TabBorderSize > 0.0f)
    {
        draw_list->PathLineTo(ImVec2(bb.Min.x + 0.5f, y2));
        draw_list->PathArcToFast(ImVec2(bb.Min.x + rounding + 0.5f, y1 + rounding + 0.5f), rounding, 6, 9);
        draw_list->PathArcToFast(ImVec2(bb.Max.x - rounding - 0.5f, y1 + rounding + 0.5f), rounding, 9, 12);
        draw_list->PathLineTo(ImVec2(bb.Max.x - 0.5f, y2));
        draw_list->PathStroke(GetColorU32(ImGuiCol_Border), false, g.Style.TabBorderSize);
    }
}

// Render text label (with custom clipping) + Unsaved Document marker + Close Button logic
// We tend to lock style.FramePadding for a given tab-bar, hence the 'frame_padding' parameter.
void ImGui::TabItemLabelAndCloseButton(ImDrawList* draw_list, const ImRect& bb, ImGuiTabItemFlags flags, ImVec2 frame_padding, const char* label, ImGuiID tab_id, ImGuiID close_button_id, bool is_contents_visible, bool* out_just_closed, bool* out_text_clipped)
{
    ImGuiContext& g = *GImGui;
    ImVec2 label_size = CalcTextSize(label, NULL, true);

    if (out_just_closed)
        *out_just_closed = false;
    if (out_text_clipped)
        *out_text_clipped = false;

    if (bb.GetWidth() <= 1.0f)
        return;

    // In Style V2 we'll have full override of all colors per state (e.g. focused, selected)
    // But right now if you want to alter text color of tabs this is what you need to do.
#if 0
    const float backup_alpha = g.Style.Alpha;
    if (!is_contents_visible)
        g.Style.Alpha *= 0.7f;
#endif

    // Render text label (with clipping + alpha gradient) + unsaved marker
    const char* TAB_UNSAVED_MARKER = "*";
    ImRect text_pixel_clip_bb(bb.Min.x + frame_padding.x, bb.Min.y + frame_padding.y, bb.Max.x - frame_padding.x, bb.Max.y);
    if (flags & ImGuiTabItemFlags_UnsavedDocument)
    {
        text_pixel_clip_bb.Max.x -= CalcTextSize(TAB_UNSAVED_MARKER, NULL, false).x;
        ImVec2 unsaved_marker_pos(ImMin(bb.Min.x + frame_padding.x + label_size.x + 2, text_pixel_clip_bb.Max.x), bb.Min.y + frame_padding.y + IM_FLOOR(-g.FontSize * 0.25f));
        RenderTextClippedEx(draw_list, unsaved_marker_pos, bb.Max - frame_padding, TAB_UNSAVED_MARKER, NULL, NULL);
    }
    ImRect text_ellipsis_clip_bb = text_pixel_clip_bb;

    // Return clipped state ignoring the close button
    if (out_text_clipped)
    {
        *out_text_clipped = (text_ellipsis_clip_bb.Min.x + label_size.x) > text_pixel_clip_bb.Max.x;
        //draw_list->AddCircle(text_ellipsis_clip_bb.Min, 3.0f, *out_text_clipped ? IM_COL32(255, 0, 0, 255) : IM_COL32(0, 255, 0, 255));
    }

    // Close Button
    // We are relying on a subtle and confusing distinction between 'hovered' and 'g.HoveredId' which happens because we are using ImGuiButtonFlags_AllowOverlapMode + SetItemAllowOverlap()
    //  'hovered' will be true when hovering the Tab but NOT when hovering the close button
    //  'g.HoveredId==id' will be true when hovering the Tab including when hovering the close button
    //  'g.ActiveId==close_button_id' will be true when we are holding on the close button, in which case both hovered booleans are false
    bool close_button_pressed = false;
    bool close_button_visible = false;
    if (close_button_id != 0)
        if (is_contents_visible || bb.GetWidth() >= g.Style.TabMinWidthForCloseButton)
            if (g.HoveredId == tab_id || g.HoveredId == close_button_id || g.ActiveId == tab_id || g.ActiveId == close_button_id)
                close_button_visible = true;
    if (close_button_visible)
    {
        ImGuiLastItemDataBackup last_item_backup;
        const float close_button_sz = g.FontSize;
        PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
        if (CloseButton(close_button_id, ImVec2(bb.Max.x - frame_padding.x * 2.0f - close_button_sz, bb.Min.y)))
            close_button_pressed = true;
        PopStyleVar();
        last_item_backup.Restore();

        // Close with middle mouse button
        if (!(flags & ImGuiTabItemFlags_NoCloseWithMiddleMouseButton) && IsMouseClicked(2))
            close_button_pressed = true;

        text_pixel_clip_bb.Max.x -= close_button_sz;
    }

    // FIXME: if FramePadding is noticeably large, ellipsis_max_x will be wrong here (e.g. #3497), maybe for consistency that parameter of RenderTextEllipsis() shouldn't exist..
    float ellipsis_max_x = close_button_visible ? text_pixel_clip_bb.Max.x : bb.Max.x - 1.0f;
    RenderTextEllipsis(draw_list, text_ellipsis_clip_bb.Min, text_ellipsis_clip_bb.Max, text_pixel_clip_bb.Max.x, ellipsis_max_x, label, NULL, &label_size);

#if 0
    if (!is_contents_visible)
        g.Style.Alpha = backup_alpha;
#endif

    if (out_just_closed)
        *out_just_closed = close_button_pressed;
}


//-------------------------------------------------------------------------
// [SECTION] Widgets: Columns, BeginColumns, EndColumns, etc.
// In the current version, Columns are very weak. Needs to be replaced with a more full-featured system.
//-------------------------------------------------------------------------
// - SetWindowClipRectBeforeSetChannel() [Internal]
// - GetColumnIndex()
// - GetColumnsCount()
// - GetColumnOffset()
// - GetColumnWidth()
// - SetColumnOffset()
// - SetColumnWidth()
// - PushColumnClipRect() [Internal]
// - PushColumnsBackground() [Internal]
// - PopColumnsBackground() [Internal]
// - FindOrCreateColumns() [Internal]
// - GetColumnsID() [Internal]
// - BeginColumns()
// - NextColumn()
// - EndColumns()
// - Columns()
//-------------------------------------------------------------------------

// [Internal] Small optimization to avoid calls to PopClipRect/SetCurrentChannel/PushClipRect in sequences,
// they would meddle many times with the underlying ImDrawCmd.
// Instead, we do a preemptive overwrite of clipping rectangle _without_ altering the command-buffer and let
// the subsequent single call to SetCurrentChannel() does it things once.
void ImGui::SetWindowClipRectBeforeSetChannel(ImGuiWindow* window, const ImRect& clip_rect)
{
    ImVec4 clip_rect_vec4 = clip_rect.ToVec4();
    window->ClipRect = clip_rect;
    window->DrawList->_CmdHeader.ClipRect = clip_rect_vec4;
    window->DrawList->_ClipRectStack.Data[window->DrawList->_ClipRectStack.Size - 1] = clip_rect_vec4;
}

int ImGui::GetColumnIndex()
{
    ImGuiWindow* window = GetCurrentWindowRead();
    return window->DC.CurrentColumns ? window->DC.CurrentColumns->Current : 0;
}

int ImGui::GetColumnsCount()
{
    ImGuiWindow* window = GetCurrentWindowRead();
    return window->DC.CurrentColumns ? window->DC.CurrentColumns->Count : 1;
}

float ImGui::GetColumnOffsetFromNorm(const ImGuiColumns* columns, float offset_norm)
{
    return offset_norm * (columns->OffMaxX - columns->OffMinX);
}

float ImGui::GetColumnNormFromOffset(const ImGuiColumns* columns, float offset)
{
    return offset / (columns->OffMaxX - columns->OffMinX);
}

static const float COLUMNS_HIT_RECT_HALF_WIDTH = 4.0f;

static float GetDraggedColumnOffset(ImGuiColumns* columns, int column_index)
{
    // Active (dragged) column always follow mouse. The reason we need this is that dragging a column to the right edge of an auto-resizing
    // window creates a feedback loop because we store normalized positions. So while dragging we enforce absolute positioning.
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    IM_ASSERT(column_index > 0); // We are not supposed to drag column 0.
    IM_ASSERT(g.ActiveId == columns->ID + ImGuiID(column_index));

    float x = g.IO.MousePos.x - g.ActiveIdClickOffset.x + COLUMNS_HIT_RECT_HALF_WIDTH - window->Pos.x;
    x = ImMax(x, ImGui::GetColumnOffset(column_index - 1) + g.Style.ColumnsMinSpacing);
    if ((columns->Flags & ImGuiColumnsFlags_NoPreserveWidths))
        x = ImMin(x, ImGui::GetColumnOffset(column_index + 1) - g.Style.ColumnsMinSpacing);

    return x;
}

float ImGui::GetColumnOffset(int column_index)
{
    ImGuiWindow* window = GetCurrentWindowRead();
    ImGuiColumns* columns = window->DC.CurrentColumns;
    if (columns == NULL)
        return 0.0f;

    if (column_index < 0)
        column_index = columns->Current;
    IM_ASSERT(column_index < columns->Columns.Size);

    const float t = columns->Columns[column_index].OffsetNorm;
    const float x_offset = ImLerp(columns->OffMinX, columns->OffMaxX, t);
    return x_offset;
}

static float GetColumnWidthEx(ImGuiColumns* columns, int column_index, bool before_resize = false)
{
    if (column_index < 0)
        column_index = columns->Current;

    float offset_norm;
    if (before_resize)
        offset_norm = columns->Columns[column_index + 1].OffsetNormBeforeResize - columns->Columns[column_index].OffsetNormBeforeResize;
    else
        offset_norm = columns->Columns[column_index + 1].OffsetNorm - columns->Columns[column_index].OffsetNorm;
    return ImGui::GetColumnOffsetFromNorm(columns, offset_norm);
}

float ImGui::GetColumnWidth(int column_index)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiColumns* columns = window->DC.CurrentColumns;
    if (columns == NULL)
        return GetContentRegionAvail().x;

    if (column_index < 0)
        column_index = columns->Current;
    return GetColumnOffsetFromNorm(columns, columns->Columns[column_index + 1].OffsetNorm - columns->Columns[column_index].OffsetNorm);
}

void ImGui::SetColumnOffset(int column_index, float offset)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiColumns* columns = window->DC.CurrentColumns;
    IM_ASSERT(columns != NULL);

    if (column_index < 0)
        column_index = columns->Current;
    IM_ASSERT(column_index < columns->Columns.Size);

    const bool preserve_width = !(columns->Flags & ImGuiColumnsFlags_NoPreserveWidths) && (column_index < columns->Count - 1);
    const float width = preserve_width ? GetColumnWidthEx(columns, column_index, columns->IsBeingResized) : 0.0f;

    if (!(columns->Flags & ImGuiColumnsFlags_NoForceWithinWindow))
        offset = ImMin(offset, columns->OffMaxX - g.Style.ColumnsMinSpacing * (columns->Count - column_index));
    columns->Columns[column_index].OffsetNorm = GetColumnNormFromOffset(columns, offset - columns->OffMinX);

    if (preserve_width)
        SetColumnOffset(column_index + 1, offset + ImMax(g.Style.ColumnsMinSpacing, width));
}

void ImGui::SetColumnWidth(int column_index, float width)
{
    ImGuiWindow* window = GetCurrentWindowRead();
    ImGuiColumns* columns = window->DC.CurrentColumns;
    IM_ASSERT(columns != NULL);

    if (column_index < 0)
        column_index = columns->Current;
    SetColumnOffset(column_index + 1, GetColumnOffset(column_index) + width);
}

void ImGui::PushColumnClipRect(int column_index)
{
    ImGuiWindow* window = GetCurrentWindowRead();
    ImGuiColumns* columns = window->DC.CurrentColumns;
    if (column_index < 0)
        column_index = columns->Current;

    ImGuiColumnData* column = &columns->Columns[column_index];
    PushClipRect(column->ClipRect.Min, column->ClipRect.Max, false);
}

// Get into the columns background draw command (which is generally the same draw command as before we called BeginColumns)
void ImGui::PushColumnsBackground()
{
    ImGuiWindow* window = GetCurrentWindowRead();
    ImGuiColumns* columns = window->DC.CurrentColumns;
    if (columns->Count == 1)
        return;

    // Optimization: avoid SetCurrentChannel() + PushClipRect()
    columns->HostBackupClipRect = window->ClipRect;
    SetWindowClipRectBeforeSetChannel(window, columns->HostInitialClipRect);
    columns->Splitter.SetCurrentChannel(window->DrawList, 0);
}

void ImGui::PopColumnsBackground()
{
    ImGuiWindow* window = GetCurrentWindowRead();
    ImGuiColumns* columns = window->DC.CurrentColumns;
    if (columns->Count == 1)
        return;

    // Optimization: avoid PopClipRect() + SetCurrentChannel()
    SetWindowClipRectBeforeSetChannel(window, columns->HostBackupClipRect);
    columns->Splitter.SetCurrentChannel(window->DrawList, columns->Current + 1);
}

ImGuiColumns* ImGui::FindOrCreateColumns(ImGuiWindow* window, ImGuiID id)
{
    // We have few columns per window so for now we don't need bother much with turning this into a faster lookup.
    for (int n = 0; n < window->ColumnsStorage.Size; n++)
        if (window->ColumnsStorage[n].ID == id)
            return &window->ColumnsStorage[n];

    window->ColumnsStorage.push_back(ImGuiColumns());
    ImGuiColumns* columns = &window->ColumnsStorage.back();
    columns->ID = id;
    return columns;
}

ImGuiID ImGui::GetColumnsID(const char* str_id, int columns_count)
{
    ImGuiWindow* window = GetCurrentWindow();

    // Differentiate column ID with an arbitrary prefix for cases where users name their columns set the same as another widget.
    // In addition, when an identifier isn't explicitly provided we include the number of columns in the hash to make it uniquer.
    PushID(0x11223347 + (str_id ? 0 : columns_count));
    ImGuiID id = window->GetID(str_id ? str_id : "columns");
    PopID();

    return id;
}

void ImGui::BeginColumns(const char* str_id, int columns_count, ImGuiColumnsFlags flags)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();

    IM_ASSERT(columns_count >= 1);
    IM_ASSERT(window->DC.CurrentColumns == NULL);   // Nested columns are currently not supported

    // Acquire storage for the columns set
    ImGuiID id = GetColumnsID(str_id, columns_count);
    ImGuiColumns* columns = FindOrCreateColumns(window, id);
    IM_ASSERT(columns->ID == id);
    columns->Current = 0;
    columns->Count = columns_count;
    columns->Flags = flags;
    window->DC.CurrentColumns = columns;

    columns->HostCursorPosY = window->DC.CursorPos.y;
    columns->HostCursorMaxPosX = window->DC.CursorMaxPos.x;
    columns->HostInitialClipRect = window->ClipRect;
    columns->HostBackupParentWorkRect = window->ParentWorkRect;
    window->ParentWorkRect = window->WorkRect;

    // Set state for first column
    // We aim so that the right-most column will have the same clipping width as other after being clipped by parent ClipRect
    const float column_padding = g.Style.ItemSpacing.x;
    const float half_clip_extend_x = ImFloor(ImMax(window->WindowPadding.x * 0.5f, window->WindowBorderSize));
    const float max_1 = window->WorkRect.Max.x + column_padding - ImMax(column_padding - window->WindowPadding.x, 0.0f);
    const float max_2 = window->WorkRect.Max.x + half_clip_extend_x;
    columns->OffMinX = window->DC.Indent.x - column_padding + ImMax(column_padding - window->WindowPadding.x, 0.0f);
    columns->OffMaxX = ImMax(ImMin(max_1, max_2) - window->Pos.x, columns->OffMinX + 1.0f);
    columns->LineMinY = columns->LineMaxY = window->DC.CursorPos.y;

    // Clear data if columns count changed
    if (columns->Columns.Size != 0 && columns->Columns.Size != columns_count + 1)
        columns->Columns.resize(0);

    // Initialize default widths
    columns->IsFirstFrame = (columns->Columns.Size == 0);
    if (columns->Columns.Size == 0)
    {
        columns->Columns.reserve(columns_count + 1);
        for (int n = 0; n < columns_count + 1; n++)
        {
            ImGuiColumnData column;
            column.OffsetNorm = n / (float)columns_count;
            columns->Columns.push_back(column);
        }
    }

    for (int n = 0; n < columns_count; n++)
    {
        // Compute clipping rectangle
        ImGuiColumnData* column = &columns->Columns[n];
        float clip_x1 = IM_ROUND(window->Pos.x + GetColumnOffset(n));
        float clip_x2 = IM_ROUND(window->Pos.x + GetColumnOffset(n + 1) - 1.0f);
        column->ClipRect = ImRect(clip_x1, -FLT_MAX, clip_x2, +FLT_MAX);
        column->ClipRect.ClipWithFull(window->ClipRect);
    }

    if (columns->Count > 1)
    {
        columns->Splitter.Split(window->DrawList, 1 + columns->Count);
        columns->Splitter.SetCurrentChannel(window->DrawList, 1);
        PushColumnClipRect(0);
    }

    // We don't generally store Indent.x inside ColumnsOffset because it may be manipulated by the user.
    float offset_0 = GetColumnOffset(columns->Current);
    float offset_1 = GetColumnOffset(columns->Current + 1);
    float width = offset_1 - offset_0;
    PushItemWidth(width * 0.65f);
    window->DC.ColumnsOffset.x = ImMax(column_padding - window->WindowPadding.x, 0.0f);
    window->DC.CursorPos.x = IM_FLOOR(window->Pos.x + window->DC.Indent.x + window->DC.ColumnsOffset.x);
    window->WorkRect.Max.x = window->Pos.x + offset_1 - column_padding;
}

void ImGui::NextColumn()
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems || window->DC.CurrentColumns == NULL)
        return;

    ImGuiContext& g = *GImGui;
    ImGuiColumns* columns = window->DC.CurrentColumns;

    if (columns->Count == 1)
    {
        window->DC.CursorPos.x = IM_FLOOR(window->Pos.x + window->DC.Indent.x + window->DC.ColumnsOffset.x);
        IM_ASSERT(columns->Current == 0);
        return;
    }

    // Next column
    if (++columns->Current == columns->Count)
        columns->Current = 0;

    PopItemWidth();

    // Optimization: avoid PopClipRect() + SetCurrentChannel() + PushClipRect()
    // (which would needlessly attempt to update commands in the wrong channel, then pop or overwrite them),
    ImGuiColumnData* column = &columns->Columns[columns->Current];
    SetWindowClipRectBeforeSetChannel(window, column->ClipRect);
    columns->Splitter.SetCurrentChannel(window->DrawList, columns->Current + 1);

    const float column_padding = g.Style.ItemSpacing.x;
    columns->LineMaxY = ImMax(columns->LineMaxY, window->DC.CursorPos.y);
    if (columns->Current > 0)
    {
        // Columns 1+ ignore IndentX (by canceling it out)
        // FIXME-COLUMNS: Unnecessary, could be locked?
        window->DC.ColumnsOffset.x = GetColumnOffset(columns->Current) - window->DC.Indent.x + column_padding;
    }
    else
    {
        // New row/line: column 0 honor IndentX.
        window->DC.ColumnsOffset.x = ImMax(column_padding - window->WindowPadding.x, 0.0f);
        columns->LineMinY = columns->LineMaxY;
    }
    window->DC.CursorPos.x = IM_FLOOR(window->Pos.x + window->DC.Indent.x + window->DC.ColumnsOffset.x);
    window->DC.CursorPos.y = columns->LineMinY;
    window->DC.CurrLineSize = ImVec2(0.0f, 0.0f);
    window->DC.CurrLineTextBaseOffset = 0.0f;

    // FIXME-COLUMNS: Share code with BeginColumns() - move code on columns setup.
    float offset_0 = GetColumnOffset(columns->Current);
    float offset_1 = GetColumnOffset(columns->Current + 1);
    float width = offset_1 - offset_0;
    PushItemWidth(width * 0.65f);
    window->WorkRect.Max.x = window->Pos.x + offset_1 - column_padding;
}

void ImGui::EndColumns()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    ImGuiColumns* columns = window->DC.CurrentColumns;
    IM_ASSERT(columns != NULL);

    PopItemWidth();
    if (columns->Count > 1)
    {
        PopClipRect();
        columns->Splitter.Merge(window->DrawList);
    }

    const ImGuiColumnsFlags flags = columns->Flags;
    columns->LineMaxY = ImMax(columns->LineMaxY, window->DC.CursorPos.y);
    window->DC.CursorPos.y = columns->LineMaxY;
    if (!(flags & ImGuiColumnsFlags_GrowParentContentsSize))
        window->DC.CursorMaxPos.x = columns->HostCursorMaxPosX;  // Restore cursor max pos, as columns don't grow parent

    // Draw columns borders and handle resize
    // The IsBeingResized flag ensure we preserve pre-resize columns width so back-and-forth are not lossy
    bool is_being_resized = false;
    if (!(flags & ImGuiColumnsFlags_NoBorder) && !window->SkipItems)
    {
        // We clip Y boundaries CPU side because very long triangles are mishandled by some GPU drivers.
        const float y1 = ImMax(columns->HostCursorPosY, window->ClipRect.Min.y);
        const float y2 = ImMin(window->DC.CursorPos.y, window->ClipRect.Max.y);
        int dragging_column = -1;
        for (int n = 1; n < columns->Count; n++)
        {
            ImGuiColumnData* column = &columns->Columns[n];
            float x = window->Pos.x + GetColumnOffset(n);
            const ImGuiID column_id = columns->ID + ImGuiID(n);
            const float column_hit_hw = COLUMNS_HIT_RECT_HALF_WIDTH;
            const ImRect column_hit_rect(ImVec2(x - column_hit_hw, y1), ImVec2(x + column_hit_hw, y2));
            KeepAliveID(column_id);
            if (IsClippedEx(column_hit_rect, column_id, false))
                continue;

            bool hovered = false, held = false;
            if (!(flags & ImGuiColumnsFlags_NoResize))
            {
                ButtonBehavior(column_hit_rect, column_id, &hovered, &held);
                if (hovered || held)
                    g.MouseCursor = ImGuiMouseCursor_ResizeEW;
                if (held && !(column->Flags & ImGuiColumnsFlags_NoResize))
                    dragging_column = n;
            }

            // Draw column
            const ImU32 col = GetColorU32(held ? ImGuiCol_SeparatorActive : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
            const float xi = IM_FLOOR(x);
            window->DrawList->AddLine(ImVec2(xi, y1 + 1.0f), ImVec2(xi, y2), col);
        }

        // Apply dragging after drawing the column lines, so our rendered lines are in sync with how items were displayed during the frame.
        if (dragging_column != -1)
        {
            if (!columns->IsBeingResized)
                for (int n = 0; n < columns->Count + 1; n++)
                    columns->Columns[n].OffsetNormBeforeResize = columns->Columns[n].OffsetNorm;
            columns->IsBeingResized = is_being_resized = true;
            float x = GetDraggedColumnOffset(columns, dragging_column);
            SetColumnOffset(dragging_column, x);
        }
    }
    columns->IsBeingResized = is_being_resized;

    window->WorkRect = window->ParentWorkRect;
    window->ParentWorkRect = columns->HostBackupParentWorkRect;
    window->DC.CurrentColumns = NULL;
    window->DC.ColumnsOffset.x = 0.0f;
    window->DC.CursorPos.x = IM_FLOOR(window->Pos.x + window->DC.Indent.x + window->DC.ColumnsOffset.x);
}

// [2018-03: This is currently the only public API, while we are working on making BeginColumns/EndColumns user-facing]
void ImGui::Columns(int columns_count, const char* id, bool border)
{
    ImGuiWindow* window = GetCurrentWindow();
    IM_ASSERT(columns_count >= 1);

    ImGuiColumnsFlags flags = (border ? 0 : ImGuiColumnsFlags_NoBorder);
    //flags |= ImGuiColumnsFlags_NoPreserveWidths; // NB: Legacy behavior
    ImGuiColumns* columns = window->DC.CurrentColumns;
    if (columns != NULL && columns->Count == columns_count && columns->Flags == flags)
        return;

    if (columns != NULL)
        EndColumns();

    if (columns_count != 1)
        BeginColumns(id, columns_count, flags);
}


//-----------------------------------------------------------------------------
// [SECTION] Widgets: BeginTable, EndTable, etc.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Typical call flow: (root level is public API):
// - BeginTable()                               user begin into a table
//    | BeginChild()                            - (if ScrollX/ScrollY is set)
//    | TableBeginUpdateColumns()               - apply resize/order requests, lock columns active state, order
//    | - TableSetColumnWidth()                 - apply resizing width (for mouse resize, often requested by previous frame)
//    |    - TableUpdateColumnsWeightFromWidth()- recompute columns weights (of stretch columns) from their respective width
// - TableSetupColumn()                         user submit columns details (optional)
// - TableSetupScrollFreeze()                   user submit scroll freeze information (optional)
// - TableUpdateLayout() [Internal]             automatically called by the FIRST call to TableNextRow() or TableHeadersRow(): lock all widths, columns positions, clipping rectangles
//    | TableUpdateDrawChannels()               - setup ImDrawList channels
//    | TableUpdateBorders()                    - detect hovering columns for resize, ahead of contents submission
//    | TableDrawContextMenu()                  - draw right-click context menu
//-----------------------------------------------------------------------------
// - TableHeadersRow() or TableHeader()         user submit a headers row (optional)
//    | TableSortSpecsClickColumn()             - when left-clicked: alter sort order and sort direction
//    | TableOpenContextMenu()                  - when right-clicked: trigger opening of the default context menu
// - TableGetSortSpecs()                        user queries updated sort specs (optional, generally after submitting headers)
// - TableNextRow()                             user begin into a new row (also automatically called by TableHeadersRow())
//    | TableEndRow()                           - finish existing row
//    | TableBeginRow()                         - add a new row
// - TableSetColumnIndex() / TableNextColumn()  user begin into a cell
//    | TableEndCell()                          - close existing cell
//    | TableBeginCell()                        - enter into current cell
// - [...]                                      user emit contents
//-----------------------------------------------------------------------------
// - EndTable()                                 user ends the table
//    | TableDrawBorders()                      - draw outer borders, inner vertical borders
//    | TableReorderDrawChannelsForMerge()      - merge draw channels if clipping isn't required
//    | EndChild()                              - (if ScrollX/ScrollY is set)
//-----------------------------------------------------------------------------

// Configuration
static const int TABLE_DRAW_CHANNEL_BG0 = 0;
static const int TABLE_DRAW_CHANNEL_BG1_FROZEN = 1;
static const int TABLE_DRAW_CHANNEL_UNCLIPPED = 2;                  // When using ImGuiTableFlags_NoClip
static const float TABLE_BORDER_SIZE                     = 1.0f;    // FIXME-TABLE: Currently hard-coded.
static const float TABLE_RESIZE_SEPARATOR_HALF_THICKNESS = 4.0f;    // Extend outside inner borders.
static const float TABLE_RESIZE_SEPARATOR_FEEDBACK_TIMER = 0.06f;   // Delay/timer before making the hover feedback (color+cursor) visible because tables/columns tends to be more cramped.

// Helper
inline ImGuiTableFlags TableFixFlags(ImGuiTableFlags flags, ImGuiWindow* outer_window)
{
    // Adjust flags: set default sizing policy
    if ((flags & ImGuiTableFlags_SizingPolicyMaskX_) == 0)
        flags |= (flags & ImGuiTableFlags_ScrollX) ? ImGuiTableFlags_SizingPolicyFixedX : ImGuiTableFlags_SizingPolicyStretchX;

    // Adjust flags: MultiSortable automatically enable Sortable
    if (flags & ImGuiTableFlags_MultiSortable)
        flags |= ImGuiTableFlags_Sortable;

    // Adjust flags: disable saved settings if there's nothing to save
    if ((flags & (ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Sortable)) == 0)
        flags |= ImGuiTableFlags_NoSavedSettings;

    // Adjust flags: enforce borders when resizable
    if (flags & ImGuiTableFlags_Resizable)
        flags |= ImGuiTableFlags_BordersInnerV;

    // Adjust flags: disable NoHostExtendY if we have any scrolling going on
    if ((flags & ImGuiTableFlags_NoHostExtendY) && (flags & (ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY)) != 0)
        flags &= ~ImGuiTableFlags_NoHostExtendY;

    // Adjust flags: NoBordersInBodyUntilResize takes priority over NoBordersInBody
    if (flags & ImGuiTableFlags_NoBordersInBodyUntilResize)
        flags &= ~ImGuiTableFlags_NoBordersInBody;

    // Inherit _NoSavedSettings from top-level window (child windows always have _NoSavedSettings set)
#ifdef IMGUI_HAS_DOCK
    ImGuiWindow* window_for_settings = outer_window->RootWindowDockStop;
#else
    ImGuiWindow* window_for_settings = outer_window->RootWindow;
#endif
    if (window_for_settings->Flags & ImGuiWindowFlags_NoSavedSettings)
        flags |= ImGuiTableFlags_NoSavedSettings;

    return flags;
}

ImGuiTable* ImGui::TableFindByID(ImGuiID id)
{
    ImGuiContext& g = *GImGui;
    return g.Tables.GetByKey(id);
}

ImGuiTable::ImGuiTable()
{
    memset(this, 0, sizeof(*this));
    SettingsOffset = -1;
    InstanceInteracted = -1;
    LastFrameActive = -1;
    LastResizedColumn = -1;
    ContextPopupColumn = -1;
    ReorderColumn = -1;
    ResizedColumn = -1;
    HoveredColumnBody = HoveredColumnBorder = -1;
}

ImGuiTable::~ImGuiTable()
{
    IM_FREE(RawData);
}

// (Read carefully because this is subtle but it does make sense!)
// About 'outer_size', its meaning needs to differ slightly depending of if we are using ScrollX/ScrollY flags:
//   X:
//   - outer_size.x < 0.0f  ->  right align from window/work-rect maximum x edge.
//   - outer_size.x = 0.0f  ->  auto enlarge, use all available space.
//   - outer_size.x > 0.0f  ->  fixed width
//   Y with ScrollX/ScrollY: using a child window for scrolling:
//   - outer_size.y < 0.0f  ->  bottom align
//   - outer_size.y = 0.0f  ->  bottom align, consistent with BeginChild(). not recommended unless table is last item in parent window.
//   - outer_size.y > 0.0f  ->  fixed child height. recommended when using Scrolling on any axis.
//   Y without scrolling, we output table directly in parent window:
//   - outer_size.y < 0.0f  ->  bottom align (will auto extend, unless NoHostExtendV is set)
//   - outer_size.y = 0.0f  ->  zero minimum height (will auto extend, unless NoHostExtendV is set)
//   - outer_size.y > 0.0f  ->  minimum height (will auto extend, unless NoHostExtendV is set)
// About 'inner_width':
//   With ScrollX:
//   - inner_width  < 0.0f  ->  *illegal* fit in known width (right align from outer_size.x) <-- weird
//   - inner_width  = 0.0f  ->  fit in outer_width: Fixed size columns will take space they need (if avail, otherwise shrink down), Stretch columns becomes Fixed columns.
//   - inner_width  > 0.0f  ->  override scrolling width, generally to be larger than outer_size.x. Fixed column take space they need (if avail, otherwise shrink down), Stretch columns share remaining space!
//   Without ScrollX:
//   - inner_width          ->  *ignored*
// Details:
// - If you want to use Stretch columns with ScrollX, you generally need to specify 'inner_width' otherwise the concept
//   of "available space" doesn't make sense.
// - Even if not really useful, we allow 'inner_width < outer_size.x' for consistency and to facilitate understanding
//   of what the value does.
bool    ImGui::BeginTable(const char* str_id, int columns_count, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width)
{
    ImGuiID id = GetID(str_id);
    return BeginTableEx(str_id, id, columns_count, flags, outer_size, inner_width);
}

// For reference, the total _allocation count_ for a table is:
// + 0 (for ImGuiTable instance, we are pooling allocations in g.Tables)
// + 1 (for table->RawData allocated below)
// + 1 (for table->ColumnsNames, if names are used)
// + 1 (for table->Splitter._Channels)
// + 2 * active_channels_count (for ImDrawCmd and ImDrawIdx buffers inside channels)
// Where active_channels_count is variable but often == columns_count or columns_count + 1, see TableUpdateDrawChannels() for details.
// Unused channels don't perform their +2 allocations.
static void TableBeginInitMemory(ImGuiTable* table, int columns_count)
{
    // Allocate single buffer for our arrays
    ImSpanAllocator<3> span_allocator;
    span_allocator.ReserveBytes(0, columns_count * sizeof(ImGuiTableColumn));
    span_allocator.ReserveBytes(1, columns_count * sizeof(ImS8));
    span_allocator.ReserveBytes(2, columns_count * sizeof(ImGuiTableCellData));
    table->RawData = IM_ALLOC(span_allocator.GetArenaSizeInBytes());
    span_allocator.SetArenaBasePtr(table->RawData);
    span_allocator.GetSpan(0, &table->Columns);
    span_allocator.GetSpan(1, &table->DisplayOrderToIndex);
    span_allocator.GetSpan(2, &table->RowCellData);

    for (int n = 0; n < columns_count; n++)
    {
        table->Columns[n] = ImGuiTableColumn();
        table->Columns[n].DisplayOrder = table->DisplayOrderToIndex[n] = (ImS8)n;
    }
}

bool    ImGui::BeginTableEx(const char* name, ImGuiID id, int columns_count, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* outer_window = GetCurrentWindow();
    if (outer_window->SkipItems) // Consistent with other tables + beneficial side effect that assert on miscalling EndTable() will be more visible.
        return false;

    // Sanity checks
    IM_ASSERT(columns_count > 0 && columns_count <= IMGUI_TABLE_MAX_COLUMNS && "Only 1..64 columns allowed!");
    if (flags & ImGuiTableFlags_ScrollX)
        IM_ASSERT(inner_width >= 0.0f);

    // If an outer size is specified ahead we will be able to early out when not visible. Exact clipping rules may evolve.
    const bool use_child_window = (flags & (ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY)) != 0;
    const ImVec2 avail_size = GetContentRegionAvail();
    ImVec2 actual_outer_size = CalcItemSize(outer_size, ImMax(avail_size.x, 1.0f), use_child_window ? ImMax(avail_size.y, 1.0f) : 0.0f);
    ImRect outer_rect(outer_window->DC.CursorPos, outer_window->DC.CursorPos + actual_outer_size);
    if (use_child_window && IsClippedEx(outer_rect, 0, false))
    {
        ItemSize(outer_rect);
        return false;
    }

    // Acquire storage for the table
    ImGuiTable* table = g.Tables.GetOrAddByKey(id);
    const bool table_is_new = (table->ID == 0);
    const int instance_no = (table->LastFrameActive != g.FrameCount) ? 0 : table->InstanceCurrent + 1;
    const ImGuiID instance_id = id + instance_no;
    const ImGuiTableFlags table_last_flags = table->Flags;
    if (instance_no > 0)
        IM_ASSERT(table->ColumnsCount == columns_count && "BeginTable(): Cannot change columns count mid-frame while preserving same ID");

    // Fix flags
    flags = TableFixFlags(flags, outer_window);

    // Initialize
    table->ID = id;
    table->Flags = flags;
    table->InstanceCurrent = (ImS16)instance_no;
    table->LastFrameActive = g.FrameCount;
    table->OuterWindow = table->InnerWindow = outer_window;
    table->ColumnsCount = columns_count;
    table->ColumnsNames.Buf.resize(0);
    table->IsInitializing = false;
    table->IsLayoutLocked = false;
    table->InnerWidth = inner_width;
    table->OuterRect = outer_rect;
    table->WorkRect = outer_rect;

    // When not using a child window, WorkRect.Max will grow as we append contents.
    if (use_child_window)
    {
        // Ensure no vertical scrollbar appears if we only want horizontal one, to make flag consistent
        // (we have no other way to disable vertical scrollbar of a window while keeping the horizontal one showing)
        ImVec2 override_content_size(FLT_MAX, FLT_MAX);
        if ((flags & ImGuiTableFlags_ScrollX) && !(flags & ImGuiTableFlags_ScrollY))
            override_content_size.y = FLT_MIN;

        // Ensure specified width (when not specified, Stretched columns will act as if the width == OuterWidth and
        // never lead to any scrolling). We don't handle inner_width < 0.0f, we could potentially use it to right-align
        // based on the right side of the child window work rect, which would require knowing ahead if we are going to
        // have decoration taking horizontal spaces (typically a vertical scrollbar).
        if ((flags & ImGuiTableFlags_ScrollX) && inner_width > 0.0f)
            override_content_size.x = inner_width;

        if (override_content_size.x != FLT_MAX || override_content_size.y != FLT_MAX)
            SetNextWindowContentSize(ImVec2(override_content_size.x != FLT_MAX ? override_content_size.x : 0.0f, override_content_size.y != FLT_MAX ? override_content_size.y : 0.0f));

        // Create scrolling region (without border and zero window padding)
        ImGuiWindowFlags child_flags = (flags & ImGuiTableFlags_ScrollX) ? ImGuiWindowFlags_HorizontalScrollbar : ImGuiWindowFlags_None;
        BeginChildEx(name, instance_id, table->OuterRect.GetSize(), false, child_flags);
        table->InnerWindow = g.CurrentWindow;
        table->WorkRect = table->InnerWindow->WorkRect;
        table->OuterRect = table->InnerWindow->Rect();
        IM_ASSERT(table->InnerWindow->WindowPadding.x == 0.0f && table->InnerWindow->WindowPadding.y == 0.0f && table->InnerWindow->WindowBorderSize == 0.0f);
    }

    // Push a standardized ID for both child-using and not-child-using tables
    PushOverrideID(instance_id);

    // Backup a copy of host window members we will modify
    ImGuiWindow* inner_window = table->InnerWindow;
    table->HostIndentX = inner_window->DC.Indent.x;
    table->HostClipRect = inner_window->ClipRect;
    table->HostSkipItems = inner_window->SkipItems;
    table->HostBackupWorkRect = inner_window->WorkRect;
    table->HostBackupParentWorkRect = inner_window->ParentWorkRect;
    table->HostBackupColumnsOffset = outer_window->DC.ColumnsOffset;
    table->HostBackupCursorMaxPos = inner_window->DC.CursorMaxPos;
    inner_window->ParentWorkRect = table->WorkRect;

    // Padding and Spacing
    // - None               ........Content..... Pad .....Content........
    // - PadOuter           | Pad ..Content..... Pad .....Content.. Pad |
    // - PadInner           ........Content.. Pad | Pad ..Content........
    // - PadOuter+PadInner  | Pad ..Content.. Pad | Pad ..Content.. Pad |
    const bool pad_outer_x = (flags & ImGuiTableFlags_NoPadOuterX) ? false : (flags & ImGuiTableFlags_PadOuterX) ? true : (flags & ImGuiTableFlags_BordersOuterV) != 0;
    const bool pad_inner_x = (flags & ImGuiTableFlags_NoPadInnerX) ? false : true;
    const float inner_spacing_for_border = (flags & ImGuiTableFlags_BordersInnerV) ? TABLE_BORDER_SIZE : 0.0f;
    const float inner_spacing_explicit = (pad_inner_x && (flags & ImGuiTableFlags_BordersInnerV) == 0) ? g.Style.CellPadding.x : 0.0f;
    const float inner_padding_explicit = (pad_inner_x && (flags & ImGuiTableFlags_BordersInnerV) != 0) ? g.Style.CellPadding.x : 0.0f;
    const float inner_spacing = inner_spacing_for_border + inner_spacing_explicit;
    table->CellSpacingX1 = ImCeil(inner_spacing * 0.5f);
    table->CellSpacingX2 = inner_spacing - table->CellSpacingX1;
    table->CellPaddingX = inner_padding_explicit;
    table->CellPaddingY = g.Style.CellPadding.y;

    const float outer_padding_for_border = (flags & ImGuiTableFlags_BordersOuterV) ? TABLE_BORDER_SIZE : 0.0f;
    const float outer_padding_explicit = pad_outer_x ? g.Style.CellPadding.x : 0.0f;
    table->OuterPaddingX = (outer_padding_for_border + outer_padding_explicit) - table->CellPaddingX;

    table->CurrentColumn = -1;
    table->CurrentRow = -1;
    table->RowBgColorCounter = 0;
    table->LastRowFlags = ImGuiTableRowFlags_None;
    table->InnerClipRect = (inner_window == outer_window) ? table->WorkRect : inner_window->ClipRect;
    table->InnerClipRect.ClipWith(table->WorkRect);     // We need this to honor inner_width
    table->InnerClipRect.ClipWithFull(table->HostClipRect);
    table->InnerClipRect.Max.y = (flags & ImGuiTableFlags_NoHostExtendY) ? ImMin(table->InnerClipRect.Max.y, inner_window->WorkRect.Max.y) : inner_window->ClipRect.Max.y;

    // Initial draw cmd starts with a BgClipRect that matches the one of its host, to facilitate merge draw commands by default.
    // This is because all our cell highlight are manually clipped with BgClipRect
    // Larger at first, if/after unfreezing will become same as tight
    table->BgClipRect = table->InnerClipRect;
    table->BgClipRectForDrawCmd = table->HostClipRect;
    IM_ASSERT(table->BgClipRect.Min.y <= table->BgClipRect.Max.y);

    table->RowPosY1 = table->RowPosY2 = table->WorkRect.Min.y; // This is needed somehow
    table->RowTextBaseline = 0.0f; // This will be cleared again by TableBeginRow()
    table->FreezeRowsRequest = table->FreezeRowsCount = 0; // This will be setup by TableSetupScrollFreeze(), if any
    table->FreezeColumnsRequest = table->FreezeColumnsCount = 0;
    table->IsUnfrozen = true;
    table->DeclColumnsCount = 0;
    table->RightMostVisibleColumn = -1;

    // Using opaque colors facilitate overlapping elements of the grid
    table->BorderColorStrong = GetColorU32(ImGuiCol_TableBorderStrong);
    table->BorderColorLight = GetColorU32(ImGuiCol_TableBorderLight);
    table->BorderX1 = table->InnerClipRect.Min.x;// +((table->Flags & ImGuiTableFlags_BordersOuter) ? 0.0f : -1.0f);
    table->BorderX2 = table->InnerClipRect.Max.x;// +((table->Flags & ImGuiTableFlags_BordersOuter) ? 0.0f : +1.0f);

    // Make table current
    const int table_idx = g.Tables.GetIndex(table);
    g.CurrentTableStack.push_back(ImGuiPtrOrIndex(table_idx));
    g.CurrentTable = table;
    outer_window->DC.CurrentTableIdx = table_idx;
    if (inner_window != outer_window) // So EndChild() within the inner window can restore the table properly.
        inner_window->DC.CurrentTableIdx = table_idx;

    if ((table_last_flags & ImGuiTableFlags_Reorderable) && (flags & ImGuiTableFlags_Reorderable) == 0)
        table->IsResetDisplayOrderRequest = true;

    // Mark as used
    if (table_idx >= g.TablesLastTimeActive.Size)
        g.TablesLastTimeActive.resize(table_idx + 1, -1.0f);
    g.TablesLastTimeActive[table_idx] = (float)g.Time;
    table->MemoryCompacted = false;

    // Setup memory buffer (clear data if columns count changed)
    const int stored_size = table->Columns.size();
    if (stored_size != 0 && stored_size != columns_count)
    {
        IM_FREE(table->RawData);
        table->RawData = NULL;
    }
    if (table->RawData == NULL)
    {
        TableBeginInitMemory(table, columns_count);
        if (table_is_new)
            table->IsInitializing = true;
        table->IsSortSpecsDirty = table->IsSettingsRequestLoad = true;
        table->SettingsOffset = -1;
    }

    // Load settings
    if (table->IsSettingsRequestLoad)
        TableLoadSettings(table);

    // Handle DPI/font resize
    // This is designed to facilitate DPI changes with the assumption that e.g. style.CellPadding has been scaled as well.
    // It will also react to changing fonts with mixed results. It doesn't need to be perfect but merely provide a decent transition.
    // FIXME-DPI: Provide consistent standards for reference size. Perhaps using g.CurrentDpiScale would be more self explanatory.
    // This is will lead us to non-rounded WidthRequest in columns, which should work but is a poorly tested path.
    const float new_ref_scale_unit = g.FontSize; // g.Font->GetCharAdvance('A') ?
    if (table->RefScale != 0.0f && table->RefScale != new_ref_scale_unit)
    {
        const float scale_factor = new_ref_scale_unit / table->RefScale;
        //IMGUI_DEBUG_LOG("[table] %08X RefScaleUnit %.3f -> %.3f, scaling width by %.3f\n", table->ID, table->RefScaleUnit, new_ref_scale_unit, scale_factor);
        for (int n = 0; n < columns_count; n++)
            table->Columns[n].WidthRequest = table->Columns[n].WidthRequest * scale_factor;
    }
    table->RefScale = new_ref_scale_unit;

    // Disable output until user calls TableNextRow() or TableNextColumn() leading to the TableUpdateLayout() call..
    // This is not strictly necessary but will reduce cases were "out of table" output will be misleading to the user.
    // Because we cannot safely assert in EndTable() when no rows have been created, this seems like our best option.
    inner_window->SkipItems = true;

    // Update/lock which columns will be Visible for the frame
    TableBeginUpdateColumns(table);

    return true;
}

void ImGui::TableBeginUpdateColumns(ImGuiTable* table)
{
    // Handle resizing request
    // (We process this at the first TableBegin of the frame)
    // FIXME-TABLE: Preserve contents width _while resizing down_ until releasing.
    // FIXME-TABLE: Contains columns if our work area doesn't allow for scrolling.
    if (table->InstanceCurrent == 0)
    {
        if (table->ResizedColumn != -1 && table->ResizedColumnNextWidth != FLT_MAX)
            TableSetColumnWidth(table->ResizedColumn, table->ResizedColumnNextWidth);
        table->LastResizedColumn = table->ResizedColumn;
        table->ResizedColumnNextWidth = FLT_MAX;
        table->ResizedColumn = -1;
    }

    // Handle reordering request
    // Note: we don't clear ReorderColumn after handling the request.
    if (table->InstanceCurrent == 0)
    {
        if (table->HeldHeaderColumn == -1 && table->ReorderColumn != -1)
            table->ReorderColumn = -1;
        table->HeldHeaderColumn = -1;
        if (table->ReorderColumn != -1 && table->ReorderColumnDir != 0)
        {
            // We need to handle reordering across hidden columns.
            // In the configuration below, moving C to the right of E will lead to:
            //    ... C [D] E  --->  ... [D] E  C   (Column name/index)
            //    ... 2  3  4        ...  2  3  4   (Display order)
            const int reorder_dir = table->ReorderColumnDir;
            IM_ASSERT(reorder_dir == -1 || reorder_dir == +1);
            IM_ASSERT(table->Flags & ImGuiTableFlags_Reorderable);
            ImGuiTableColumn* src_column = &table->Columns[table->ReorderColumn];
            ImGuiTableColumn* dst_column = &table->Columns[(reorder_dir == -1) ? src_column->PrevVisibleColumn : src_column->NextVisibleColumn];
            IM_UNUSED(dst_column);
            const int src_order = src_column->DisplayOrder;
            const int dst_order = dst_column->DisplayOrder;
            src_column->DisplayOrder = (ImS8)dst_order;
            for (int order_n = src_order + reorder_dir; order_n != dst_order + reorder_dir; order_n += reorder_dir)
                table->Columns[table->DisplayOrderToIndex[order_n]].DisplayOrder -= (ImS8)reorder_dir;
            IM_ASSERT(dst_column->DisplayOrder == dst_order - reorder_dir);

            // Display order is stored in both columns->IndexDisplayOrder and table->DisplayOrder[],
            // rebuild the later from the former.
            for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
                table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = (ImS8)column_n;
            table->ReorderColumnDir = 0;
            table->IsSettingsDirty = true;
        }
    }

    // Handle display order reset request
    if (table->IsResetDisplayOrderRequest)
    {
        for (int n = 0; n < table->ColumnsCount; n++)
            table->DisplayOrderToIndex[n] = table->Columns[n].DisplayOrder = (ImS8)n;
        table->IsResetDisplayOrderRequest = false;
        table->IsSettingsDirty = true;
    }

    // Setup and lock Visible state and order
    table->ColumnsVisibleCount = 0;
    table->IsDefaultDisplayOrder = true;
    ImGuiTableColumn* last_visible_column = NULL;
    bool want_column_auto_fit = false;
    for (int order_n = 0; order_n < table->ColumnsCount; order_n++)
    {
        const int column_n = table->DisplayOrderToIndex[order_n];
        if (column_n != order_n)
            table->IsDefaultDisplayOrder = false;
        ImGuiTableColumn* column = &table->Columns[column_n];
        column->NameOffset = -1;
        if (!(table->Flags & ImGuiTableFlags_Hideable) || (column->Flags & ImGuiTableColumnFlags_NoHide))
            column->IsVisibleNextFrame = true;
        if (column->IsVisible != column->IsVisibleNextFrame)
        {
            column->IsVisible = column->IsVisibleNextFrame;
            table->IsSettingsDirty = true;
            if (!column->IsVisible && column->SortOrder != -1)
                table->IsSortSpecsDirty = true;
        }
        if (column->SortOrder > 0 && !(table->Flags & ImGuiTableFlags_MultiSortable))
            table->IsSortSpecsDirty = true;
        if (column->AutoFitQueue != 0x00)
            want_column_auto_fit = true;

        ImU64 index_mask = (ImU64)1 << column_n;
        ImU64 display_order_mask = (ImU64)1 << column->DisplayOrder;
        if (column->IsVisible)
        {
            column->PrevVisibleColumn = column->NextVisibleColumn = -1;
            if (last_visible_column)
            {
                last_visible_column->NextVisibleColumn = (ImS8)column_n;
                column->PrevVisibleColumn = (ImS8)table->Columns.index_from_ptr(last_visible_column);
            }
            column->IndexWithinVisibleSet = (ImS8)table->ColumnsVisibleCount;
            table->ColumnsVisibleCount++;
            table->VisibleMaskByIndex |= index_mask;
            table->VisibleMaskByDisplayOrder |= display_order_mask;
            last_visible_column = column;
        }
        else
        {
            column->IndexWithinVisibleSet = -1;
            table->VisibleMaskByIndex &= ~index_mask;
            table->VisibleMaskByDisplayOrder &= ~display_order_mask;
        }
        IM_ASSERT(column->IndexWithinVisibleSet <= column->DisplayOrder);
    }
    table->VisibleUnclippedMaskByIndex = table->VisibleMaskByIndex; // Columns will be masked out by TableUpdateLayout() when Clipped
    table->RightMostVisibleColumn = (ImS8)(last_visible_column ? table->Columns.index_from_ptr(last_visible_column) : -1);

    // Disable child window clipping while fitting columns. This is not strictly necessary but makes it possible to avoid
    // the column fitting to wait until the first visible frame of the child container (may or not be a good thing).
    if (want_column_auto_fit && table->OuterWindow != table->InnerWindow)
        table->InnerWindow->SkipItems = false;

    if (want_column_auto_fit)
        table->IsSettingsDirty = true;
}

// Adjust flags: default width mode + stretch columns are not allowed when auto extending
static ImGuiTableColumnFlags TableFixColumnFlags(ImGuiTable* table, ImGuiTableColumnFlags flags)
{
    // Sizing Policy
    if ((flags & ImGuiTableColumnFlags_WidthMask_) == 0)
    {
        // FIXME-TABLE: Inconsistent to promote columns to WidthAlwaysAutoResize
        if (table->Flags & ImGuiTableFlags_SizingPolicyFixedX)
            flags |= ((table->Flags & ImGuiTableFlags_Resizable) && !(flags & ImGuiTableColumnFlags_NoResize)) ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthAlwaysAutoResize;
        else
            flags |= ImGuiTableColumnFlags_WidthStretch;
    }
    IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiTableColumnFlags_WidthMask_)); // Check that only 1 of each set is used.
    if ((flags & ImGuiTableColumnFlags_WidthAlwaysAutoResize))// || ((flags & ImGuiTableColumnFlags_WidthStretch) && (table->Flags & ImGuiTableFlags_SizingPolicyStretchX)))
        flags |= ImGuiTableColumnFlags_NoResize;
    //if ((flags & ImGuiTableColumnFlags_WidthStretch) && (table->Flags & ImGuiTableFlags_SizingPolicyFixedX))
    //    flags = (flags & ~ImGuiTableColumnFlags_WidthMask_) | ImGuiTableColumnFlags_WidthFixed;

    // Sorting
    if ((flags & ImGuiTableColumnFlags_NoSortAscending) && (flags & ImGuiTableColumnFlags_NoSortDescending))
        flags |= ImGuiTableColumnFlags_NoSort;

    // Alignment
    //if ((flags & ImGuiTableColumnFlags_AlignMask_) == 0)
    //    flags |= ImGuiTableColumnFlags_AlignCenter;
    //IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiTableColumnFlags_AlignMask_)); // Check that only 1 of each set is used.

    return flags;
}

static void TableFixColumnSortDirection(ImGuiTableColumn* column)
{
    // Initial sort state
    if (column->SortDirection == ImGuiSortDirection_None)
        column->SortDirection = (column->Flags & ImGuiTableColumnFlags_PreferSortDescending) ? (ImS8)ImGuiSortDirection_Descending : (ImS8)(ImGuiSortDirection_Ascending);

    // Handle NoSortAscending/NoSortDescending
    if (column->SortDirection == ImGuiSortDirection_Ascending && (column->Flags & ImGuiTableColumnFlags_NoSortAscending))
        column->SortDirection = ImGuiSortDirection_Descending;
    else if (column->SortDirection == ImGuiSortDirection_Descending && (column->Flags & ImGuiTableColumnFlags_NoSortDescending))
        column->SortDirection = ImGuiSortDirection_Ascending;
}

// Minimum column content width (without padding)
static float TableGetMinColumnWidth()
{
    ImGuiContext& g = *GImGui;
    //return g.Style.ColumnsMinSpacing; // FIXME-TABLE
    return g.Style.FramePadding.x * 1.0f;
}

// Layout columns for the frame
// Runs on the first call to TableNextRow(), to give a chance for TableSetupColumn() to be called first.
// FIXME-TABLE: Our width (and therefore our WorkRect) will be minimal in the first frame for WidthAlwaysAutoResize
// columns, increase feedback side-effect with widgets relying on WorkRect.Max.x. Maybe provide a default distribution
// for WidthAlwaysAutoResize columns?
void    ImGui::TableUpdateLayout(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    IM_ASSERT(table->IsLayoutLocked == false);

    table->HoveredColumnBody = -1;
    table->HoveredColumnBorder = -1;

    // Compute offset, clip rect for the frame
    // (can't make auto padding larger than what WorkRect knows about so right-alignment matches)
    const ImRect work_rect = table->WorkRect;
    const float min_column_width = TableGetMinColumnWidth();
    const float min_column_distance = min_column_width + table->CellPaddingX * 2.0f;

    int count_fixed = 0;
    float sum_weights_stretched = 0.0f;     // Sum of all weights for weighted columns.
    float sum_width_fixed_requests = 0.0f;  // Sum of all width for fixed and auto-resize columns, excluding width contributed by Stretch columns.
    table->LeftMostStretchedColumnDisplayOrder = -1;
    for (int order_n = 0; order_n < table->ColumnsCount; order_n++)
    {
        if (!(table->VisibleMaskByDisplayOrder & ((ImU64)1 << order_n)))
            continue;
        const int column_n = table->DisplayOrderToIndex[order_n];
        ImGuiTableColumn* column = &table->Columns[column_n];

        // Adjust flags: default width mode + weighted columns are not allowed when auto extending
        // FIXME-TABLE: Clarify why we need to do this again here and not just in TableSetupColumn()
        column->Flags = TableFixColumnFlags(table, column->FlagsIn);
        if ((column->Flags & ImGuiTableColumnFlags_IndentMask_) == 0)
            column->Flags |= (column_n == 0) ? ImGuiTableColumnFlags_IndentEnable : ImGuiTableColumnFlags_IndentDisable;

        // We have a unusual edge case where if the user doesn't call TableGetSortSpecs() but has sorting enabled
        // or varying sorting flags, we still want the sorting arrows to honor those flags.
        if (table->Flags & ImGuiTableFlags_Sortable)
            TableFixColumnSortDirection(column);

        // Calculate "ideal" column width for nothing to be clipped.
        // Combine width from regular rows + width from headers unless requested not to.
        const float content_width_body = (float)ImMax(column->ContentMaxXFrozen, column->ContentMaxXUnfrozen) - column->WorkMinX;
        const float content_width_headers = (float)column->ContentMaxXHeadersIdeal - column->WorkMinX;
        float width_auto = content_width_body;
        if (!(table->Flags & ImGuiTableFlags_NoHeadersWidth) && !(column->Flags & ImGuiTableColumnFlags_NoHeaderWidth))
            width_auto = ImMax(width_auto, content_width_headers);
        width_auto = ImMax(width_auto, min_column_width);

        // Non-resizable columns also submit their requested width
        if ((column->Flags & ImGuiTableColumnFlags_WidthFixed) && column->InitStretchWeightOrWidth > 0.0f)
            if (!(table->Flags & ImGuiTableFlags_Resizable) || !(column->Flags & ImGuiTableColumnFlags_NoResize))
                width_auto = ImMax(width_auto, column->InitStretchWeightOrWidth);

        column->WidthAuto = width_auto;

        if (column->Flags & (ImGuiTableColumnFlags_WidthAlwaysAutoResize | ImGuiTableColumnFlags_WidthFixed))
        {
            // Latch initial size for fixed columns and update it constantly for auto-resizing column (unless clipped!)
            if ((column->AutoFitQueue != 0x00) || ((column->Flags & ImGuiTableColumnFlags_WidthAlwaysAutoResize) && !column->IsClipped))
                column->WidthRequest = width_auto;

            // FIXME-TABLE: Increase minimum size during init frame to avoid biasing auto-fitting widgets
            // (e.g. TextWrapped) too much. Otherwise what tends to happen is that TextWrapped would output a very
            // large height (= first frame scrollbar display very off + clipper would skip lots of items).
            // This is merely making the side-effect less extreme, but doesn't properly fixes it.
            // FIXME: Move this to ->WidthGiven to avoid temporary lossyless?
            if (column->AutoFitQueue > 0x01 && table->IsInitializing)
                column->WidthRequest = ImMax(column->WidthRequest, min_column_width * 4.0f);

            count_fixed += 1;
            sum_width_fixed_requests += column->WidthRequest;
        }
        else
        {
            IM_ASSERT(column->Flags & ImGuiTableColumnFlags_WidthStretch);
            const int init_size = (column->StretchWeight < 0.0f);
            if (init_size)
                column->StretchWeight = 1.0f;
            sum_weights_stretched += column->StretchWeight;
            if (table->LeftMostStretchedColumnDisplayOrder == -1)
                table->LeftMostStretchedColumnDisplayOrder = (ImS8)column->DisplayOrder;
        }
        sum_width_fixed_requests += table->CellPaddingX * 2.0f;
    }

    // Layout
    const float width_spacings = (table->OuterPaddingX * 2.0f) + (table->CellSpacingX1 + table->CellSpacingX2) * (table->ColumnsVisibleCount - 1);
    float width_avail;
    if ((table->Flags & ImGuiTableFlags_ScrollX) && table->InnerWidth == 0.0f)
        width_avail = table->InnerClipRect.GetWidth() - width_spacings;
    else
        width_avail = work_rect.GetWidth() - width_spacings;
    const float width_avail_for_stretched_columns = width_avail - sum_width_fixed_requests;
    float width_remaining_for_stretched_columns = width_avail_for_stretched_columns;

    // Apply final width based on requested widths
    // Mark some columns as not resizable
    int count_resizable = 0;
    table->ColumnsTotalWidth = width_spacings;
    table->ColumnsAutoFitWidth = width_spacings;
    for (int order_n = 0; order_n < table->ColumnsCount; order_n++)
    {
        if (!(table->VisibleMaskByDisplayOrder & ((ImU64)1 << order_n)))
            continue;
        ImGuiTableColumn* column = &table->Columns[table->DisplayOrderToIndex[order_n]];

        // Allocate width for stretched/weighted columns
        if (column->Flags & ImGuiTableColumnFlags_WidthStretch)
        {
            // StretchWeight gets converted into WidthRequest
            float weight_ratio = column->StretchWeight / sum_weights_stretched;
            column->WidthRequest = IM_FLOOR(ImMax(width_avail_for_stretched_columns * weight_ratio, min_column_width) + 0.01f);
            width_remaining_for_stretched_columns -= column->WidthRequest;

            // [Resize Rule 2] Resizing from right-side of a stretch column preceding a fixed column
            // needs to forward resizing to left-side of fixed column. We also need to copy the NoResize flag..
            if (column->NextVisibleColumn != -1)
                if (ImGuiTableColumn* next_column = &table->Columns[column->NextVisibleColumn])
                    if (next_column->Flags & ImGuiTableColumnFlags_WidthFixed)
                        column->Flags |= (next_column->Flags & ImGuiTableColumnFlags_NoDirectResize_);
        }

        // [Resize Rule 1] The right-most Visible column is not resizable if there is at least one Stretch column
        // (see comments in TableResizeColumn())
        if (column->NextVisibleColumn == -1 && table->LeftMostStretchedColumnDisplayOrder != -1)
            column->Flags |= ImGuiTableColumnFlags_NoDirectResize_;

        if (!(column->Flags & ImGuiTableColumnFlags_NoResize))
            count_resizable++;

        // Assign final width, record width in case we will need to shrink
        column->WidthGiven = ImFloor(ImMax(column->WidthRequest, min_column_width));
        table->ColumnsTotalWidth += column->WidthGiven + table->CellPaddingX * 2.0f;
        table->ColumnsAutoFitWidth += column->WidthAuto + table->CellPaddingX * 2.0f;
    }

    // Redistribute remainder width due to rounding (remainder width is < 1.0f * number of Stretch column).
    // Using right-to-left distribution (more likely to match resizing cursor), could be adjusted depending
    // on where the mouse cursor is and/or relative weights.
    if (width_remaining_for_stretched_columns >= 1.0f && !(table->Flags & ImGuiTableFlags_PreciseStretchWidths))
        for (int order_n = table->ColumnsCount - 1; sum_weights_stretched > 0.0f && width_remaining_for_stretched_columns >= 1.0f && order_n >= 0; order_n--)
        {
            if (!(table->VisibleMaskByDisplayOrder & ((ImU64)1 << order_n)))
                continue;
            ImGuiTableColumn* column = &table->Columns[table->DisplayOrderToIndex[order_n]];
            if (!(column->Flags & ImGuiTableColumnFlags_WidthStretch))
                continue;
            column->WidthRequest += 1.0f;
            column->WidthGiven += 1.0f;
            width_remaining_for_stretched_columns -= 1.0f;
        }

    // Detect hovered column
    const ImRect mouse_hit_rect(table->OuterRect.Min.x, table->OuterRect.Min.y, table->OuterRect.Max.x, ImMax(table->OuterRect.Max.y, table->OuterRect.Min.y + table->LastOuterHeight));
    const bool is_hovering_table = ItemHoverable(mouse_hit_rect, 0);

    // Setup final position, offset and clipping rectangles
    int visible_n = 0;
    float offset_x = (table->FreezeColumnsCount > 0) ? table->OuterRect.Min.x : work_rect.Min.x;
    offset_x += table->OuterPaddingX;
    offset_x -= table->CellSpacingX1;
    ImRect host_clip_rect = table->InnerClipRect;
    for (int order_n = 0; order_n < table->ColumnsCount; order_n++)
    {
        const int column_n = table->DisplayOrderToIndex[order_n];
        ImGuiTableColumn* column = &table->Columns[column_n];

        column->NavLayerCurrent = (ImS8)((table->FreezeRowsCount > 0 || column_n < table->FreezeColumnsCount) ? ImGuiNavLayer_Menu : ImGuiNavLayer_Main);

        if (table->FreezeColumnsCount > 0 && table->FreezeColumnsCount == visible_n)
            offset_x += work_rect.Min.x - table->OuterRect.Min.x;

        if ((table->VisibleMaskByDisplayOrder & ((ImU64)1 << order_n)) == 0)
        {
            // Hidden column: clear a few fields and we are done with it for the remainder of the function.
            // We set a zero-width clip rect but set Min.y/Max.y properly to not interfere with the clipper.
            column->MinX = column->MaxX = column->WorkMinX = column->ClipRect.Min.x = column->ClipRect.Max.x = offset_x;
            column->WidthGiven = 0.0f;
            column->ClipRect.Min.y = work_rect.Min.y;
            column->ClipRect.Max.y = FLT_MAX;
            column->ClipRect.ClipWithFull(host_clip_rect);
            column->IsClipped = column->IsSkipItems = true;
            continue;
        }

        float max_x = FLT_MAX;
        if (table->Flags & ImGuiTableFlags_ScrollX)
        {
            // Frozen columns can't reach beyond visible width else scrolling will naturally break.
            if (order_n < table->FreezeColumnsRequest)
                max_x = table->InnerClipRect.Max.x - (table->FreezeColumnsRequest - order_n) * min_column_distance - table->OuterPaddingX;
        }
        else if ((table->Flags & ImGuiTableFlags_NoKeepColumnsVisible) == 0)
        {
            // If horizontal scrolling if disabled, we apply a final lossless shrinking of columns in order to make
            // sure they are all visible. Because of this we also know that all of the columns will always fit in
            // table->WorkRect and therefore in table->InnerRect (because ScrollX is off)
            max_x = table->WorkRect.Max.x - (table->ColumnsVisibleCount - (column->IndexWithinVisibleSet + 1)) * min_column_distance - table->OuterPaddingX;
        }
        if (offset_x + column->WidthGiven + table->CellPaddingX * 2.0f + table->CellSpacingX1 > max_x)
            column->WidthGiven = ImMax(max_x - offset_x - table->CellPaddingX * 2.0f - table->CellSpacingX1, min_column_width);

        // Lock all our positions
        column->MinX = offset_x;
        column->MaxX = offset_x + column->WidthGiven + table->CellSpacingX1 + table->CellSpacingX2 + table->CellPaddingX * 2.0f;
        column->WorkMinX = column->MinX + table->CellPaddingX + table->CellSpacingX1;
        column->WorkMaxX = column->MaxX - table->CellPaddingX - table->CellSpacingX2; // Expected max
        column->ClipRect.Min.x = column->MinX;
        column->ClipRect.Min.y = work_rect.Min.y;
        column->ClipRect.Max.x = column->MaxX;
        column->ClipRect.Max.y = FLT_MAX;
        column->ClipRect.ClipWithFull(host_clip_rect);

        column->IsClipped = (column->ClipRect.Max.x <= column->ClipRect.Min.x) && (column->AutoFitQueue & 1) == 0 && (column->CannotSkipItemsQueue & 1) == 0;
        if (column->IsClipped)
            table->VisibleUnclippedMaskByIndex &= ~((ImU64)1 << column_n);  // Columns with the _WidthAlwaysAutoResize sizing policy will never be updated then.

        column->IsSkipItems = !column->IsVisible || table->HostSkipItems;

        // Detect hovered column
        if (is_hovering_table && g.IO.MousePos.x >= column->ClipRect.Min.x && g.IO.MousePos.x < column->ClipRect.Max.x)
            table->HoveredColumnBody = (ImS8)column_n;

        // Alignment
        // FIXME-TABLE: This align based on the whole column width, not per-cell, and therefore isn't useful in
        // many cases (to be able to honor this we might be able to store a log of cells width, per row, for
        // visible rows, but nav/programmatic scroll would have visible artifacts.)
        //if (column->Flags & ImGuiTableColumnFlags_AlignRight)
        //    column->WorkMinX = ImMax(column->WorkMinX, column->MaxX - column->ContentWidthRowsUnfrozen);
        //else if (column->Flags & ImGuiTableColumnFlags_AlignCenter)
        //    column->WorkMinX = ImLerp(column->WorkMinX, ImMax(column->StartX, column->MaxX - column->ContentWidthRowsUnfrozen), 0.5f);

        // Reset content width variables
        column->ContentMaxXFrozen = column->ContentMaxXUnfrozen = column->WorkMinX;
        column->ContentMaxXHeadersUsed = column->ContentMaxXHeadersIdeal = column->WorkMinX;

        // Don't decrement auto-fit counters until container window got a chance to submit its items
        if (table->HostSkipItems == false)
        {
            column->AutoFitQueue >>= 1;
            column->CannotSkipItemsQueue >>= 1;
        }

        if (visible_n < table->FreezeColumnsCount)
            host_clip_rect.Min.x = ImMax(host_clip_rect.Min.x, column->MaxX + 2.0f);

        offset_x += column->WidthGiven + table->CellSpacingX1 + table->CellSpacingX2 + table->CellPaddingX * 2.0f;
        visible_n++;
    }

    // Detect/store when we are hovering the unused space after the right-most column (so e.g. context menus can react on it)
    if (is_hovering_table && table->HoveredColumnBody == -1)
    {
        float unused_x1 = table->WorkRect.Min.x;
        if (table->RightMostVisibleColumn != -1)
            unused_x1 = ImMax(unused_x1, table->Columns[table->RightMostVisibleColumn].ClipRect.Max.x);
        if (g.IO.MousePos.x >= unused_x1)
            table->HoveredColumnBody = (ImS8)table->ColumnsCount;
    }

    // Clear Resizable flag if none of our column are actually resizable (either via an explicit _NoResize flag,
    // either because of using _WidthAlwaysAutoResize/_WidthStretch).
    // This will hide the resizing option from the context menu.
    if (count_resizable == 0 && (table->Flags & ImGuiTableFlags_Resizable))
        table->Flags &= ~ImGuiTableFlags_Resizable;

    // Allocate draw channels
    TableUpdateDrawChannels(table);

    // Borders
    if (table->Flags & ImGuiTableFlags_Resizable)
        TableUpdateBorders(table);

    // Reset fields after we used them in TableSetupResize()
    table->LastFirstRowHeight = 0.0f;
    table->IsLayoutLocked = true;
    table->IsUsingHeaders = false;

    // Context menu
    if (table->IsContextPopupOpen && table->InstanceCurrent == table->InstanceInteracted)
    {
        const ImGuiID context_menu_id = ImHashStr("##ContextMenu", 0, table->ID);
        if (BeginPopupEx(context_menu_id, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings))
        {
            TableDrawContextMenu(table);
            EndPopup();
        }
        else
        {
            table->IsContextPopupOpen = false;
        }
    }

    // Initial state
    ImGuiWindow* inner_window = table->InnerWindow;
    if (table->Flags & ImGuiTableFlags_NoClip)
        table->DrawSplitter.SetCurrentChannel(inner_window->DrawList, TABLE_DRAW_CHANNEL_UNCLIPPED);
    else
        inner_window->DrawList->PushClipRect(inner_window->ClipRect.Min, inner_window->ClipRect.Max, false);

    // Sanitize and build sort specs before we have a change to use them for display.
    // This path will only be exercised when sort specs are modified before header rows (e.g. init or visibility change)
    if (table->IsSortSpecsDirty && (table->Flags & ImGuiTableFlags_Sortable))
        TableSortSpecsBuild(table);
}

// Process interaction on resizing borders. Actual size change will be applied in EndTable()
// - Set table->HoveredColumnBorder with a short delay/timer to reduce feedback noise
// - Submit ahead of table contents and header, use ImGuiButtonFlags_AllowItemOverlap to prioritize widgets
//   overlapping the same area.
void    ImGui::TableUpdateBorders(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    IM_ASSERT(table->Flags & ImGuiTableFlags_Resizable);

    // At this point OuterRect height may be zero or under actual final height, so we rely on temporal coherency and
    // use the final height from last frame. Because this is only affecting _interaction_ with columns, it is not
    // really problematic (whereas the actual visual will be displayed in EndTable() and using the current frame height).
    // Actual columns highlight/render will be performed in EndTable() and not be affected.
    const float hit_half_width = TABLE_RESIZE_SEPARATOR_HALF_THICKNESS;
    const float hit_y1 = table->OuterRect.Min.y;
    const float hit_y2_body = ImMax(table->OuterRect.Max.y, hit_y1 + table->LastOuterHeight);
    const float hit_y2_head = hit_y1 + table->LastFirstRowHeight;

    for (int order_n = 0; order_n < table->ColumnsCount; order_n++)
    {
        if (!(table->VisibleMaskByDisplayOrder & ((ImU64)1 << order_n)))
            continue;

        const int column_n = table->DisplayOrderToIndex[order_n];
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (column->Flags & (ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoDirectResize_))
            continue;

        // ImGuiTableFlags_NoBordersInBodyUntilResize will be honored in TableDrawBorders()
        const float border_y2_hit = (table->Flags & ImGuiTableFlags_NoBordersInBody) ? hit_y2_head : hit_y2_body;
        if ((table->Flags & ImGuiTableFlags_NoBordersInBody) && table->IsUsingHeaders == false)
            continue;

        ImGuiID column_id = TableGetColumnResizeID(table, column_n, table->InstanceCurrent);
        ImRect hit_rect(column->MaxX - hit_half_width, hit_y1, column->MaxX + hit_half_width, border_y2_hit);
        //GetForegroundDrawList()->AddRect(hit_rect.Min, hit_rect.Max, IM_COL32(255, 0, 0, 100));
        KeepAliveID(column_id);

        bool hovered = false, held = false;
        bool pressed = ButtonBehavior(hit_rect, column_id, &hovered, &held, ImGuiButtonFlags_FlattenChildren | ImGuiButtonFlags_AllowItemOverlap | ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_PressedOnDoubleClick);
        if (pressed && IsMouseDoubleClicked(0) && !(column->Flags & ImGuiTableColumnFlags_WidthStretch))
        {
            // FIXME-TABLE: Double-clicking on column edge could auto-fit Stretch column?
            TableSetColumnAutofit(table, column_n);
            ClearActiveID();
            held = hovered = false;
        }
        if (held)
        {
            table->ResizedColumn = (ImS8)column_n;
            table->InstanceInteracted = table->InstanceCurrent;
        }
        if ((hovered && g.HoveredIdTimer > TABLE_RESIZE_SEPARATOR_FEEDBACK_TIMER) || held)
        {
            table->HoveredColumnBorder = (ImS8)column_n;
            SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
    }
}

void    ImGui::EndTable()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Only call EndTable() if BeginTable() returns true!");

    // This assert would be very useful to catch a common error... unfortunately it would probably trigger in some
    // cases, and for consistency user may sometimes output empty tables (and still benefit from e.g. outer border)
    //IM_ASSERT(table->IsLayoutLocked && "Table unused: never called TableNextRow(), is that the intent?");

    // If the user never got to call TableNextRow() or TableNextColumn(), we call layout ourselves to ensure all our
    // code paths are consistent (instead of just hoping that TableBegin/TableEnd will work), get borders drawn, etc.
    if (!table->IsLayoutLocked)
        TableUpdateLayout(table);

    const ImGuiTableFlags flags = table->Flags;
    ImGuiWindow* inner_window = table->InnerWindow;
    ImGuiWindow* outer_window = table->OuterWindow;
    IM_ASSERT(inner_window == g.CurrentWindow);
    IM_ASSERT(outer_window == inner_window || outer_window == inner_window->ParentWindow);

    if (table->IsInsideRow)
        TableEndRow(table);

    // Context menu in columns body
    if (flags & ImGuiTableFlags_ContextMenuInBody)
        if (table->HoveredColumnBody != -1 && !ImGui::IsAnyItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            TableOpenContextMenu((int)table->HoveredColumnBody);

    // Finalize table height
    inner_window->DC.CursorMaxPos = table->HostBackupCursorMaxPos;
    if (inner_window != outer_window)
    {
        table->OuterRect.Max.y = ImMax(table->OuterRect.Max.y, inner_window->Pos.y + inner_window->Size.y);
        inner_window->DC.CursorMaxPos.y = table->RowPosY2;
    }
    else if (!(flags & ImGuiTableFlags_NoHostExtendY))
    {
        table->OuterRect.Max.y = ImMax(table->OuterRect.Max.y, inner_window->DC.CursorPos.y);
        inner_window->DC.CursorMaxPos.y = table->RowPosY2;
    }
    table->WorkRect.Max.y = ImMax(table->WorkRect.Max.y, table->OuterRect.Max.y);
    table->LastOuterHeight = table->OuterRect.GetHeight();

    if (!(flags & ImGuiTableFlags_NoClip))
        inner_window->DrawList->PopClipRect();
    inner_window->ClipRect = inner_window->DrawList->_ClipRectStack.back();

    // Draw borders
    if ((flags & ImGuiTableFlags_Borders) != 0)
        TableDrawBorders(table);
    table->DrawSplitter.SetCurrentChannel(inner_window->DrawList, 0);

    // Store content width reference for each column (before attempting to merge draw calls)
    const float backup_outer_cursor_pos_x = outer_window->DC.CursorPos.x;
    const float backup_outer_max_pos_x = outer_window->DC.CursorMaxPos.x;
    const float backup_inner_max_pos_x = inner_window->DC.CursorMaxPos.x;
    float max_pos_x = backup_inner_max_pos_x;
    if (table->RightMostVisibleColumn != -1)
        max_pos_x = ImMax(max_pos_x, table->Columns[table->RightMostVisibleColumn].MaxX);

#if 0
    // Strip out dummy channel draw calls
    // We have no way to prevent user submitting direct ImDrawList calls into a hidden column (but ImGui:: calls will be clipped out)
    // FIXME-TABLE: The problem with this approach is we are effectively making it harder for users watching metrics to spot wasted vertices.
    if (table->DummyDrawChannel != (ImU8)-1)
    {
        ImDrawChannel* dummy_channel = &table->DrawSplitter._Channels[table->DummyDrawChannel];
        dummy_channel->_CmdBuffer.resize(0);
        dummy_channel->_IdxBuffer.resize(0);
    }
#endif

    // Flatten channels and merge draw calls
    if ((table->Flags & ImGuiTableFlags_NoClip) == 0)
        TableReorderDrawChannelsForMerge(table);
    table->DrawSplitter.Merge(inner_window->DrawList);

    if (!(table->Flags & ImGuiTableFlags_ScrollX) && inner_window != outer_window)
    {
        inner_window->Scroll.x = 0.0f;
    }
    else if (table->LastResizedColumn != -1 && table->ResizedColumn == -1 && inner_window->ScrollbarX && table->InstanceInteracted == table->InstanceCurrent)
    {
        // When releasing a column being resized, scroll to keep the resulting column in sight
        const float neighbor_width_to_keep_visible = TableGetMinColumnWidth() + table->CellPaddingX * 2.0f;
        ImGuiTableColumn* column = &table->Columns[table->LastResizedColumn];
        if (column->MaxX < table->InnerClipRect.Min.x)
            SetScrollFromPosX(inner_window, column->MaxX - inner_window->Pos.x - neighbor_width_to_keep_visible, 1.0f);
        else if (column->MaxX > table->InnerClipRect.Max.x)
            SetScrollFromPosX(inner_window, column->MaxX - inner_window->Pos.x + neighbor_width_to_keep_visible, 1.0f);
    }

    // Apply resizing/dragging at the end of the frame
    if (table->ResizedColumn != -1)
    {
        ImGuiTableColumn* column = &table->Columns[table->ResizedColumn];
        const float new_x2 = (g.IO.MousePos.x - g.ActiveIdClickOffset.x + TABLE_RESIZE_SEPARATOR_HALF_THICKNESS);
        const float new_width = ImFloor(new_x2 - column->MinX - table->CellSpacingX1 - table->CellPaddingX * 2.0f);
        table->ResizedColumnNextWidth = new_width;
    }

    // Layout in outer window
    IM_ASSERT_USER_ERROR(inner_window->IDStack.back() == table->ID + table->InstanceCurrent, "Mismatching PushID/PopID!");
    PopID();
    inner_window->WorkRect = table->HostBackupWorkRect;
    inner_window->ParentWorkRect = table->HostBackupParentWorkRect;
    inner_window->SkipItems = table->HostSkipItems;
    outer_window->DC.CursorPos = table->OuterRect.Min;
    outer_window->DC.ColumnsOffset = table->HostBackupColumnsOffset;
    if (inner_window != outer_window)
    {
        EndChild();
    }
    else
    {
        ImVec2 item_size = table->OuterRect.GetSize();
        item_size.x = table->ColumnsTotalWidth;
        ItemSize(item_size);
    }

    // Override EndChild/ItemSize max extent with our own to enable auto-resize on the X axis when possible
    // FIXME-TABLE: This can be improved (e.g. for Fixed columns we don't want to auto AutoFitWidth? or propagate window auto-fit to table?)
    if (table->Flags & ImGuiTableFlags_ScrollX)
    {
        inner_window->DC.CursorMaxPos.x = max_pos_x; // Set contents width for scrolling
        outer_window->DC.CursorMaxPos.x = ImMax(backup_outer_max_pos_x, backup_outer_cursor_pos_x + table->ColumnsTotalWidth + inner_window->ScrollbarSizes.x); // For auto-fit
    }
    else
    {
        outer_window->DC.CursorMaxPos.x = ImMax(backup_outer_max_pos_x, table->WorkRect.Min.x + table->ColumnsAutoFitWidth); // For auto-fit
    }

    // Save settings
    if (table->IsSettingsDirty)
        TableSaveSettings(table);

    // Clear or restore current table, if any
    IM_ASSERT(g.CurrentWindow == outer_window && g.CurrentTable == table);
    g.CurrentTableStack.pop_back();
    g.CurrentTable = g.CurrentTableStack.Size ? g.Tables.GetByIndex(g.CurrentTableStack.back().Index) : NULL;
    outer_window->DC.CurrentTableIdx = g.CurrentTable ? g.Tables.GetIndex(g.CurrentTable) : -1;
}

// FIXME-TABLE: This is a mess, need to redesign how we render borders.
void ImGui::TableDrawBorders(ImGuiTable* table)
{
    ImGuiWindow* inner_window = table->InnerWindow;
    ImGuiWindow* outer_window = table->OuterWindow;
    table->DrawSplitter.SetCurrentChannel(inner_window->DrawList, TABLE_DRAW_CHANNEL_BG0);
    if (inner_window->Hidden || !table->HostClipRect.Overlaps(table->InnerClipRect))
        return;
    ImDrawList* inner_drawlist = inner_window->DrawList;
    ImDrawList* outer_drawlist = outer_window->DrawList;

    // Draw inner border and resizing feedback
    const float border_size = TABLE_BORDER_SIZE;
    const float draw_y1 = table->OuterRect.Min.y;
    const float draw_y2_body = table->OuterRect.Max.y;
    const float draw_y2_head = table->IsUsingHeaders ? ((table->FreezeRowsCount >= 1 ? table->OuterRect.Min.y : table->WorkRect.Min.y) + table->LastFirstRowHeight) : draw_y1;

    if (table->Flags & ImGuiTableFlags_BordersInnerV)
    {
        for (int order_n = 0; order_n < table->ColumnsCount; order_n++)
        {
            if (!(table->VisibleMaskByDisplayOrder & ((ImU64)1 << order_n)))
                continue;

            const int column_n = table->DisplayOrderToIndex[order_n];
            ImGuiTableColumn* column = &table->Columns[column_n];
            const bool is_hovered = (table->HoveredColumnBorder == column_n);
            const bool is_resized = (table->ResizedColumn == column_n) && (table->InstanceInteracted == table->InstanceCurrent);
            const bool is_resizable = (column->Flags & (ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoDirectResize_)) == 0;

            if (column->MaxX > table->InnerClipRect.Max.x && !is_resized)// && is_hovered)
                continue;
            if (column->NextVisibleColumn == -1 && !is_resizable)
                continue;
            if (column->MaxX <= column->ClipRect.Min.x) // FIXME-TABLE FIXME-STYLE: Assume BorderSize==1, this is problematic if we want to increase the border size..
                continue;

            // Draw in outer window so right-most column won't be clipped
            // Always draw full height border when being resized/hovered, or on the delimitation of frozen column scrolling.
            ImU32 col;
            float draw_y2;
            if (is_hovered || is_resized || (table->FreezeColumnsCount != -1 && table->FreezeColumnsCount == order_n + 1))
            {
                draw_y2 = draw_y2_body;
                col = is_resized ? GetColorU32(ImGuiCol_SeparatorActive) : is_hovered ? GetColorU32(ImGuiCol_SeparatorHovered) : table->BorderColorStrong;
            }
            else
            {
                draw_y2 = (table->Flags & (ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoBordersInBodyUntilResize)) ? draw_y2_head : draw_y2_body;
                col = (table->Flags & (ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_NoBordersInBodyUntilResize)) ? table->BorderColorStrong : table->BorderColorLight;
            }

            if (draw_y2 > draw_y1)
                inner_drawlist->AddLine(ImVec2(column->MaxX, draw_y1), ImVec2(column->MaxX, draw_y2), col, border_size);
        }
    }

    // Draw outer border
    if (table->Flags & ImGuiTableFlags_BordersOuter)
    {
        // Display outer border offset by 1 which is a simple way to display it without adding an extra draw call
        // (Without the offset, in outer_window it would be rendered behind cells, because child windows are above their
        // parent. In inner_window, it won't reach out over scrollbars. Another weird solution would be to display part
        // of it in inner window, and the part that's over scrollbars in the outer window..)
        // Either solution currently won't allow us to use a larger border size: the border would clipped.
        ImRect outer_border = table->OuterRect;
        const ImU32 outer_col = table->BorderColorStrong;
        if (inner_window != outer_window) // FIXME-TABLE
            outer_border.Expand(1.0f);
        if ((table->Flags & ImGuiTableFlags_BordersOuter) == ImGuiTableFlags_BordersOuter)
        {
            outer_drawlist->AddRect(outer_border.Min, outer_border.Max, outer_col, 0.0f, ~0, border_size);
        }
        else if (table->Flags & ImGuiTableFlags_BordersOuterV)
        {
            // FIXME-TABLE: could use AddRect or explicit VLine/HLine helper?
            outer_drawlist->AddLine(outer_border.Min, ImVec2(outer_border.Min.x, outer_border.Max.y), outer_col, border_size);
            outer_drawlist->AddLine(ImVec2(outer_border.Max.x, outer_border.Min.y), outer_border.Max, outer_col, border_size);
        }
        else if (table->Flags & ImGuiTableFlags_BordersOuterH)
        {
            outer_drawlist->AddLine(outer_border.Min, ImVec2(outer_border.Max.x, outer_border.Min.y), outer_col, border_size);
            outer_drawlist->AddLine(ImVec2(outer_border.Min.x, outer_border.Max.y), outer_border.Max, outer_col, border_size);
        }
    }
    if ((table->Flags & ImGuiTableFlags_BordersInnerH) && table->RowPosY2 < table->OuterRect.Max.y)
    {
        // Draw bottom-most row border
        // FIXME-TABLE: could use AddRect or explicit VLine/HLine helper?
        const float border_y = table->RowPosY2;
        if (border_y >= table->BgClipRect.Min.y && border_y < table->BgClipRect.Max.y)
            inner_drawlist->AddLine(ImVec2(table->BorderX1, border_y), ImVec2(table->BorderX2, border_y), table->BorderColorLight, border_size);
    }
}

static void TableUpdateColumnsWeightFromWidth(ImGuiTable* table)
{
    IM_ASSERT(table->LeftMostStretchedColumnDisplayOrder != -1);

    // Measure existing quantity
    float visible_weight = 0.0f;
    float visible_width = 0.0f;
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (!column->IsVisible || !(column->Flags & ImGuiTableColumnFlags_WidthStretch))
            continue;
        visible_weight += column->StretchWeight;
        visible_width += column->WidthRequest;
    }
    IM_ASSERT(visible_weight > 0.0f && visible_width > 0.0f);

    // Apply new weights
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (!column->IsVisible || !(column->Flags & ImGuiTableColumnFlags_WidthStretch))
            continue;
        column->StretchWeight = ((column->WidthRequest + 0.0f) / visible_width) * visible_weight;
    }
}

// 'width' = inner column width, without padding
void ImGui::TableSetColumnWidth(int column_n, float width)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && table->IsLayoutLocked == false);
    IM_ASSERT(column_n >= 0 && column_n < table->ColumnsCount);
    ImGuiTableColumn* column_0 = &table->Columns[column_n];
    float column_0_width = width;

    // Constraints
    const float min_width = TableGetMinColumnWidth();
    float max_width_0 = FLT_MAX;
    if (!(table->Flags & ImGuiTableFlags_ScrollX))
        max_width_0 = (table->WorkRect.Max.x - column_0->MinX) - (table->ColumnsVisibleCount - (column_0->IndexWithinVisibleSet + 1)) * min_width;
    column_0_width = ImClamp(column_0_width, min_width, max_width_0);

    // Compare both requested and actual given width to avoid overwriting requested width when column is stuck (minimum size, bounded)
    if (column_0->WidthGiven == column_0_width || column_0->WidthRequest == column_0_width)
        return;

    ImGuiTableColumn* column_1 = (column_0->NextVisibleColumn != -1) ? &table->Columns[column_0->NextVisibleColumn] : NULL;

    // In this surprisingly not simple because of how we support mixing Fixed and Stretch columns.
    // When forwarding resize from Wn| to Fn+1| we need to be considerate of the _NoResize flag on Fn+1.
    // FIXME-TABLE: Find a way to rewrite all of this so interactions feel more consistent for the user.
    // Scenarios:
    // - F1 F2 F3  resize from F1| or F2|   --> ok: alter ->WidthRequested of Fixed column. Subsequent columns will be offset.
    // - F1 F2 F3  resize from F3|          --> ok: alter ->WidthRequested of Fixed column. If active, ScrollX extent can be altered.
    // - F1 F2 W3  resize from F1| or F2|   --> ok: alter ->WidthRequested of Fixed column. If active, ScrollX extent can be altered, but it doesn't make much sense as the Stretch column will always be minimal size.
    // - F1 F2 W3  resize from W3|          --> ok: no-op (disabled by Resize Rule 1)
    // - W1 W2 W3  resize from W1| or W2|   --> FIXME
    // - W1 W2 W3  resize from W3|          --> ok: no-op (disabled by Resize Rule 1)
    // - W1 F2 F3  resize from F3|          --> ok: no-op (disabled by Resize Rule 1)
    // - W1 F2     resize from F2|          --> ok: no-op (disabled by Resize Rule 1)
    // - W1 W2 F3  resize from W1| or W2|   --> ok
    // - W1 F2 W3  resize from W1| or F2|   --> FIXME
    // - F1 W2 F3  resize from W2|          --> ok
    // - W1 F2 F3  resize from W1|          --> ok: equivalent to resizing |F2. F3 will not move. (forwarded by Resize Rule 2)
    // - W1 F2 F3  resize from F2|          --> FIXME should resize F2, F3 and not have effect on W1 (Stretch columns are _before_ the Fixed column).

    // Rules:
    // - [Resize Rule 1] Can't resize from right of right-most visible column if there is any Stretch column. Implemented in TableUpdateLayout().
    // - [Resize Rule 2] Resizing from right-side of a Stretch column before a fixed column forward sizing to left-side of fixed column.
    // - [Resize Rule 3] If we are are followed by a fixed column and we have a Stretch column before, we need to ensure that our left border won't move.
    table->IsSettingsDirty = true;
    if (column_0->Flags & ImGuiTableColumnFlags_WidthFixed)
    {
        // [Resize Rule 3] If we are are followed by a fixed column and we have a Stretch column before, we need to ensure
        // that our left border won't move, which we can do by making sure column_a/column_b resizes cancels each others.
        if (column_1 && (column_1->Flags & ImGuiTableColumnFlags_WidthFixed))
            if (table->LeftMostStretchedColumnDisplayOrder != -1 && table->LeftMostStretchedColumnDisplayOrder < column_0->DisplayOrder)
            {
                // (old_a + old_b == new_a + new_b) --> (new_a == old_a + old_b - new_b)
                float column_1_width = ImMax(column_1->WidthRequest - (column_0_width - column_0->WidthRequest), min_width);
                column_0_width = column_0->WidthRequest + column_1->WidthRequest - column_1_width;
                column_1->WidthRequest = column_1_width;
            }

        // Apply
        //IMGUI_DEBUG_LOG("TableSetColumnWidth(%d, %.1f->%.1f)\n", column_0_idx, column_0->WidthRequested, column_0_width);
        column_0->WidthRequest = column_0_width;
    }
    else if (column_0->Flags & ImGuiTableColumnFlags_WidthStretch)
    {
        // [Resize Rule 2]
        if (column_1 && (column_1->Flags & ImGuiTableColumnFlags_WidthFixed))
        {
            float off = (column_0->WidthGiven - column_0_width);
            float column_1_width = column_1->WidthGiven + off;
            column_1->WidthRequest = ImMax(min_width, column_1_width);
            return;
        }

        // (old_a + old_b == new_a + new_b) --> (new_a == old_a + old_b - new_b)
        float column_1_width = ImMax(column_1->WidthRequest - (column_0_width - column_0->WidthRequest), min_width);
        column_0_width = column_0->WidthRequest + column_1->WidthRequest - column_1_width;
        column_1->WidthRequest = column_1_width;
        column_0->WidthRequest = column_0_width;
        TableUpdateColumnsWeightFromWidth(table);
    }
}

// Public wrapper
void ImGui::TableSetColumnVisible(int column_n, bool visible)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && table->IsLayoutLocked == false);
    IM_ASSERT(column_n >= 0 && column_n < table->ColumnsCount);
    table->Columns[column_n].IsVisibleNextFrame = visible;
}

// Allocate draw channels. Called by TableUpdateLayout()
// - We allocate them following storage order instead of display order so reordering columns won't needlessly
//   increase overall dormant memory cost.
// - We isolate headers draw commands in their own channels instead of just altering clip rects.
//   This is in order to facilitate merging of draw commands.
// - After crossing FreezeRowsCount, all columns see their current draw channel changed to a second set of channels.
// - We only use the dummy draw channel so we can push a null clipping rectangle into it without affecting other
//   channels, while simplifying per-row/per-cell overhead. It will be empty and discarded when merged.
// - We allocate 1 or 2 background draw channels. This is because we know PushTableBackground() is only used for
//   horizontal spanning. If we allowed vertical spanning we'd need one background draw channel per merge group (1-4).
// Draw channel allocation (before merging):
// - NoClip                       --> 2+D+1 channels: bg0 + bg1 + foreground (same clip rect == 1 draw call) (FIXME-TABLE: could merge bg1 and foreground?)
// - Clip                         --> 2+D+N channels
// - FreezeRows                   --> 2+D+N*2 (unless scrolling value is zero)
// - FreezeRows || FreezeColunns  --> 3+D+N*2 (unless scrolling value is zero)
// Where D is 1 if any column is clipped or hidden (dummy channel) otherwise 0.
void ImGui::TableUpdateDrawChannels(ImGuiTable* table)
{
    const int freeze_row_multiplier = (table->FreezeRowsCount > 0) ? 2 : 1;
    const int channels_for_row = (table->Flags & ImGuiTableFlags_NoClip) ? 1 : table->ColumnsVisibleCount;
    const int channels_for_bg = 1 + 1 * freeze_row_multiplier;
    const int channels_for_dummy = (table->ColumnsVisibleCount < table->ColumnsCount || table->VisibleUnclippedMaskByIndex != table->VisibleMaskByIndex) ? +1 : 0;
    const int channels_total = channels_for_bg + (channels_for_row * freeze_row_multiplier) + channels_for_dummy;
    table->DrawSplitter.Split(table->InnerWindow->DrawList, channels_total);
    table->DummyDrawChannel = (ImU8)((channels_for_dummy > 0) ? channels_total - 1 : -1);
    table->Bg1DrawChannelCurrent = TABLE_DRAW_CHANNEL_BG1_FROZEN;
    table->Bg1DrawChannelUnfrozen = (ImU8)((table->FreezeRowsCount > 0) ? 2 + channels_for_row : TABLE_DRAW_CHANNEL_BG1_FROZEN);

    int draw_channel_current = 2;
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (!column->IsClipped)
        {
            column->DrawChannelFrozen = (ImU8)(draw_channel_current);
            column->DrawChannelUnfrozen = (ImU8)(draw_channel_current + (table->FreezeRowsCount > 0 ? channels_for_row + 1 : 0));
            if (!(table->Flags & ImGuiTableFlags_NoClip))
                draw_channel_current++;
        }
        else
        {
            column->DrawChannelFrozen = column->DrawChannelUnfrozen = table->DummyDrawChannel;
        }
        column->DrawChannelCurrent = column->DrawChannelFrozen;
    }
}

// This function reorder draw channels based on matching clip rectangle, to facilitate merging them. Called by EndTable().
//
// Columns where the contents didn't stray off their local clip rectangle can be merged. To achieve
// this we merge their clip rect and make them contiguous in the channel list, so they can be merged
// by the call to DrawSplitter.Merge() following to the call to this function.
//
// We reorder draw commands by arranging them into a maximum of 4 distinct groups:
//
//   1 group:               2 groups:              2 groups:              4 groups:
//   [ 0. ] no freeze       [ 0. ] row freeze      [ 01 ] col freeze      [ 01 ] row+col freeze
//   [ .. ]  or no scroll   [ 2. ]  and v-scroll   [ .. ]  and h-scroll   [ 23 ]  and v+h-scroll
//
// Each column itself can use 1 channel (row freeze disabled) or 2 channels (row freeze enabled).
// When the contents of a column didn't stray off its limit, we move its channels into the corresponding group
// based on its position (within frozen rows/columns groups or not).
// At the end of the operation our 1-4 groups will each have a ImDrawCmd using the same ClipRect.
//
// This function assume that each column are pointing to a distinct draw channel,
// otherwise merge_group->ChannelsCount will not match set bit count of merge_group->ChannelsMask.
//
// Column channels will not be merged into one of the 1-4 groups in the following cases:
// - The contents stray off its clipping rectangle (we only compare the MaxX value, not the MinX value).
//   Direct ImDrawList calls won't be taken into account by default, if you use them make sure the ImGui:: bounds
//   matches, by e.g. calling SetCursorScreenPos().
// - The channel uses more than one draw command itself. We drop all our attempt at merging stuff here..
//   we could do better but it's going to be rare and probably not worth the hassle.
// Columns for which the draw channel(s) haven't been merged with other will use their own ImDrawCmd.
//
// This function is particularly tricky to understand.. take a breath.
void    ImGui::TableReorderDrawChannelsForMerge(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    ImDrawListSplitter* splitter = &table->DrawSplitter;
    const bool has_freeze_v = (table->FreezeRowsCount > 0);
    const bool has_freeze_h = (table->FreezeColumnsCount > 0);

    // Track which groups we are going to attempt to merge, and which channels goes into each group.
    struct MergeGroup
    {
        ImRect  ClipRect;
        int     ChannelsCount;
        ImBitArray<IMGUI_TABLE_MAX_DRAW_CHANNELS> ChannelsMask;
    };
    int merge_group_mask = 0x00;
    MergeGroup merge_groups[4];
    memset(merge_groups, 0, sizeof(merge_groups));

    // 1. Scan channels and take note of those which can be merged
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        if (!(table->VisibleUnclippedMaskByIndex & ((ImU64)1 << column_n)))
            continue;
        ImGuiTableColumn* column = &table->Columns[column_n];

        const int merge_group_sub_count = has_freeze_v ? 2 : 1;
        for (int merge_group_sub_n = 0; merge_group_sub_n < merge_group_sub_count; merge_group_sub_n++)
        {
            const int channel_no = (merge_group_sub_n == 0) ? column->DrawChannelFrozen : column->DrawChannelUnfrozen;

            // Don't attempt to merge if there are multiple draw calls within the column
            ImDrawChannel* src_channel = &splitter->_Channels[channel_no];
            if (src_channel->_CmdBuffer.Size > 0 && src_channel->_CmdBuffer.back().ElemCount == 0)
                src_channel->_CmdBuffer.pop_back();
            if (src_channel->_CmdBuffer.Size != 1)
                continue;

            // Find out the width of this merge group and check if it will fit in our column
            // (note that we assume that rendering didn't stray on the left direction. we should need a CursorMinPos to detect it)
            if (!(column->Flags & ImGuiTableColumnFlags_NoClipX))
            {
                float content_max_x;
                if (!has_freeze_v)
                    content_max_x = ImMax(column->ContentMaxXUnfrozen, column->ContentMaxXHeadersUsed); // No row freeze
                else if (merge_group_sub_n == 0)
                    content_max_x = ImMax(column->ContentMaxXFrozen, column->ContentMaxXHeadersUsed);   // Row freeze: use width before freeze
                else
                    content_max_x = column->ContentMaxXUnfrozen;                                        // Row freeze: use width after freeze
                if (content_max_x > column->ClipRect.Max.x)
                    continue;
            }

            const int merge_group_n = (has_freeze_h && column_n < table->FreezeColumnsCount ? 0 : 1) + (has_freeze_v && merge_group_sub_n == 0 ? 0 : 2);
            IM_ASSERT(channel_no < IMGUI_TABLE_MAX_DRAW_CHANNELS);
            MergeGroup* merge_group = &merge_groups[merge_group_n];
            if (merge_group->ChannelsCount == 0)
                merge_group->ClipRect = ImRect(+FLT_MAX, +FLT_MAX, -FLT_MAX, -FLT_MAX);
            merge_group->ChannelsMask.SetBit(channel_no);
            merge_group->ChannelsCount++;
            merge_group->ClipRect.Add(src_channel->_CmdBuffer[0].ClipRect);
            merge_group_mask |= (1 << merge_group_n);
        }

        // Invalidate current draw channel
        // (we don't clear DrawChannelFrozen/DrawChannelUnfrozen solely to facilitate debugging/later inspection of data)
        column->DrawChannelCurrent = (ImU8)-1;
    }

    // [DEBUG] Display merge groups
#if 0
    if (g.IO.KeyShift)
        for (int merge_group_n = 0; merge_group_n < IM_ARRAYSIZE(merge_groups); merge_group_n++)
        {
            MergeGroup* merge_group = &merge_groups[merge_group_n];
            if (merge_group->ChannelsCount == 0)
                continue;
            char buf[32];
            ImFormatString(buf, 32, "MG%d:%d", merge_group_n, merge_group->ChannelsCount);
            ImVec2 text_pos = merge_group->ClipRect.Min + ImVec2(4, 4);
            ImVec2 text_size = CalcTextSize(buf, NULL);
            GetForegroundDrawList()->AddRectFilled(text_pos, text_pos + text_size, IM_COL32(0, 0, 0, 255));
            GetForegroundDrawList()->AddText(text_pos, IM_COL32(255, 255, 0, 255), buf, NULL);
            GetForegroundDrawList()->AddRect(merge_group->ClipRect.Min, merge_group->ClipRect.Max, IM_COL32(255, 255, 0, 255));
        }
#endif

    // 2. Rewrite channel list in our preferred order
    if (merge_group_mask != 0)
    {
        // We skip channel 0 (Bg0) and 1 (Bg1 frozen) from the shuffling since they won't move - see channels allocation in TableUpdateDrawChannels().
        const int LEADING_DRAW_CHANNELS = 2;
        g.DrawChannelsTempMergeBuffer.resize(splitter->_Count - LEADING_DRAW_CHANNELS); // Use shared temporary storage so the allocation gets amortized
        ImDrawChannel* dst_tmp = g.DrawChannelsTempMergeBuffer.Data;
        ImBitArray<IMGUI_TABLE_MAX_DRAW_CHANNELS> remaining_mask;                       // We need 132-bit of storage
        remaining_mask.ClearBits();
        remaining_mask.SetBitRange(LEADING_DRAW_CHANNELS, splitter->_Count - 1);
        remaining_mask.ClearBit(table->Bg1DrawChannelUnfrozen);
        IM_ASSERT(has_freeze_v == false || table->Bg1DrawChannelUnfrozen != TABLE_DRAW_CHANNEL_BG1_FROZEN);
        int remaining_count = splitter->_Count - (has_freeze_v ? LEADING_DRAW_CHANNELS + 1 : LEADING_DRAW_CHANNELS);
        //ImRect host_rect = (table->InnerWindow == table->OuterWindow) ? table->InnerClipRect : table->HostClipRect;
        ImRect host_rect = table->HostClipRect;
        for (int merge_group_n = 0; merge_group_n < IM_ARRAYSIZE(merge_groups); merge_group_n++)
        {
            if (int merge_channels_count = merge_groups[merge_group_n].ChannelsCount)
            {
                MergeGroup* merge_group = &merge_groups[merge_group_n];
                ImRect merge_clip_rect = merge_group->ClipRect;

                // Extend outer-most clip limits to match those of host, so draw calls can be merged even if
                // outer-most columns have some outer padding offsetting them from their parent ClipRect.
                // The principal cases this is dealing with are:
                // - On a same-window table (not scrolling = single group), all fitting columns ClipRect -> will extend and match host ClipRect -> will merge
                // - Columns can use padding and have left-most ClipRect.Min.x and right-most ClipRect.Max.x != from host ClipRect -> will extend and match host ClipRect -> will merge
                // FIXME-TABLE FIXME-WORKRECT: We are wasting a merge opportunity on tables without scrolling if column doesn't fit
                // within host clip rect, solely because of the half-padding difference between window->WorkRect and window->InnerClipRect.
                if ((merge_group_n & 1) == 0 || !has_freeze_h)
                    merge_clip_rect.Min.x = ImMin(merge_clip_rect.Min.x, host_rect.Min.x);
                if ((merge_group_n & 2) == 0 || !has_freeze_v)
                    merge_clip_rect.Min.y = ImMin(merge_clip_rect.Min.y, host_rect.Min.y);
                if ((merge_group_n & 1) != 0)
                    merge_clip_rect.Max.x = ImMax(merge_clip_rect.Max.x, host_rect.Max.x);
                if ((merge_group_n & 2) != 0 && (table->Flags & ImGuiTableFlags_NoHostExtendY) == 0)
                    merge_clip_rect.Max.y = ImMax(merge_clip_rect.Max.y, host_rect.Max.y);
#if 0
                GetOverlayDrawList()->AddRect(merge_group->ClipRect.Min, merge_group->ClipRect.Max, IM_COL32(255, 0, 0, 200), 0.0f, ~0, 1.0f);
                GetOverlayDrawList()->AddLine(merge_group->ClipRect.Min, merge_clip_rect.Min, IM_COL32(255, 100, 0, 200));
                GetOverlayDrawList()->AddLine(merge_group->ClipRect.Max, merge_clip_rect.Max, IM_COL32(255, 100, 0, 200));
#endif
                remaining_count -= merge_group->ChannelsCount;
                for (int n = 0; n < IM_ARRAYSIZE(remaining_mask.Storage); n++)
                    remaining_mask.Storage[n] &= ~merge_group->ChannelsMask.Storage[n];
                for (int n = 0; n < splitter->_Count && merge_channels_count != 0; n++)
                {
                    // Copy + overwrite new clip rect
                    if (!merge_group->ChannelsMask.TestBit(n))
                        continue;
                    merge_group->ChannelsMask.ClearBit(n);
                    merge_channels_count--;

                    ImDrawChannel* channel = &splitter->_Channels[n];
                    IM_ASSERT(channel->_CmdBuffer.Size == 1 && merge_clip_rect.Contains(ImRect(channel->_CmdBuffer[0].ClipRect)));
                    channel->_CmdBuffer[0].ClipRect = merge_clip_rect.ToVec4();
                    memcpy(dst_tmp++, channel, sizeof(ImDrawChannel));
                }
            }

            // Make sure Bg1DrawChannelUnfrozen appears in the middle of our groups (whereas Bg0 and Bg1 frozen are fixed to 0 and 1)
            if (merge_group_n == 1 && has_freeze_v)
                memcpy(dst_tmp++, &splitter->_Channels[table->Bg1DrawChannelUnfrozen], sizeof(ImDrawChannel));
        }

        // Append unmergeable channels that we didn't reorder at the end of the list
        for (int n = 0; n < splitter->_Count && remaining_count != 0; n++)
        {
            if (!remaining_mask.TestBit(n))
                continue;
            ImDrawChannel* channel = &splitter->_Channels[n];
            memcpy(dst_tmp++, channel, sizeof(ImDrawChannel));
            remaining_count--;
        }
        IM_ASSERT(dst_tmp == g.DrawChannelsTempMergeBuffer.Data + g.DrawChannelsTempMergeBuffer.Size);
        memcpy(splitter->_Channels.Data + LEADING_DRAW_CHANNELS, g.DrawChannelsTempMergeBuffer.Data, (splitter->_Count - LEADING_DRAW_CHANNELS) * sizeof(ImDrawChannel));
    }
}

// We use a default parameter of 'init_width_or_weight == -1',
// - with ImGuiTableColumnFlags_WidthFixed,    width  <= 0 --> init width == auto
// - with ImGuiTableColumnFlags_WidthFixed,    width  >  0 --> init width == manual
// - with ImGuiTableColumnFlags_WidthStretch,  weight <  0 --> init weight == 1.0f
// - with ImGuiTableColumnFlags_WidthStretch,  weight >= 0 --> init weight == custom
// Widths are specified _without_ CellPadding. So if you specify a width of 100.0f the column will be 100.0f+Padding*2.0f and you can fit a 100.0-wide item in it.
void    ImGui::TableSetupColumn(const char* label, ImGuiTableColumnFlags flags, float init_width_or_weight, ImGuiID user_id)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Need to call TableSetupColumn() after BeginTable()!");
    IM_ASSERT(table->IsLayoutLocked == false && "Need to call call TableSetupColumn() before first row!");
    IM_ASSERT(table->DeclColumnsCount >= 0 && table->DeclColumnsCount < table->ColumnsCount && "Called TableSetupColumn() too many times!");

    ImGuiTableColumn* column = &table->Columns[table->DeclColumnsCount];
    table->DeclColumnsCount++;

    // When passing a width automatically enforce WidthFixed policy
    // (vs TableFixColumnFlags would default to WidthAlwaysAutoResize)
    // (we write to FlagsIn which is a little misleading, another solution would be to pass init_width_or_weight to TableFixColumnFlags)
    if ((flags & ImGuiTableColumnFlags_WidthMask_) == 0)
        if ((table->Flags & ImGuiTableFlags_SizingPolicyFixedX) && (init_width_or_weight > 0.0f))
            flags |= ImGuiTableColumnFlags_WidthFixed;

    column->UserID = user_id;
    column->FlagsIn = flags;
    column->Flags = TableFixColumnFlags(table, column->FlagsIn);
    flags = column->Flags;

    // Initialize defaults
    if (flags & ImGuiTableColumnFlags_WidthStretch)
    {
        IM_ASSERT(init_width_or_weight != 0.0f && "Need to provide a valid weight!");
        if (init_width_or_weight < 0.0f)
            init_width_or_weight = 1.0f;
    }
    column->InitStretchWeightOrWidth = init_width_or_weight;
    if (table->IsInitializing && column->WidthRequest < 0.0f && column->StretchWeight < 0.0f)
    {
        // Init width or weight
        if ((flags & ImGuiTableColumnFlags_WidthFixed) && init_width_or_weight > 0.0f)
        {
            // Disable auto-fit if a default fixed width has been specified
            column->WidthRequest = init_width_or_weight;
            column->AutoFitQueue = 0x00;
        }
        if (flags & ImGuiTableColumnFlags_WidthStretch)
            column->StretchWeight = init_width_or_weight;
        else
            column->StretchWeight = 1.0f;
    }
    if (table->IsInitializing)
    {
        // Init default visibility/sort state
        if ((flags & ImGuiTableColumnFlags_DefaultHide) && (table->SettingsLoadedFlags & ImGuiTableFlags_Hideable) == 0)
            column->IsVisible = column->IsVisibleNextFrame = false;
        if (flags & ImGuiTableColumnFlags_DefaultSort && (table->SettingsLoadedFlags & ImGuiTableFlags_Sortable) == 0)
        {
            column->SortOrder = 0; // Multiple columns using _DefaultSort will be reordered when building the sort specs.
            column->SortDirection = (column->Flags & ImGuiTableColumnFlags_PreferSortDescending) ? (ImS8)ImGuiSortDirection_Descending : (ImU8)(ImGuiSortDirection_Ascending);
        }
    }

    // Store name (append with zero-terminator in contiguous buffer)
    IM_ASSERT(column->NameOffset == -1);
    if (label != NULL && label[0] != 0)
    {
        column->NameOffset = (ImS16)table->ColumnsNames.size();
        table->ColumnsNames.append(label, label + strlen(label) + 1);
    }
}

void ImGui::TableSetupScrollFreeze(int columns, int rows)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Need to call TableSetupColumn() after BeginTable()!");
    IM_ASSERT(table->IsLayoutLocked == false && "Need to call TableSetupColumn() before first row!");
    IM_ASSERT(columns >= 0 && columns < IMGUI_TABLE_MAX_COLUMNS);
    IM_ASSERT(rows >= 0 && rows < 128); // Arbitrary limit

    table->FreezeColumnsRequest = (table->Flags & ImGuiTableFlags_ScrollX) ? (ImS8)columns : 0;
    table->FreezeColumnsCount = (table->InnerWindow->Scroll.x != 0.0f) ? table->FreezeColumnsRequest : 0;
    table->FreezeRowsRequest = (table->Flags & ImGuiTableFlags_ScrollY) ? (ImS8)rows : 0;
    table->FreezeRowsCount = (table->InnerWindow->Scroll.y != 0.0f) ? table->FreezeRowsRequest : 0;
    table->IsUnfrozen = (table->FreezeRowsCount == 0); // Make sure this is set before TableUpdateLayout() so ImGuiListClipper can benefit from it.b
}

// Starts into the first cell of a new row
void    ImGui::TableNextRow(ImGuiTableRowFlags row_flags, float row_min_height)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;

    if (table->CurrentRow == -1)
        TableUpdateLayout(table);
    if (table->IsInsideRow)
        TableEndRow(table);

    table->LastRowFlags = table->RowFlags;
    table->RowFlags = row_flags;
    table->RowMinHeight = row_min_height;
    TableBeginRow(table);

    // We honor min_row_height requested by user, but cannot guarantee per-row maximum height,
    // because that would essentially require a unique clipping rectangle per-cell.
    table->RowPosY2 += table->CellPaddingY * 2.0f;
    table->RowPosY2 = ImMax(table->RowPosY2, table->RowPosY1 + row_min_height);

    // Disable output until user calls TableNextColumn()
    table->InnerWindow->SkipItems = true;
}

// [Internal]
void    ImGui::TableBeginRow(ImGuiTable* table)
{
    ImGuiWindow* window = table->InnerWindow;
    IM_ASSERT(!table->IsInsideRow);

    // New row
    table->CurrentRow++;
    table->CurrentColumn = -1;
    table->RowBgColor[0] = table->RowBgColor[1] = IM_COL32_DISABLE;
    table->RowCellDataCurrent = -1;
    table->IsInsideRow = true;

    // Begin frozen rows
    float next_y1 = table->RowPosY2;
    if (table->CurrentRow == 0 && table->FreezeRowsCount > 0)
        next_y1 = window->DC.CursorPos.y = table->OuterRect.Min.y;

    table->RowPosY1 = table->RowPosY2 = next_y1;
    table->RowTextBaseline = 0.0f;
    table->RowIndentOffsetX = window->DC.Indent.x - table->HostIndentX; // Lock indent
    window->DC.PrevLineTextBaseOffset = 0.0f;
    window->DC.CursorMaxPos.y = next_y1;

    // Making the header BG color non-transparent will allow us to overlay it multiple times when handling smooth dragging.
    if (table->RowFlags & ImGuiTableRowFlags_Headers)
    {
        TableSetBgColor(ImGuiTableBgTarget_RowBg0, GetColorU32(ImGuiCol_TableHeaderBg));
        if (table->CurrentRow == 0)
            table->IsUsingHeaders = true;
    }
}

// [Internal]
void    ImGui::TableEndRow(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    IM_ASSERT(window == table->InnerWindow);
    IM_ASSERT(table->IsInsideRow);

    if (table->CurrentColumn != -1)
        TableEndCell(table);

    // Position cursor at the bottom of our row so it can be used for e.g. clipping calculation. However it is
    // likely that the next call to TableBeginCell() will reposition the cursor to take account of vertical padding.
    window->DC.CursorPos.y = table->RowPosY2;

    // Row background fill
    const float bg_y1 = table->RowPosY1;
    const float bg_y2 = table->RowPosY2;

    const bool unfreeze_rows_actual = (table->CurrentRow + 1 == table->FreezeRowsCount);
    const bool unfreeze_rows_request = (table->CurrentRow + 1 == table->FreezeRowsRequest);
    if (table->CurrentRow == 0)
        table->LastFirstRowHeight = bg_y2 - bg_y1;

    const bool is_visible = (bg_y2 >= table->InnerClipRect.Min.y && bg_y1 <= table->InnerClipRect.Max.y);
    if (is_visible)
    {
        // Decide of background color for the row
        ImU32 bg_col0 = 0;
        ImU32 bg_col1 = 0;
        if (table->RowBgColor[0] != IM_COL32_DISABLE)
            bg_col0 = table->RowBgColor[0];
        else if (table->Flags & ImGuiTableFlags_RowBg)
            bg_col0 = GetColorU32((table->RowBgColorCounter & 1) ? ImGuiCol_TableRowBgAlt : ImGuiCol_TableRowBg);
        if (table->RowBgColor[1] != IM_COL32_DISABLE)
            bg_col1 = table->RowBgColor[1];

        // Decide of top border color
        ImU32 border_col = 0;
        const float border_size = TABLE_BORDER_SIZE;
        if (table->CurrentRow > 0 || table->InnerWindow == table->OuterWindow)
            if (table->Flags & ImGuiTableFlags_BordersInnerH)
                border_col = (table->LastRowFlags & ImGuiTableRowFlags_Headers) ? table->BorderColorStrong : table->BorderColorLight;

        const bool draw_cell_bg_color = table->RowCellDataCurrent >= 0;
        const bool draw_strong_bottom_border = unfreeze_rows_actual;
        if ((bg_col0 | bg_col1 | border_col) != 0 || draw_strong_bottom_border || draw_cell_bg_color)
        {
            // In theory we could call SetWindowClipRectBeforeSetChannel() but since we know TableEndRow() is
            // always followed by a change of clipping rectangle we perform the smallest overwrite possible here.
            if ((table->Flags & ImGuiTableFlags_NoClip) == 0)
                window->DrawList->_CmdHeader.ClipRect = table->BgClipRectForDrawCmd.ToVec4();
            table->DrawSplitter.SetCurrentChannel(window->DrawList, TABLE_DRAW_CHANNEL_BG0);
        }

        // Draw row background
        // We soft/cpu clip this so all backgrounds and borders can share the same clipping rectangle
        if (bg_col0 || bg_col1)
        {
            ImRect row_rect(table->WorkRect.Min.x, bg_y1, table->WorkRect.Max.x, bg_y2);
            row_rect.ClipWith(table->BgClipRect);
            if (bg_col0 != 0 && row_rect.Min.y < row_rect.Max.y)
                window->DrawList->AddRectFilled(row_rect.Min, row_rect.Max, bg_col0);
            if (bg_col1 != 0 && row_rect.Min.y < row_rect.Max.y)
                window->DrawList->AddRectFilled(row_rect.Min, row_rect.Max, bg_col1);
        }

        // Draw cell background color
        if (draw_cell_bg_color)
        {
            ImGuiTableCellData* cell_data_end = &table->RowCellData[table->RowCellDataCurrent];
            for (ImGuiTableCellData* cell_data = &table->RowCellData[0]; cell_data <= cell_data_end; cell_data++)
            {
                const ImGuiTableColumn* column = &table->Columns[cell_data->Column];
                ImRect cell_bg_rect = TableGetCellBgRect(table, cell_data->Column);
                cell_bg_rect.ClipWith(table->BgClipRect);
                cell_bg_rect.Min.x = ImMax(cell_bg_rect.Min.x, column->ClipRect.Min.x);     // So that first column after frozen one gets clipped
                cell_bg_rect.Max.x = ImMin(cell_bg_rect.Max.x, column->ClipRect.Max.x);
                window->DrawList->AddRectFilled(cell_bg_rect.Min, cell_bg_rect.Max, cell_data->BgColor);
            }
        }

        // Draw top border
        if (border_col && bg_y1 >= table->BgClipRect.Min.y && bg_y1 < table->BgClipRect.Max.y)
            window->DrawList->AddLine(ImVec2(table->BorderX1, bg_y1), ImVec2(table->BorderX2, bg_y1), border_col, border_size);

        // Draw bottom border at the row unfreezing mark (always strong)
        if (draw_strong_bottom_border && bg_y2 >= table->BgClipRect.Min.y && bg_y2 < table->BgClipRect.Max.y)
            window->DrawList->AddLine(ImVec2(table->BorderX1, bg_y2), ImVec2(table->BorderX2, bg_y2), table->BorderColorStrong, border_size);
    }

    // End frozen rows (when we are past the last frozen row line, teleport cursor and alter clipping rectangle)
    // We need to do that in TableEndRow() instead of TableBeginRow() so the list clipper can mark end of row and
    // get the new cursor position.
    if (unfreeze_rows_request)
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        {
            ImGuiTableColumn* column = &table->Columns[column_n];
            column->NavLayerCurrent = (ImS8)((column_n < table->FreezeColumnsCount) ? ImGuiNavLayer_Menu : ImGuiNavLayer_Main);
        }
    if (unfreeze_rows_actual)
    {
        IM_ASSERT(table->IsUnfrozen == false);
        table->IsUnfrozen = true;

        // BgClipRect starts as table->InnerClipRect, reduce it now and make BgClipRectForDrawCmd == BgClipRect
        float y0 = ImMax(table->RowPosY2 + 1, window->InnerClipRect.Min.y);
        table->BgClipRect.Min.y = table->BgClipRectForDrawCmd.Min.y = ImMin(y0, window->InnerClipRect.Max.y);
        table->BgClipRect.Max.y = table->BgClipRectForDrawCmd.Max.y = window->InnerClipRect.Max.y;
        table->Bg1DrawChannelCurrent = table->Bg1DrawChannelUnfrozen;
        IM_ASSERT(table->BgClipRectForDrawCmd.Min.y <= table->BgClipRectForDrawCmd.Max.y);

        float row_height = table->RowPosY2 - table->RowPosY1;
        table->RowPosY2 = window->DC.CursorPos.y = table->WorkRect.Min.y + table->RowPosY2 - table->OuterRect.Min.y;
        table->RowPosY1 = table->RowPosY2 - row_height;
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        {
            ImGuiTableColumn* column = &table->Columns[column_n];
            column->DrawChannelCurrent = column->DrawChannelUnfrozen;
            column->ClipRect.Min.y = table->BgClipRectForDrawCmd.Min.y;
        }

        // Update cliprect ahead of TableBeginCell() so clipper can access to new ClipRect->Min.y
        SetWindowClipRectBeforeSetChannel(window, table->Columns[0].ClipRect);
        table->DrawSplitter.SetCurrentChannel(window->DrawList, table->Columns[0].DrawChannelCurrent);
    }

    if (!(table->RowFlags & ImGuiTableRowFlags_Headers))
        table->RowBgColorCounter++;
    table->IsInsideRow = false;
}

// [Internal] Called by TableNextColumn()!
// This is called very frequently, so we need to be mindful of unnecessary overhead.
// FIXME-TABLE FIXME-OPT: Could probably shortcut some things for non-active or clipped columns.
void    ImGui::TableBeginCell(ImGuiTable* table, int column_n)
{
    ImGuiTableColumn* column = &table->Columns[column_n];
    ImGuiWindow* window = table->InnerWindow;
    table->CurrentColumn = column_n;

    // Start position is roughly ~~ CellRect.Min + CellPadding + Indent
    float start_x = column->WorkMinX;
    if (column->Flags & ImGuiTableColumnFlags_IndentEnable)
        start_x += table->RowIndentOffsetX; // ~~ += window.DC.Indent.x - table->HostIndentX, except we locked it for the row.

    window->DC.CursorPos.x = start_x;
    window->DC.CursorPos.y = table->RowPosY1 + table->CellPaddingY;
    window->DC.CursorMaxPos.x = window->DC.CursorPos.x;
    window->DC.ColumnsOffset.x = start_x - window->Pos.x - window->DC.Indent.x; // FIXME-WORKRECT
    window->DC.CurrLineTextBaseOffset = table->RowTextBaseline;
    window->DC.LastItemId = 0;
    window->DC.NavLayerCurrent = (ImGuiNavLayer)column->NavLayerCurrent;

    window->WorkRect.Min.y = window->DC.CursorPos.y;
    window->WorkRect.Min.x = column->WorkMinX;
    window->WorkRect.Max.x = column->WorkMaxX;

    // To allow ImGuiListClipper to function we propagate our row height
    if (!column->IsVisible)
        window->DC.CursorPos.y = ImMax(window->DC.CursorPos.y, table->RowPosY2);

    window->SkipItems = column->IsSkipItems;
    if (table->Flags & ImGuiTableFlags_NoClip)
    {
        // FIXME: if we end up drawing all borders/bg in EndTable, could remove this and just assert that channel hasn't changed.
        table->DrawSplitter.SetCurrentChannel(window->DrawList, TABLE_DRAW_CHANNEL_UNCLIPPED);
        //IM_ASSERT(table->DrawSplitter._Current == TABLE_DRAW_CHANNEL_UNCLIPPED);
    }
    else
    {
        // FIXME-TABLE: Could avoid this if draw channel is dummy channel?
        SetWindowClipRectBeforeSetChannel(window, column->ClipRect);
        table->DrawSplitter.SetCurrentChannel(window->DrawList, column->DrawChannelCurrent);
    }
}

// [Internal] Called by TableNextRow()/TableNextColumn()!
void    ImGui::TableEndCell(ImGuiTable* table)
{
    ImGuiTableColumn* column = &table->Columns[table->CurrentColumn];
    ImGuiWindow* window = table->InnerWindow;

    // Report maximum position so we can infer content size per column.
    float* p_max_pos_x;
    if (table->RowFlags & ImGuiTableRowFlags_Headers)
        p_max_pos_x = &column->ContentMaxXHeadersUsed;  // Useful in case user submit contents in header row that is not a TableHeader() call
    else
        p_max_pos_x = table->IsUnfrozen ? &column->ContentMaxXUnfrozen : &column->ContentMaxXFrozen;
    *p_max_pos_x = ImMax(*p_max_pos_x, window->DC.CursorMaxPos.x);
    table->RowPosY2 = ImMax(table->RowPosY2, window->DC.CursorMaxPos.y + table->CellPaddingY);

    // Propagate text baseline for the entire row
    // FIXME-TABLE: Here we propagate text baseline from the last line of the cell.. instead of the first one.
    table->RowTextBaseline = ImMax(table->RowTextBaseline, window->DC.PrevLineTextBaseOffset);
}

// Append into the next column/cell
// FIXME-TABLE: Wrapping to next row could be optional?
bool    ImGui::TableNextColumn()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return false;

    if (table->IsInsideRow && table->CurrentColumn + 1 < table->ColumnsCount)
    {
        if (table->CurrentColumn != -1)
            TableEndCell(table);
        TableBeginCell(table, table->CurrentColumn + 1);
    }
    else
    {
        TableNextRow();
        TableBeginCell(table, 0);
    }

    // FIXME-TABLE: it is likely to alter layout if user skips a columns contents based on clipping.
    int column_n = table->CurrentColumn;
    return (table->VisibleUnclippedMaskByIndex & ((ImU64)1 << column_n)) != 0;
}

bool    ImGui::TableSetColumnIndex(int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return false;

    if (table->CurrentColumn != column_n)
    {
        if (table->CurrentColumn != -1)
            TableEndCell(table);
        IM_ASSERT(column_n >= 0 && table->ColumnsCount);
        TableBeginCell(table, column_n);
    }

    // FIXME-TABLE: it is likely to alter layout if user skips a columns contents based on clipping.
    return (table->VisibleUnclippedMaskByIndex & ((ImU64)1 << column_n)) != 0;
}

int ImGui::TableGetColumnCount()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    return table ? table->ColumnsCount : 0;
}

const char*   ImGui::TableGetColumnName(int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return NULL;
    if (column_n < 0)
        column_n = table->CurrentColumn;
    return TableGetColumnName(table, column_n);
}

// We expose "Visible and Unclipped" to the user, vs our internal "Visible" state which is !Hidden
bool    ImGui::TableGetColumnIsVisible(int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return false;
    if (column_n < 0)
        column_n = table->CurrentColumn;
    return (table->VisibleUnclippedMaskByIndex & ((ImU64)1 << column_n)) != 0;
}

int     ImGui::TableGetColumnIndex()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return 0;
    return table->CurrentColumn;
}

// Return the cell rectangle based on currently known height.
// - Important: we generally don't know our row height until the end of the row, so Max.y will be incorrect in many situations.
//   The only case where this is correct is if we provided a min_row_height to TableNextRow() and don't go below it.
// - Important: if ImGuiTableFlags_PadOuterX is set but ImGuiTableFlags_PadInnerX is not set, the outer-most left and right
//   columns report a small offset so their CellBgRect can extend up to the outer border.
ImRect  ImGui::TableGetCellBgRect(const ImGuiTable* table, int column_n)
{
    const ImGuiTableColumn* column = &table->Columns[column_n];
    float x1 = column->MinX;
    float x2 = column->MaxX;
    if (column->PrevVisibleColumn == -1)
        x1 -= table->CellSpacingX1;
    if (column->NextVisibleColumn == -1)
        x2 += table->CellSpacingX2;
    return ImRect(x1, table->RowPosY1, x2, table->RowPosY2);
}

const char* ImGui::TableGetColumnName(const ImGuiTable* table, int column_n)
{
    const ImGuiTableColumn* column = &table->Columns[column_n];
    if (column->NameOffset == -1)
        return "";
    return &table->ColumnsNames.Buf[column->NameOffset];
}

// Return the resizing ID for the right-side of the given column.
ImGuiID ImGui::TableGetColumnResizeID(const ImGuiTable* table, int column_n, int instance_no)
{
    IM_ASSERT(column_n < table->ColumnsCount);
    ImGuiID id = table->ID + 1 + (instance_no * table->ColumnsCount) + column_n;
    return id;
}

void    ImGui::TableSetColumnAutofit(ImGuiTable* table, int column_n)
{
    // Disable clipping then auto-fit, will take 2 frames
    // (we don't take a shortcut for unclipped columns to reduce inconsistencies when e.g. resizing multiple columns)
    ImGuiTableColumn* column = &table->Columns[column_n];
    column->CannotSkipItemsQueue = (1 << 0);
    column->AutoFitQueue = (1 << 1);
}

void    ImGui::PushTableBackground()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiTable* table = g.CurrentTable;

    // Optimization: avoid SetCurrentChannel() + PushClipRect()
    table->HostBackupClipRect = window->ClipRect;
    SetWindowClipRectBeforeSetChannel(window, table->BgClipRectForDrawCmd);
    table->DrawSplitter.SetCurrentChannel(window->DrawList, table->Bg1DrawChannelCurrent);
}

void    ImGui::PopTableBackground()
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiTable* table = g.CurrentTable;
    ImGuiTableColumn* column = &table->Columns[table->CurrentColumn];

    // Optimization: avoid PopClipRect() + SetCurrentChannel()
    SetWindowClipRectBeforeSetChannel(window, table->HostBackupClipRect);
    table->DrawSplitter.SetCurrentChannel(window->DrawList, column->DrawChannelCurrent);
}

// Output context menu into current window (generally a popup)
// FIXME-TABLE: Ideally this should be writable by the user. Full programmatic access to that data?
void    ImGui::TableDrawContextMenu(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    bool want_separator = false;
    const int column_n = (table->ContextPopupColumn >= 0 && table->ContextPopupColumn < table->ColumnsCount) ? table->ContextPopupColumn : -1;
    ImGuiTableColumn* column = (column_n != -1) ? &table->Columns[column_n] : NULL;

    // Sizing
    if (table->Flags & ImGuiTableFlags_Resizable)
    {
        if (column != NULL)
        {
            const bool can_resize = !(column->Flags & (ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthStretch)) && column->IsVisible;
            if (MenuItem("Size column to fit", NULL, false, can_resize))
                TableSetColumnAutofit(table, column_n);
        }

        if (MenuItem("Size all columns to fit", NULL))
        {
            for (int other_column_n = 0; other_column_n < table->ColumnsCount; other_column_n++)
            {
                ImGuiTableColumn* other_column = &table->Columns[other_column_n];
                if (other_column->IsVisible)
                    TableSetColumnAutofit(table, other_column_n);
            }
        }
        want_separator = true;
    }

    // Ordering
    if (table->Flags & ImGuiTableFlags_Reorderable)
    {
        if (MenuItem("Reset order", NULL, false, !table->IsDefaultDisplayOrder))
            table->IsResetDisplayOrderRequest = true;
        want_separator = true;
    }

    // Sorting
    // (modify TableOpenContextMenu() to add _Sortable flag if enabling this)
#if 0
    if ((table->Flags & ImGuiTableFlags_Sortable) && column != NULL && (column->Flags & ImGuiTableColumnFlags_NoSort) == 0)
    {
        if (want_separator)
            Separator();
        want_separator = true;

        bool append_to_sort_specs = g.IO.KeyShift;
        if (MenuItem("Sort in Ascending Order", NULL, column->SortOrder != -1 && column->SortDirection == ImGuiSortDirection_Ascending, (column->Flags & ImGuiTableColumnFlags_NoSortAscending) == 0))
            TableSetColumnSortDirection(table, column_n, ImGuiSortDirection_Ascending, append_to_sort_specs);
        if (MenuItem("Sort in Descending Order", NULL, column->SortOrder != -1 && column->SortDirection == ImGuiSortDirection_Descending, (column->Flags & ImGuiTableColumnFlags_NoSortDescending) == 0))
            TableSetColumnSortDirection(table, column_n, ImGuiSortDirection_Descending, append_to_sort_specs);
    }
#endif

    // Hiding / Visibility
    if (table->Flags & ImGuiTableFlags_Hideable)
    {
        if (want_separator)
            Separator();
        want_separator = true;

        PushItemFlag(ImGuiItemFlags_SelectableDontClosePopup, true);
        for (int other_column_n = 0; other_column_n < table->ColumnsCount; other_column_n++)
        {
            ImGuiTableColumn* other_column = &table->Columns[other_column_n];
            const char* name = TableGetColumnName(table, other_column_n);
            if (name == NULL || name[0] == 0)
                name = "<Unknown>";

            // Make sure we can't hide the last active column
            bool menu_item_active = (other_column->Flags & ImGuiTableColumnFlags_NoHide) ? false : true;
            if (other_column->IsVisible && table->ColumnsVisibleCount <= 1)
                menu_item_active = false;
            if (MenuItem(name, NULL, other_column->IsVisible, menu_item_active))
                other_column->IsVisibleNextFrame = !other_column->IsVisible;
        }
        PopItemFlag();
    }
}

// Use -1 to open menu not specific to a given column.
void    ImGui::TableOpenContextMenu(int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (column_n == -1 && table->CurrentColumn != -1)   // When called within a column automatically use this one (for consistency)
        column_n = table->CurrentColumn;
    if (column_n == table->ColumnsCount)                // To facilitate using with TableGetHoveredColumn()
        column_n = -1;
    IM_ASSERT(column_n >= -1 && column_n < table->ColumnsCount);
    if (table->Flags & (ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable))
    {
        table->IsContextPopupOpen = true;
        table->ContextPopupColumn = (ImS8)column_n;
        table->InstanceInteracted = table->InstanceCurrent;
        const ImGuiID context_menu_id = ImHashStr("##ContextMenu", 0, table->ID);
        OpenPopupEx(context_menu_id, ImGuiPopupFlags_None);
    }
}

// This is a helper to output TableHeader() calls based on the column names declared in TableSetupColumn().
// The intent is that advanced users willing to create customized headers would not need to use this helper
// and can create their own! For example: TableHeader() may be preceeded by Checkbox() or other custom widgets.
// See 'Demo->Tables->Custom headers' for a demonstration of implementing a custom version of this.
void    ImGui::TableHeadersRow()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Need to call TableHeadersRow() after BeginTable()!");

    // Calculate row height (for the unlikely case that labels may be are multi-line)
    // If we didn't do that, uneven header height would work but their highlight won't cover the full row height.
    float row_height = GetTextLineHeight();
    const float row_y1 = GetCursorScreenPos().y;
    const int columns_count = TableGetColumnCount();
    for (int column_n = 0; column_n < columns_count; column_n++)
        if (TableGetColumnIsVisible(column_n))
            row_height = ImMax(row_height, CalcTextSize(TableGetColumnName(column_n)).y);
    row_height += style.CellPadding.y * 2.0f;

    // Open row
    TableNextRow(ImGuiTableRowFlags_Headers, row_height);
    if (table->HostSkipItems) // Merely an optimization, you may skip in your own code.
        return;

    // This for loop is constructed to not make use of internal functions,
    // as this is intended to be a base template to copy and build from.
    for (int column_n = 0; column_n < columns_count; column_n++)
    {
        if (!TableSetColumnIndex(column_n))
            continue;

        // [DEBUG] Test custom user elements
#if 0
        if (column_n < 2)
        {
            static bool b[2] = {};
            PushID(column_n);
            PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            Checkbox("##", &b[column_n]);
            PopStyleVar();
            PopID();
            SameLine(0.0f, style.ItemInnerSpacing.x);
        }
#endif

        // Push an id to allow unnamed labels (generally accidental, but let's behave nicely with them)
        // - in your own code you may omit the PushID/PopID all-together, provided you know they won't collide
        // - table->InstanceCurrent is only >0 when we use multiple BeginTable/EndTable calls with same identifier.
        const char* name = TableGetColumnName(column_n);
        PushID(table->InstanceCurrent * table->ColumnsCount + column_n);
        TableHeader(name);
        PopID();
    }

    // Allow opening popup from the right-most section after the last column.
    // FIXME-TABLE: TableOpenContextMenu() is not public yet.
    ImVec2 mouse_pos = ImGui::GetMousePos();
    if (IsMouseReleased(1) && TableGetHoveredColumn() == columns_count)
        if (mouse_pos.y >= row_y1 && mouse_pos.y < row_y1 + row_height)
            TableOpenContextMenu(-1); // Will open a non-column-specific popup.
}

// Emit a column header (text + optional sort order)
// We cpu-clip text here so that all columns headers can be merged into a same draw call.
// Note that because of how we cpu-clip and display sorting indicators, you _cannot_ use SameLine() after a TableHeader()
// FIXME-TABLE: Style confusion between CellPadding.y and FramePadding.y
void    ImGui::TableHeader(const char* label)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    if (window->SkipItems)
        return;

    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL && "Need to call TableHeader() after BeginTable()!");
    IM_ASSERT(table->CurrentColumn != -1);
    const int column_n = table->CurrentColumn;
    ImGuiTableColumn* column = &table->Columns[column_n];

    // Label
    if (label == NULL)
        label = "";
    const char* label_end = FindRenderedTextEnd(label);
    ImVec2 label_size = CalcTextSize(label, label_end, true);
    ImVec2 label_pos = window->DC.CursorPos;

    // If we already got a row height, there's use that.
    // FIXME-TABLE-PADDING: Problem if the correct outer-padding CellBgRect strays off our ClipRect
    ImRect cell_r = TableGetCellBgRect(table, column_n);
    float label_height = ImMax(label_size.y, table->RowMinHeight - table->CellPaddingY * 2.0f);

    // Keep header highlighted when context menu is open.
    const bool selected = (table->IsContextPopupOpen && table->ContextPopupColumn == column_n && table->InstanceInteracted == table->InstanceCurrent);
    ImGuiID id = window->GetID(label);
    ImRect bb(cell_r.Min.x, cell_r.Min.y, cell_r.Max.x, ImMax(cell_r.Max.y, cell_r.Min.y + label_height + g.Style.CellPadding.y * 2.0f));
    ItemSize(ImVec2(0.0f, label_height)); // Don't declare unclipped width, it'll be fed ContentMaxPosHeadersIdeal
    if (!ItemAdd(bb, id))
        return;

    //GetForegroundDrawList()->AddRect(cell_r.Min, cell_r.Max, IM_COL32(255, 0, 0, 255)); // [DEBUG]
    //GetForegroundDrawList()->AddRect(bb.Min, bb.Max, IM_COL32(255, 0, 0, 255)); // [DEBUG]

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);
    if (hovered || selected)
    {
        const ImU32 col = GetColorU32(held ? ImGuiCol_HeaderActive : hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
        //RenderFrame(bb.Min, bb.Max, col, false, 0.0f);
        TableSetBgColor(ImGuiTableBgTarget_CellBg, col, table->CurrentColumn);
        RenderNavHighlight(bb, id, ImGuiNavHighlightFlags_TypeThin | ImGuiNavHighlightFlags_NoRounding);
    }
    if (held)
        table->HeldHeaderColumn = (ImS8)column_n;
    window->DC.CursorPos.y -= g.Style.ItemSpacing.y * 0.5f;

    // Drag and drop to re-order columns.
    // FIXME-TABLE: Scroll request while reordering a column and it lands out of the scrolling zone.
    if (held && (table->Flags & ImGuiTableFlags_Reorderable) && IsMouseDragging(0) && !g.DragDropActive)
    {
        // While moving a column it will jump on the other side of the mouse, so we also test for MouseDelta.x
        table->ReorderColumn = (ImS8)column_n;
        table->InstanceInteracted = table->InstanceCurrent;

        // We don't reorder: through the frozen<>unfrozen line, or through a column that is marked with ImGuiTableColumnFlags_NoReorder.
        if (g.IO.MouseDelta.x < 0.0f && g.IO.MousePos.x < cell_r.Min.x)
            if (ImGuiTableColumn* prev_column = (column->PrevVisibleColumn != -1) ? &table->Columns[column->PrevVisibleColumn] : NULL)
                if (!((column->Flags | prev_column->Flags) & ImGuiTableColumnFlags_NoReorder))
                    if ((column->IndexWithinVisibleSet < table->FreezeColumnsRequest) == (prev_column->IndexWithinVisibleSet < table->FreezeColumnsRequest))
                        table->ReorderColumnDir = -1;
        if (g.IO.MouseDelta.x > 0.0f && g.IO.MousePos.x > cell_r.Max.x)
            if (ImGuiTableColumn* next_column = (column->NextVisibleColumn != -1) ? &table->Columns[column->NextVisibleColumn] : NULL)
                if (!((column->Flags | next_column->Flags) & ImGuiTableColumnFlags_NoReorder))
                    if ((column->IndexWithinVisibleSet < table->FreezeColumnsRequest) == (next_column->IndexWithinVisibleSet < table->FreezeColumnsRequest))
                        table->ReorderColumnDir = +1;
    }

    // Sort order arrow
    float w_arrow = 0.0f;
    float w_sort_text = 0.0f;
    float ellipsis_max = cell_r.Max.x;
    if ((table->Flags & ImGuiTableFlags_Sortable) && !(column->Flags & ImGuiTableColumnFlags_NoSort))
    {
        const float ARROW_SCALE = 0.65f;
        w_arrow = ImFloor(g.FontSize * ARROW_SCALE + g.Style.FramePadding.x);// table->CellPadding.x);
        if (column->SortOrder != -1)
        {
            w_sort_text = 0.0f;

            char sort_order_suf[8];
            if (column->SortOrder > 0)
            {
                ImFormatString(sort_order_suf, IM_ARRAYSIZE(sort_order_suf), "%d", column->SortOrder + 1);
                w_sort_text = g.Style.ItemInnerSpacing.x + CalcTextSize(sort_order_suf).x;
            }

            float x = ImMax(cell_r.Min.x, cell_r.Max.x - w_arrow - w_sort_text);
            ellipsis_max -= w_arrow + w_sort_text;

            float y = label_pos.y;
            ImU32 col = GetColorU32(ImGuiCol_Text);
            if (column->SortOrder > 0)
            {
                PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_Text, 0.70f));
                RenderText(ImVec2(x + g.Style.ItemInnerSpacing.x, y), sort_order_suf);
                PopStyleColor();
                x += w_sort_text;
            }
            RenderArrow(window->DrawList, ImVec2(x, y), col, column->SortDirection == ImGuiSortDirection_Ascending ? ImGuiDir_Up : ImGuiDir_Down, ARROW_SCALE);
        }

        // Handle clicking on column header to adjust Sort Order
        if (pressed && table->ReorderColumn != column_n)
        {
            // Set new sort direction
            // - If the PreferSortDescending flag is set, we will default to a Descending direction on the first click.
            // - Note that the PreferSortAscending flag is never checked, it is essentially the default and therefore a no-op.
            ImGuiSortDirection sort_direction;
            if (column->SortOrder == -1)
                sort_direction = (column->Flags & ImGuiTableColumnFlags_PreferSortDescending) ? ImGuiSortDirection_Descending : ImGuiSortDirection_Ascending;
            else
                sort_direction = (column->SortDirection == ImGuiSortDirection_Ascending) ? ImGuiSortDirection_Descending : ImGuiSortDirection_Ascending;
            TableSetColumnSortDirection(column_n, sort_direction, g.IO.KeyShift);
        }
    }

    // Render clipped label. Clipping here ensure that in the majority of situations, all our header cells will
    // be merged into a single draw call.
    //window->DrawList->AddCircleFilled(ImVec2(ellipsis_max, label_pos.y), 40, IM_COL32_WHITE);
    RenderTextEllipsis(window->DrawList, label_pos, ImVec2(ellipsis_max, label_pos.y + label_height + g.Style.FramePadding.y), ellipsis_max, ellipsis_max, label, label_end, &label_size);

    const bool text_clipped = label_size.x > (ellipsis_max - label_pos.x);
    if (text_clipped && hovered && g.HoveredIdNotActiveTimer > TOOLTIP_DELAY)
        SetTooltip("%.*s", (int)(label_end - label), label);

    // We feed our unclipped width to the column without writing on CursorMaxPos, so that column is still considering for merging.
    float max_pos_x = label_pos.x + label_size.x + w_sort_text + w_arrow;
    column->ContentMaxXHeadersUsed = ImMax(column->ContentMaxXHeadersUsed, cell_r.Max.x);
    column->ContentMaxXHeadersIdeal = ImMax(column->ContentMaxXHeadersIdeal, max_pos_x);

    // We don't use BeginPopupContextItem() because we want the popup to stay up even after the column is hidden
    if (IsMouseReleased(1) && IsItemHovered())
        TableOpenContextMenu(column_n);
}

// Note that the NoSortAscending/NoSortDescending flags are processed in TableSortSpecsSanitize(), and they may change/revert
// the value of SortDirection. We could technically also do it here but it would be unnecessary and duplicate code.
void ImGui::TableSetColumnSortDirection(int column_n, ImGuiSortDirection sort_direction, bool append_to_sort_specs)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;

    if (!(table->Flags & ImGuiTableFlags_MultiSortable))
        append_to_sort_specs = false;

    ImS8 sort_order_max = 0;
    if (append_to_sort_specs)
        for (int other_column_n = 0; other_column_n < table->ColumnsCount; other_column_n++)
            sort_order_max = ImMax(sort_order_max, table->Columns[other_column_n].SortOrder);

    ImGuiTableColumn* column = &table->Columns[column_n];
    column->SortDirection = (ImS8)sort_direction;
    if (column->SortOrder == -1 || !append_to_sort_specs)
        column->SortOrder = append_to_sort_specs ? sort_order_max + 1 : 0;

    for (int other_column_n = 0; other_column_n < table->ColumnsCount; other_column_n++)
    {
        ImGuiTableColumn* other_column = &table->Columns[other_column_n];
        if (other_column != column && !append_to_sort_specs)
            other_column->SortOrder = -1;
        TableFixColumnSortDirection(other_column);
    }
    table->IsSettingsDirty = true;
    table->IsSortSpecsDirty = true;
}

// Return NULL if no sort specs (most often when ImGuiTableFlags_Sortable is not set)
// You can sort your data again when 'SpecsChanged == true'. It will be true with sorting specs have changed since
// last call, or the first time.
// Lifetime: don't hold on this pointer over multiple frames or past any subsequent call to BeginTable()!
ImGuiTableSortSpecs* ImGui::TableGetSortSpecs()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(table != NULL);

    if (!(table->Flags & ImGuiTableFlags_Sortable))
        return NULL;

    if (table->IsSortSpecsDirty)
        TableSortSpecsBuild(table);

    return table->SortSpecs.SpecsCount ? &table->SortSpecs : NULL;
}

bool ImGui::TableGetColumnIsSorted(int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return false;
    if (column_n < 0)
        column_n = table->CurrentColumn;
    ImGuiTableColumn* column = &table->Columns[column_n];
    return (column->SortOrder != -1);
}

// Return -1 when table is not hovered. return columns_count if the unused space at the right of visible columns is hovered.
int ImGui::TableGetHoveredColumn()
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    if (!table)
        return -1;
    return (int)table->HoveredColumnBody;
}

void ImGui::TableSetBgColor(ImGuiTableBgTarget bg_target, ImU32 color, int column_n)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = g.CurrentTable;
    IM_ASSERT(bg_target != ImGuiTableBgTarget_None);

    if (color == IM_COL32_DISABLE)
        color = 0;

    // We cannot draw neither the cell or row background immediately as we don't know the row height at this point in time.
    switch (bg_target)
    {
    case ImGuiTableBgTarget_CellBg:
    {
        if (table->RowPosY1 > table->InnerClipRect.Max.y) // Discard
            return;
        if (column_n == -1)
            column_n = table->CurrentColumn;
        if ((table->VisibleUnclippedMaskByIndex & ((ImU64)1 << column_n)) == 0)
            return;
        if (table->RowCellDataCurrent < 0 || table->RowCellData[table->RowCellDataCurrent].Column != column_n)
            table->RowCellDataCurrent++;
        ImGuiTableCellData* cell_data = &table->RowCellData[table->RowCellDataCurrent];
        cell_data->BgColor = color;
        cell_data->Column = (ImS8)column_n;
        break;
    }
    case ImGuiTableBgTarget_RowBg0:
    case ImGuiTableBgTarget_RowBg1:
    {
        if (table->RowPosY1 > table->InnerClipRect.Max.y) // Discard
            return;
        IM_ASSERT(column_n == -1);
        int bg_idx = (bg_target == ImGuiTableBgTarget_RowBg1) ? 1 : 0;
        table->RowBgColor[bg_idx] = color;
        break;
    }
    default:
        IM_ASSERT(0);
    }
}

void ImGui::TableSortSpecsSanitize(ImGuiTable* table)
{
    IM_ASSERT(table->Flags & ImGuiTableFlags_Sortable);

    // Clear SortOrder from hidden column and verify that there's no gap or duplicate.
    int sort_order_count = 0;
    ImU64 sort_order_mask = 0x00;
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (column->SortOrder != -1 && !column->IsVisible)
            column->SortOrder = -1;
        if (column->SortOrder == -1)
            continue;
        sort_order_count++;
        sort_order_mask |= ((ImU64)1 << column->SortOrder);
        IM_ASSERT(sort_order_count < (int)sizeof(sort_order_mask) * 8);
    }

    const bool need_fix_linearize = ((ImU64)1 << sort_order_count) != (sort_order_mask + 1);
    const bool need_fix_single_sort_order = (sort_order_count > 1) && !(table->Flags & ImGuiTableFlags_MultiSortable);
    if (need_fix_linearize || need_fix_single_sort_order)
    {
        ImU64 fixed_mask = 0x00;
        for (int sort_n = 0; sort_n < sort_order_count; sort_n++)
        {
            // Fix: Rewrite sort order fields if needed so they have no gap or duplicate.
            // (e.g. SortOrder 0 disappeared, SortOrder 1..2 exists --> rewrite then as SortOrder 0..1)
            int column_with_smallest_sort_order = -1;
            for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
                if ((fixed_mask & ((ImU64)1 << (ImU64)column_n)) == 0 && table->Columns[column_n].SortOrder != -1)
                    if (column_with_smallest_sort_order == -1 || table->Columns[column_n].SortOrder < table->Columns[column_with_smallest_sort_order].SortOrder)
                        column_with_smallest_sort_order = column_n;
            IM_ASSERT(column_with_smallest_sort_order != -1);
            fixed_mask |= ((ImU64)1 << column_with_smallest_sort_order);
            table->Columns[column_with_smallest_sort_order].SortOrder = (ImS8)sort_n;

            // Fix: Make sure only one column has a SortOrder if ImGuiTableFlags_MultiSortable is not set.
            if (need_fix_single_sort_order)
            {
                sort_order_count = 1;
                for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
                    if (column_n != column_with_smallest_sort_order)
                        table->Columns[column_n].SortOrder = -1;
                break;
            }
        }
    }

    // Fallback default sort order (if no column had the ImGuiTableColumnFlags_DefaultSort flag)
    if (sort_order_count == 0)
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        {
            ImGuiTableColumn* column = &table->Columns[column_n];
            if (column->IsVisible && !(column->Flags & ImGuiTableColumnFlags_NoSort))
            {
                sort_order_count = 1;
                column->SortOrder = 0;
                TableFixColumnSortDirection(column);
                break;
            }
        }

    table->SortSpecsCount = (ImS8)sort_order_count;
}

void ImGui::TableSortSpecsBuild(ImGuiTable* table)
{
    IM_ASSERT(table->IsSortSpecsDirty);
    TableSortSpecsSanitize(table);

    // Write output
    table->SortSpecsData.resize(table->SortSpecsCount);
    table->SortSpecs.ColumnsMask = 0x00;
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
    {
        ImGuiTableColumn* column = &table->Columns[column_n];
        if (column->SortOrder == -1)
            continue;
        ImGuiTableSortSpecsColumn* sort_spec = &table->SortSpecsData[column->SortOrder];
        sort_spec->ColumnUserID = column->UserID;
        sort_spec->ColumnIndex = (ImU8)column_n;
        sort_spec->SortOrder = (ImU8)column->SortOrder;
        sort_spec->SortDirection = column->SortDirection;
        table->SortSpecs.ColumnsMask |= (ImU64)1 << column_n;
    }
    table->SortSpecs.Specs = table->SortSpecsData.Data;
    table->SortSpecs.SpecsCount = table->SortSpecsData.Size;
    table->SortSpecs.SpecsDirty = true; // Mark as dirty for user
    table->IsSortSpecsDirty = false; // Mark as not dirty for us
}

//-------------------------------------------------------------------------
// TABLE - .ini settings
//-------------------------------------------------------------------------
// [Init] 1: TableSettingsHandler_ReadXXXX()   Load and parse .ini file into TableSettings.
// [Main] 2: TableLoadSettings()               When table is created, bind Table to TableSettings, serialize TableSettings data into Table.
// [Main] 3: TableSaveSettings()               When table properties are modified, serialize Table data into bound or new TableSettings, mark .ini as dirty.
// [Main] 4: TableSettingsHandler_WriteAll()   When .ini file is dirty (which can come from other source), save TableSettings into .ini file.
//-------------------------------------------------------------------------

// Clear and initialize empty settings instance
static void InitTableSettings(ImGuiTableSettings* settings, ImGuiID id, int columns_count, int columns_count_max)
{
    IM_PLACEMENT_NEW(settings) ImGuiTableSettings();
    ImGuiTableColumnSettings* settings_column = settings->GetColumnSettings();
    for (int n = 0; n < columns_count_max; n++, settings_column++)
        IM_PLACEMENT_NEW(settings_column) ImGuiTableColumnSettings();
    settings->ID = id;
    settings->ColumnsCount = (ImS8)columns_count;
    settings->ColumnsCountMax = (ImS8)columns_count_max;
    settings->WantApply = true;
}

static size_t TableSettingsCalcChunkSize(int columns_count)
{
    return sizeof(ImGuiTableSettings) + (size_t)columns_count * sizeof(ImGuiTableColumnSettings);
}

ImGuiTableSettings* ImGui::TableSettingsCreate(ImGuiID id, int columns_count)
{
    ImGuiContext& g = *GImGui;
    ImGuiTableSettings* settings = g.SettingsTables.alloc_chunk(TableSettingsCalcChunkSize(columns_count));
    InitTableSettings(settings, id, columns_count, columns_count);
    return settings;
}

// Find existing settings
ImGuiTableSettings* ImGui::TableSettingsFindByID(ImGuiID id)
{
    // FIXME-OPT: Might want to store a lookup map for this?
    ImGuiContext& g = *GImGui;
    for (ImGuiTableSettings* settings = g.SettingsTables.begin(); settings != NULL; settings = g.SettingsTables.next_chunk(settings))
        if (settings->ID == id)
            return settings;
    return NULL;
}

void ImGui::TableSettingsClearByID(ImGuiID id)
{
    if (ImGuiTableSettings* settings = TableSettingsFindByID(id))
        settings->ID = 0;
}

// Get settings for a given table, NULL if none
ImGuiTableSettings* ImGui::TableGetBoundSettings(ImGuiTable* table)
{
    if (table->SettingsOffset != -1)
    {
        ImGuiContext& g = *GImGui;
        ImGuiTableSettings* settings = g.SettingsTables.ptr_from_offset(table->SettingsOffset);
        IM_ASSERT(settings->ID == table->ID);
        if (settings->ColumnsCountMax >= table->ColumnsCount)
            return settings; // OK
        settings->ID = 0; // Invalidate storage, we won't fit because of a count change
    }
    return NULL;
}

void ImGui::TableSaveSettings(ImGuiTable* table)
{
    table->IsSettingsDirty = false;
    if (table->Flags & ImGuiTableFlags_NoSavedSettings)
        return;

    // Bind or create settings data
    ImGuiContext& g = *GImGui;
    ImGuiTableSettings* settings = TableGetBoundSettings(table);
    if (settings == NULL)
    {
        settings = TableSettingsCreate(table->ID, table->ColumnsCount);
        table->SettingsOffset = g.SettingsTables.offset_from_ptr(settings);
    }
    settings->ColumnsCount = (ImS8)table->ColumnsCount;

    // Serialize ImGuiTable/ImGuiTableColumn into ImGuiTableSettings/ImGuiTableColumnSettings
    IM_ASSERT(settings->ID == table->ID);
    IM_ASSERT(settings->ColumnsCount == table->ColumnsCount && settings->ColumnsCountMax >= settings->ColumnsCount);
    ImGuiTableColumn* column = table->Columns.Data;
    ImGuiTableColumnSettings* column_settings = settings->GetColumnSettings();

    bool save_ref_scale = false;
    settings->SaveFlags = ImGuiTableFlags_None;
    for (int n = 0; n < table->ColumnsCount; n++, column++, column_settings++)
    {
        const float width_or_weight = (column->Flags & ImGuiTableColumnFlags_WidthStretch) ? column->StretchWeight : column->WidthRequest;
        column_settings->WidthOrWeight = width_or_weight;
        column_settings->Index = (ImS8)n;
        column_settings->DisplayOrder = column->DisplayOrder;
        column_settings->SortOrder = column->SortOrder;
        column_settings->SortDirection = column->SortDirection;
        column_settings->IsVisible = column->IsVisible;
        column_settings->IsStretch = (column->Flags & ImGuiTableColumnFlags_WidthStretch) ? 1 : 0;
        if ((column->Flags & ImGuiTableColumnFlags_WidthStretch) == 0)
            save_ref_scale = true;

        // We skip saving some data in the .ini file when they are unnecessary to restore our state.
        // Note that fixed width where initial width was derived from auto-fit will always be saved as InitStretchWeightOrWidth will be 0.0f.
        // FIXME-TABLE: We don't have logic to easily compare SortOrder to DefaultSortOrder yet so it's always saved when present.
        if (width_or_weight != column->InitStretchWeightOrWidth)
            settings->SaveFlags |= ImGuiTableFlags_Resizable;
        if (column->DisplayOrder != n)
            settings->SaveFlags |= ImGuiTableFlags_Reorderable;
        if (column->SortOrder != -1)
            settings->SaveFlags |= ImGuiTableFlags_Sortable;
        if (column->IsVisible != ((column->Flags & ImGuiTableColumnFlags_DefaultHide) == 0))
            settings->SaveFlags |= ImGuiTableFlags_Hideable;
    }
    settings->SaveFlags &= table->Flags;
    settings->RefScale = save_ref_scale ? table->RefScale : 0.0f;

    MarkIniSettingsDirty();
}

void ImGui::TableLoadSettings(ImGuiTable* table)
{
    ImGuiContext& g = *GImGui;
    table->IsSettingsRequestLoad = false;
    if (table->Flags & ImGuiTableFlags_NoSavedSettings)
        return;

    // Bind settings
    ImGuiTableSettings* settings;
    if (table->SettingsOffset == -1)
    {
        settings = TableSettingsFindByID(table->ID);
        if (settings == NULL)
            return;
        if (settings->ColumnsCount != table->ColumnsCount) // Allow settings if columns count changed. We could otherwise decide to return...
            table->IsSettingsDirty = true;
        table->SettingsOffset = g.SettingsTables.offset_from_ptr(settings);
    }
    else
    {
        settings = TableGetBoundSettings(table);
    }

    table->SettingsLoadedFlags = settings->SaveFlags;
    table->RefScale = settings->RefScale;

    // Serialize ImGuiTableSettings/ImGuiTableColumnSettings into ImGuiTable/ImGuiTableColumn
    ImGuiTableColumnSettings* column_settings = settings->GetColumnSettings();
    for (int data_n = 0; data_n < settings->ColumnsCount; data_n++, column_settings++)
    {
        int column_n = column_settings->Index;
        if (column_n < 0 || column_n >= table->ColumnsCount)
            continue;

        ImGuiTableColumn* column = &table->Columns[column_n];
        if (settings->SaveFlags & ImGuiTableFlags_Resizable)
        {
            if (column_settings->IsStretch)
                column->StretchWeight = column_settings->WidthOrWeight;
            else
                column->WidthRequest = column_settings->WidthOrWeight;
            column->AutoFitQueue = 0x00;
        }
        if (settings->SaveFlags & ImGuiTableFlags_Reorderable)
            column->DisplayOrder = column_settings->DisplayOrder;
        else
            column->DisplayOrder = (ImS8)column_n;
        column->IsVisible = column->IsVisibleNextFrame = column_settings->IsVisible;
        column->SortOrder = column_settings->SortOrder;
        column->SortDirection = column_settings->SortDirection;
    }

    // FIXME-TABLE: Need to validate .ini data
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = (ImS8)column_n;
}

static void TableSettingsHandler_ClearAll(ImGuiContext* ctx, ImGuiSettingsHandler*)
{
    ImGuiContext& g = *ctx;
    for (int i = 0; i != g.Tables.GetSize(); i++)
        g.Tables.GetByIndex(i)->SettingsOffset = -1;
    g.SettingsTables.clear();
}

// Apply to existing windows (if any)
static void TableSettingsHandler_ApplyAll(ImGuiContext* ctx, ImGuiSettingsHandler*)
{
    ImGuiContext& g = *ctx;
    for (int i = 0; i != g.Tables.GetSize(); i++)
    {
        ImGuiTable* table = g.Tables.GetByIndex(i);
        table->IsSettingsRequestLoad = true;
        table->SettingsOffset = -1;
    }
}

static void* TableSettingsHandler_ReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name)
{
    ImGuiID id = 0;
    int columns_count = 0;
    if (sscanf(name, "0x%08X,%d", &id, &columns_count) < 2)
        return NULL;

    if (ImGuiTableSettings* settings = ImGui::TableSettingsFindByID(id))
    {
        if (settings->ColumnsCountMax >= columns_count)
        {
            InitTableSettings(settings, id, columns_count, settings->ColumnsCountMax); // Recycle
            return settings;
        }
        settings->ID = 0; // Invalidate storage, we won't fit because of a count change
    }
    return ImGui::TableSettingsCreate(id, columns_count);
}

static void TableSettingsHandler_ReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
{
    // "Column 0  UserID=0x42AD2D21 Width=100 Visible=1 Order=0 Sort=0v"
    ImGuiTableSettings* settings = (ImGuiTableSettings*)entry;
    float f = 0.0f;
    int column_n = 0, r = 0, n = 0;

    if (sscanf(line, "RefScale=%f", &f) == 1) { settings->RefScale = f; return; }

    if (sscanf(line, "Column %d%n", &column_n, &r) == 1)
    {
        if (column_n < 0 || column_n >= settings->ColumnsCount)
            return;
        line = ImStrSkipBlank(line + r);
        char c = 0;
        ImGuiTableColumnSettings* column = settings->GetColumnSettings() + column_n;
        column->Index = (ImS8)column_n;
        if (sscanf(line, "UserID=0x%08X%n", (ImU32*)&n, &r)==1) { line = ImStrSkipBlank(line + r); column->UserID = (ImGuiID)n; }
        if (sscanf(line, "Width=%d%n", &n, &r) == 1)            { line = ImStrSkipBlank(line + r); column->WidthOrWeight = (float)n; column->IsStretch = 0; settings->SaveFlags |= ImGuiTableFlags_Resizable; }
        if (sscanf(line, "Weight=%f%n", &f, &r) == 1)           { line = ImStrSkipBlank(line + r); column->WidthOrWeight = f; column->IsStretch = 1; settings->SaveFlags |= ImGuiTableFlags_Resizable; }
        if (sscanf(line, "Visible=%d%n", &n, &r) == 1)          { line = ImStrSkipBlank(line + r); column->IsVisible = (ImU8)n; settings->SaveFlags |= ImGuiTableFlags_Hideable; }
        if (sscanf(line, "Order=%d%n", &n, &r) == 1)            { line = ImStrSkipBlank(line + r); column->DisplayOrder = (ImS8)n; settings->SaveFlags |= ImGuiTableFlags_Reorderable; }
        if (sscanf(line, "Sort=%d%c%n", &n, &c, &r) == 2)       { line = ImStrSkipBlank(line + r); column->SortOrder = (ImS8)n; column->SortDirection = (c == '^') ? ImGuiSortDirection_Descending : ImGuiSortDirection_Ascending; settings->SaveFlags |= ImGuiTableFlags_Sortable; }
    }
}

static void TableSettingsHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    ImGuiContext& g = *ctx;
    for (ImGuiTableSettings* settings = g.SettingsTables.begin(); settings != NULL; settings = g.SettingsTables.next_chunk(settings))
    {
        if (settings->ID == 0) // Skip ditched settings
            continue;

        // TableSaveSettings() may clear some of those flags when we establish that the data can be stripped
        // (e.g. Order was unchanged)
        const bool save_size    = (settings->SaveFlags & ImGuiTableFlags_Resizable) != 0;
        const bool save_visible = (settings->SaveFlags & ImGuiTableFlags_Hideable) != 0;
        const bool save_order   = (settings->SaveFlags & ImGuiTableFlags_Reorderable) != 0;
        const bool save_sort    = (settings->SaveFlags & ImGuiTableFlags_Sortable) != 0;
        if (!save_size && !save_visible && !save_order && !save_sort)
            continue;

        buf->reserve(buf->size() + 30 + settings->ColumnsCount * 50); // ballpark reserve
        buf->appendf("[%s][0x%08X,%d]\n", handler->TypeName, settings->ID, settings->ColumnsCount);
        if (settings->RefScale != 0.0f)
            buf->appendf("RefScale=%g\n", settings->RefScale);
        ImGuiTableColumnSettings* column = settings->GetColumnSettings();
        for (int column_n = 0; column_n < settings->ColumnsCount; column_n++, column++)
        {
            // "Column 0  UserID=0x42AD2D21 Width=100 Visible=1 Order=0 Sort=0v"
            buf->appendf("Column %-2d", column_n);
            if (column->UserID != 0)                    buf->appendf(" UserID=%08X", column->UserID);
            if (save_size && column->IsStretch)         buf->appendf(" Weight=%.4f", column->WidthOrWeight);
            if (save_size && !column->IsStretch)        buf->appendf(" Width=%d", (int)column->WidthOrWeight);
            if (save_visible)                           buf->appendf(" Visible=%d", column->IsVisible);
            if (save_order)                             buf->appendf(" Order=%d", column->DisplayOrder);
            if (save_sort && column->SortOrder != -1)   buf->appendf(" Sort=%d%c", column->SortOrder, (column->SortDirection == ImGuiSortDirection_Ascending) ? 'v' : '^');
            buf->append("\n");
        }
        buf->append("\n");
    }
}

void    ImGui::TableSettingsInstallHandler(ImGuiContext* context)
{
    ImGuiContext& g = *context;
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "Table";
    ini_handler.TypeHash = ImHashStr("Table");
    ini_handler.ClearAllFn = TableSettingsHandler_ClearAll;
    ini_handler.ReadOpenFn = TableSettingsHandler_ReadOpen;
    ini_handler.ReadLineFn = TableSettingsHandler_ReadLine;
    ini_handler.ApplyAllFn = TableSettingsHandler_ApplyAll;
    ini_handler.WriteAllFn = TableSettingsHandler_WriteAll;
    g.SettingsHandlers.push_back(ini_handler);
}

//-------------------------------------------------------------------------
// TABLE - Garbage Collection
//-------------------------------------------------------------------------

// Remove Table (currently only used by TestEngine)
void    ImGui::TableRemove(ImGuiTable* table)
{
    //IMGUI_DEBUG_LOG("TableRemove() id=0x%08X\n", table->ID);
    ImGuiContext& g = *GImGui;
    int table_idx = g.Tables.GetIndex(table);
    //memset(table->RawData.Data, 0, table->RawData.size_in_bytes());
    //memset(table, 0, sizeof(ImGuiTable));
    g.Tables.Remove(table->ID, table);
    g.TablesLastTimeActive[table_idx] = -1.0f;
}

// Free up/compact internal Table buffers for when it gets unused
void    ImGui::TableGcCompactTransientBuffers(ImGuiTable* table)
{
    //IMGUI_DEBUG_LOG("TableGcCompactTransientBuffers() id=0x%08X\n", table->ID);
    ImGuiContext& g = *GImGui;
    IM_ASSERT(table->MemoryCompacted == false);
    table->DrawSplitter.ClearFreeMemory();
    table->SortSpecsData.clear();
    table->SortSpecs.Specs = NULL;
    table->IsSortSpecsDirty = true;
    table->ColumnsNames.clear();
    table->MemoryCompacted = true;
    for (int n = 0; n < table->ColumnsCount; n++)
        table->Columns[n].NameOffset = -1;
    g.TablesLastTimeActive[g.Tables.GetIndex(table)] = -1.0f;
}

// Compact and remove unused settings data (currently only used by TestEngine)
void ImGui::TableGcCompactSettings()
{
    ImGuiContext& g = *GImGui;
    int required_memory = 0;
    for (ImGuiTableSettings* settings = g.SettingsTables.begin(); settings != NULL; settings = g.SettingsTables.next_chunk(settings))
        if (settings->ID != 0)
            required_memory += (int)TableSettingsCalcChunkSize(settings->ColumnsCount);
    if (required_memory == g.SettingsTables.Buf.Size)
        return;
    ImChunkStream<ImGuiTableSettings> new_chunk_stream;
    new_chunk_stream.Buf.reserve(required_memory);
    for (ImGuiTableSettings* settings = g.SettingsTables.begin(); settings != NULL; settings = g.SettingsTables.next_chunk(settings))
        if (settings->ID != 0)
            memcpy(new_chunk_stream.alloc_chunk(TableSettingsCalcChunkSize(settings->ColumnsCount)), settings, TableSettingsCalcChunkSize(settings->ColumnsCount));
    g.SettingsTables.swap(new_chunk_stream);
}

//-------------------------------------------------------------------------
// TABLE - Debugging
//-------------------------------------------------------------------------
// - DebugNodeTable() [Internal]
//-------------------------------------------------------------------------

#ifndef IMGUI_DISABLE_METRICS_WINDOW

void ImGui::DebugNodeTable(ImGuiTable* table)
{
    char buf[512];
    char* p = buf;
    const char* buf_end = buf + IM_ARRAYSIZE(buf);
    const bool is_active = (table->LastFrameActive >= ImGui::GetFrameCount() - 2);
    ImFormatString(p, buf_end - p, "Table 0x%08X (%d columns, in '%s')%s", table->ID, table->ColumnsCount, table->OuterWindow->Name, is_active ? "" : " *Inactive*");
    if (!is_active) { PushStyleColor(ImGuiCol_Text, GetStyleColorVec4(ImGuiCol_TextDisabled)); }
    bool open = TreeNode(table, "%s", buf);
    if (!is_active) { PopStyleColor(); }
    if (IsItemHovered())
        GetForegroundDrawList()->AddRect(table->OuterRect.Min, table->OuterRect.Max, IM_COL32(255, 255, 0, 255));
    if (!open)
        return;
    BulletText("OuterRect: Pos: (%.1f,%.1f) Size: (%.1f,%.1f)", table->OuterRect.Min.x, table->OuterRect.Min.y, table->OuterRect.GetWidth(), table->OuterRect.GetHeight());
    BulletText("ColumnsWidth: %.1f, AutoFitWidth: %.1f, InnerWidth: %.1f%s", table->ColumnsTotalWidth, table->ColumnsAutoFitWidth, table->InnerWidth, table->InnerWidth == 0.0f ? " (auto)" : "");
    BulletText("CellPaddingX: %.1f, CellSpacingX: %.1f/%.1f, OuterPaddingX: %.1f", table->CellPaddingX, table->CellSpacingX1, table->CellSpacingX2, table->OuterPaddingX);
    BulletText("HoveredColumnBody: %d, HoveredColumnBorder: %d", table->HoveredColumnBody, table->HoveredColumnBorder);
    BulletText("ResizedColumn: %d, ReorderColumn: %d, HeldHeaderColumn: %d", table->ResizedColumn, table->ReorderColumn, table->HeldHeaderColumn);
    //BulletText("BgDrawChannels: %d/%d", 0, table->BgDrawChannelUnfrozen);
    for (int n = 0; n < table->ColumnsCount; n++)
    {
        ImGuiTableColumn* column = &table->Columns[n];
        const char* name = TableGetColumnName(table, n);
        ImFormatString(buf, IM_ARRAYSIZE(buf),
            "Column %d order %d name '%s': offset %+.2f to %+.2f\n"
            "Visible: %d, Clipped: %d, DrawChannels: %d,%d\n"
            "WidthGiven: %.1f, Request/Auto: %.1f/%.1f, StretchWeight: %.3f\n"
            "MinX: %.1f, MaxX: %.1f (%+.1f), ClipRect: %.1f to %.1f (+%.1f)\n"
            "ContentWidth: %.1f,%.1f, HeadersUsed/Ideal %.1f/%.1f\n"
            "SortOrder: %d, SortDir: %s\n"
            "UserID: 0x%08X, Flags: 0x%04X: %s%s%s%s..",
            n, column->DisplayOrder, name, column->MinX - table->WorkRect.Min.x, column->MaxX - table->WorkRect.Min.x,
            column->IsVisible, column->IsClipped, column->DrawChannelFrozen, column->DrawChannelUnfrozen,
            column->WidthGiven, column->WidthRequest, column->WidthAuto, column->StretchWeight,
            column->MinX, column->MaxX, column->MaxX - column->MinX, column->ClipRect.Min.x, column->ClipRect.Max.x, column->ClipRect.Max.x - column->ClipRect.Min.x,
            column->ContentMaxXFrozen - column->WorkMinX, column->ContentMaxXUnfrozen - column->WorkMinX, column->ContentMaxXHeadersUsed - column->WorkMinX, column->ContentMaxXHeadersIdeal - column->WorkMinX,
            column->SortOrder, (column->SortDirection == ImGuiSortDirection_Ascending) ? "Ascending" : (column->SortDirection == ImGuiSortDirection_Descending) ? "Descending" : "None",
            column->UserID, column->Flags,
            (column->Flags & ImGuiTableColumnFlags_WidthFixed) ? "WidthFixed " : "",
            (column->Flags & ImGuiTableColumnFlags_WidthStretch) ? "WidthStretch " : "",
            (column->Flags & ImGuiTableColumnFlags_WidthAlwaysAutoResize) ? "WidthAlwaysAutoResize " : "",
            (column->Flags & ImGuiTableColumnFlags_NoResize) ? "NoResize " : "");
        Bullet();
        Selectable(buf);
        if (IsItemHovered())
        {
            ImRect r(column->MinX, table->OuterRect.Min.y, column->MaxX, table->OuterRect.Max.y);
            GetForegroundDrawList()->AddRect(r.Min, r.Max, IM_COL32(255, 255, 0, 255));
        }
    }
    if (ImGuiTableSettings* settings = TableGetBoundSettings(table))
        DebugNodeTableSettings(settings);
    TreePop();
}

void ImGui::DebugNodeTableSettings(ImGuiTableSettings* settings)
{
    if (!TreeNode((void*)(intptr_t)settings->ID, "Settings 0x%08X (%d columns)", settings->ID, settings->ColumnsCount))
        return;
    BulletText("SaveFlags: 0x%08X", settings->SaveFlags);
    BulletText("ColumnsCount: %d (max %d)", settings->ColumnsCount, settings->ColumnsCountMax);
    for (int n = 0; n < settings->ColumnsCount; n++)
    {
        ImGuiTableColumnSettings* column_settings = &settings->GetColumnSettings()[n];
        ImGuiSortDirection sort_dir = (column_settings->SortOrder != -1) ? (ImGuiSortDirection)column_settings->SortDirection : ImGuiSortDirection_None;
        BulletText("Column %d Order %d SortOrder %d %s Vis %d %s %7.3f UserID 0x%08X",
            n, column_settings->DisplayOrder, column_settings->SortOrder,
            (sort_dir == ImGuiSortDirection_Ascending) ? "Asc" : (sort_dir == ImGuiSortDirection_Descending) ? "Des" : "---",
            column_settings->IsVisible, column_settings->IsStretch ? "Weight" : "Width ", column_settings->WidthOrWeight, column_settings->UserID);
    }
    TreePop();
}

#endif // #ifndef IMGUI_DISABLE_METRICS_WINDOW

//-------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE
