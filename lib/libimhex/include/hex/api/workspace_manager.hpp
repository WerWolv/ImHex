#pragma once

#include <wolv/io/fs.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <map>
#include <string>

EXPORT_MODULE namespace hex {

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

        static const std::map<std::string, Workspace>& getWorkspaces();
        static const std::map<std::string, Workspace>::iterator& getCurrentWorkspace();

        static void reset();
        static void reload();

        static void process();

    private:
        WorkspaceManager() = default;
    };

}