#include <hex/api/content_registry.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/api/localization.hpp>

#include <hex/ui/view.hpp>

#include <content/tools_entries.hpp>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {
    namespace impl {

        std::string getWikipediaApiUrl() {
            std::string setting = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.wiki_explain_language", "en").get<std::string>();
            return "https://" + setting + ".wikipedia.org/w/api.php?format=json&action=query&prop=extracts&explaintext&redirects=10&formatversion=2";
        }
        
        void drawWikiExplainer() {
            static HttpRequest request("GET", "");

            static std::string resultTitle, resultExtract;
            static std::future<HttpRequest::Result<std::string>> searchProcess;
            static bool extendedSearch = false;
            static std::string searchString;

            ImGui::Header("hex.builtin.tools.wiki_explain.control"_lang, true);

            bool startSearch = ImGui::InputTextIcon("##search", ICON_VS_SYMBOL_KEY, searchString, ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();

            ImGui::BeginDisabled((searchProcess.valid() && searchProcess.wait_for(0s) != std::future_status::ready) || searchString.empty());
            startSearch = ImGui::Button("hex.builtin.tools.wiki_explain.search"_lang) || startSearch;
            ImGui::EndDisabled();

            if (startSearch && !searchString.empty()) {
                request.setUrl(getWikipediaApiUrl() + "&exintro"s + "&titles="s + request.urlEncode(searchString));
                searchProcess = request.execute();
            }

            ImGui::Header("hex.builtin.tools.wiki_explain.results"_lang);

            if (ImGui::BeginChild("##summary", ImVec2(0, 300), true)) {
                if (!resultTitle.empty() && !resultExtract.empty()) {
                    ImGui::HeaderColored(resultTitle.c_str(), ImGui::GetCustomColorVec4(ImGuiCustomCol_Highlight), true);
                    ImGui::TextFormattedWrapped("{}", resultExtract.c_str());
                }
            }
            ImGui::EndChild();

            if (searchProcess.valid() && searchProcess.wait_for(0s) == std::future_status::ready) {
                try {
                    auto response = searchProcess.get();
                    if (response.getStatusCode() != 200) throw std::runtime_error("Invalid response");

                    auto json = nlohmann::json::parse(response.getData());

                    resultTitle   = json.at("query").at("pages").at(0).at("title").get<std::string>();
                    resultExtract = json.at("query").at("pages").at(0).at("extract").get<std::string>();

                    if (!extendedSearch && resultExtract.ends_with(':')) {
                        extendedSearch = true;

                        request.setUrl(getWikipediaApiUrl() + "&titles="s + request.urlEncode(searchString));
                        searchProcess  = request.execute();

                        resultTitle.clear();
                        resultExtract.clear();
                        searchString.clear();
                    } else {
                        extendedSearch = false;
                        searchString.clear();
                    }
                } catch (...) {
                    searchString.clear();
                    resultTitle.clear();
                    resultExtract.clear();
                    extendedSearch = false;
                    searchProcess  = {};

                    resultTitle   = "???";
                    resultExtract = "hex.builtin.tools.wiki_explain.invalid_response"_lang.get();
                }
            }
        }
    }
}
