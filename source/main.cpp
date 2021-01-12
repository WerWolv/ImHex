#include "helpers/utils.hpp"
#include "window.hpp"

#include <helpers/content_registry.hpp>
#include <providers/provider.hpp>

#include "lang/pattern_data.hpp"
#include "views/view_hexeditor.hpp"
#include "views/view_pattern.hpp"
#include "views/view_pattern_data.hpp"
#include "views/view_hashes.hpp"
#include "views/view_information.hpp"
#include "views/view_help.hpp"
#include "views/view_tools.hpp"
#include "views/view_strings.hpp"
#include "views/view_data_inspector.hpp"
#include "views/view_disassembler.hpp"
#include "views/view_bookmarks.hpp"
#include "views/view_patches.hpp"
#include "views/view_command_palette.hpp"
#include "views/view_settings.hpp"


#include <vector>

int main(int argc, char **argv) {
    hex::Window window(argc, argv);

    // Shared Data
    std::vector<hex::lang::PatternData*> patternData;

    // Create views
    hex::ContentRegistry::Views::add<hex::ViewHexEditor>(patternData);
    hex::ContentRegistry::Views::add<hex::ViewPattern>(patternData);
    hex::ContentRegistry::Views::add<hex::ViewPatternData>(patternData);
    hex::ContentRegistry::Views::add<hex::ViewDataInspector>();
    hex::ContentRegistry::Views::add<hex::ViewHashes>();
    hex::ContentRegistry::Views::add<hex::ViewInformation>();
    hex::ContentRegistry::Views::add<hex::ViewStrings>();
    hex::ContentRegistry::Views::add<hex::ViewDisassembler>();
    hex::ContentRegistry::Views::add<hex::ViewBookmarks>();
    hex::ContentRegistry::Views::add<hex::ViewPatches>();
    hex::ContentRegistry::Views::add<hex::ViewTools>();
    hex::ContentRegistry::Views::add<hex::ViewCommandPalette>();
    hex::ContentRegistry::Views::add<hex::ViewHelp>();
    hex::ContentRegistry::Views::add<hex::ViewSettings>();

    if (argc > 1)
        hex::View::postEvent(hex::Events::FileDropped, argv[1]);

    window.initPlugins();
    window.loop();


    return EXIT_SUCCESS;
}
