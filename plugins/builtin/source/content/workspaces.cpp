#include <hex/api/content_registry.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/api/workspace_manager.hpp>

#include <hex/helpers/fs.hpp>

namespace hex::plugin::builtin {

    void loadWorkspaces() {
        WorkspaceManager::reload();

        auto currentWorkspace = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.curr_workspace", "Default");
        TaskManager::doLater([currentWorkspace] {
            WorkspaceManager::switchWorkspace(currentWorkspace);
        });
    }

}
