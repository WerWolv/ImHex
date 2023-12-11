#include <hex/api/workspace_manager.hpp>
#include <hex/api/layout_manager.hpp>

#include <hex/helpers/logger.hpp>

#include <wolv/io/file.hpp>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace hex {

    std::map<std::string, WorkspaceManager::Workspace> WorkspaceManager::s_workspaces;
    decltype(WorkspaceManager::s_workspaces)::iterator WorkspaceManager::s_currentWorkspace = WorkspaceManager::s_workspaces.end();

    void WorkspaceManager::createWorkspace(const std::string& name, const std::string &layout) {
        s_workspaces[name] = Workspace {
            .layout = layout.empty() ? LayoutManager::saveToString() : layout,
            .path = {}
        };

        WorkspaceManager::switchWorkspace(name);

        for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Workspaces)) {
            if (WorkspaceManager::exportToFile(path / (name + ".hexws")))
                break;
        }
    }

    void WorkspaceManager::switchWorkspace(const std::string& name) {
        if (s_currentWorkspace != s_workspaces.end()) {
            auto &[name, workspace] = *s_currentWorkspace;
            workspace.layout = LayoutManager::saveToString();

            WorkspaceManager::exportToFile(workspace.path);
        }

        auto it = s_workspaces.find(name);
        if (it == s_workspaces.end()) {
            log::error("Failed to switch workspace. Workspace '{}' does not exist", name);
            return;
        }

        auto &[newName, newWorkspace] = *it;
        s_currentWorkspace = it;
        LayoutManager::loadFromString(newWorkspace.layout);
    }

    void WorkspaceManager::importFromFile(const std::fs::path& path) {
        wolv::io::File file(path, wolv::io::File::Mode::Read);
        if (!file.isValid()) {
            log::error("Failed to load workspace from file '{}'", path.string());
            return;
        }

        auto content = file.readString();
        try {
            auto json = nlohmann::json::parse(content.begin(), content.end());

            std::string name = json["name"];
            std::string layout = json["layout"];

            s_workspaces[name] = Workspace {
                .layout = std::move(layout),
                .path = path
            };
        } catch (nlohmann::json::exception &e) {
            log::error("Failed to load workspace from file '{}': {}", path.string(), e.what());
        }
    }

    bool WorkspaceManager::exportToFile(std::fs::path path) {
        if (path.empty()) {
            if (s_currentWorkspace == s_workspaces.end())
                return false;

            path = s_currentWorkspace->second.path;
        }

        wolv::io::File file(path, wolv::io::File::Mode::Create);

        if (!file.isValid())
            return false;

        nlohmann::json json;
        json["name"] = s_currentWorkspace->first;
        json["layout"] = LayoutManager::saveToString();

        file.writeString(json.dump(4));

        return true;
    }


    void WorkspaceManager::reset() {
        s_workspaces.clear();
        s_currentWorkspace = WorkspaceManager::s_workspaces.end();
    }



}
