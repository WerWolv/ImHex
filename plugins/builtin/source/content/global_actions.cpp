
#include <hex/ui/view.hpp>
#include <hex/api/project_file_manager.hpp>


namespace hex::plugin::builtin {

    void saveProject() {
        if (!ProjectFile::store()) {
            View::showErrorPopup("hex.builtin.popup.error.project.save"_lang);
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
