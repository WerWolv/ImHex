#include "content/views/view_hex_editor.hpp"
#include "content/views/view_pattern_editor.hpp"
#include "content/views/view_pattern_data.hpp"
#include "content/views/view_information.hpp"
#include "content/views/view_about.hpp"
#include "content/views/view_tools.hpp"
#include "content/views/view_data_inspector.hpp"
#include "content/views/view_bookmarks.hpp"
#include "content/views/view_patches.hpp"
#include "content/views/view_command_palette.hpp"
#include "content/views/view_settings.hpp"
#include "content/views/view_data_processor.hpp"
#include "content/views/view_constants.hpp"
#include "content/views/view_store.hpp"
#include "content/views/view_provider_settings.hpp"
#include "content/views/view_find.hpp"
#include "content/views/view_theme_manager.hpp"
#include "content/views/view_logs.hpp"
#include "content/views/view_achievements.hpp"
#include "content/views/view_highlight_rules.hpp"
#include "content/views/view_tutorials.hpp"

#include <hex/api/layout_manager.hpp>

namespace hex::plugin::builtin {

    void registerViews() {
        ContentRegistry::Views::add<ViewHexEditor>();
        ContentRegistry::Views::add<ViewPatternEditor>();
        ContentRegistry::Views::add<ViewPatternData>();
        ContentRegistry::Views::add<ViewDataInspector>();
        ContentRegistry::Views::add<ViewInformation>();
        ContentRegistry::Views::add<ViewBookmarks>();
        ContentRegistry::Views::add<ViewPatches>();
        ContentRegistry::Views::add<ViewTools>();
        ContentRegistry::Views::add<ViewCommandPalette>();
        ContentRegistry::Views::add<ViewAbout>();
        ContentRegistry::Views::add<ViewSettings>();
        ContentRegistry::Views::add<ViewDataProcessor>();
        ContentRegistry::Views::add<ViewConstants>();
        ContentRegistry::Views::add<ViewStore>();
        ContentRegistry::Views::add<ViewProviderSettings>();
        ContentRegistry::Views::add<ViewFind>();
        ContentRegistry::Views::add<ViewThemeManager>();
        ContentRegistry::Views::add<ViewLogs>();
        ContentRegistry::Views::add<ViewAchievements>();
        ContentRegistry::Views::add<ViewHighlightRules>();
        ContentRegistry::Views::add<ViewTutorials>();


        LayoutManager::registerLoadCallback([](std::string_view line) {
            for (auto &[name, view] : ContentRegistry::Views::impl::getEntries()) {
                if (!view->shouldStoreWindowState())
                    continue;

                std::string format = hex::format("{}=%d", view->getUnlocalizedName().get());
                sscanf(line.data(), format.c_str(), &view->getWindowOpenState());
            }
        });

        LayoutManager::registerStoreCallback([](ImGuiTextBuffer *buffer) {
            for (auto &[name, view] : ContentRegistry::Views::impl::getEntries()) {
                if (!view->shouldStoreWindowState())
                    continue;

                buffer->appendf("%s=%d\n", name.c_str(), view->getWindowOpenState());
            }
        });
    }

}