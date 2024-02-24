#include <hex/api/content_registry.hpp>
#include <hex/helpers/magic.hpp>
#include <hex/providers/provider.hpp>

#include <imgui.h>
#include <hex/api/task_manager.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <content/yara_rule.hpp>
#include <romfs/romfs.hpp>

namespace hex::plugin::yara {

    class InformationAdvancedFileInformation : public ContentRegistry::DataInformation::InformationSection {
    public:
        InformationAdvancedFileInformation() : InformationSection("hex.yara.information_section.advanced_data_info") { }
        ~InformationAdvancedFileInformation() override = default;

        struct Category {
            struct Comperator {
                bool operator()(const YaraRule::Rule &a, const YaraRule::Rule &b) const {
                    return a.identifier < b.identifier;
                }
            };

            std::set<YaraRule::Rule, Comperator> matchedRules;
        };

        void process(Task &task, prv::Provider *provider, Region region) override {
            const auto &ruleFilePaths = romfs::list("rules");
            task.setMaxValue(ruleFilePaths.size());

            for (const auto &ruleFilePath : ruleFilePaths) {
                const std::string fileContent = romfs::get(ruleFilePath).data<const char>();

                YaraRule yaraRule(fileContent);
                const auto result = yaraRule.match(provider, region);
                if (result.has_value()) {
                    const auto &rules = result.value().matchedRules;
                    for (const auto &rule : rules) {
                        if (!rule.metadata.contains("category")) continue;

                        const auto &categoryName = rule.metadata.at("category");
                        m_categories[categoryName].matchedRules.insert(rule);
                    }
                }

                task.increment();
            }
        }

        void reset() override {
            m_categories.clear();
        }

        void drawContent() override {
            if (ImGui::BeginTable("information", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoKeepColumnsVisible)) {
                ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.5F);

                ImGui::TableNextRow();

                for (auto &[categoryName, category] : m_categories) {
                    if (category.matchedRules.empty())
                        continue;

                    ImGui::TableNextColumn();
                    ImGuiExt::BeginSubWindow(categoryName.c_str());
                    {
                        for (const auto &match : category.matchedRules) {
                            const auto &ruleName = match.metadata.contains("name") ? match.metadata.at("name") : match.identifier;
                            ImGui::TextUnformatted(ruleName.c_str());
                        }
                    }
                    ImGuiExt::EndSubWindow();
                }

                ImGui::EndTable();
            }
        }

    private:
        std::map<std::string, Category> m_categories;
    };

    void registerDataInformationSections() {
        ContentRegistry::DataInformation::addInformationSection<InformationAdvancedFileInformation>();
    }

}
