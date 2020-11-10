#include "window.hpp"

#include "views/highlight.hpp"
#include "views/view_hexeditor.hpp"
#include "views/view_pattern.hpp"
#include "views/view_pattern_data.hpp"

#include <tuple>
#include <vector>

int main() {
    hex::Window window;

    // Shared Data
    std::vector<hex::Highlight> highlights;
    FILE *file = nullptr;

    // Create views
    window.addView<hex::ViewHexEditor>(file, highlights);
    window.addView<hex::ViewPattern>(highlights);
    window.addView<hex::ViewPatternData>(file, highlights);

    window.loop();

    return 0;
}
