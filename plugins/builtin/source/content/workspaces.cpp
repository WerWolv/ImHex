#include <hex/api/content_registry.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/workspace_manager.hpp>

#include <hex/helpers/fs.hpp>

namespace hex::plugin::builtin {

    void loadWorkspaces() {
        for (const auto &defaultPath : fs::getDefaultPaths(fs::ImHexPath::Workspaces)) {
            for (const auto &entry : std::fs::directory_iterator(defaultPath)) {
                if (!entry.is_regular_file())
                    continue;

                const auto &path = entry.path();
                if (path.extension() != ".hexws")
                    continue;

                WorkspaceManager::importFromFile(path);
            }
        }

        std::string currentWorkspace = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.curr_workspace", "Default");

        TaskManager::doLater([currentWorkspace] {
            WorkspaceManager::switchWorkspace(currentWorkspace);
        });
    }

}
