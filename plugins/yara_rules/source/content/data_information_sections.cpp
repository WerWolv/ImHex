#include <hex/api/content_registry.hpp>

#include <imgui.h>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/default_paths.hpp>
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
            for (const auto &yaraSignaturePath : paths::YaraAdvancedAnalysis.read()) {
                for (const auto &ruleFilePath : std::fs::recursive_directory_iterator(yaraSignaturePath)) {
                    wolv::io::File file(ruleFilePath.path(), wolv::io::File::Mode::Read);
                    if (!file.isValid())
                        continue;

                    YaraRule yaraRule(file.readString());
                    task.setInterruptCallback([&yaraRule] {
                        yaraRule.interrupt();
                    });

                    const auto result = yaraRule.match(provider, region);
                    if (result.has_value()) {
                        const auto &rules = result.value().matchedRules;
                        for (const auto &rule : rules) {
                            if (!rule.metadata.contains("category")) continue;

                            const auto &categoryName = rule.metadata.at("category");
                            m_categories[categoryName].matchedRules.insert(rule);
                        }
                    }

                    task.update();
                }
            }
        }

        void reset() override {
            m_categories.clear();
        }

        void drawContent() override {
            const auto empty = !std::ranges::any_of(m_categories, [](const auto &entry) {
                const auto &[categoryName, category] = entry;
                return !category.matchedRules.empty();
            });

            if (!empty) {
                if (ImGui::BeginTable("information", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoKeepColumnsVisible)) {
                    ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                    ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.5F);

                    ImGui::TableNextRow();

                    for (auto &[categoryName, category] : m_categories) {
                        if (category.matchedRules.empty())
                            continue;

                        ImGui::TableNextColumn();
                        if (ImGuiExt::BeginSubWindow(categoryName.c_str())) {
                            for (const auto &match : category.matchedRules) {
                                const auto &ruleName = match.metadata.contains("name") ? match.metadata.at("name") : match.identifier;
                                ImGuiExt::TextFormattedSelectable("{}", ruleName);
                            }
                        }
                        ImGuiExt::EndSubWindow();

                    }

                    ImGui::EndTable();
                }
            } else {
                ImGui::NewLine();
                ImGuiExt::TextFormattedCenteredHorizontal("{}", "hex.yara.information_section.advanced_data_info.no_information"_lang);
                ImGui::NewLine();
            }
        }

    private:
        std::map<std::string, Category> m_categories;
    };

    void registerDataInformationSections() {
        ContentRegistry::DataInformation::addInformationSection<InformationAdvancedFileInformation>();
    }

}
