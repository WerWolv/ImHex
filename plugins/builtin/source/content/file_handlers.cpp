#include <hex/api/content_registry.hpp>

#include <hex/api/project_file_manager.hpp>

namespace hex::plugin::builtin {

    void registerFileHandlers() {

        ContentRegistry::FileHandler::add({ ".hexproj" }, [](const std::fs::path &path) {
            return ProjectFile::load(path);
        });

        ContentRegistry::FileHandler::add({ ".hexlyt" }, [](const std::fs::path &path) {
            for (const auto &folder : fs::getDefaultPaths(fs::ImHexPath::Layouts)) {
                if (wolv::io::fs::copyFile(path, folder / path.filename()))
                    return true;
            }

            return false;
        });
    }

}