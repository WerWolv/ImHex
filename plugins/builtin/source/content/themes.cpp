#include <hex/api/theme_manager.hpp>
#include <hex/api/event_manager.hpp>

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/helpers/default_paths.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <imnodes.h>
#include <TextEditor.h>
#include <romfs/romfs.hpp>

#include <wolv/io/file.hpp>

namespace hex::plugin::builtin {

    void registerThemeHandlers() {
        RequestInitThemeHandlers::subscribe([] {
            {
                const static ThemeManager::ColorMap ImGuiColorMap = {
                    { "text",                           ImGuiCol_Text                   },
                    { "text-disabled",                  ImGuiCol_TextDisabled           },
                    { "window-background",              ImGuiCol_WindowBg               },
                    { "child-background",               ImGuiCol_ChildBg                },
                    { "popup-background",               ImGuiCol_PopupBg                },
                    { "border",                         ImGuiCol_Border                 },
                    { "border-shadow",                  ImGuiCol_BorderShadow           },
                    { "frame-background",               ImGuiCol_FrameBg                },
                    { "frame-background-hovered",       ImGuiCol_FrameBgHovered         },
                    { "frame-background-active",        ImGuiCol_FrameBgActive          },
                    { "title-background",               ImGuiCol_TitleBg                },
                    { "title-background-active",        ImGuiCol_TitleBgActive          },
                    { "title-background-collapse",      ImGuiCol_TitleBgCollapsed       },
                    { "menu-bar-background",            ImGuiCol_MenuBarBg              },
                    { "scrollbar-background",           ImGuiCol_ScrollbarBg            },
                    { "scrollbar-grab",                 ImGuiCol_ScrollbarGrab          },
                    { "scrollbar-grab-hovered",         ImGuiCol_ScrollbarGrabHovered   },
                    { "scrollbar-grab-active",          ImGuiCol_ScrollbarGrabActive    },
                    { "check-mark",                     ImGuiCol_CheckMark              },
                    { "slider-grab",                    ImGuiCol_SliderGrab             },
                    { "slider-grab-active",             ImGuiCol_SliderGrabActive       },
                    { "button",                         ImGuiCol_Button                 },
                    { "button-hovered",                 ImGuiCol_ButtonHovered          },
                    { "button-active",                  ImGuiCol_ButtonActive           },
                    { "header",                         ImGuiCol_Header                 },
                    { "header-hovered",                 ImGuiCol_HeaderHovered          },
                    { "header-active",                  ImGuiCol_HeaderActive           },
                    { "separator",                      ImGuiCol_Separator              },
                    { "separator-hovered",              ImGuiCol_SeparatorHovered       },
                    { "separator-active",               ImGuiCol_SeparatorActive        },
                    { "resize-grip",                    ImGuiCol_ResizeGrip             },
                    { "resize-grip-hovered",            ImGuiCol_ResizeGripHovered      },
                    { "resize-grip-active",             ImGuiCol_ResizeGripActive       },
                    { "tab",                            ImGuiCol_Tab                    },
                    { "tab-hovered",                    ImGuiCol_TabHovered             },
                    { "tab-active",                     ImGuiCol_TabSelected            },
                    { "tab-unfocused",                  ImGuiCol_TabDimmed              },
                    { "tab-unfocused-active",           ImGuiCol_TabDimmedSelected      },
                    { "docking-preview",                ImGuiCol_DockingPreview         },
                    { "docking-empty-background",       ImGuiCol_DockingEmptyBg         },
                    { "plot-lines",                     ImGuiCol_PlotLines              },
                    { "plot-lines-hovered",             ImGuiCol_PlotLinesHovered       },
                    { "plot-histogram",                 ImGuiCol_PlotHistogram          },
                    { "plot-histogram-hovered",         ImGuiCol_PlotHistogramHovered   },
                    { "table-header-background",        ImGuiCol_TableHeaderBg          },
                    { "table-border-strong",            ImGuiCol_TableBorderStrong      },
                    { "table-border-light",             ImGuiCol_TableBorderLight       },
                    { "table-row-background",           ImGuiCol_TableRowBg             },
                    { "table-row-background-alt",       ImGuiCol_TableRowBgAlt          },
                    { "text-selected-background",       ImGuiCol_TextSelectedBg         },
                    { "drag-drop-target",               ImGuiCol_DragDropTarget         },
                    { "nav-highlight",                  ImGuiCol_NavCursor              },
                    { "nav-windowing-highlight",        ImGuiCol_NavWindowingHighlight  },
                    { "nav-windowing-background",       ImGuiCol_NavWindowingDimBg      },
                    { "modal-window-dim-background",    ImGuiCol_ModalWindowDimBg       },
                    { "window-shadow",                  ImGuiCol_WindowShadow           }
                };

                ThemeManager::addThemeHandler("imgui", ImGuiColorMap,
                    [](u32 colorId) -> ImColor {
                        return ImGui::GetStyle().Colors[colorId];
                    },
                    [](u32 colorId, ImColor color) {
                        ImGui::GetStyle().Colors[colorId] = color;
                    }
                );
            }

            {
                const static ThemeManager::ColorMap ImPlotColorMap = {
                    { "line",               ImPlotCol_Line              },
                    { "fill",               ImPlotCol_Fill              },
                    { "marker-outline",     ImPlotCol_MarkerOutline     },
                    { "marker-fill",        ImPlotCol_MarkerFill        },
                    { "error-bar",          ImPlotCol_ErrorBar          },
                    { "frame-bg",           ImPlotCol_FrameBg           },
                    { "plot-bg",            ImPlotCol_PlotBg            },
                    { "plot-border",        ImPlotCol_PlotBorder        },
                    { "legend-bg",          ImPlotCol_LegendBg          },
                    { "legend-border",      ImPlotCol_LegendBorder      },
                    { "legend-text",        ImPlotCol_LegendText        },
                    { "title-text",         ImPlotCol_TitleText         },
                    { "inlay-text",         ImPlotCol_InlayText         },
                    { "axis-text",          ImPlotCol_AxisText          },
                    { "axis-grid",          ImPlotCol_AxisGrid          },
                    { "axis-tick",          ImPlotCol_AxisTick          },
                    { "axis-bg",            ImPlotCol_AxisBg            },
                    { "axis-bg-hovered",    ImPlotCol_AxisBgHovered     },
                    { "axis-bg-active",     ImPlotCol_AxisBgActive      },
                    { "selection",          ImPlotCol_Selection         },
                    { "crosshairs",         ImPlotCol_Crosshairs        }
                };

                ThemeManager::addThemeHandler("implot", ImPlotColorMap,
                   [](u32 colorId) -> ImColor {
                       return ImPlot::GetStyle().Colors[colorId];
                   },
                   [](u32 colorId, ImColor color) {
                       ImPlot::GetStyle().Colors[colorId] = color;
                   }
                );
            }

            {
                const static ThemeManager::ColorMap ImNodesColorMap = {
                    { "node-background",                    ImNodesCol_NodeBackground                   },
                    { "node-background-hovered",            ImNodesCol_NodeBackgroundHovered            },
                    { "node-background-selected",           ImNodesCol_NodeBackgroundSelected           },
                    { "node-outline",                       ImNodesCol_NodeOutline                      },
                    { "title-bar",                          ImNodesCol_TitleBar                         },
                    { "title-bar-hovered",                  ImNodesCol_TitleBarHovered                  },
                    { "title-bar-selected",                 ImNodesCol_TitleBarSelected                 },
                    { "link",                               ImNodesCol_Link                             },
                    { "link-hovered",                       ImNodesCol_LinkHovered                      },
                    { "link-selected",                      ImNodesCol_LinkSelected                     },
                    { "pin",                                ImNodesCol_Pin                              },
                    { "pin-hovered",                        ImNodesCol_PinHovered                       },
                    { "box-selector",                       ImNodesCol_BoxSelector                      },
                    { "box-selector-outline",               ImNodesCol_BoxSelectorOutline               },
                    { "grid-background",                    ImNodesCol_GridBackground                   },
                    { "grid-line",                          ImNodesCol_GridLine                         },
                    { "grid-line-primary",                  ImNodesCol_GridLinePrimary                  },
                    { "mini-map-background",                ImNodesCol_MiniMapBackground                },
                    { "mini-map-background-hovered",        ImNodesCol_MiniMapBackgroundHovered         },
                    { "mini-map-outline",                   ImNodesCol_MiniMapOutline                   },
                    { "mini-map-outline-hovered",           ImNodesCol_MiniMapOutlineHovered            },
                    { "mini-map-node-background",           ImNodesCol_MiniMapNodeBackground            },
                    { "mini-map-node-background-hovered",   ImNodesCol_MiniMapNodeBackgroundHovered     },
                    { "mini-map-node-background-selected",  ImNodesCol_MiniMapNodeBackgroundSelected    },
                    { "mini-map-node-outline",              ImNodesCol_MiniMapNodeOutline               },
                    { "mini-map-link",                      ImNodesCol_MiniMapLink                      },
                    { "mini-map-link-selected",             ImNodesCol_MiniMapLinkSelected              },
                    { "mini-map-canvas",                    ImNodesCol_MiniMapCanvas                    },
                    { "mini-map-canvas-outline",            ImNodesCol_MiniMapCanvasOutline             },
                };

                ThemeManager::addThemeHandler("imnodes", ImNodesColorMap,
                   [](u32 colorId) -> ImColor {
                       return ImNodes::GetStyle().Colors[colorId];
                   },
                   [](u32 colorId, ImColor color) {
                       ImNodes::GetStyle().Colors[colorId] = color;
                   }
                );
            }

            {
                const static ThemeManager::ColorMap ImHexColorMap = {
                    { "desc-button",                ImGuiCustomCol_DescButton                   },
                    { "desc-button-hovered",        ImGuiCustomCol_DescButtonHovered            },
                    { "desc-button-active",         ImGuiCustomCol_DescButtonActive             },
                    { "toolbar-gray",               ImGuiCustomCol_ToolbarGray                  },
                    { "toolbar-red",                ImGuiCustomCol_ToolbarRed                   },
                    { "toolbar-yellow",             ImGuiCustomCol_ToolbarYellow                },
                    { "toolbar-green",              ImGuiCustomCol_ToolbarGreen                 },
                    { "toolbar-blue",               ImGuiCustomCol_ToolbarBlue                  },
                    { "toolbar-purple",             ImGuiCustomCol_ToolbarPurple                },
                    { "toolbar-brown",              ImGuiCustomCol_ToolbarBrown                 },
                    { "logger-debug",               ImGuiCustomCol_LoggerDebug                  },
                    { "logger-info",                ImGuiCustomCol_LoggerInfo                   },
                    { "logger-warning",             ImGuiCustomCol_LoggerWarning                },
                    { "logger-error",               ImGuiCustomCol_LoggerError                  },
                    { "logger-fatal",               ImGuiCustomCol_LoggerFatal                  },
                    { "achievement-unlocked",       ImGuiCustomCol_AchievementUnlocked          },
                    { "find-highlight",             ImGuiCustomCol_FindHighlight                },
                    { "highlight",                  ImGuiCustomCol_Highlight                    },
                    { "diff-added",                 ImGuiCustomCol_DiffAdded                    },
                    { "diff-removed",               ImGuiCustomCol_DiffRemoved                  },
                    { "diff-changed",               ImGuiCustomCol_DiffChanged                  },
                    { "advanced-encoding-ascii",    ImGuiCustomCol_AdvancedEncodingASCII        },
                    { "advanced-encoding-single",   ImGuiCustomCol_AdvancedEncodingSingleChar   },
                    { "advanced-encoding-multi",    ImGuiCustomCol_AdvancedEncodingMultiChar    },
                    { "advanced-encoding-unknown",  ImGuiCustomCol_AdvancedEncodingUnknown      },
                    { "patches",                    ImGuiCustomCol_Patches                      },
                    { "pattern-selected",           ImGuiCustomCol_PatternSelected              },
                    { "IEEE-tool-sign",             ImGuiCustomCol_IEEEToolSign                 },
                    { "IEEE-tool-exp",              ImGuiCustomCol_IEEEToolExp                  },
                    { "IEEE-tool-mantissa",         ImGuiCustomCol_IEEEToolMantissa             },
                    { "blur-background",            ImGuiCustomCol_BlurBackground               }

                };

                ThemeManager::addThemeHandler("imhex", ImHexColorMap,
                   [](u32 colorId) -> ImColor {
                       return static_cast<ImGuiExt::ImHexCustomData *>(GImGui->IO.UserData)->Colors[colorId];

                   },
                   [](u32 colorId, ImColor color) {
                       static_cast<ImGuiExt::ImHexCustomData *>(GImGui->IO.UserData)->Colors[colorId] = color;
                   }
                );
            }

            {
                const static ThemeManager::ColorMap TextEditorColorMap = {
                    { "default",                    u32(TextEditor::PaletteIndex::Default)                 },
                    { "keyword",                    u32(TextEditor::PaletteIndex::Keyword)                 },
                    { "number",                     u32(TextEditor::PaletteIndex::Number)                  },
                    { "string",                     u32(TextEditor::PaletteIndex::String)                  },
                    { "char-literal",               u32(TextEditor::PaletteIndex::CharLiteral)             },
                    { "punctuation",                u32(TextEditor::PaletteIndex::Punctuation)             },
                    { "preprocessor",               u32(TextEditor::PaletteIndex::Preprocessor)            },
                    { "identifier",                 u32(TextEditor::PaletteIndex::Identifier)              },
                    { "known-identifier",           u32(TextEditor::PaletteIndex::KnownIdentifier)         },
                    { "preproc-identifier",         u32(TextEditor::PaletteIndex::PreprocIdentifier)       },
                    { "global-doc-comment",         u32(TextEditor::PaletteIndex::GlobalDocComment)        },
                    { "doc-comment",                u32(TextEditor::PaletteIndex::DocComment)              },
                    { "comment",                    u32(TextEditor::PaletteIndex::Comment)                 },
                    { "multi-line-comment",         u32(TextEditor::PaletteIndex::MultiLineComment)        },
                    { "preprocessor-deactivated",   u32(TextEditor::PaletteIndex::PreprocessorDeactivated) },
                    { "background",                 u32(TextEditor::PaletteIndex::Background)              },
                    { "cursor",                     u32(TextEditor::PaletteIndex::Cursor)                  },
                    { "selection",                  u32(TextEditor::PaletteIndex::Selection)               },
                    { "error-marker",               u32(TextEditor::PaletteIndex::ErrorMarker)             },
                    { "breakpoint",                 u32(TextEditor::PaletteIndex::Breakpoint)              },
                    { "line-number",                u32(TextEditor::PaletteIndex::LineNumber)              },
                    { "current-line-fill",          u32(TextEditor::PaletteIndex::CurrentLineFill)         },
                    { "current-line-fill-inactive", u32(TextEditor::PaletteIndex::CurrentLineFillInactive) },
                    { "current-line-edge",          u32(TextEditor::PaletteIndex::CurrentLineEdge)         }
                };

                ThemeManager::addThemeHandler("text-editor", TextEditorColorMap,
                    [](u32 colorId) -> ImColor {
                        return TextEditor::GetPalette()[colorId];
                    },
                   [](u32 colorId, ImColor color) {
                        auto palette = TextEditor::GetPalette();
                        palette[colorId] = color;
                        TextEditor::SetPalette(palette);
                    }
                );
            }
        });
    }

