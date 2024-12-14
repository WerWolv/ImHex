#include <hex/api/content_registry.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/api/localization_manager.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/vscode_icons.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    using namespace std::literals::string_literals;
    using namespace std::literals::chrono_literals;

    std::string getWikipediaApiUrl() {
        auto setting = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.interface", "hex.builtin.setting.interface.wiki_explain_language", "en");
        return "https://" + setting + ".wikipedia.org/w/api.php?format=json&action=query&prop=extracts&explaintext&redirects=10&formatversion=2";
    }

    void drawWikiExplainer() {
        static HttpRequest request("GET", "");

        static std::string resultTitle, resultExtract;
        static std::future<HttpRequest::Result<std::string>> searchProcess;
        static bool extendedSearch = false;
        static std::string searchString;

        ImGuiExt::Header("hex.builtin.tools.wiki_explain.control"_lang, true);

        bool startSearch = ImGuiExt::InputTextIcon("##search", ICON_VS_SYMBOL_KEY, searchString, ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();

        ImGui::BeginDisabled((searchProcess.valid() && searchProcess.wait_for(0s) != std::future_status::ready) || searchString.empty());
        startSearch = ImGui::Button("hex.builtin.tools.wiki_explain.search"_lang) || startSearch;
        ImGui::EndDisabled();

        if (startSearch && !searchString.empty()) {
            request.setUrl(getWikipediaApiUrl() + "&exintro"s + "&titles="s + HttpRequest::urlEncode(searchString));
            searchProcess = request.execute();
        }

        ImGuiExt::Header("hex.builtin.tools.wiki_explain.results"_lang);

        if (ImGui::BeginChild("##summary", ImVec2(0, 300), true)) {
            if (!resultTitle.empty() && !resultExtract.empty()) {
                ImGuiExt::HeaderColored(resultTitle.c_str(), ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_Highlight), true);
                ImGuiExt::TextFormattedWrapped("{}", resultExtract.c_str());
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

                    request.setUrl(getWikipediaApiUrl() + "&titles="s + HttpRequest::urlEncode(searchString));
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
