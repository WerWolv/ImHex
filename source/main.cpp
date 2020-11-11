#include "window.hpp"

#include "views/highlight.hpp"
#include "views/view_hexeditor.hpp"
#include "views/view_pattern.hpp"
#include "views/view_pattern_data.hpp"
#include "views/view_hashes.hpp"
#include "views/view_entropy.hpp"

#include "providers/provider.hpp"

#include <vector>

int main() {
    hex::Window window;

    // Shared Data
    std::vector<hex::Highlight> highlights;
    hex::prv::Provider *dataProvider = nullptr;

    // Create views
    window.addView<hex::ViewHexEditor>(dataProvider, highlights);
    window.addView<hex::ViewPattern>(highlights);
    window.addView<hex::ViewPatternData>(dataProvider, highlights);
    window.addView<hex::ViewHashes>(dataProvider);
    window.addView<hex::ViewEntropy>(dataProvider);

    window.loop();

    if (dataProvider != nullptr)
        delete dataProvider;

    return 0;
}
