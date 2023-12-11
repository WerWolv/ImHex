#pragma once

#include <wolv/io/fs.hpp>

#include <map>
#include <string>

namespace hex {

    class WorkspaceManager {
    public:
        struct Workspace {
            std::string layout;
            std::fs::path path;
        };

        static void createWorkspace(const std::string &name, const std::string &layout = "");
        static void switchWorkspace(const std::string &name);

        static void importFromFile(const std::fs::path &path);
        static bool exportToFile(std::fs::path path = {});

        static const auto& getWorkspaces() { return s_workspaces; }
        static const auto& getCurrentWorkspace() { return s_currentWorkspace; }

        static void reset();

    private:
        WorkspaceManager() = default;

        static std::map<std::string, WorkspaceManager::Workspace> s_workspaces;
        static decltype(s_workspaces)::iterator s_currentWorkspace;
    };

}