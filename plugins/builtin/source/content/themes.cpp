#include <hex/api/theme_manager.hpp>
#include <hex/api/events/requests_lifecycle.hpp>

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/helpers/default_paths.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <imnodes.h>
#include <ui/text_editor.hpp>
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
                    { "input-text-cursor",              ImGuiCol_InputTextCursor        },
                    { "tab",                            ImGuiCol_Tab                    },
                    { "tab-hovered",                    ImGuiCol_TabHovered             },
                    { "tab-active",                     ImGuiCol_TabSelected            },
                    { "tab-active-overline",            ImGuiCol_TabSelectedOverline    },
                    { "tab-unfocused",                  ImGuiCol_TabDimmed              },
                    { "tab-unfocused-active",           ImGuiCol_TabDimmedSelected      },
                    { "tab-unfocused-active-overline",  ImGuiCol_TabDimmedSelectedOverline },
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
                    { "text-link",                      ImGuiCol_TextLink               },
                    { "text-selected-background",       ImGuiCol_TextSelectedBg         },
                    { "tree-lines",                     ImGuiCol_TreeLines              },
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
                       return static_cast<ImGuiExt::ImHexCustomData *>(ImGui::GetCurrentContext()->IO.UserData)->Colors[colorId];

                   },
                   [](u32 colorId, ImColor color) {
                       static_cast<ImGuiExt::ImHexCustomData *>(ImGui::GetCurrentContext()->IO.UserData)->Colors[colorId] = color;
                   }
                );
            }
            {
                const static ThemeManager::ColorMap TextEditorColorMap = {
                    { "attribute",                  u32(ui::TextEditor::PaletteIndex::Attribute)                },
                    { "background",                 u32(ui::TextEditor::PaletteIndex::Background)               },
                    { "breakpoint",                 u32(ui::TextEditor::PaletteIndex::Breakpoint)               },
                    { "calculated-pointer",         u32(ui::TextEditor::PaletteIndex::CalculatedPointer)        },
                    { "char-literal",               u32(ui::TextEditor::PaletteIndex::CharLiteral)              },
                    { "comment",                    u32(ui::TextEditor::PaletteIndex::Comment)                  },
                    { "current-line-edge",          u32(ui::TextEditor::PaletteIndex::CurrentLineEdge)          },
                    { "current-line-fill",          u32(ui::TextEditor::PaletteIndex::CurrentLineFill)          },
                    { "current-line-fill-inactive", u32(ui::TextEditor::PaletteIndex::CurrentLineFillInactive)  },
                    { "cursor",                     u32(ui::TextEditor::PaletteIndex::Cursor)                   },
                    { "debug-text",                 u32(ui::TextEditor::PaletteIndex::DebugText)                },
                    { "default",                    u32(ui::TextEditor::PaletteIndex::Default)                  },
                    { "default-text",               u32(ui::TextEditor::PaletteIndex::DefaultText)              },
                    { "doc-block-comment",          u32(ui::TextEditor::PaletteIndex::DocBlockComment)          },
                    { "doc-comment",                u32(ui::TextEditor::PaletteIndex::DocComment)               },
                    { "doc-global-comment",         u32(ui::TextEditor::PaletteIndex::GlobalDocComment)         },
                    { "error-marker",               u32(ui::TextEditor::PaletteIndex::ErrorMarker)              },
                    { "error-text",                 u32(ui::TextEditor::PaletteIndex::ErrorText)                },
                    { "function",                   u32(ui::TextEditor::PaletteIndex::Function)                 },
                    { "function-parameter",         u32(ui::TextEditor::PaletteIndex::FunctionParameter)        },
                    { "function-variable",          u32(ui::TextEditor::PaletteIndex::FunctionVariable)         },
                    { "global-variable",            u32(ui::TextEditor::PaletteIndex::GlobalVariable)           },
                    { "identifier" ,                u32(ui::TextEditor::PaletteIndex::Identifier)               },
                    { "keyword",                    u32(ui::TextEditor::PaletteIndex::Keyword)                  },
                    { "known-identifier",           u32(ui::TextEditor::PaletteIndex::BuiltInType)              },
                    { "line-number",                u32(ui::TextEditor::PaletteIndex::LineNumber)               },
                    { "local-variable",             u32(ui::TextEditor::PaletteIndex::LocalVariable)            },
                    { "multi-line-comment",         u32(ui::TextEditor::PaletteIndex::BlockComment)             },
                    { "namespace",                  u32(ui::TextEditor::PaletteIndex::NameSpace)                },
                    { "number",                     u32(ui::TextEditor::PaletteIndex::NumericLiteral)           },
                    { "pattern-variable",           u32(ui::TextEditor::PaletteIndex::PatternVariable)          },
                    { "placed-variable",            u32(ui::TextEditor::PaletteIndex::PlacedVariable)           },
                    { "preprocessor",               u32(ui::TextEditor::PaletteIndex::Directive)                },
                    { "preprocessor-deactivated",   u32(ui::TextEditor::PaletteIndex::PreprocessorDeactivated)  },
                    { "preproc-identifier",         u32(ui::TextEditor::PaletteIndex::PreprocIdentifier)        },
                    { "punctuation",                u32(ui::TextEditor::PaletteIndex::Operator)                 },
                    { "selection",                  u32(ui::TextEditor::PaletteIndex::Selection)                },
                    { "separator",                  u32(ui::TextEditor::PaletteIndex::Separator)                },
                    { "string",                     u32(ui::TextEditor::PaletteIndex::StringLiteral)            },
                    { "template-variable",          u32(ui::TextEditor::PaletteIndex::TemplateArgument)         },
                    { "typedef",                    u32(ui::TextEditor::PaletteIndex::TypeDef)                  },
                    { "unknown-identifier",         u32(ui::TextEditor::PaletteIndex::UnkIdentifier)            },
                    { "user-defined-type",          u32(ui::TextEditor::PaletteIndex::UserDefinedType)          },
                    { "view",                       u32(ui::TextEditor::PaletteIndex::View)                     },
                    { "warning-text",               u32(ui::TextEditor::PaletteIndex::WarningText)              }
                };

                ThemeManager::addThemeHandler("text-editor", TextEditorColorMap,
                    [](u32 colorId) -> ImColor {
                        return ui::TextEditor::getPalette()[colorId];
                    },
                   [](u32 colorId, ImColor color) {
                        auto palette = ui::TextEditor::getPalette();
                        palette[colorId] = color;
                       ui::TextEditor::setPalette(palette);
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
                    { "alpha",                                  { &style.Alpha,                             0.1F,    1.0F,    false } },
                    { "disabled-alpha",                         { &style.DisabledAlpha,                     0.0F,    1.0F,    false } },
                    { "window-padding",                         { &style.WindowPadding,                     0.0F,    20.0F,   true  } },
                    { "window-rounding",                        { &style.WindowRounding,                    0.0F,    12.0F,   true  } },
                    { "window-border-size",                     { &style.WindowBorderSize,                  0.0F,    1.0F,    true  } },
                    { "window-border-hover-padding",            { &style.WindowBorderHoverPadding,          1.0F,    20.0F,   true  } },
                    { "window-min-size",                        { &style.WindowMinSize,                     0.0F,    1000.0F, true  } },
                    { "window-title-align",                     { &style.WindowTitleAlign,                  0.0F,    1.0F,    false } },
                    { "child-rounding",                         { &style.ChildRounding,                     0.0F,    12.0F,   true  } },
                    { "child-border-size",                      { &style.ChildBorderSize,                   0.0F,    1.0F,    true  } },
                    { "popup-rounding",                         { &style.PopupRounding,                     0.0F,    12.0F,   true  } },
                    { "popup-border-size",                      { &style.PopupBorderSize,                   0.0F,    1.0F,    true  } },
                    { "frame-padding",                          { &style.FramePadding,                      0.0F,    20.0F,   true  } },
                    { "frame-rounding",                         { &style.FrameRounding,                     0.0F,    12.0F,   true  } },
                    { "frame-border-size",                      { &style.FrameBorderSize,                   0.0F,    1.0F,    true  } },
                    { "item-spacing",                           { &style.ItemSpacing,                       0.0F,    20.0F,   true  } },
                    { "item-inner-spacing",                     { &style.ItemInnerSpacing,                  0.0F,    20.0F,   true  } },
                    { "indent-spacing",                         { &style.IndentSpacing,                     0.0F,    30.0F,   true  } },
                    { "cell-padding",                           { &style.CellPadding,                       0.0F,    20.0F,   true  } },
                    { "touch-extra-padding",                    { &style.TouchExtraPadding,                 0.0F,    10.0F,   true  } },
                    { "columns-min-spacing",                    { &style.ColumnsMinSpacing,                 0.0F,    20.0F,   true  } },
                    { "scrollbar-size",                         { &style.ScrollbarSize,                     0.0F,    20.0F,   true  } },
                    { "scrollbar-rounding",                     { &style.ScrollbarRounding,                 0.0F,    12.0F,   true  } },
                    { "grab-min-size",                          { &style.GrabMinSize,                       0.0F,    20.0F,   true  } },
                    { "grab-rounding",                          { &style.GrabRounding,                      0.0F,    12.0F,   true  } },
                    { "log-slider-deadzone",                    { &style.LogSliderDeadzone,                 0.0F,    12.0F,   true  } },
                    { "image-border-size",                      { &style.ImageBorderSize,                   0.0F,    1.0F,    true  } },
                    { "tab-rounding",                           { &style.TabRounding,                       0.0F,    12.0F,   true  } },
                    { "tab-border-size",                        { &style.TabBorderSize,                     0.0F,    1.0F,    true  } },
                    { "tab-min-width-base",                     { &style.TabMinWidthBase,                   0.0F,    500.0F,  true  } },
                    { "tab-min-width-shrink",                   { &style.TabMinWidthShrink,                 0.0F,    500.0F,  true  } },
                    { "tab-close-button-min-width-selected",    { &style.TabCloseButtonMinWidthSelected,   -1.0F,    100.0F,  false } },
                    { "tab-close-button-min-width-unselected",  { &style.TabCloseButtonMinWidthUnselected, -1.0F,    100.0F,  false } },
                    { "tab-bar-border-size",                    { &style.TabBarBorderSize,                  0.0F,    10.0F,   true  } },
                    { "tab-bar-overline-size",                  { &style.TabBarOverlineSize,                0.0F,    10.0F,   true  } },
                    { "button-text-align",                      { &style.ButtonTextAlign,                   0.0F,    1.0F,    false } },
                    { "selectable-text-align",                  { &style.SelectableTextAlign,               0.0F,    1.0F,    false } },
                    { "separator-text-border-size",             { &style.SeparatorTextBorderSize,           0.0F,    5.0F,    true  } },
                    { "separator-text-align",                   { &style.SeparatorTextAlign,                0.0F,    1.0F,    false } },
                    { "separator-text-padding",                 { &style.SeparatorTextPadding,              0.0F,    20.0F,   true  } },
                    { "display-window-padding",                 { &style.DisplayWindowPadding,              0.0F,    20.0F,   true  } },
                    { "display-safe-area-padding",              { &style.DisplaySafeAreaPadding,            0.0F,    20.0F,   true  } },
                    { "docking-separator-size",                 { &style.DockingSeparatorSize,              0.0F,    20.0F,   true  } },
                    { "mouse-cursor-scale",                     { &style.MouseCursorScale,                  0.1F,    10.0F,   true  } },
                    { "curve-tessellation-tol",                 { &style.CurveTessellationTol,              0.0F,    10.0F,   true  } },
                    { "circle-tessellation-max-error",          { &style.CircleTessellationMaxError,        0.0F,    10.0F,   true  } },
                    { "window-shadow-size",                     { &style.WindowShadowSize,                  0.0F,    100.0F,  true  } },
                    { "window-shadow-offset",                   { &style.WindowShadowOffsetDist,            0.0F,    100.0F,  true  } },
                    { "window-shadow-angle",                    { &style.WindowShadowOffsetAngle,           0.0F,    10.0F,   false } }
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