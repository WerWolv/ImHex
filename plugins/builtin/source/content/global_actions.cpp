
#include <content/global_actions.hpp>
#include <hex/ui/view.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/helpers/logger.hpp>

#include <toasts/toast_notification.hpp>

#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    void openProject() {
        fs::openFileBrowser(fs::DialogMode::Open, { {"Project File", "hexproj"} },
                            [](const auto &path) {
                                if (!ProjectFile::load(path)) {
                                    ui::ToastError::open(hex::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
                                }
                            });
    }

    bool saveProject() {
        if (!ImHexApi::Provider::isValid())
            return false;

        if (ProjectFile::hasPath()) {
            if (!ProjectFile::store()) {
                ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                return false;
            } else {
                log::debug("Project saved");
                return true;
            }
        } else {
            return saveProjectAs();
        }
    }

    bool saveProjectAs() {
        return fs::openFileBrowser(fs::DialogMode::Save, { {"Project File", "hexproj"} },
                            [](std::fs::path path) {
                                if (path.extension() != ".hexproj") {
                                    path.replace_extension(".hexproj");
                                }

                                if (!ProjectFile::store(path)) {
                                    ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                                }
                            });
    }
}
