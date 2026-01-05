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
                    { "alpha",                                  { .value=&style.Alpha,                             .min=0.1F,    .max=1.0F,    .needsScaling=false } },
                    { "disabled-alpha",                         { .value=&style.DisabledAlpha,                     .min=0.0F,    .max=1.0F,    .needsScaling=false } },
                    { "window-padding",                         { .value=&style.WindowPadding,                     .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "window-rounding",                        { .value=&style.WindowRounding,                    .min=0.0F,    .max=12.0F,   .needsScaling=true  } },
                    { "window-border-size",                     { .value=&style.WindowBorderSize,                  .min=0.0F,    .max=1.0F,    .needsScaling=true  } },
                    { "window-border-hover-padding",            { .value=&style.WindowBorderHoverPadding,          .min=1.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "window-min-size",                        { .value=&style.WindowMinSize,                     .min=0.0F,    .max=1000.0F, .needsScaling=true  } },
                    { "window-title-align",                     { .value=&style.WindowTitleAlign,                  .min=0.0F,    .max=1.0F,    .needsScaling=false } },
                    { "child-rounding",                         { .value=&style.ChildRounding,                     .min=0.0F,    .max=12.0F,   .needsScaling=true  } },
                    { "child-border-size",                      { .value=&style.ChildBorderSize,                   .min=0.0F,    .max=1.0F,    .needsScaling=true  } },
                    { "popup-rounding",                         { .value=&style.PopupRounding,                     .min=0.0F,    .max=12.0F,   .needsScaling=true  } },
                    { "popup-border-size",                      { .value=&style.PopupBorderSize,                   .min=0.0F,    .max=1.0F,    .needsScaling=true  } },
                    { "frame-padding",                          { .value=&style.FramePadding,                      .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "frame-rounding",                         { .value=&style.FrameRounding,                     .min=0.0F,    .max=12.0F,   .needsScaling=true  } },
                    { "frame-border-size",                      { .value=&style.FrameBorderSize,                   .min=0.0F,    .max=1.0F,    .needsScaling=true  } },
                    { "item-spacing",                           { .value=&style.ItemSpacing,                       .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "item-inner-spacing",                     { .value=&style.ItemInnerSpacing,                  .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "indent-spacing",                         { .value=&style.IndentSpacing,                     .min=0.0F,    .max=30.0F,   .needsScaling=true  } },
                    { "cell-padding",                           { .value=&style.CellPadding,                       .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "touch-extra-padding",                    { .value=&style.TouchExtraPadding,                 .min=0.0F,    .max=10.0F,   .needsScaling=true  } },
                    { "columns-min-spacing",                    { .value=&style.ColumnsMinSpacing,                 .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "scrollbar-size",                         { .value=&style.ScrollbarSize,                     .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "scrollbar-rounding",                     { .value=&style.ScrollbarRounding,                 .min=0.0F,    .max=12.0F,   .needsScaling=true  } },
                    { "grab-min-size",                          { .value=&style.GrabMinSize,                       .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "grab-rounding",                          { .value=&style.GrabRounding,                      .min=0.0F,    .max=12.0F,   .needsScaling=true  } },
                    { "log-slider-deadzone",                    { .value=&style.LogSliderDeadzone,                 .min=0.0F,    .max=12.0F,   .needsScaling=true  } },
                    { "image-border-size",                      { .value=&style.ImageBorderSize,                   .min=0.0F,    .max=1.0F,    .needsScaling=true  } },
                    { "tab-rounding",                           { .value=&style.TabRounding,                       .min=0.0F,    .max=12.0F,   .needsScaling=true  } },
                    { "tab-border-size",                        { .value=&style.TabBorderSize,                     .min=0.0F,    .max=1.0F,    .needsScaling=true  } },
                    { "tab-min-width-base",                     { .value=&style.TabMinWidthBase,                   .min=0.0F,    .max=500.0F,  .needsScaling=true  } },
                    { "tab-min-width-shrink",                   { .value=&style.TabMinWidthShrink,                 .min=0.0F,    .max=500.0F,  .needsScaling=true  } },
                    { "tab-close-button-min-width-selected",    { .value=&style.TabCloseButtonMinWidthSelected,   .min=-1.0F,    .max=100.0F,  .needsScaling=false } },
                    { "tab-close-button-min-width-unselected",  { .value=&style.TabCloseButtonMinWidthUnselected, .min=-1.0F,    .max=100.0F,  .needsScaling=false } },
                    { "tab-bar-border-size",                    { .value=&style.TabBarBorderSize,                  .min=0.0F,    .max=10.0F,   .needsScaling=true  } },
                    { "tab-bar-overline-size",                  { .value=&style.TabBarOverlineSize,                .min=0.0F,    .max=10.0F,   .needsScaling=true  } },
                    { "button-text-align",                      { .value=&style.ButtonTextAlign,                   .min=0.0F,    .max=1.0F,    .needsScaling=false } },
                    { "selectable-text-align",                  { .value=&style.SelectableTextAlign,               .min=0.0F,    .max=1.0F,    .needsScaling=false } },
                    { "separator-text-border-size",             { .value=&style.SeparatorTextBorderSize,           .min=0.0F,    .max=5.0F,    .needsScaling=true  } },
                    { "separator-text-align",                   { .value=&style.SeparatorTextAlign,                .min=0.0F,    .max=1.0F,    .needsScaling=false } },
                    { "separator-text-padding",                 { .value=&style.SeparatorTextPadding,              .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "display-window-padding",                 { .value=&style.DisplayWindowPadding,              .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "display-safe-area-padding",              { .value=&style.DisplaySafeAreaPadding,            .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "docking-separator-size",                 { .value=&style.DockingSeparatorSize,              .min=0.0F,    .max=20.0F,   .needsScaling=true  } },
                    { "mouse-cursor-scale",                     { .value=&style.MouseCursorScale,                  .min=0.1F,    .max=10.0F,   .needsScaling=true  } },
                    { "curve-tessellation-tol",                 { .value=&style.CurveTessellationTol,              .min=0.0F,    .max=10.0F,   .needsScaling=true  } },
                    { "circle-tessellation-max-error",          { .value=&style.CircleTessellationMaxError,        .min=0.0F,    .max=10.0F,   .needsScaling=true  } },
                    { "window-shadow-size",                     { .value=&style.WindowShadowSize,                  .min=0.0F,    .max=100.0F,  .needsScaling=true  } },
                    { "window-shadow-offset",                   { .value=&style.WindowShadowOffsetDist,            .min=0.0F,    .max=100.0F,  .needsScaling=true  } },
                    { "window-shadow-angle",                    { .value=&style.WindowShadowOffsetAngle,           .min=0.0F,    .max=10.0F,   .needsScaling=false } }
                };

                ThemeManager::addStyleHandler("imgui", ImGuiStyleMap);
            }

            {
                auto &style = ImPlot::GetStyle();
                const static ThemeManager::StyleMap ImPlotStyleMap = {
                        { "line-weight",            { .value=&style.LineWeight,         .min=0.0F, .max=5.0F,    .needsScaling=true  } },
                        { "marker-size",            { .value=&style.MarkerSize,         .min=2.0F, .max=10.0F,   .needsScaling=true  } },
                        { "marker-weight",          { .value=&style.MarkerWeight,       .min=0.0F, .max=5.0F,    .needsScaling=true  } },
                        { "fill-alpha",             { .value=&style.FillAlpha,          .min=0.0F, .max=1.0F,    .needsScaling=false } },
                        { "error-bar-size",         { .value=&style.ErrorBarSize,       .min=0.0F, .max=10.0F,   .needsScaling=true  } },
                        { "error-bar-weight",       { .value=&style.ErrorBarWeight,     .min=0.0F, .max=5.0F,    .needsScaling=true  } },
                        { "digital-bit-height",     { .value=&style.DigitalBitHeight,   .min=0.0F, .max=20.0F,   .needsScaling=true  } },
                        { "digital-bit-gap",        { .value=&style.DigitalBitGap,      .min=0.0F, .max=20.0F,   .needsScaling=true  } },
                        { "plot-border-size",       { .value=&style.PlotBorderSize,     .min=0.0F, .max=2.0F,    .needsScaling=true  } },
                        { "minor-alpha",            { .value=&style.MinorAlpha,         .min=0.0F, .max=1.0F,    .needsScaling=false } },
                        { "major-tick-len",         { .value=&style.MajorTickLen,       .min=0.0F, .max=20.0F,   .needsScaling=true  } },
                        { "minor-tick-len",         { .value=&style.MinorTickLen,       .min=0.0F, .max=20.0F,   .needsScaling=true  } },
                        { "major-tick-size",        { .value=&style.MajorTickSize,      .min=0.0F, .max=2.0F,    .needsScaling=true  } },
                        { "minor-tick-size",        { .value=&style.MinorTickSize,      .min=0.0F, .max=2.0F,    .needsScaling=true  } },
                        { "major-grid-size",        { .value=&style.MajorGridSize,      .min=0.0F, .max=2.0F,    .needsScaling=true  } },
                        { "minor-grid-size",        { .value=&style.MinorGridSize,      .min=0.0F, .max=2.0F,    .needsScaling=true  } },
                        { "plot-padding",           { .value=&style.PlotPadding,        .min=0.0F, .max=20.0F,   .needsScaling=true  } },
                        { "label-padding",          { .value=&style.LabelPadding,       .min=0.0F, .max=20.0F,   .needsScaling=true  } },
                        { "legend-padding",         { .value=&style.LegendPadding,      .min=0.0F, .max=20.0F,   .needsScaling=true  } },
                        { "legend-inner-padding",   { .value=&style.LegendInnerPadding, .min=0.0F, .max=10.0F,   .needsScaling=true  } },
                        { "legend-spacing",         { .value=&style.LegendSpacing,      .min=0.0F, .max=5.0F,    .needsScaling=true  } },
                        { "mouse-pos-padding",      { .value=&style.MousePosPadding,    .min=0.0F, .max=20.0F,   .needsScaling=true  } },
                        { "annotation-padding",     { .value=&style.AnnotationPadding,  .min=0.0F, .max=5.0F,    .needsScaling=true  } },
                        { "fit-padding",            { .value=&style.FitPadding,         .min=0.0F, .max=0.2F,    .needsScaling=true  } },
                        { "plot-default-size",      { .value=&style.PlotDefaultSize,    .min=0.0F, .max=1000.0F, .needsScaling=true  } },
                        { "plot-min-size",          { .value=&style.PlotMinSize,        .min=0.0F, .max=300.0F,  .needsScaling=true  } },
                };

                ThemeManager::addStyleHandler("implot", ImPlotStyleMap);
            }

            {
                auto &style = ImNodes::GetStyle();
                const static ThemeManager::StyleMap ImNodesStyleMap = {
                        { "grid-spacing",                  { .value=&style.GridSpacing,               .min=0.0F,    .max=100.0F, .needsScaling=true } },
                        { "node-corner-rounding",          { .value=&style.NodeCornerRounding,        .min=0.0F,    .max=12.0F,  .needsScaling=true } },
                        { "node-padding",                  { .value=&style.NodePadding,               .min=0.0F,    .max=20.0F,  .needsScaling=true } },
                        { "node-border-thickness",         { .value=&style.NodeBorderThickness,       .min=0.0F,    .max=1.0F,   .needsScaling=true } },
                        { "link-thickness",                { .value=&style.LinkThickness,             .min=0.0F,    .max=10.0F,  .needsScaling=true } },
                        { "link-line-segments-per-length", { .value=&style.LinkLineSegmentsPerLength, .min=0.0F,    .max=2.0F,   .needsScaling=true } },
                        { "link-hover-distance",           { .value=&style.LinkHoverDistance,         .min=0.0F,    .max=20.0F,  .needsScaling=true } },
                        { "pin-circle-radius",             { .value=&style.PinCircleRadius,           .min=0.0F,    .max=20.0F,  .needsScaling=true } },
                        { "pin-quad-side-length",          { .value=&style.PinQuadSideLength,         .min=0.0F,    .max=20.0F,  .needsScaling=true } },
                        { "pin-triangle-side-length",      { .value=&style.PinTriangleSideLength,     .min=0.0F,    .max=20.0F,  .needsScaling=true } },
                        { "pin-line-thickness",            { .value=&style.PinLineThickness,          .min=0.0F,    .max=5.0F,   .needsScaling=true } },
                        { "pin-hover-radius",              { .value=&style.PinHoverRadius,            .min=0.0F,    .max=20.0F,  .needsScaling=true } },
                        { "pin-offset",                    { .value=&style.PinOffset,                 .min=-10.0F,  .max=10.0F,  .needsScaling=true } },
                        { "mini-map-padding",              { .value=&style.MiniMapPadding,            .min=0.0F,    .max=20.0F,  .needsScaling=true } },
                        { "mini-map-offset",               { .value=&style.MiniMapOffset,             .min=-10.0F,  .max=10.0F,  .needsScaling=true } },
                };

                ThemeManager::addStyleHandler("imnodes", ImNodesStyleMap);
            }

            {
                auto &style = ImGuiExt::GetCustomStyle();
                const static ThemeManager::StyleMap ImHexStyleMap = {
                        { "window-blur",    { .value=&style.WindowBlur,    .min=0.0F,   .max=1.0F,   .needsScaling=true } },
                        { "popup-alpha",            { .value=&style.PopupWindowAlpha,          .min=0.0F,   .max=1.0F,    .needsScaling=false } },
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