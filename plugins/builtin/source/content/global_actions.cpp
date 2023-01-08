
#include <hex/ui/view.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/helpers/logger.hpp>


namespace hex::plugin::builtin {

    void openProject() {
        fs::openFileBrowser(fs::DialogMode::Open, { {"Project File", "hexproj"} },
                            [](const auto &path) {
                                if (!ProjectFile::load(path)) {
                                    View::showErrorPopup("hex.builtin.popup.error.project.load"_lang);
                                }
                            });
    }

    void saveProject() {
        if (ImHexApi::Provider::isValid() && ProjectFile::hasPath()) {
            if (!ProjectFile::store()) {
                View::showErrorPopup("hex.builtin.popup.error.project.save"_lang);
            } else {
                log::debug("Project saved");
            }
        }
    }

    void saveProjectAs() {
        fs::openFileBrowser(fs::DialogMode::Save, { {"Project File", "hexproj"} },
                            [](std::fs::path path) {
                                if (path.extension() != ".hexproj") {
                                    path.replace_extension(".hexproj");
                                }

                                if (!ProjectFile::store(path)) {
                                    View::showErrorPopup("hex.builtin.popup.error.project.save"_lang);
                                }
                            });
    }
}
