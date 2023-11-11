#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/literals.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/api/localization.hpp>

#include <hex/ui/view.hpp>

#include <algorithm>
#include <chrono>
#include <random>
#include <regex>

#include <llvm/Demangle/Demangle.h>
#include <content/helpers/math_evaluator.hpp>
#include <content/tools_entries.hpp>

#include <imgui.h>
#include <implot.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <content/popups/popup_notification.hpp>
#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/net/socket_client.hpp>
#include <wolv/net/socket_server.hpp>

namespace hex::plugin::builtin {

    namespace impl {

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
                    impl::drawFileToolCombiner();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
    }

    void registerToolEntries() {
        ContentRegistry::Tools::add("hex.builtin.tools.demangler", impl::drawDemangler);
        ContentRegistry::Tools::add("hex.builtin.tools.ascii_table", impl::drawASCIITable);
        ContentRegistry::Tools::add("hex.builtin.tools.regex_replacer", impl::drawRegexReplacer);
        ContentRegistry::Tools::add("hex.builtin.tools.color", impl::drawColorPicker);
        ContentRegistry::Tools::add("hex.builtin.tools.calc", impl::drawMathEvaluator);
        ContentRegistry::Tools::add("hex.builtin.tools.graphing", impl::drawGraphingCalculator);
        ContentRegistry::Tools::add("hex.builtin.tools.base_converter", impl::drawBaseConverter);
        ContentRegistry::Tools::add("hex.builtin.tools.byte_swapper", impl::drawByteSwapper);
        ContentRegistry::Tools::add("hex.builtin.tools.permissions", impl::drawPermissionsCalculator);
        // ContentRegistry::Tools::add("hex.builtin.tools.file_uploader", impl::drawFileUploader);
        ContentRegistry::Tools::add("hex.builtin.tools.wiki_explain", impl::drawWikiExplainer);
        ContentRegistry::Tools::add("hex.builtin.tools.file_tools", impl::drawFileTools);
        ContentRegistry::Tools::add("hex.builtin.tools.ieee754", impl::drawIEEE754Decoder);
        ContentRegistry::Tools::add("hex.builtin.tools.invariant_multiplication", impl::drawInvariantMultiplicationDecoder);
        ContentRegistry::Tools::add("hex.builtin.tools.tcp_client_server", impl::drawTCPClientServer);
        ContentRegistry::Tools::add("hex.builtin.tools.euclidean_algorithm", impl::drawEuclidianAlgorithm);
    }

}