    void registerStyleHandlers() {
        RequestInitThemeHandlers::subscribe([] {
            {
                auto &style = ImGui::GetStyle();
                const static ThemeManager::StyleMap ImGuiStyleMap = {
                    { "alpha",                  { &style.Alpha,                     0.1F,   1.0F,    false } },
                    { "disabled-alpha",         { &style.DisabledAlpha,             0.0F,   1.0F,    false } },
                    { "window-padding",         { &style.WindowPadding,             0.0F,   20.0F,   true  } },
                    { "window-rounding",        { &style.WindowRounding,            0.0F,   12.0F,   true  } },
                    { "window-border-size",     { &style.WindowBorderSize,          0.0F,   1.0F,    true  } },
                    { "window-min-size",        { &style.WindowMinSize,             0.0F,   1000.0F, true  } },
                    { "window-title-align",     { &style.WindowTitleAlign,          0.0F,   1.0F ,   false } },
                    { "child-rounding",         { &style.ChildRounding,             0.0F,   12.0F,   true  } },
                    { "child-border-size",      { &style.ChildBorderSize,           0.0F,   1.0F ,   true  } },
                    { "popup-rounding",         { &style.PopupRounding,             0.0F,   12.0F,   true  } },
                    { "popup-border-size",      { &style.PopupBorderSize,           0.0F,   1.0F,    true  } },
                    { "frame-padding",          { &style.FramePadding,              0.0F,   20.0F,   true  } },
                    { "frame-rounding",         { &style.FrameRounding,             0.0F,   12.0F,   true  } },
                    { "frame-border-size",      { &style.FrameBorderSize,           0.0F,   1.0F,    true  } },
                    { "item-spacing",           { &style.ItemSpacing,               0.0F,   20.0F,   true  } },
                    { "item-inner-spacing",     { &style.ItemInnerSpacing,          0.0F,   20.0F,   true  } },
                    { "indent-spacing",         { &style.IndentSpacing,             0.0F,   30.0F,   true  } },
                    { "cell-padding",           { &style.CellPadding,               0.0F,   20.0F,   true  } },
                    { "scrollbar-size",         { &style.ScrollbarSize,             0.0F,   20.0F,   true  } },
                    { "scrollbar-rounding",     { &style.ScrollbarRounding,         0.0F,   12.0F,   true  } },
                    { "grab-min-size",          { &style.GrabMinSize,               0.0F,   20.0F,   true  } },
                    { "grab-rounding",          { &style.GrabRounding,              0.0F,   12.0F,   true  } },
                    { "tab-rounding",           { &style.TabRounding,               0.0F,   12.0F,   true  } },
                    { "button-text-align",      { &style.ButtonTextAlign,           0.0F,   1.0F,    false } },
                    { "selectable-text-align",  { &style.SelectableTextAlign,       0.0F,   1.0F,    false } },
                    { "window-shadow-size",     { &style.WindowShadowSize,          0.0F,   100.0F,  true  } },
                    { "window-shadow-offset",   { &style.WindowShadowOffsetDist,    0.0F,   100.0F,  true  } },
                    { "window-shadow-angle",    { &style.WindowShadowOffsetAngle,   0.0F,   10.0F,   false } },
                };

                ThemeManager::addStyleHandler("imgui", ImGuiStyleMap);
            }

            {
                auto &style = ImPlot::GetStyle();
                const static ThemeManager::StyleMap ImPlotStyleMap = {
                        { "line-weight",            { &style.LineWeight,         0.0F, 5.0F,    true  } },
                        { "marker-size",            { &style.MarkerSize,         2.0F, 10.0F,   true  } },
                        { "marker-weight",          { &style.MarkerWeight,       0.0F, 5.0F,    true  } },
                        { "fill-alpha",             { &style.FillAlpha,          0.0F, 1.0F,    false } },
                        { "error-bar-size",         { &style.ErrorBarSize,       0.0F, 10.0F,   true  } },
                        { "error-bar-weight",       { &style.ErrorBarWeight,     0.0F, 5.0F,    true  } },
                        { "digital-bit-height",     { &style.DigitalBitHeight,   0.0F, 20.0F,   true  } },
                        { "digital-bit-gap",        { &style.DigitalBitGap,      0.0F, 20.0F,   true  } },
                        { "plot-border-size",       { &style.PlotBorderSize,     0.0F, 2.0F,    true  } },
                        { "minor-alpha",            { &style.MinorAlpha,         0.0F, 1.0F,    false } },
                        { "major-tick-len",         { &style.MajorTickLen,       0.0F, 20.0F,   true  } },
                        { "minor-tick-len",         { &style.MinorTickLen,       0.0F, 20.0F,   true  } },
                        { "major-tick-size",        { &style.MajorTickSize,      0.0F, 2.0F,    true  } },
                        { "minor-tick-size",        { &style.MinorTickSize,      0.0F, 2.0F,    true  } },
                        { "major-grid-size",        { &style.MajorGridSize,      0.0F, 2.0F,    true  } },
                        { "minor-grid-size",        { &style.MinorGridSize,      0.0F, 2.0F,    true  } },
                        { "plot-padding",           { &style.PlotPadding,        0.0F, 20.0F,   true  } },
                        { "label-padding",          { &style.LabelPadding,       0.0F, 20.0F,   true  } },
                        { "legend-padding",         { &style.LegendPadding,      0.0F, 20.0F,   true  } },
                        { "legend-inner-padding",   { &style.LegendInnerPadding, 0.0F, 10.0F,   true  } },
                        { "legend-spacing",         { &style.LegendSpacing,      0.0F, 5.0F,    true  } },
                        { "mouse-pos-padding",      { &style.MousePosPadding,    0.0F, 20.0F,   true  } },
                        { "annotation-padding",     { &style.AnnotationPadding,  0.0F, 5.0F,    true  } },
                        { "fit-padding",            { &style.FitPadding,         0.0F, 0.2F,    true  } },
                        { "plot-default-size",      { &style.PlotDefaultSize,    0.0F, 1000.0F, true  } },
                        { "plot-min-size",          { &style.PlotMinSize,        0.0F, 300.0F,  true  } },
                };

                ThemeManager::addStyleHandler("implot", ImPlotStyleMap);
            }

            {
                auto &style = ImNodes::GetStyle();
                const static ThemeManager::StyleMap ImNodesStyleMap = {
                        { "grid-spacing",                  { &style.GridSpacing,               0.0F,    100.0F, true } },
                        { "node-corner-rounding",          { &style.NodeCornerRounding,        0.0F,    12.0F,  true } },
                        { "node-padding",                  { &style.NodePadding,               0.0F,    20.0F,  true } },
                        { "node-border-thickness",         { &style.NodeBorderThickness,       0.0F,    1.0F,   true } },
                        { "link-thickness",                { &style.LinkThickness,             0.0F,    10.0F,  true } },
                        { "link-line-segments-per-length", { &style.LinkLineSegmentsPerLength, 0.0F,    2.0F,   true } },
                        { "link-hover-distance",           { &style.LinkHoverDistance,         0.0F,    20.0F,  true } },
                        { "pin-circle-radius",             { &style.PinCircleRadius,           0.0F,    20.0F,  true } },
                        { "pin-quad-side-length",          { &style.PinQuadSideLength,         0.0F,    20.0F,  true } },
                        { "pin-triangle-side-length",      { &style.PinTriangleSideLength,     0.0F,    20.0F,  true } },
                        { "pin-line-thickness",            { &style.PinLineThickness,          0.0F,    5.0F,   true } },
                        { "pin-hover-radius",              { &style.PinHoverRadius,            0.0F,    20.0F,  true } },
                        { "pin-offset",                    { &style.PinOffset,                 -10.0F,  10.0F,  true } },
                        { "mini-map-padding",              { &style.MiniMapPadding,            0.0F,    20.0F,  true } },
                        { "mini-map-offset",               { &style.MiniMapOffset,             -10.0F,  10.0F,  true } },
                };

                ThemeManager::addStyleHandler("imnodes", ImNodesStyleMap);
            }

            {
                auto &style = ImGuiExt::GetCustomStyle();
                const static ThemeManager::StyleMap ImHexStyleMap = {
                        { "window-blur",    { &style.WindowBlur,    0.0F,   1.0F,   true } },
                        { "popup-alpha",            { &style.PopupWindowAlpha,          0.0F,   1.0F,    false } },
                };

                ThemeManager::addStyleHandler("imhex", ImHexStyleMap);
            }
        });
    }

    void registerThemes() {
        // Load built-in themes
        for (const auto &theme : romfs::list("themes")) {
            ThemeManager::addTheme(std::string(romfs::get(theme).string()));
        }

        // Load user themes
        for (const auto &themeFolder : paths::Themes.read()) {
            for (const auto &theme : std::fs::directory_iterator(themeFolder)) {
                if (theme.is_regular_file())
                    ThemeManager::addTheme(wolv::io::File(theme.path(), wolv::io::File::Mode::Read).readString());
            }
        }
    }

}