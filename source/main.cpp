#include "helpers/utils.hpp"
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
    hex::prv::Provider::setProviderStorage(dataProvider);

    // Create views
    window.addView<hex::ViewHexEditor>(patternData);
    window.addView<hex::ViewPattern>(patternData);
    window.addView<hex::ViewPatternData>(patternData);
    window.addView<hex::ViewDataInspector>();
    window.addView<hex::ViewHashes>();
    window.addView<hex::ViewInformation>();
    window.addView<hex::ViewStrings>();
    window.addView<hex::ViewDisassembler>();
    window.addView<hex::ViewBookmarks>();
    window.addView<hex::ViewPatches>();
    window.addView<hex::ViewTools>();
    window.addView<hex::ViewCommandPalette>();
    window.addView<hex::ViewHelp>();

    if (argc > 1)
        hex::View::postEvent(hex::Events::FileDropped, argv[1]);

    window.loop();

    return 0;
}
