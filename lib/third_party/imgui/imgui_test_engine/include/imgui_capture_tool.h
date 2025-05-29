// dear imgui test engine
// (screen/video capture tool)
// This is usable as a standalone applet or controlled by the test engine.

// This file is governed by the "Dear ImGui Test Engine License".
// Details of the license are provided in the LICENSE.txt file in the same directory.

#pragma once

// Need "imgui_te_engine.h" included for ImFuncPtr

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

// Our types
struct ImGuiCaptureArgs;                // Parameters for Capture
struct ImGuiCaptureContext;             // State of an active capture tool
struct ImGuiCaptureImageBuf;            // Simple helper to store an RGBA image in memory
struct ImGuiCaptureToolUI;              // Capture tool instance + UI window

typedef unsigned int ImGuiCaptureFlags; // See enum: ImGuiCaptureFlags_

// Capture function which needs to be provided by user application
typedef bool (ImGuiScreenCaptureFunc)(ImGuiID viewport_id, int x, int y, int w, int h, unsigned int* pixels, void* user_data);

// External types
struct ImGuiWindow; // imgui.h

//-----------------------------------------------------------------------------

// [Internal]
// Helper class for simple bitmap manipulation (not particularly efficient!)
struct IMGUI_API ImGuiCaptureImageBuf
{
    int             Width;
    int             Height;
    unsigned int*   Data;       // RGBA8

    ImGuiCaptureImageBuf()      { Width = Height = 0; Data = nullptr; }
    ~ImGuiCaptureImageBuf()     { Clear(); }

    void Clear();                                           // Free allocated memory buffer if such exists.
    void CreateEmpty(int w, int h);                         // Reallocate buffer for pixel data and zero it.
    bool SaveFile(const char* filename);                    // Save pixel data to specified image file.
    void RemoveAlpha();                                     // Clear alpha channel from all pixels.
};

enum ImGuiCaptureFlags_ : unsigned int
{
    ImGuiCaptureFlags_None                      = 0,
    ImGuiCaptureFlags_StitchAll                 = 1 << 0,   // Capture entire window scroll area (by scrolling and taking multiple screenshot). Only works for a single window.
    ImGuiCaptureFlags_IncludeOtherWindows       = 1 << 1,   // Disable hiding other windows (when CaptureAddWindow has been called by default other windows are hidden)
    ImGuiCaptureFlags_IncludePopups             = 1 << 2,   // Expand capture area to automatically include visible popups (Unused if ImGuiCaptureFlags_IncludeOtherWindows is set)
    ImGuiCaptureFlags_HideMouseCursor           = 1 << 3,   // Hide render software mouse cursor during capture.
    ImGuiCaptureFlags_Instant                   = 1 << 4,   // Perform capture on very same frame. Only works when capturing a rectangular region. Unsupported features: content stitching, window hiding, window relocation.
    ImGuiCaptureFlags_NoSave                    = 1 << 5    // Do not save output image.
};

// Defines input and output arguments for capture process.
// When capturing from tests you can usually use the ImGuiTestContext::CaptureXXX() helpers functions.
struct ImGuiCaptureArgs
{
    // [Input]
    ImGuiCaptureFlags       InFlags = 0;                    // Flags for customizing behavior of screenshot tool.
    ImVector<ImGuiWindow*>  InCaptureWindows;               // Windows to capture. All other windows will be hidden. May be used with InCaptureRect to capture only some windows in specified rect.
    ImRect                  InCaptureRect;                  // Screen rect to capture. Does not include padding.
    float                   InPadding = 16.0f;              // Extra padding at the edges of the screenshot. Ensure that there is available space around capture rect horizontally, also vertically if ImGuiCaptureFlags_StitchFullContents is not used.
    char                    InOutputFile[256] = "";         // Output will be saved to a file if InOutputImageBuf is nullptr.
    ImGuiCaptureImageBuf*   InOutputImageBuf = nullptr;     // _OR_ Output will be saved to image buffer if specified.
    int                     InRecordFPSTarget = 30;         // FPS target for recording videos.
    int                     InSizeAlign = 0;                // Resolution alignment (0 = auto, 1 = no alignment, >= 2 = align width/height to be multiple of given value)

    // [Output]
    ImVec2                  OutImageSize;                   // Produced image size.
};

enum ImGuiCaptureStatus
{
    ImGuiCaptureStatus_InProgress,
    ImGuiCaptureStatus_Done,
    ImGuiCaptureStatus_Error
};

struct ImGuiCaptureWindowData
{
    ImGuiWindow*            Window;
    ImRect                  BackupRect;
    ImVec2                  PosDuringCapture;
};

// Implements functionality for capturing images
struct IMGUI_API ImGuiCaptureContext
{
    // IO
    ImFuncPtr(ImGuiScreenCaptureFunc) ScreenCaptureFunc = nullptr;  // Graphics backend specific function that captures specified portion of framebuffer and writes RGBA data to `pixels` buffer.
    void*                   ScreenCaptureUserData = nullptr;        // Custom user pointer which is passed to ScreenCaptureFunc. (Optional)
    char*                   VideoCaptureEncoderPath = nullptr;      // Video encoder path (not owned, stored externally).
    int                     VideoCaptureEncoderPathSize = 0;        // Optional. Set in order to edit this parameter from UI.
    char*                   VideoCaptureEncoderParams = nullptr;    // Video encoder params (not owned, stored externally).
    int                     VideoCaptureEncoderParamsSize = 0;      // Optional. Set in order to edit this parameter from UI.
    char*                   GifCaptureEncoderParams = nullptr;      // Video encoder params for GIF output (not owned, stored externally).
    int                     GifCaptureEncoderParamsSize = 0;        // Optional. Set in order to edit this parameter from UI.

