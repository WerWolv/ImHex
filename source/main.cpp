#include "window.hpp"

#include "views/view_hexeditor.hpp"
#include "views/view_pattern.hpp"

int main() {
    hex::Window window;

    auto* hexEditor = window.addView<hex::ViewHexEditor>();
    window.addView<hex::ViewPattern>(hexEditor);

    window.loop();

    return 0;
}
