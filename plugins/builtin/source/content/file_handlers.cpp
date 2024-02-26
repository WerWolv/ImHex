#include <hex/api/content_registry.hpp>

#include <hex/api/project_file_manager.hpp>
#include <toasts/toast_notification.hpp>

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

        ContentRegistry::FileHandler::add({ ".mgc" }, [](const auto &path) {
            for (const auto &destPath : fs::getDefaultPaths(fs::ImHexPath::Magic)) {
                if (wolv::io::fs::copyFile(path, destPath / path.filename(), std::fs::copy_options::overwrite_existing)) {
                    ui::ToastInfo::open("hex.builtin.view.information.magic_db_added"_lang);
                    return true;
                }
            }

            return false;
        });
    }

}