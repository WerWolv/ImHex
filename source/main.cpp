#include "window.hpp"

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

#include "providers/provider.hpp"

#include <vector>

int mainArgc;
char **mainArgv;

int main(int argc, char **argv) {
    mainArgc = argc;
    mainArgv = argv;

    hex::Window window;

    // Shared Data
    std::vector<hex::lang::PatternData*> patternData;
    hex::prv::Provider *dataProvider = nullptr;

    // Create views
    window.addView<hex::ViewHexEditor>(dataProvider, patternData);
    window.addView<hex::ViewPattern>(dataProvider, patternData);
    window.addView<hex::ViewPatternData>(dataProvider, patternData);
    window.addView<hex::ViewDataInspector>(dataProvider);
    window.addView<hex::ViewHashes>(dataProvider);
    window.addView<hex::ViewInformation>(dataProvider);
    window.addView<hex::ViewStrings>(dataProvider);
    window.addView<hex::ViewDisassembler>(dataProvider);
    window.addView<hex::ViewBookmarks>(dataProvider);
    window.addView<hex::ViewPatches>(dataProvider);
    window.addView<hex::ViewTools>(dataProvider);
    window.addView<hex::ViewCommandPalette>();
    window.addView<hex::ViewHelp>();

    if (argc > 1)
        hex::View::postEvent(hex::Events::FileDropped, argv[1]);

    window.loop();

    return 0;
}