    // [Internal]
    ImRect                  _CaptureRect;                   // Viewport rect that is being captured.
    ImRect                  _CapturedWindowRect;            // Top-left corner of region that covers all windows included in capture. This is not same as _CaptureRect.Min when capturing explicitly specified rect.
    int                     _ChunkNo = 0;                   // Number of chunk that is being captured when capture spans multiple frames.
    int                     _FrameNo = 0;                   // Frame number during capture process that spans multiple frames.
    ImVec2                  _MouseRelativeToWindowPos;      // Mouse cursor position relative to captured window (when _StitchAll is in use).
    ImGuiWindow*            _HoveredWindow = nullptr;       // Window which was hovered at capture start.
    ImGuiCaptureImageBuf    _CaptureBuf;                    // Output image buffer.
    const ImGuiCaptureArgs* _CaptureArgs = nullptr;         // Current capture args. Set only if capture is in progress.
    ImVector<ImGuiCaptureWindowData> _WindowsData;          // Backup windows that will have their rect modified and restored. args->InCaptureWindows can not be used because popups may get closed during capture and no longer appear in that list.

    // [Internal] Video recording
    bool                    _VideoRecording = false;        // Flag indicating that video recording is in progress.
    double                  _VideoLastFrameTime = 0;        // Time when last video frame was recorded.
    FILE*                   _VideoEncoderPipe = nullptr;    // File writing to stdin of video encoder process.

    // [Internal] Backups
    bool                    _BackupMouseDrawCursor = false; // Initial value of g.IO.MouseDrawCursor
    ImVec2                  _BackupDisplayWindowPadding;    // Backup padding. We set it to {0, 0} during capture.
    ImVec2                  _BackupDisplaySafeAreaPadding;  // Backup padding. We set it to {0, 0} during capture.

    //-------------------------------------------------------------------------
    // Functions
    //-------------------------------------------------------------------------

    ImGuiCaptureContext(ImGuiScreenCaptureFunc capture_func = nullptr) { ScreenCaptureFunc = capture_func; _MouseRelativeToWindowPos = ImVec2(-FLT_MAX, -FLT_MAX); }

    // These functions should be called from appropriate context hooks. See ImGui::AddContextHook() for more info.
    // (ImGuiTestEngine automatically calls that for you, so this only apply to independently created instance)
    void                    PreNewFrame();
    void                    PreRender();
    void                    PostRender();

    // Update capturing. If this function returns true then it should be called again with same arguments on the next frame.
    ImGuiCaptureStatus      CaptureUpdate(ImGuiCaptureArgs* args);
    void                    RestoreBackedUpData();
    void                    ClearState();

    // Begin video capture. Call CaptureUpdate() every frame afterwards until it returns false.
    void                    BeginVideoCapture(ImGuiCaptureArgs* args);
    void                    EndVideoCapture();
    bool                    IsCapturingVideo();
    bool                    IsCapturing();
};

//-----------------------------------------------------------------------------
// ImGuiCaptureToolUI
//-----------------------------------------------------------------------------

// Implements UI for capturing images
// (when using ImGuiTestEngine scripting API you may not need to use this at all)
struct IMGUI_API ImGuiCaptureToolUI
{
    float               SnapGridSize = 32.0f;               // Size of the grid cell for "snap to grid" functionality.
    char                OutputLastFilename[256] = "";       // File name of last captured file.
    char*               VideoCaptureExtension = nullptr;    // Video file extension (e.g. ".gif" or ".mp4")
    int                 VideoCaptureExtensionSize = 0;      // Optional. Set in order to edit this parameter from UI.

    ImGuiCaptureArgs    _CaptureArgs;                       // Capture args
    bool                _StateIsPickingWindow = false;
    bool                _StateIsCapturing = false;
    ImVector<ImGuiID>   _SelectedWindows;
    char                _OutputFileTemplate[256] = "";      //
    int                 _FileCounter = 0;                   // Counter which may be appended to file name when saving. By default, counting starts from 1. When done this field holds number of saved files.

    // Public
    ImGuiCaptureToolUI();
    void    ShowCaptureToolWindow(ImGuiCaptureContext* context, bool* p_open = nullptr);   // Render a capture tool window with various options and utilities.

    // [Internal]
    void    _CaptureWindowPicker(ImGuiCaptureArgs* args);       // Render a window picker that captures picked window to file specified in file_name.
    void    _CaptureWindowsSelector(ImGuiCaptureContext* context, ImGuiCaptureArgs* args);    // Render a selector for selecting multiple windows for capture.
    void    _SnapWindowsToGrid(float cell_size);                // Snap edges of all visible windows to a virtual grid.
    bool    _InitializeOutputFile();                            // Format output file template into capture args struct and ensure target directory exists.
    bool    _ShowEncoderConfigFields(ImGuiCaptureContext* context);
};

#define IMGUI_CAPTURE_DEFAULT_VIDEO_PARAMS_FOR_FFMPEG   "-hide_banner -loglevel error -r $FPS -f rawvideo -pix_fmt rgba -s $WIDTHx$HEIGHT -i - -threads 0 -y -preset ultrafast -pix_fmt yuv420p -crf 20 $OUTPUT"
#define IMGUI_CAPTURE_DEFAULT_GIF_PARAMS_FOR_FFMPEG     "-hide_banner -loglevel error -r $FPS -f rawvideo -pix_fmt rgba -s $WIDTHx$HEIGHT -i - -threads 0 -y -filter_complex \"split=2 [a] [b]; [a] palettegen [pal]; [b] [pal] paletteuse\" $OUTPUT"
