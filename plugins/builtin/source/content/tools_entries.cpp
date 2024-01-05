#include <hex/api/content_registry.hpp>

#include <hex/api/localization_manager.hpp>

#include <content/tools_entries.hpp>

#include <imgui.h>

namespace hex::plugin::builtin {

    void drawFileTools() {
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
        ContentRegistry::Tools::add("hex.builtin.tools.demangler",                  drawDemangler);
        ContentRegistry::Tools::add("hex.builtin.tools.ascii_table",                drawASCIITable);
        ContentRegistry::Tools::add("hex.builtin.tools.regex_replacer",             drawRegexReplacer);
        ContentRegistry::Tools::add("hex.builtin.tools.color",                      drawColorPicker);
        ContentRegistry::Tools::add("hex.builtin.tools.calc",                       drawMathEvaluator);
        ContentRegistry::Tools::add("hex.builtin.tools.graphing",                   drawGraphingCalculator);
        ContentRegistry::Tools::add("hex.builtin.tools.base_converter",             drawBaseConverter);
        ContentRegistry::Tools::add("hex.builtin.tools.byte_swapper",               drawByteSwapper);
        ContentRegistry::Tools::add("hex.builtin.tools.permissions",                drawPermissionsCalculator);
        //ContentRegistry::Tools::add("hex.builtin.tools.file_uploader",              drawFileUploader);
        ContentRegistry::Tools::add("hex.builtin.tools.wiki_explain",               drawWikiExplainer);
        ContentRegistry::Tools::add("hex.builtin.tools.file_tools",                 drawFileTools);
        ContentRegistry::Tools::add("hex.builtin.tools.ieee754",                    drawIEEE754Decoder);
        ContentRegistry::Tools::add("hex.builtin.tools.invariant_multiplication",   drawInvariantMultiplicationDecoder);
        ContentRegistry::Tools::add("hex.builtin.tools.tcp_client_server",          drawTCPClientServer);
        ContentRegistry::Tools::add("hex.builtin.tools.euclidean_algorithm",        drawEuclidianAlgorithm);
        ContentRegistry::Tools::add("hex.builtin.tools.http_requests",              drawHTTPRequestMaker);
    }

}
