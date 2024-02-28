#pragma once

#include <wolv/io/fs.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <map>
#include <string>

namespace hex {

    class WorkspaceManager {
    public:
        struct Workspace {
            std::string layout;
            std::fs::path path;
            bool builtin;
        };

        static void createWorkspace(const std::string &name, const std::string &layout = "");
        static void switchWorkspace(const std::string &name);

        static void importFromFile(const std::fs::path &path);
        static bool exportToFile(std::fs::path path = {}, std::string workspaceName = {}, bool builtin = false);

        static void removeWorkspace(const std::string &name);

        static const auto& getWorkspaces() { return *s_workspaces; }
        static const auto& getCurrentWorkspace() { return s_currentWorkspace; }

        static void reset();
        static void reload();

        static void process();

    private:
        WorkspaceManager() = default;

        static AutoReset<std::map<std::string, Workspace>> s_workspaces;
        static decltype(s_workspaces)::Type::iterator s_currentWorkspace, s_previousWorkspace, s_workspaceToRemove;
    };

}