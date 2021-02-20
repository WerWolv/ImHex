#include <hex.hpp>

#include "window.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>
#include <hex/lang/pattern_data.hpp>
#include <hex/helpers/utils.hpp>

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
#include "views/view_data_processor.hpp"
#include "views/view_yara.hpp"

#include <vector>

int main(int argc, char **argv) {
    using namespace hex;

    Window window(argc, argv);

    // Shared Data
    std::vector<lang::PatternData*> patternData;

    // Create views
    ContentRegistry::Views::add<ViewHexEditor>(patternData);
    ContentRegistry::Views::add<ViewPattern>(patternData);
    ContentRegistry::Views::add<ViewPatternData>(patternData);
    ContentRegistry::Views::add<ViewDataInspector>();
    ContentRegistry::Views::add<ViewHashes>();
    ContentRegistry::Views::add<ViewInformation>();
    ContentRegistry::Views::add<ViewStrings>();
    ContentRegistry::Views::add<ViewDisassembler>();
    ContentRegistry::Views::add<ViewBookmarks>();
    ContentRegistry::Views::add<ViewPatches>();
    ContentRegistry::Views::add<ViewTools>();
    ContentRegistry::Views::add<ViewCommandPalette>();
    ContentRegistry::Views::add<ViewHelp>();
    ContentRegistry::Views::add<ViewSettings>();
    ContentRegistry::Views::add<ViewDataProcessor>();
    ContentRegistry::Views::add<ViewYara>();

    if (argc > 1)
        View::postEvent(Events::FileDropped, argv[1]);

    window.loop();


    return EXIT_SUCCESS;
}
