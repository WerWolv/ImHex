#include <hex/api/workspace_manager.hpp>
#include <hex/api/layout_manager.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <wolv/io/file.hpp>

#include <nlohmann/json.hpp>

#include <imgui.h>

namespace hex {

    AutoReset<std::map<std::string, WorkspaceManager::Workspace>> WorkspaceManager::s_workspaces;
    decltype(WorkspaceManager::s_workspaces)::Type::iterator WorkspaceManager::s_currentWorkspace  = s_workspaces->end();
    decltype(WorkspaceManager::s_workspaces)::Type::iterator WorkspaceManager::s_previousWorkspace = s_workspaces->end();

    void WorkspaceManager::createWorkspace(const std::string& name, const std::string &layout) {
        s_currentWorkspace = s_workspaces->insert_or_assign(name, Workspace {
            .layout = layout.empty() ? LayoutManager::saveToString() : layout,
            .path = {}
        }).first;

        for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Workspaces)) {
            if (exportToFile(path / (name + ".hexws")))
                break;
        }
    }

    void WorkspaceManager::switchWorkspace(const std::string& name) {
        const auto newWorkspace = s_workspaces->find(name);
        if (newWorkspace != s_workspaces->end()) {
            s_currentWorkspace = newWorkspace;
            log::info("Switching to workspace '{}'", name);
        }
    }

    void WorkspaceManager::importFromFile(const std::fs::path& path) {
        wolv::io::File file(path, wolv::io::File::Mode::Read);
        if (!file.isValid()) {
            log::error("Failed to load workspace from file '{}'", path.string());
            file.remove();
            return;
        }

        auto content = file.readString();
        try {
            auto json = nlohmann::json::parse(content.begin(), content.end());

            const std::string name = json["name"];
            std::string layout = json["layout"];

            (*s_workspaces)[name] = Workspace {
                .layout = std::move(layout),
                .path = path
            };
        } catch (nlohmann::json::exception &e) {
            log::error("Failed to load workspace from file '{}': {}", path.string(), e.what());
            file.remove();
        }
    }

    bool WorkspaceManager::exportToFile(std::fs::path path, std::string workspaceName) {
        if (path.empty()) {
            if (s_currentWorkspace == s_workspaces->end())
                return false;

            path = s_currentWorkspace->second.path;
        }

        if (workspaceName.empty())
            workspaceName = s_currentWorkspace->first;

        wolv::io::File file(path, wolv::io::File::Mode::Create);

        if (!file.isValid())
            return false;

        nlohmann::json json;
        json["name"]    = workspaceName;
        json["layout"]  = LayoutManager::saveToString();

        file.writeString(json.dump(4));

        return true;
    }


    void WorkspaceManager::process() {
        if (s_previousWorkspace != s_currentWorkspace) {
            if (s_previousWorkspace != s_workspaces->end())
                exportToFile(s_previousWorkspace->second.path, s_previousWorkspace->first);

            LayoutManager::closeAllViews();
            ImGui::LoadIniSettingsFromMemory(s_currentWorkspace->second.layout.c_str());

            s_previousWorkspace = s_currentWorkspace;
        }
    }


    void WorkspaceManager::reset() {
        s_workspaces->clear();
        s_currentWorkspace  = s_workspaces->end();
        s_previousWorkspace = s_workspaces->end();
    }



}
