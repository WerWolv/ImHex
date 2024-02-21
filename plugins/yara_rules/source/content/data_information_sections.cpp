#include <hex/api/content_registry.hpp>
#include <hex/helpers/magic.hpp>
#include <hex/providers/provider.hpp>

#include <imgui.h>
#include <hex/api/task_manager.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <content/yara_rule.hpp>
#include <romfs/romfs.hpp>

namespace hex::plugin::yara {

    class InformationYaraRules : public ContentRegistry::DataInformation::InformationSection {
    public:
        InformationYaraRules() : InformationSection("hex.yara.information_section.yara_rules") { }
        ~InformationYaraRules() override = default;

        void process(Task &task, prv::Provider *provider, Region region) override {
            const auto &ruleFilePaths = romfs::list("rules");
            task.setMaxValue(ruleFilePaths.size());

            u32 progress = 0;
            for (const auto &ruleFilePath : ruleFilePaths) {
                const std::string fileContent = romfs::get(ruleFilePath).data<const char>();

                YaraRule rule(fileContent);
                auto result = rule.match(provider, region);
                if (result.has_value()) {
                    m_matches[ruleFilePath.filename().string()] = result.value().matches;
                }

                task.update(progress);
                progress += 1;
            }
        }

        void reset() override {
            m_matches.clear();
        }

        void drawContent() override {
            if (ImGui::BeginTable("information", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoKeepColumnsVisible)) {
                ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.5F);

                ImGui::TableNextRow();

                for (auto &[name, matches] : m_matches) {
                    if (matches.empty())
                        continue;

                    ImGui::TableNextColumn();
                    ImGuiExt::BeginSubWindow(name.c_str());
                    {
                        for (const auto &match : matches) {
                            ImGui::TextUnformatted(match.identifier.c_str());
                        }
                    }
                    ImGuiExt::EndSubWindow();
                }

                ImGui::EndTable();
            }
        }

    private:
        std::map<std::string, std::vector<YaraRule::Match>> m_matches;
    };

    void registerDataInformationSections() {
        ContentRegistry::DataInformation::addInformationSection<InformationYaraRules>();
    }

}
