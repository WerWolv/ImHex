#include <hex/api/theme_manager.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <imnodes.h>
#include <TextEditor.h>
#include <romfs/romfs.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/file.hpp>

#include <hex/api/event.hpp>

namespace hex::plugin::builtin {

    void registerThemeHandlers() {
        EventManager::subscribe<RequestInitThemeHandlers>([]() {
            {
                const static api::ThemeManager::ColorMap ImGuiColorMap = {
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
                    { "tab-active",                     ImGuiCol_TabActive              },
                    { "tab-unfocused",                  ImGuiCol_TabUnfocused           },
                    { "tab-unfocused-active",           ImGuiCol_TabUnfocusedActive     },
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
                    { "nav-highlight",                  ImGuiCol_NavHighlight           },
                    { "nav-windowing-highlight",        ImGuiCol_NavWindowingHighlight  },
                    { "nav-windowing-background",       ImGuiCol_NavWindowingDimBg      },
                    { "modal-window-dim-background",    ImGuiCol_ModalWindowDimBg       }
                };

                api::ThemeManager::addThemeHandler("imgui", ImGuiColorMap,
                    [](u32 colorId) -> ImColor {
                        return ImGui::GetStyle().Colors[colorId];
                    },
                    [](u32 colorId, ImColor color) {
                        ImGui::GetStyle().Colors[colorId] = color;
                    }
                );
            }

            {
                const static api::ThemeManager::ColorMap ImPlotColorMap = {
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

                api::ThemeManager::addThemeHandler("implot", ImPlotColorMap,
                   [](u32 colorId) -> ImColor {
                       return ImPlot::GetStyle().Colors[colorId];
                   },
                   [](u32 colorId, ImColor color) {
                       ImPlot::GetStyle().Colors[colorId] = color;
                   }
                );
            }

            {
                const static api::ThemeManager::ColorMap ImNodesColorMap = {
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

                api::ThemeManager::addThemeHandler("imnodes", ImNodesColorMap,
                   [](u32 colorId) -> ImColor {
                       return ImNodes::GetStyle().Colors[colorId];
                   },
                   [](u32 colorId, ImColor color) {
                       ImNodes::GetStyle().Colors[colorId] = color;
                   }
                );
            }

            {
                const static api::ThemeManager::ColorMap ImHexColorMap = {
                    { "desc-button",            ImGuiCustomCol_DescButton           },
                    { "desc-button-hovered",    ImGuiCustomCol_DescButtonHovered    },
                    { "desc-button-active",     ImGuiCustomCol_DescButtonActive     },
                    { "toolbar-gray",           ImGuiCustomCol_ToolbarGray          },
                    { "toolbar-red",            ImGuiCustomCol_ToolbarRed           },
                    { "toolbar-yellow",         ImGuiCustomCol_ToolbarYellow        },
                    { "toolbar-green",          ImGuiCustomCol_ToolbarGreen         },
                    { "toolbar-blue",           ImGuiCustomCol_ToolbarBlue          },
                    { "toolbar-purple",         ImGuiCustomCol_ToolbarPurple        },
                    { "toolbar-brown",          ImGuiCustomCol_ToolbarBrown         },
                    { "highlight",              ImGuiCustomCol_Highlight            }
                };

                api::ThemeManager::addThemeHandler("imhex", ImHexColorMap,
                   [](u32 colorId) -> ImColor {
                       return static_cast<ImGui::ImHexCustomData *>(GImGui->IO.UserData)->Colors[colorId];

                   },
                   [](u32 colorId, ImColor color) {
                       static_cast<ImGui::ImHexCustomData *>(GImGui->IO.UserData)->Colors[colorId] = color;
                   }
                );
            }

            {
                const static api::ThemeManager::ColorMap TextEditorColorMap = {
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
                    { "comment",                    u32(TextEditor::PaletteIndex::Comment)                 },
                    { "multi-line-comment",         u32(TextEditor::PaletteIndex::MultiLineComment)        },
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

                api::ThemeManager::addThemeHandler("text-editor", TextEditorColorMap,
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
        EventManager::subscribe<RequestInitThemeHandlers>([]() {
            {
                auto &style = ImGui::GetStyle();
                const static api::ThemeManager::StyleMap ImGuiStyleMap = {
                    { "alpha",                  { &style.Alpha,                0.001F, 1.0F    } },
                    { "disabled-alpha",         { &style.DisabledAlpha,        0.0F,   1.0F    } },
                    { "window-padding",         { &style.WindowPadding,        0.0F,   20.0F   } },
                    { "window-rounding",        { &style.WindowRounding,       0.0F,   12.0F   } },
                    { "window-border-size",     { &style.WindowBorderSize,     0.0F,   1.0F    } },
                    { "window-min-size",        { &style.WindowMinSize,        0.0F,   1000.0F } },
                    { "window-title-align",     { &style.WindowTitleAlign,     0.0F,   1.0F    } },
                    { "child-rounding",         { &style.ChildRounding,        0.0F,   12.0F   } },
                    { "child-border-size",      { &style.ChildBorderSize,      0.0F,   1.0F    } },
                    { "popup-rounding",         { &style.PopupRounding,        0.0F,   12.0F   } },
                    { "popup-border-size",      { &style.PopupBorderSize,      0.0F,   1.0F    } },
                    { "frame-padding",          { &style.FramePadding,         0.0F,   20.0F   } },
                    { "frame-rounding",         { &style.FrameRounding,        0.0F,   12.0F   } },
                    { "frame-border-size",      { &style.FrameBorderSize,      0.0F,   1.0F    } },
                    { "item-spacing",           { &style.ItemSpacing,          0.0F,   20.0F   } },
                    { "item-inner-spacing",     { &style.ItemInnerSpacing,     0.0F,   20.0F   } },
                    { "indent-spacing",         { &style.IndentSpacing,        0.0F,   30.0F   } },
                    { "cell-padding",           { &style.CellPadding,          0.0F,   20.0F   } },
                    { "scrollbar-size",         { &style.ScrollbarSize,        0.0F,   20.0F   } },
                    { "scrollbar-rounding",     { &style.ScrollbarRounding,    0.0F,   12.0F   } },
                    { "grab-min-size",          { &style.GrabMinSize,          0.0F,   20.0F   } },
                    { "grab-rounding",          { &style.GrabRounding,         0.0F,   12.0F   } },
                    { "tab-rounding",           { &style.TabRounding,          0.0F,   12.0F   } },
                    { "button-text-align",      { &style.ButtonTextAlign,      0.0F,   1.0F    } },
                    { "selectable-text-align",  { &style.SelectableTextAlign,  0.0F,   1.0F    } },
                };

                api::ThemeManager::addStyleHandler("imgui", ImGuiStyleMap);
            }

            {
                auto &style = ImPlot::GetStyle();
                const static api::ThemeManager::StyleMap ImPlotStyleMap = {
                        { "line-weight",            { &style.LineWeight,         0.0F, 5.0F     } },
                        { "marker-size",            { &style.MarkerSize,         2.0F, 10.0F    } },
                        { "marker-weight",          { &style.MarkerWeight,       0.0F, 5.0F     } },
                        { "fill-alpha",             { &style.FillAlpha,          0.0F, 1.0F     } },
                        { "error-bar-size",         { &style.ErrorBarSize,       0.0F, 10.0F    } },
                        { "error-bar-weight",       { &style.ErrorBarWeight,     0.0F, 5.0F     } },
                        { "digital-bit-height",     { &style.DigitalBitHeight,   0.0F, 20.0F    } },
                        { "digital-bit-gap",        { &style.DigitalBitGap,      0.0F, 20.0F    } },
                        { "plot-border-size",       { &style.PlotBorderSize,     0.0F, 2.0F     } },
                        { "minor-alpha",            { &style.MinorAlpha,         0.0F, 1.0F     } },
                        { "major-tick-len",         { &style.MajorTickLen,       0.0F, 20.0F    } },
                        { "minor-tick-len",         { &style.MinorTickLen,       0.0F, 20.0F    } },
                        { "major-tick-size",        { &style.MajorTickSize,      0.0F, 2.0F     } },
                        { "minor-tick-size",        { &style.MinorTickSize,      0.0F, 2.0F     } },
                        { "major-grid-size",        { &style.MajorGridSize,      0.0F, 2.0F     } },
                        { "minor-grid-size",        { &style.MinorGridSize,      0.0F, 2.0F     } },
                        { "plot-padding",           { &style.PlotPadding,        0.0F, 20.0F    } },
                        { "label-padding",          { &style.LabelPadding,       0.0F, 20.0F    } },
                        { "legend-padding",         { &style.LegendPadding,      0.0F, 20.0F    } },
                        { "legend-inner-padding",   { &style.LegendInnerPadding, 0.0F, 10.0F    } },
                        { "legend-spacing",         { &style.LegendSpacing,      0.0F, 5.0F     } },
                        { "mouse-pos-padding",      { &style.MousePosPadding,    0.0F, 20.0F    } },
                        { "annotation-padding",     { &style.AnnotationPadding,  0.0F, 5.0F     } },
                        { "fit-padding",            { &style.FitPadding,         0.0F, 0.2F     } },
                        { "plot-default-size",      { &style.PlotDefaultSize,    0.0F, 1000.0F  } },
                        { "plot-min-size",          { &style.PlotMinSize,        0.0F, 300.0F   } },
                };

                api::ThemeManager::addStyleHandler("implot", ImPlotStyleMap);
            }

            {
                auto &style = ImNodes::GetStyle();
                const static api::ThemeManager::StyleMap ImNodesStyleMap = {
                        { "grid-spacing",                  { &style.GridSpacing,               0.0F, 100.0F     } },
                        { "node-corner-rounding",          { &style.NodeCornerRounding,        0.0F, 12.0F      } },
                        { "node-padding",                  { &style.NodePadding,               0.0F, 20.0F      } },
                        { "node-border-thickness",         { &style.NodeBorderThickness,       0.0F, 1.0F       } },
                        { "link-thickness",                { &style.LinkThickness,             0.0F, 10.0F      } },
                        { "link-line-segments-per-length", { &style.LinkLineSegmentsPerLength, 0.0F, 2.0F       } },
                        { "link-hover-distance",           { &style.LinkHoverDistance,         0.0F, 20.0F      } },
                        { "pin-circle-radius",             { &style.PinCircleRadius,           0.0F, 20.0F      } },
                        { "pin-quad-side-length",          { &style.PinQuadSideLength,         0.0F, 20.0F      } },
                        { "pin-triangle-side-length",      { &style.PinTriangleSideLength,     0.0F, 20.0F      } },
                        { "pin-line-thickness",            { &style.PinLineThickness,          0.0F, 5.0F       } },
                        { "pin-hover-radius",              { &style.PinHoverRadius,            0.0F, 20.0F      } },
                        { "pin-offset",                    { &style.PinOffset,                 -10.0F, 10.0F    } },
                        { "mini-map-padding",              { &style.MiniMapPadding,            0.0F, 20.0F      } },
                        { "mini-map-offset",               { &style.MiniMapOffset,             -10.0F, 10.0F    } },
                };

                api::ThemeManager::addStyleHandler("imnodes", ImNodesStyleMap);
            }
        });
    }

    void registerThemes() {
        // Load built-in themes
        for (const auto &theme : romfs::list("themes")) {
            api::ThemeManager::addTheme(std::string(romfs::get(theme).string()));
        }

        // Load user themes
        for (const auto &themeFolder : fs::getDefaultPaths(fs::ImHexPath::Themes)) {
            for (const auto &theme : std::fs::directory_iterator(themeFolder)) {
                if (theme.is_regular_file())
                    api::ThemeManager::addTheme(fs::File(theme.path(), fs::File::Mode::Read).readString());
            }
        }
    }

}