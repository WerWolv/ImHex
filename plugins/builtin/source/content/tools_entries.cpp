#include <hex/api/content_registry/tools.hpp>

#include <hex/api/localization_manager.hpp>

#include <content/tools_entries.hpp>

#include <imgui.h>
#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    static void drawFileTools() {
        if (ImGui::BeginTabBar("file_tools_tabs")) {
            if (ImGui::BeginTabItem("hex.builtin.tools.file_tools.shredder"_lang)) {
                drawFileToolShredder();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("hex.builtin.tools.file_tools.splitter"_lang)) {
                drawFileToolSplitter();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("hex.builtin.tools.file_tools.combiner"_lang)) {
                drawFileToolCombiner();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

    }

    void registerToolEntries() {
        ContentRegistry::Tools::add("hex.builtin.tools.demangler",                  ICON_VS_CODE,               drawDemangler);
        ContentRegistry::Tools::add("hex.builtin.tools.ascii_table",                ICON_VS_TABLE,              drawASCIITable);
        ContentRegistry::Tools::add("hex.builtin.tools.regex_replacer",             ICON_VS_REGEX,              drawRegexReplacer);
        ContentRegistry::Tools::add("hex.builtin.tools.color",                      ICON_VS_SYMBOL_COLOR,       drawColorPicker);
        ContentRegistry::Tools::add("hex.builtin.tools.calc",                       ICON_VS_SYMBOL_OPERATOR,    drawMathEvaluator);
        ContentRegistry::Tools::add("hex.builtin.tools.graphing",                   ICON_VS_GRAPH_LINE,         drawGraphingCalculator);
        ContentRegistry::Tools::add("hex.builtin.tools.base_converter",             ICON_VS_SYMBOL_RULER,       drawBaseConverter);
        ContentRegistry::Tools::add("hex.builtin.tools.byte_swapper",               ICON_VS_ARROW_SWAP,         drawByteSwapper);
        ContentRegistry::Tools::add("hex.builtin.tools.permissions",                ICON_VS_ACCOUNT,            drawPermissionsCalculator);
        // ContentRegistry::Tools::add("hex.builtin.tools.file_uploader",              ICON_VS_CLOUD_UPLOAD,       drawFileUploader);
        ContentRegistry::Tools::add("hex.builtin.tools.wiki_explain",               ICON_VS_GLOBE,              drawWikiExplainer);
        ContentRegistry::Tools::add("hex.builtin.tools.file_tools",                 ICON_VS_FILES,              drawFileTools);
        ContentRegistry::Tools::add("hex.builtin.tools.ieee754",                    ICON_VS_PERCENTAGE,         drawIEEE754Decoder);
        ContentRegistry::Tools::add("hex.builtin.tools.invariant_multiplication",   ICON_VS_UNFOLD,             drawInvariantMultiplicationDecoder);
        ContentRegistry::Tools::add("hex.builtin.tools.tcp_client_server",          ICON_VS_SERVER_ENVIRONMENT, drawTCPClientServer);
        ContentRegistry::Tools::add("hex.builtin.tools.euclidean_algorithm",        ICON_VS_GROUP_BY_REF_TYPE,  drawEuclidianAlgorithm);
        ContentRegistry::Tools::add("hex.builtin.tools.http_requests",              ICON_VS_SERVER_PROCESS,     drawHTTPRequestMaker);
    }

}
