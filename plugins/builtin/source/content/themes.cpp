#include <hex/api/theme_manager.hpp>

#include <imgui.h>
#include <implot.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <imnodes.h>
#include <TextEditor.h>
#include <romfs/romfs.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/file.hpp>

namespace hex::plugin::builtin {

    namespace {

        [[maybe_unused]] void printThemeData(const auto &colorMap, const auto &colors) {
            for (const auto &[name, id] : colorMap)
                fmt::print("\"{}\": \"#{:08X}\",\n", name, hex::changeEndianess(colors[id], std::endian::big));
        }

    }

    void registerThemeHandlers() {
        api::ThemeManager::addThemeHandler("imgui", [](const std::string &key, const std::string &value) {
            const static std::map<std::string, ImGuiCol> ColorMap = {
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

            auto color = api::ThemeManager::parseColorString(value);
            auto colors = ImGui::GetStyle().Colors;

            if (ColorMap.contains(key) && color.has_value())
                colors[ColorMap.at(key)] = color->Value;
        });

        api::ThemeManager::addThemeHandler("implot", [](const std::string &key, const std::string &value) {
            const static std::map<std::string, ImPlotCol> ColorMap = {
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

            auto color = api::ThemeManager::parseColorString(value);
            auto &colors = ImPlot::GetStyle().Colors;

            if (ColorMap.contains(key) && color.has_value())
                colors[ColorMap.at(key)] = color->Value;
        });

        api::ThemeManager::addThemeHandler("imnodes", [](const std::string &key, const std::string &value) {
            const static std::map<std::string, ImNodesCol> ColorMap = {
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

            auto color = api::ThemeManager::parseColorString(value);
            auto &colors = ImNodes::GetStyle().Colors;

            if (ColorMap.contains(key) && color.has_value())
                colors[ColorMap.at(key)] = *color;
        });

        api::ThemeManager::addThemeHandler("imhex", [](const std::string &key, const std::string &value) {
            const static std::map<std::string, ImGuiCustomCol> ColorMap = {
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

            auto color = api::ThemeManager::parseColorString(value);
            auto &colors = static_cast<ImGui::ImHexCustomData *>(GImGui->IO.UserData)->Colors;

            if (ColorMap.contains(key) && color.has_value())
                colors[ColorMap.at(key)] = color->Value;
        });

        api::ThemeManager::addThemeHandler("text-editor", [](const std::string &key, const std::string &value) {
            using enum TextEditor::PaletteIndex;
            const static std::map<std::string, TextEditor::PaletteIndex> ColorMap = {
                    { "default",                    Default                 },
                    { "keyword",                    Keyword                 },
                    { "number",                     Number                  },
                    { "string",                     String                  },
                    { "char-literal",               CharLiteral             },
                    { "punctuation",                Punctuation             },
                    { "preprocessor",               Preprocessor            },
                    { "identifier",                 Identifier              },
                    { "known-identifier",           KnownIdentifier         },
                    { "preproc-identifier",         PreprocIdentifier       },
                    { "comment",                    Comment                 },
                    { "multi-line-comment",         MultiLineComment        },
                    { "background",                 Background              },
                    { "cursor",                     Cursor                  },
                    { "selection",                  Selection               },
                    { "error-marker",               ErrorMarker             },
                    { "breakpoint",                 Breakpoint              },
                    { "line-number",                LineNumber              },
                    { "current-line-fill",          CurrentLineFill         },
                    { "current-line-fill-inactive", CurrentLineFillInactive },
                    { "current-line-edge",          CurrentLineEdge         }
            };

            auto color = api::ThemeManager::parseColorString(value);
            auto colors = TextEditor::GetPalette();

            if (ColorMap.contains(key) && color.has_value())
                colors[size_t(ColorMap.at(key))] = ImU32(*color);

            TextEditor::SetPalette(colors);
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