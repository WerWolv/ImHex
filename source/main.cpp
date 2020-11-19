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

#include "providers/provider.hpp"

#include <vector>

int main() {
    hex::Window window;

    // Shared Data
    std::vector<hex::lang::PatternData*> patternData;
    hex::prv::Provider *dataProvider = nullptr;

    // Create views
    window.addView<hex::ViewHexEditor>(dataProvider, patternData);
    window.addView<hex::ViewPattern>(dataProvider, patternData);
    window.addView<hex::ViewPatternData>(dataProvider, patternData);
    window.addView<hex::ViewHashes>(dataProvider);
    window.addView<hex::ViewInformation>(dataProvider);
    window.addView<hex::ViewStrings>(dataProvider);
    window.addView<hex::ViewHelp>();
    window.addView<hex::ViewTools>();

    window.loop();

    return 0;
}
