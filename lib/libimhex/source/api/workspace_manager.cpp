#include <hex/api/workspace_manager.hpp>
#include <hex/api/layout_manager.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/auto_reset.hpp>
#include <hex/helpers/default_paths.hpp>

#include <wolv/io/file.hpp>

#include <nlohmann/json.hpp>

#include <imgui.h>
#include <wolv/utils/string.hpp>

namespace hex {

    AutoReset<std::map<std::string, WorkspaceManager::Workspace>> WorkspaceManager::s_workspaces;
    decltype(WorkspaceManager::s_workspaces)::Type::iterator WorkspaceManager::s_currentWorkspace  = s_workspaces->end();
    decltype(WorkspaceManager::s_workspaces)::Type::iterator WorkspaceManager::s_previousWorkspace = s_workspaces->end();
    decltype(WorkspaceManager::s_workspaces)::Type::iterator WorkspaceManager::s_workspaceToRemove = s_workspaces->end();

    void WorkspaceManager::createWorkspace(const std::string& name, const std::string &layout) {
        s_currentWorkspace = s_workspaces->insert_or_assign(name, Workspace {
            .layout  = layout.empty() ? LayoutManager::saveToString() : layout,
            .path    = {},
            .builtin = false
        }).first;

        for (const auto &workspaceFolder : paths::Workspaces.write()) {
            const auto workspacePath = workspaceFolder / (name + ".hexws");
            if (exportToFile(workspacePath)) {
                s_currentWorkspace->second.path = workspacePath;
                break;
            }
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
        if (std::ranges::any_of(*s_workspaces, [path](const auto &pair) { return pair.second.path == path; })) {
            return;
        }

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
            const bool builtin = json.value("builtin", false);

            (*s_workspaces)[name] = Workspace {
                .layout = std::move(layout),
                .path = path,
                .builtin = builtin
            };
        } catch (nlohmann::json::exception &e) {
            log::error("Failed to load workspace from file '{}': {}", path.string(), e.what());
            file.remove();
        }
    }

    bool WorkspaceManager::exportToFile(std::fs::path path, std::string workspaceName, bool builtin) {
        if (path.empty()) {
            if (s_currentWorkspace == s_workspaces->end()) {
                return false;
            }

            path = s_currentWorkspace->second.path;
        }

        if (workspaceName.empty()) {
            workspaceName = s_currentWorkspace->first;
        }


        wolv::io::File file(path, wolv::io::File::Mode::Create);

        if (!file.isValid()) {
            return false;
        }

        nlohmann::json json;
        json["name"]    = workspaceName;
        json["layout"]  = LayoutManager::saveToString();
        json["builtin"] = builtin;

        file.writeString(json.dump(4));

        return true;
    }

    void WorkspaceManager::removeWorkspace(const std::string& name) {
        bool deletedCurrentWorkspace = false;
        for (const auto &[workspaceName, workspace] : *s_workspaces) {
            if (workspaceName == name) {
                log::info("Removing workspace file '{}'", wolv::util::toUTF8String(workspace.path));
                if (wolv::io::fs::remove(workspace.path)) {
                    log::info("Removed workspace '{}'", name);

                    if (workspaceName == s_currentWorkspace->first) {
                        deletedCurrentWorkspace = true;
                    }
                } else {
                    log::error("Failed to remove workspace '{}'", name);
                }
            }
        }

        WorkspaceManager::reload();

        if (deletedCurrentWorkspace && !s_workspaces->empty()) {
            s_currentWorkspace = s_workspaces->begin();
        }
    }


    void WorkspaceManager::process() {
        if (s_previousWorkspace != s_currentWorkspace) {
            log::info("Updating workspace");
            if (s_previousWorkspace != s_workspaces->end()) {
                exportToFile(s_previousWorkspace->second.path, s_previousWorkspace->first, s_previousWorkspace->second.builtin);
            }

            LayoutManager::closeAllViews();
            ImGui::LoadIniSettingsFromMemory(s_currentWorkspace->second.layout.c_str());

            s_previousWorkspace = s_currentWorkspace;

            if (s_workspaceToRemove != s_workspaces->end()) {
                s_workspaces->erase(s_workspaceToRemove);
                s_workspaceToRemove = s_workspaces->end();
            }
        }
    }


    void WorkspaceManager::reset() {
        s_workspaces->clear();
        s_currentWorkspace  = s_workspaces->end();
        s_previousWorkspace = s_workspaces->end();
    }

    void WorkspaceManager::reload() {
        WorkspaceManager::reset();

        for (const auto &defaultPath : paths::Workspaces.read()) {
            for (const auto &entry : std::fs::directory_iterator(defaultPath)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                const auto &path = entry.path();
                if (path.extension() != ".hexws") {
                    continue;
                }

                WorkspaceManager::importFromFile(path);
            }
        }
    }




}
