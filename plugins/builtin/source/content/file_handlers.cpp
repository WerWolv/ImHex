#include <hex/api/content_registry/file_type_handler.hpp>
#include <hex/helpers/default_paths.hpp>

#include <fonts/vscode_icons.hpp>

#include <toasts/toast_notification.hpp>

namespace hex::plugin::builtin {

    void registerFileHandlers() {


        ContentRegistry::FileTypeHandler::add({ ".hexlyt" }, [](const std::fs::path &path) {
            for (const auto &folder : paths::Layouts.write()) {
                if (wolv::io::fs::copyFile(path, folder / path.filename()))
                    return true;
            }

            return false;
        }, ICON_VS_LAYOUT);

        ContentRegistry::FileTypeHandler::add({ ".mgc" }, [](const auto &path) {
            for (const auto &destPath : paths::Magic.write()) {
                if (wolv::io::fs::copyFile(path, destPath / path.filename(), std::fs::copy_options::overwrite_existing)) {
                    ui::ToastInfo::open("hex.builtin.view.information.magic_db_added"_lang);
                    return true;
                }
            }

            return false;
        }, ICON_VS_DATABASE);
    }

}
