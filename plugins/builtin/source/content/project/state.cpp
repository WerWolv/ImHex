#include "internal.hpp"

#include <hex/api/project_manager.hpp>

namespace hex::plugin::builtin::project::impl {

    ProjectState &state() {
        static ProjectState state;
        return state;
    }

    const std::fs::path &projectSettingsPath() {
        static const auto path = std::fs::path(ProjectManager::ProjectDirectory) / "project.json";
        return path;
    }

}
